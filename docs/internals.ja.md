# 内部設計

[English](internals.md) | **日本語**

本ドキュメントは attest のアーキテクチャ、設計判断、プラットフォーム固有の実装詳細を解説します。

## アーキテクチャ概観

```
attest/
├── include/attest/
│   └── attest.h           # 公開 API
├── src/
│   ├── attest.c           # エントリポイント、テスト実行ループ
│   ├── attest_main.c      # attest_main() を呼ぶ main() ラッパ
│   ├── attest_cli.c       # CLI パース
│   ├── attest_assert.c    # アサーション実装、コンテキスト管理
│   ├── attest_capture.c   # stderr キャプチャ
│   ├── attest_fixture.c   # フィクスチャレジストリと実行
│   ├── attest_parallel.c  # 並列実行（ワーカープール）
│   └── internal/
│       ├── attest_internal.h      # 内部型、プラットフォームマクロ
│       ├── attest_context.h       # コンテキスト型、タイムアウト/フィクスチャ アクセサ
│       ├── attest_timeout.h       # プラットフォーム非依存のタイムアウトインターフェース
│       ├── attest_timeout_posix.c # POSIX タイムアウト（SIGALRM、Human68k スタブを含む）
│       ├── attest_timeout_win32.c # Windows タイムアウト（スレッド + イベント）
│       └── attest_registry.c      # テスト登録ストレージ
└── tests/
    └── selftest_main.c    # フレームワークの selftest
```

## コアコンポーネント

### テストレジストリ

**ファイル：** `src/internal/attest_registry.c`

- スイート名、テスト名、ファイル位置とともに登録済みテストを格納
- 初期化後はフリーズ（遅延登録不可）
- テストは登録順に格納

### 実行エンジン

**ファイル：** `src/attest.c`

- `attest_main()` がエントリポイント
- 致命的アサーション処理に `setjmp/longjmp` を使用
- 各テストは自分の `setjmp` コンテキスト内で動く
- 実行フロー：レジストリ凍結 → フィルタ → 実行 → サマリ → 終了コード

### アサーションシステム

**ファイル：** `src/attest_assert.c`

- `_Generic` マクロが型ごとのハンドラへディスパッチ：
  - `att_handle_compare_signed` — 符号付き整数（`long long` にキャスト）
  - `att_handle_compare_unsigned` — 符号なし整数（`unsigned long long` にキャスト）
  - `att_handle_compare_double` — `float` / `double`
  - `att_handle_compare_long_double` — `long double`
  - `att_handle_compare_pointer` — ポインタ（`uintptr_t` として比較）
- コンテキスト状態が追跡するもの：現在のテスト、失敗カウント、longjmp バッファ、タイムアウト状態、info スタック
- 符号付き / 符号なし / double / long double 用の比較関数、フォーマッタ、ハンドラ群はそれぞれ単一のテキストテンプレートを共有しており、ファイルローカルな `ATT_DEFINE_COMPARE` / `ATT_DEFINE_FORMATTER` / `ATT_DEFINE_HANDLER` マクロから生成されます（`src/attest_assert.c` 参照）。新しい数値型を増やす作業は、各ファミリにつきマクロ呼び出し 1 行で済みます。ポインタ、bool、文字列のハンドラは（16 進アドレス、`(null)` センチネル、複数行の差分出力など）固有のフォーマットが必要なため、手書きのまま残しています。

### CLI パーサ

**ファイル：** `src/attest_cli.c`

- `--list`、`--filter`、`--no-color`、`--timeout-ms`、`--shuffle`、`--jobs`、`--format`、`--output` をパース
- `--format` は `default` / `tap` / `junit` を受理し、`--output` は `--format=junit` のときだけ有効でそれ以外はエラーになる
- `--jobs=auto` と `--jobs=0` はどちらも `att_get_cpu_count()`（POSIX は sysconf、Windows は `GetSystemInfo`）で検出した CPU 数に解決される
- フィルタ構文：ワイルドカード（`*`、`?`）、省略形（`Suite` → `Suite.*`）、複数パターン（`;` 区切り）、否定フィルタ（`-Pattern`）
- 不明なオプション → 終了コード 2

