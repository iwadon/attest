# attest 並列実行設計

この文書は attest フレームワークにおけるテスト並列実行機能の設計・実現可能性を検討するものです。

---

## 1. 概要

テストの実行時間を短縮するため、複数のテストを並列に実行する機能を提供します。

### 目標
- テスト実行時間の短縮
- マルチコア環境の効率的な利用
- 既存のテストコードとの互換性維持

---

## 2. 基本仕様（案）

### 2.1 コマンドラインオプション
```
--jobs=N
```
- `N`: 最大同時実行数（1以上の整数）
- デフォルト: 1（並列実行なし）
- `N=0` または `--jobs=auto`: CPU コア数を自動検出

### 2.2 実行単位
- テスト関数単位で並列化
- Fixture の setup/teardown は各テストごとに独立して実行

---

## 3. 技術的課題

### 3.1 スレッド安全性

#### 3.1.1 グローバル変数の現状分析

**読み取り専用（スレッドセーフ）:**
- `g_registry` (attest_registry.c:10)
  - テスト実行開始時に `frozen=true` に設定され、以降変更されない
  - 複数スレッドからの同時読み取りは安全

- `g_fixture_entries` (attest_fixture.c:15)
  - テスト登録フェーズで初期化され、実行フェーズでは読み取り専用
  - 複数スレッドからの同時読み取りは安全

**書き込みあり（要対応）:**
- **`g_ctx`** (attest_assert.c:42-43) - **最重要課題**
  - 各テストのコンテキスト状態を保持
  - 内容: テスト結果、longjmp バッファ、fixture 状態、failure ログ、timeout 状態、info stack
  - **対策**: スレッドローカルストレージ化が必須
    - C11: `_Thread_local`
    - POSIX: `pthread_key_t` + `pthread_getspecific/setspecific`
    - Windows: `__declspec(thread)` または TLS API

- `g_timeout_handler_installed` (attest_assert.c:49)
  - シグナルハンドラのインストール状態フラグ
  - **対策**: 一度きりの初期化なので、atomic または mutex で保護

#### 3.1.2 標準出力の競合
- 現在の実装では `fprintf(stderr, ...)` を直接呼び出し
- 並列実行時は複数スレッドからの出力が混在
- **対策**:
  1. テストごとに出力をメモリバッファにキャプチャ
  2. テスト完了後、メインスレッドで登録順に出力
  3. JUnit形式では既に `failure_log` にバッファリングされているため、この機構を拡張可能

### 3.2 タイムアウト機構の課題

**現在の実装 (attest_assert.c:353-375):**
```c
void att_context_timeout_start(int timeout_ms) {
    // ...
    setitimer(ITIMER_REAL, &timer, NULL);  // プロセス全体で1つのみ
}
```

**問題点:**
- `ITIMER_REAL` はプロセス全体で1つのみ使用可能
- 並列実行時に複数のテストが同時にタイマーを設定すると競合

**対策案:**
1. **スレッドごとの監視スレッド方式**
   - 各ワーカースレッドに対応する監視スレッドを作成
   - 監視スレッドは `sleep()` でタイムアウトを待ち、必要に応じてワーカースレッドをキャンセル
   - POSIX: `pthread_cancel()` または条件変数

2. **タイマースレッド方式**
   - 1つのタイマー管理スレッドが全テストのタイムアウトを一元管理
   - 優先度キューで次にタイムアウトするテストを追跡

3. **スレッド非対応環境向けの fallback**
   - 並列実行無効時は既存の `setitimer` を使用

### 3.3 実行順序の保証
- 出力は登録順を維持
- 実行順序自体は不定（並列実行のため）
- 依存関係のあるテストは順次実行する必要あり（スコープ外）

### 3.4 共有リソース
- ファイルシステム、環境変数、グローバル状態など
- ユーザー責任で管理（ドキュメントで注意喚起）
- 将来的に `ATT_SERIAL` マクロで明示的に順次実行を指定可能にする案