### フィクスチャ

**ファイル：** `src/attest_fixture.c`

- フィクスチャエントリは起動時に登録
- setup/teardown 関数はフィクスチャ型名で検索
- teardown は ASSERT 失敗やスキップ後でも必ず実行

### 出力キャプチャ

**ファイル：** `src/attest_capture.c`

- `att_capture_begin()` が stderr を内部バッファへリダイレクト
- `att_capture_end()` がキャプチャされた内容を返す
- 非リエントラント（ネスト不可）

### 並列実行

**ファイル：** `src/attest_parallel.c`

- スレッド数を設定可能なワーカープール構成
- 各ワーカーはスレッドローカルコンテキスト（`g_ctx`）を保持
- 結果はテストごとに収集し、登録順に出力
- ミューテックスで保護されたワークキューでテストを分配
- コンパイル・実行に入るのは `ATT_THREADS_POSIX` のみ。他のスレッドバックエンド（`ATT_THREADS_C11`、`ATT_THREADS_WIN32`、`ATT_THREADS_NONE`）では `--jobs=N` 指定でも逐次ランナーへ落ちる
- 並列ランナーが出すのはデフォルトの人間向けフォーマットだけ。TAP のテスト行と JUnit XML 出力は逐次パスでのみ生成されるため、`--format=tap` や `--format=junit` を `--jobs > 1` と組み合わせるのはサポート外

---

## プラットフォームサポート

### プラットフォーム判定

`src/internal/attest_internal.h` で定義：

| マクロ | 意味 |
|--------|------|
| `ATT_PLATFORM_WINDOWS` | Windows（任意） |
| `ATT_PLATFORM_POSIX` | POSIX 準拠（Linux、macOS など） |
| `ATT_PLATFORM_HUMAN68K` | Sharp X680x0（Human68k）— シングルスレッド、タイムアウトなし |
| `ATT_COMPILER_MSVC` | Microsoft Visual C++ |
| `ATT_COMPILER_GCC_LIKE` | GCC または Clang |

### スレッド対応の判定

| マクロ | 条件 |
|--------|------|
| `ATT_THREADS_C11` | C11 `<threads.h>` が利用可能 |
| `ATT_THREADS_POSIX` | POSIX threads（`pthread`） |
| `ATT_THREADS_WIN32` | Windows threads |
| `ATT_THREADS_NONE` | スレッド非対応 |

### setjmp/longjmp 抽象化

POSIX ではシグナルマスクを保つため `sigsetjmp/siglongjmp`、Windows では標準の `setjmp/longjmp` を使用します。

```c
typedef att_jmp_buf;
#define att_setjmp(env)      // プラットフォーム依存
#define att_longjmp(env, v)  // プラットフォーム依存
```

**重要な制約：** `setjmp` は呼び出し元のスタックフレーム内で直接呼ばれなければなりません（関数呼び出し越しには不可）。これはマクロ展開で強制しています。

### コンパイラ属性

| 属性 | GCC/Clang | MSVC |
|------|-----------|------|
| Constructor | `__attribute__((constructor))` | `.CRT$XCU` セクション |
| Alignment | `__attribute__((aligned(n)))` | `__declspec(align(n))` |
| Cleanup | `__attribute__((cleanup(fn)))` | サポートなし |
| Thread-local | `__thread` または `_Thread_local` | `__declspec(thread)` |

### メモリ確保

コンテキスト構造体用のアラインメント付きアロケーション：

| プラットフォーム | 関数 |
|-----------------|------|
| Linux/GCC | `aligned_alloc()`（C11） |
| MSVC | `_aligned_malloc()` / `_aligned_free()` |
| その他 | 標準の `malloc()` / `free()` |

---

## タイムアウト実装

### POSIX

- `sigaction()` と `setitimer(ITIMER_REAL)` を使用
- シグナルハンドラがタイムアウトフラグを立てて `longjmp` を呼ぶ
- 無限ループにも有効（シグナルが実行を中断する）

### Windows