### 3.5 スレッド対応の検出とフォールバック

**コンパイル時検出:**
```c
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
  #define ATT_THREADS_C11 1
#elif defined(_POSIX_VERSION) && _POSIX_VERSION >= 200112L
  #define ATT_THREADS_POSIX 1
#elif defined(_WIN32) || defined(_WIN64)
  #define ATT_THREADS_WIN32 1
#else
  #define ATT_THREADS_NONE 1
#endif
```

**動作の分岐:**
- `ATT_THREADS_NONE` が定義される環境（Human68k 等）では:
  - `--jobs` オプション指定時にエラーまたは警告を表示
  - 常に逐次実行モードで動作
  - ビルド時に並列実行関連のコードを完全に除外可能

---

## 4. 実装アプローチ

### 4.1 スレッドプール方式
```
[Main Thread]
  ↓
  1. registry を frozen 状態に設定（既存）
  2. テストキューを初期化
  3. 結果バッファを初期化（テスト数分の配列）
  ↓
[Worker Thread 1] [Worker Thread 2] ... [Worker Thread N]
  ↓                ↓                     ↓
  各ワーカー:
    - キューから次のテスト index を取得（mutex保護）
    - スレッドローカルな g_ctx を初期化
    - テスト実行
    - 結果を results[index] に格納（index は排他的なので mutex 不要）
  ↓
[Main Thread]
  ↓
  すべてのワーカーの完了を待機
  ↓
  結果を登録順（index順）に出力
```

### 4.2 データ構造

#### 4.2.1 既存の構造を活用
```c
// 既存: att_test_case (attest_internal.h:9-16)
// テスト情報は g_registry->tests[] に既に格納されている

// 既存: att_test_result (attest_context.h:9-19)
// テスト結果の構造は既に定義済み

// 新規: 並列実行用の結果配列
typedef struct {
    const att_test_case *test;
    att_test_result result;
    bool completed;
} att_parallel_result;
```

#### 4.2.2 ワーカープール
```c
typedef struct {
    size_t next_test_index;  // 次に実行するテストの index
    size_t total_tests;      // 実行対象のテスト総数
    pthread_mutex_t lock;    // next_test_index の保護用
    const att_registry *registry;
    const att_cli_options *options;
    att_parallel_result *results;  // 結果配列（テスト数分）
} att_worker_pool;

typedef struct {
    pthread_t thread;
    att_worker_pool *pool;
    int worker_id;
} att_worker;
```

### 4.3 実行フロー

#### 4.3.1 メインスレッド（att_run_tests内）
```c
int att_run_tests(const att_registry *registry, const att_cli_options *opts, att_summary *summary) {
    // 既存: registry の frozen 確認、フィルタリングなど

    if (opts->jobs > 1 && ATT_THREADS_SUPPORTED) {
        // 並列実行パス
        return att_run_tests_parallel(registry, opts, summary);
    } else {
        // 既存の逐次実行パス（変更なし）
        return att_run_tests_sequential(registry, opts, summary);
    }
}
```

#### 4.3.2 並列実行パス
```c
int att_run_tests_parallel(...) {
    // 1. ワーカープール初期化
    att_worker_pool pool = {
        .next_test_index = 0,
        .total_tests = count_filtered_tests(registry, opts),
        .registry = registry,
        .options = opts,
        .results = calloc(pool.total_tests, sizeof(att_parallel_result))
    };
    pthread_mutex_init(&pool.lock, NULL);

    // 2. ワーカースレッド起動
    size_t worker_count = opts->jobs;
    att_worker *workers = malloc(worker_count * sizeof(att_worker));
    for (size_t i = 0; i < worker_count; i++) {
        workers[i].pool = &pool;
        workers[i].worker_id = i;
        pthread_create(&workers[i].thread, NULL, att_worker_main, &workers[i]);
    }

    // 3. すべてのワーカーの完了を待機
    for (size_t i = 0; i < worker_count; i++) {
        pthread_join(workers[i].thread, NULL);
    }

    // 4. 結果を登録順に出力
    for (size_t i = 0; i < pool.total_tests; i++) {
        att_output_test_result(&pool.results[i], opts);
    }

    // 5. クリーンアップ
    // ...
}
```

#### 4.3.3 ワーカースレッド
```c
void *att_worker_main(void *arg) {
    att_worker *worker = arg;
    att_worker_pool *pool = worker->pool;

    while (true) {
        // キューから次のテストを取得（排他制御）
        pthread_mutex_lock(&pool->lock);
        if (pool->next_test_index >= pool->total_tests) {
            pthread_mutex_unlock(&pool->lock);
            break;  // すべてのテストが割り当て済み
        }
        size_t test_index = pool->next_test_index++;
        pthread_mutex_unlock(&pool->lock);

        // テスト実行（排他制御不要 - 各ワーカーは独立）
        const att_test_case *test = get_filtered_test(pool->registry, pool->options, test_index);

        // スレッドローカルなコンテキストで実行
        att_context_begin(test, pool->options->color_enabled, pool->options->format);
        // タイムアウト設定は並列実行用の新機構を使用
        att_parallel_timeout_start(pool->options->timeout_ms, worker->worker_id);

        int protect_rc = att_context_protect();
        if (protect_rc == 0) {
            test->fn();
        }

        att_context_end(&pool->results[test_index].result);
        pool->results[test_index].test = test;
        pool->results[test_index].completed = true;
    }

    return NULL;
}
```

### 4.4 スレッドローカルストレージの実装

```c
// attest_context.c (新規)
#if defined(ATT_THREADS_C11)
    _Thread_local att_context_state *g_ctx = NULL;
#elif defined(ATT_THREADS_POSIX)
    pthread_key_t g_ctx_key;
    pthread_once_t g_ctx_key_once = PTHREAD_ONCE_INIT;

    static void att_ctx_key_init(void) {
        pthread_key_create(&g_ctx_key, NULL);
    }

    static att_context_state *att_get_ctx(void) {
        pthread_once(&g_ctx_key_once, att_ctx_key_init);
        return pthread_getspecific(g_ctx_key);
    }

    static void att_set_ctx(att_context_state *ctx) {
        pthread_once(&g_ctx_key_once, att_ctx_key_init);
        pthread_setspecific(g_ctx_key, ctx);
    }
#elif defined(ATT_THREADS_WIN32)
    __declspec(thread) att_context_state *g_ctx = NULL;
#else
    // スレッド非対応: 既存のグローバル変数
    static att_context_state g_ctx_root;
    static att_context_state *g_ctx = &g_ctx_root;
#endif
```

---

## 5. 段階的実装計画

### Phase 0: コードベース再構成（前提作業）
**目的:** 既存コードを並列実行に備えて整理

- [ ] `att_run_tests()` を `att_run_tests_sequential()` に改名し、既存ロジックを保持
- [ ] 新しい `att_run_tests()` を作成し、並列/逐次の分岐を実装
- [ ] スレッド検出マクロの追加 (`ATT_THREADS_*`)
- [ ] CLI に `--jobs=N` オプションを追加（スレッド非対応環境では警告表示）
- [ ] `att_cli_options` 構造体に `int jobs` フィールドを追加

**検証:** 既存のテストがすべてパスすることを確認

### Phase 1: スレッドローカルストレージ化（POSIX優先）
**目的:** `g_ctx` をスレッドセーフにする

#### 1.1 TLS 抽象化レイヤー
- [ ] `att_tls_ctx_get()` / `att_tls_ctx_set()` 関数の実装
- [ ] POSIX pthread版の実装
- [ ] 非スレッド環境向けフォールバック（既存のグローバル変数）

#### 1.2 既存コードの移行
- [ ] `attest_assert.c` 内のすべての `g_ctx` アクセスを TLS 関数経由に変更
- [ ] `att_context_begin()` でスレッドローカルなコンテキストを割り当て
- [ ] `att_context_end()` でスレッドローカルなコンテキストを解放