- `_beginthreadex()` で専用のタイマースレッドを起動
- タイマースレッドは手動リセットの `CreateEvent()` ハンドルを `WaitForSingleObject(event, timeout_ms)` で待機する。`WAIT_TIMEOUT` ならば締切超過を意味し、スレッドは `triggered` フラグを立てたうえでイベントを再シグナルし、メインスレッドが観測できるようにする。`WAIT_OBJECT_0` の場合は `att_timeout_stop()` が待機を早期キャンセルしたことを意味する。
- メインスレッドは `att_context_record_assert()` から同じイベントを `WaitForSingleObject(event, 0)` でポーリングする。アサーション 32 回ごとに 1 度に絞ってアサーションあたりのコストを無視できる程度に抑えている。ヒットしたら `att_context_abort()` を呼んでテストランナーへ longjmp で戻る。
- **制限：** メインスレッドは次のアサーションマクロを実行したときにしかタイムアウトを認識できないため、アサーションを呼ばないタイトループは Windows では中断できない。タイマースレッド自体は発火しているが、失敗の記録はアサーション経由か `att_context_end()` への制御戻りまで遅延する。長時間走るコードパスでは、定期的なアサーション（`EXPECT_TRUE(true)` でも可）を挟んでタイムアウトが効くようにすること。

---

## 並列実行設計

### アーキテクチャ

```
[メインスレッド]
     │
     ├─ レジストリ凍結
     ├─ ワークキュー初期化（ミューテックス保護のインデックス）
     ├─ 結果配列の確保
     │
     ├─────────────────────────────────────────────┐
     ▼                                             ▼
[Worker 1]                                    [Worker N]
     │                                             │
     ├─ ループ:                                    ├─ ループ:
     │   ├─ ミューテックス取得                     │   ├─ ミューテックス取得
     │   ├─ 次のテストインデックスを取得           │   ├─ 次のテストインデックスを取得
     │   ├─ ミューテックス解放                     │   ├─ ミューテックス解放
     │   ├─ スレッドローカルコンテキスト初期化     │   ├─ スレッドローカルコンテキスト初期化
     │   ├─ テスト実行                             │   ├─ テスト実行
     │   └─ 結果保存                               │   └─ 結果保存
     │                                             │
     └─────────────────────────────────────────────┘
                          │
                          ▼
                   [メインスレッド]
                          │
                          ├─ 全ワーカーを join
                          ├─ 結果出力（登録順）
                          └─ サマリを返す
```

### スレッドローカルストレージ

各ワーカースレッドは自分の `g_ctx` を持ちます：

```c
#if defined(ATT_THREADS_C11)
    _Thread_local att_context_state *g_ctx;
#elif defined(ATT_THREADS_POSIX)
    __thread att_context_state *g_ctx;
#elif defined(ATT_THREADS_WIN32)
    __declspec(thread) att_context_state *g_ctx;
#endif
```

### 出力バッファリング

- 各テストの出力はテストごとのバッファにキャプチャ
- 全ワーカー完了後、メインスレッドが登録順に出力
- 並走するテスト間で出力が混ざらないようにする

---

## 既知の問題

### テストランナーにおけるクロスフレーム setjmp（解決済み）

**症状：** Apple Silicon 上で full selftest を実行すると SIGILL / SIGFPE / SIGSEGV が発生。GCC 12.5、13.4、15.2 と Clang 22.1 の `-O2` 以上で観測。GCC 14.2（旧ポイントリリース）と Clang 22 のリグレッションでは特に顕著で、過去のリリースでは「GCC 14.2.0 sigsetjmp バグ」と整理し、`CMakeLists.txt` で ARM64 に `-O1` ワークアラウンドを当てていた。

**根本原因：** attest 自身のテストランナー。元の `att_context_protect()` は `src/attest_assert.c` のアウトオブライン関数で、`setjmp` を呼んで return していた。`src/attest.c`（あるいは `src/attest_parallel.c`）のランナーが後から `longjmp` で戻ろうとした時点で、その `jmp_buf` はすでに存在しないスタックフレームを指している。これは未定義動作で、現代の GCC/Clang オプティマイザは「そんなことは起こらない」前提で容赦なくランナーが頼っているコードを巻き上げ・削除する。

**修正：** `att_context_protect()` を `src/internal/attest_context.h` のマクロにした：

```c
#define att_context_protect() att_setjmp(*att__get_abort_env_ptr())
```

`attest.c` と `attest_parallel.c` のいずれも、このマクロを `longjmp` を受けるのと同じスタックフレーム内で展開するため、`jmp_buf` は有効なまま保たれる。`-O1` ワークアラウンド、`-fno-omit-frame-pointer` / `-fno-optimize-sibling-calls` 緩和策、`sigjmp_buf` を 16 バイト境界に明示アラインさせていたガードは、すべて削除済み。Apple Silicon 上の GCC 12〜15 / Clang 16〜22 でデフォルトの `Release` 最適化レベルにて検証済み。

サブテスト側では同じ制約が `att_subtest_scope_protect`（`include/attest/attest.h` のマクロ）として既に適用されていた。

### Windows setjmp スタック破壊（解決済み）

**問題：** 関数呼び出し越しに `setjmp` を使うと `STATUS_BAD_STACK (0xC0000028)` が発生。

**解決：** `setjmp` は呼び出し元のスタックフレームへ直接マクロ展開する必要がある。

```c
// 誤り：setjmp を関数でラップ
int helper(void) { return setjmp(env); }  // return 後、env は無効

// 正解：マクロ展開
#define PROTECT() setjmp(*get_env_ptr())
if (PROTECT() == 0) { ... }  // setjmp は今のスタックフレームで実行
```

これは前述のクロスフレーム `setjmp` 修正がトップレベルのテストランナーへ拡張したものと同じ制約。

### MSVC の SCOPED_INFO 制限

MSVC は `__attribute__((cleanup))` をサポートしません。`SCOPED_INFO` は文脈を push しますが自動 pop されません。テスト終了時にスタックがリセットされるため、機能上は等価です。

---

## 実装詳細

### テスト命名

- 形式：`Suite.Name`
- 重複は初期化時に検出 → 終了コード 3
- エラー報告のためファイルパスと行番号も保存

### 致命的 vs 非致命的アサーション

| 種類 | 挙動 | 実装 |
|------|------|------|
| `ASSERT_*` | 現在のテストを中断 | テストの `setjmp` ポイントへ `longjmp` |
| `EXPECT_*` | 失敗を記録して継続 | 失敗カウンタを増分 |

### 文字列比較

- `NULL == NULL` → 真
- `NULL` と非NULL → 偽
- 出力では NULL ポインタを `"(null)"` と表示

### 浮動小数点比較

- `NEAR`: `fabs(a - b) <= epsilon`
- `NEAR_REL`: `fabs(a - b) <= rel_eps * max(fabs(a), fabs(b))`
  - ゼロ近傍（< 1e-15）の値は絶対比較に切り替え
- `ULP_EQ`: IEEE 754 のビット表現上の距離
  - 非正規化数や符号遷移を扱う
- NaN は常に失敗
- ±Infinity は同符号で完全一致した場合のみ一致

### 終了コード

| コード | 条件 |
|--------|------|
| 0 | 全テスト合格、または `--list` モード |
| 1 | 1 件以上の失敗 |
| 2 | CLI エラー（不明オプション） |
| 3 | 初期化エラー（名前重複、内部失敗） |

---

## 開発ガイドライン

### コードスタイル

- WebKit ベース：タブ（幅 4）、右側ポインタ揃え
- コミット前に `clang-format -i <file>` を実行
- 1 行は 100 列を目安

### 命名規約

| 種別 | 規約 | 例 |
|------|------|----|
| 公開 API | `att_` プレフィックス、snake_case | `att_run_subtest` |
| マクロ | UPPER_SNAKE_CASE | `ASSERT_EQ` |
| ファイル | snake_case | `attest_assert.c` |

### 変更のテスト

1. `tests/selftest_main.c` に selftest を追加
2. フルスイートを実行：`./build/attest_selftest`
3. CLI 動作を確認：`--list`、`--filter=...`
4. 可能であれば複数プラットフォームで検証