**検証:** 逐次実行モードで既存テストがすべてパスすることを確認

### Phase 2: 基本的な並列実行（POSIX のみ）
**目的:** 最小限の並列実行機能を実装

#### 2.1 ワーカープール実装
- [ ] `att_worker_pool` 構造体の定義
- [ ] `att_worker` 構造体の定義
- [ ] `att_worker_main()` 関数の実装
- [ ] `att_run_tests_parallel()` 関数の実装

#### 2.2 結果バッファリング
- [ ] `att_parallel_result` 構造体の定義
- [ ] 結果配列の初期化・クリーンアップ
- [ ] 登録順での結果出力処理

#### 2.3 出力の分離
- [ ] デフォルト形式: 各テストの出力を完全にバッファリング
- [ ] TAP 形式: バッファリングして順次出力
- [ ] JUnit 形式: 既存の `failure_log` を活用

**検証:**
- `--jobs=2` で簡単なテストが並列実行されることを確認
- 出力が混在しないことを確認
- 結果の順序が登録順であることを確認

### Phase 3: タイムアウト対応
**目的:** 並列実行時のタイムアウト機構を実装

#### 3.1 監視スレッド方式の実装（POSIX）
- [ ] ワーカースレッドごとの監視スレッド起動
- [ ] タイムアウト時のスレッドキャンセル処理
- [ ] 既存の `setitimer` ベースの実装との切り替え

**検証:**
- `--jobs=2 --timeout-ms=100` で長時間実行テストが適切にタイムアウトすることを確認

### Phase 4: クロスプラットフォーム対応
**目的:** Windows と C11 threads.h のサポート

#### 4.1 Windows 対応
- [ ] TLS: `__declspec(thread)` または Win32 TLS API
- [ ] スレッド作成: Win32 threads
- [ ] Mutex: CRITICAL_SECTION
- [ ] タイムアウト: Windows イベント

#### 4.2 C11 threads.h 対応（オプション）
- [ ] TLS: `_Thread_local`
- [ ] スレッド: `thrd_create` / `thrd_join`
- [ ] Mutex: `mtx_t`

**検証:** 各プラットフォームでテストスイートがパスすることを確認

### Phase 5: 高度な制御（将来機能）
**目的:** ユーザーによる並列実行の細かい制御

- [ ] `ATT_SERIAL` マクロで特定のテストを順次実行指定
- [ ] テストグループごとの並列度制御
- [ ] fail-fast オプション（1つでも失敗したら全体を停止）

---

## 6. 各フェーズの優先度

| Phase | 優先度 | 理由 |
|-------|--------|------|
| Phase 0 | 必須 | すべての後続作業の前提 |
| Phase 1 | 必須 | 並列実行の核心課題 |
| Phase 2 | 必須 | 基本的な並列実行機能 |
| Phase 3 | 高 | タイムアウトは重要機能だが、回避策あり（タイムアウトなしで実行）|
| Phase 4 | 中 | POSIX 対応が完了すれば、主要な環境はカバー済み |
| Phase 5 | 低 | 利便性向上だが、必須ではない |

---

## 7. パフォーマンス見積もり

### 前提
- テスト数: 1000
- 平均実行時間: 10ms/test
- CPU コア数: 8

### 理論値
- 順次実行: 10s
- 並列実行（8 並列）: 10s / 8 = 1.25s
- オーバーヘッド込み: 約 1.5-2s

### 制約
- I/O バウンドなテストでは効果が薄い
- セットアップコストが大きい場合は逆効果の可能性
- テストの実行時間が不均一な場合、最後の長時間テストがボトルネックになる

### 実際の効果が期待できるケース
- CPU バウンドなテスト（計算処理、アルゴリズム検証など）
- 大量の短時間テスト
- テスト間にリソース競合がない場合

---

## 8. リスクと対策

| リスク | 影響度 | 対策 |
|--------|--------|------|
| **デッドロック** | 高 | タイムアウト機構で検出・回復 |
| **出力の混在** | 高 | 完全なバッファリングで解決（実装済み方針） |
| **共有リソース競合** | 中 | ドキュメントで注意喚起 + 将来的に ATT_SERIAL サポート |
| **デバッグ困難化** | 中 | 各テストの出力を明確に分離、順次実行モードへの切り替えを推奨 |
| **TLS 実装の複雑性** | 中 | 段階的実装とテストによる検証 |
| **タイムアウト実装の複雑性** | 中 | Phase 3 で対応、それまではタイムアウトなしで運用可能 |
| **プラットフォーム依存性** | 低 | 非対応環境では逐次実行にフォールバック |

---

## 9. 互換性と下位互換性の保証

### デフォルト動作
- `--jobs` オプション未指定時は `--jobs=1`（逐次実行）
- 既存の挙動を完全に維持
- 既存のテストコードに変更不要

### スレッド非対応環境
- コンパイル時に `ATT_THREADS_NONE` が定義される
- `--jobs` オプション指定時にエラーメッセージを表示:
  ```
  error: parallel execution is not supported on this platform
  ```
- または警告を表示して逐次実行にフォールバック:
  ```
  warning: ignoring --jobs option (parallel execution not supported)
  ```

### 既存機能との共存
- すべての出力フォーマット（default, TAP, JUnit）で動作
- タイムアウト機能との併用（Phase 3 以降）
- フィルタリング、カラー出力などの既存機能は引き続き動作

---

## 10. 実装における留意事項

### 10.1 既存コードの変更最小化
- `att_run_tests()` の既存ロジックは `att_run_tests_sequential()` として保存
- 新規の並列実行コードは別ファイル（`attest_parallel.c`）に分離
- `g_ctx` のアクセス方法を抽象化（TLS ラッパー関数）

### 10.2 テスト駆動開発
各フェーズで以下を実施:
1. フェーズの目標を達成する最小限のコードを実装
2. 既存のテストスイートがすべてパスすることを確認
3. 並列実行用の新しいテストを追加
4. リファクタリングと最適化

### 10.3 ドキュメント更新
- ユーザー向けドキュメントに `--jobs` オプションの説明を追加
- 並列実行時の注意事項（共有リソース、グローバル状態など）を明記
- トラブルシューティングガイドの作成

---

## 11. 未解決の検討事項

### 高優先度
- [ ] Phase 3 のタイムアウト実装方式の最終決定
  - 監視スレッド方式 vs タイマースレッド方式
  - pthread_cancel の可否（一部環境で未サポート）

- [ ] 出力バッファリングの詳細設計
  - メモリ使用量の上限設定（大量のテストや長い出力への対処）
  - デフォルト形式での進捗表示方法（並列実行中は出力が遅延する）

### 中優先度
- [ ] `--jobs=auto` の実装
  - CPU コア数の自動検出（sysconf, GetSystemInfo など）
  - デフォルト値の決定（全コア vs コア数-1 など）

- [ ] fail-fast オプションの実装
  - 1つでもテストが失敗したら残りのテストをキャンセル
  - 既に実行中のテストの扱い

### 低優先度
- [ ] テスト実行順序の最適化
  - 過去の実行時間に基づくスケジューリング
  - 長時間テストを優先的に実行してアイドル時間を削減

- [ ] リソース消費量の制限
  - 並列度の動的調整
  - メモリ使用量の監視

---

## 12. 参考実装

- **Google Test**: `--gtest_parallel` (外部スクリプト)
- **pytest**: `-n` オプション（pytest-xdist プラグイン）
- **Rust cargo test**: `--test-threads=N`
- **Go testing**: `go test -parallel N`

これらの実装から学べる点:
- デフォルトで並列実行を有効にする（Go, Rust）
- ユーザーが明示的に有効化する（Google Test, pytest）
- attest は後者のアプローチ（opt-in）を採用
