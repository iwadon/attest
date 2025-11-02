# Windows対応ガイド

## 概要

attestフレームワークはWindows環境でのビルドと実行を完全サポートしています。この文書では、実装されたクロスプラットフォーム対応の詳細、並列実行機能、および既知の制限事項について説明します。

**最新の検証結果** (2025-11-02):
- ✅ Windows (MSVC + C11 threads): 53 tests ran, 52 passed, 1 skipped
- ✅ setjmp/longjmp スタック破損問題: 完全解決
- ✅ 並列実行: `--jobs=4` で **1.85倍** の高速化を達成

## 実装された対応

### 1. プラットフォーム検出

`src/internal/attest_internal.h` にプラットフォームとコンパイラの検出マクロを追加：

- `ATT_PLATFORM_WINDOWS`: Windows環境を検出
- `ATT_PLATFORM_POSIX`: POSIX環境を検出
- `ATT_COMPILER_MSVC`: MSVC コンパイラを検出
- `ATT_COMPILER_GCC_LIKE`: GCC/Clang を検出

### 2. setjmp/longjmp の抽象化

POSIXとWindowsで異なるsetjmp実装を抽象化：

```c
// POSIX: シグナルマスクを保存する sigsetjmp/siglongjmp
// Windows: 標準C の setjmp/longjmp

typedef att_jmp_buf;
#define att_setjmp(env) ...
#define att_longjmp(env, val) ...
```

#### 重要な実装制約: setjmp はマクロ展開で呼び出し元に直接埋め込む必要がある

**C標準の要求**: `setjmp()` を呼び出した関数が `return` した後の `longjmp()` は**未定義動作**です。

**根本原因**: 関数呼び出し経由で `setjmp()` を実行すると、スタックフレームのずれによりWindowsでは `STATUS_BAD_STACK (0xC0000028)`、macOS/Linuxでは `SIGBUS/SIGSEGV` が発生します。

**正しい実装パターン**:

```c
// ❌ 間違い: 関数呼び出し経由
int helper_function(void) {
    return setjmp(env);  // ← 関数がreturnするとenvは無効
}
if (helper_function() == 0) {
    longjmp(env, 1);  // ← 未定義動作！スタック破損
}

// ✅ 正しい: マクロによる直接展開
#define PROTECT() setjmp(*get_env_ptr())
if (PROTECT() == 0) {  // ← setjmpがこのスタックフレームで実行
    longjmp(env, 1);   // ← 正しい
}
```

**実装の詳細** (`src/internal/attest_internal.h`, `src/attest_assert.c`):
- `att_subtest_scope_protect()`: マクロとして定義し、`att_setjmp()` を呼び出し元に直接埋め込み
- `att__get_abort_env_ptr()`: `jmp_buf` のポインタのみを返す関数（`setjmp` は呼ばない）
- `att_run_subtest()`: マクロ経由で `att_setjmp()` を直接実行

この実装により、Windows/macOS/Linux すべてで安全に動作します。

### 3. コンパイラ属性の抽象化

#### アライメント属性

```c
// GCC/Clang: __attribute__((aligned(n)))
// MSVC: __declspec(align(n))
#define ATT_ALIGN(n)
```

#### コンストラクタ属性（自動テスト登録）

```c
// GCC/Clang: __attribute__((constructor))
// MSVC: .CRT$XCU セクションを使用
#define ATT_CONSTRUCTOR
```

#### クリーンアップ属性（スコープ情報）

```c
// GCC/Clang: __attribute__((cleanup(fn)))
// MSVC: サポートなし（手動管理）
#define ATT_CLEANUP(fn)
```

### 4. メモリアロケーション

アライメント付きメモリ確保をプラットフォーム別に実装：

- **Linux/GCC**: `aligned_alloc()` (C11)
- **MSVC**: `_aligned_malloc()` / `_aligned_free()`
- **その他**: 標準の `malloc()` / `free()`

### 5. タイムアウト機能

タイムアウト機能はプラットフォームごとに異なる実装：

- **POSIX**: `sigaction()`, `setitimer()` を使用したシグナルベースの実装
- **Windows**: スレッドと `WaitForSingleObject()` を使用したポーリングベースの実装

#### Windows実装の詳細

Windowsでは以下の仕組みでタイムアウトを実現：

1. **タイマースレッド**: `_beginthreadex()` で別スレッドを作成し、指定時間スリープ
2. **イベントオブジェクト**: タイムアウト発生を通知するための `CreateEvent()`
3. **ポーリングチェック**: 各assertマクロ実行時に `WaitForSingleObject()` でタイムアウトをチェック
4. **クリーンアップ**: テスト終了時にスレッドとイベントを適切に破棄

この実装により、POSIXのシグナル割り込みと同様の動作を実現しています。

### 6. 並列実行サポート (P1+ 機能)

Windows環境で並列テスト実行をフルサポート：

#### スレッドサポート

- **C11 threads** (`_Thread_local`): MSVC の C11/C17 モードで使用
- **Win32 threads** (`__declspec(thread)`): フォールバック実装
- **POSIX threads** (`pthread`): Linux/macOS

#### スレッドローカルストレージ (TLS)

```c
// attest_internal.h
#if defined(ATT_THREADS_C11)
  #define ATT_THREAD_LOCAL _Thread_local
#elif defined(ATT_THREADS_WIN32)
  #define ATT_THREAD_LOCAL __declspec(thread)
#elif defined(ATT_THREADS_POSIX)
  #define ATT_THREAD_LOCAL __thread
#endif

// attest_assert.c
static ATT_THREAD_LOCAL att_context_state *g_ctx;
```

#### パフォーマンスベンチマーク (Windows MSVC)

| オプション | 実行時間 | 高速化率 |
|---------|---------|---------|
| `--jobs=1` | 113.3 ms | baseline |
| `--jobs=4` | 61.1 ms | **1.85x** (46%削減) |
| `--jobs=auto` | 88.2 ms | 1.29x |

#### アーキテクチャ

```c
// attest_parallel.c
typedef struct {
    size_t next_test_index;       // mutex保護
    pthread_mutex_t lock;         // または Win32 CRITICAL_SECTION
    att_parallel_result *results; // 結果配列（登録順）
} att_worker_pool;

// フロー:
// 1. ワーカープール初期化
// 2. N個のワーカースレッド起動
// 3. 各ワーカーがwork queueからテストを取得・実行
// 4. 結果を登録順に出力
```

### 7. ビルドシステム

`CMakeLists.txt` でプラットフォーム別の設定を追加：

```cmake
if(WIN32)
    # Windows: MSVC 警告を抑制
    target_compile_definitions(attest PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    # POSIX: シグナル/タイマー機能のため
    target_compile_definitions(attest PRIVATE _POSIX_C_SOURCE=200809L)
endif()
```

## ビルド方法

### Windows (Visual Studio)

```powershell
# 設定
cmake -S . -B build

# ビルド
cmake --build build --config Debug

# テスト実行
.\build\Debug\attest_selftest.exe
```

### Windows (MSVC コマンドライン)

```cmd
cmake -S . -B build -G "NMake Makefiles"
cmake --build build
build\attest_selftest.exe
```

### Linux/macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/attest_selftest
```

## 既知の制限事項

### 1. Windows タイムアウト実装の制約

Windows版のタイムアウトはポーリングベースのため、以下の制約があります：

**制約**: assertマクロ（`EXPECT_*`, `ASSERT_*`など）が呼ばれないとタイムアウトが検出されません。

**影響**: 以下のようなコードではタイムアウトが機能しません：
```c
TEST(Infinite, Loop) {
    for (;;) {
        /* assertマクロがないため、タイムアウト検出されない */
    }
}
```

**回避策**: assertマクロを定期的に呼び出すようにテストを記述：
```c
TEST(Infinite, LoopWithAssert) {
    for (int i = 0; ; ++i) {
        EXPECT_TRUE(true);  /* タイムアウトチェックが行われる */
    }
}
```

**技術詳細**: POSIXではシグナルによる非同期割り込みが可能ですが、Windowsでは別スレッドから`longjmp`を呼び出せないため、メインスレッドでポーリングチェックする必要があります。将来的には、テスト本体を別スレッドで実行する実装に移行することで、この制約を解消できます。

### 2. SCOPED_INFO の自動クリーンアップ（MSVC）

MSVCは `__attribute__((cleanup))` をサポートしていないため、`SCOPED_INFO` マクロの自動クリーンアップが動作しません。

**影響**: スコープ情報はプッシュされますが、スコープ終了時の自動ポップは行われません。テスト終了時にスタックはリセットされるため、機能的な問題はありません。

**回避策**: 手動で `att_info_scope_pop_impl()` を呼び出すことも可能ですが、通常は不要です。

## 動作確認

### Windows環境でのセルフテスト結果 (2025-11-02)

**最新の検証** (MSVC + C11 threads):

```
[==========] Running 53 tests from 14 suites.
[  PASSED  ] 52 tests.
[  SKIPPED ] 1 test (CustomAssert.ExpectCustomFailure)
[==========] 53 tests ran. 0 failures.
[  PASSED  ] All tests passed.
```

**並列実行の検証**:

```powershell
PS> .\build\Debug\attest_selftest.exe --jobs=4
[==========] Running 53 tests from 14 suites with 4 workers.
[  PASSED  ] 52 tests.
[  SKIPPED ] 1 test.
[==========] 53 tests ran. 0 failures.
Execution time: 61.1 ms (1.85x faster than sequential)
```

**主要な解決済み問題**:

- ✅ **STATUS_BAD_STACK (0xC0000028)**: setjmp/longjmpのマクロ化により完全解決
- ✅ **並列実行**: C11 threads使用、4ワーカーで1.85倍高速化
- ✅ **CustomAssert.FailingAssert**: スタック破損修正により動作

タイムアウト機能のテスト：

```powershell
PS> .\build\Debug\attest_timeout_test.exe --timeout-ms=50
[==========] Running 1 tests from 1 suites.
[  FAILED  ] QuickTimeout.ShouldTimeout
  reason: timeout after 50 ms
[==========] 1 tests ran. 1 failures.
[  FAILED  ] 1 tests.
[----------] timeouts: 1
```

主要な機能は正常に動作しています：

- ✅ TEST() / TEST_F() マクロ
- ✅ ASSERT_* / EXPECT_* アサーション（型安全な`_Generic`実装）
- ✅ フィクスチャ（setup/teardown）
- ✅ サブテスト（`att_run_subtest()`）
- ✅ カスタムアサーション（`ATT_CUSTOM_ASSERT`）
- ✅ 並列実行（`--jobs=N`, `--jobs=auto`）
- ✅ --filter によるテストフィルタリング
- ✅ --list によるテスト一覧
- ✅ カラー出力の制御
- ✅ TAP/JUnit XML出力（並列実行は非対応）
- ✅ タイムアウト機能（assertベースの制約あり）

## テスト実行例

```powershell
# すべてのテストを実行
.\build\Debug\attest_selftest.exe

# 並列実行（4ワーカー）
.\build\Debug\attest_selftest.exe --jobs=4

# CPU自動検出で並列実行
.\build\Debug\attest_selftest.exe --jobs=auto

# 並列実行 + フィルタ
.\build\Debug\attest_selftest.exe --jobs=4 --filter=Parallel.*

# 特定のテストスイートのみ実行
.\build\Debug\attest_selftest.exe --filter=Fixture.*

# テスト一覧を表示
.\build\Debug\attest_selftest.exe --list

# 色付きなしで実行
.\build\Debug\attest_selftest.exe --no-color

# JUnit XML出力
.\build\Debug\attest_selftest.exe --output=junit:results.xml

# TAP形式出力
.\build\Debug\attest_selftest.exe --output=tap

# タイムアウト付きで実行（50ミリ秒）
.\build\Debug\attest_timeout_test.exe --timeout-ms=50

# パフォーマンス測定
powershell -Command "Measure-Command { .\build\Debug\attest_selftest.exe --jobs=4 2>&1 | Out-Null }"
```

## 今後の改善項目

### 高優先度

1. **TAP/JUnit形式の並列対応**: 現在デフォルト形式のみ並列実行可能。TAP/JUnit出力も並列対応を追加。
   - 実装場所: `src/attest_parallel.c`
   - 工数: 小-中

2. **Windows タイムアウトの完全な非同期化**: テスト本体を別スレッドで実行してassertなしでもタイムアウト検出
   - 実装場所: 新規スレッドベース実装
   - 工数: 中

### 中優先度

3. **タイムアウト機構の並列対応**: 現在の`setitimer`（POSIXのみ）は並列実行時に競合。監視スレッド方式へ移行。
   - 設計: `docs/parallel_execution_design.md` Phase 3
   - 工数: 中-大

4. **CI/CD**: GitHub Actions でWindows/Linux/macOS テスト自動化
   - 並列実行のパフォーマンス回帰検出
   - 工数: 中

### 低優先度

5. **MSVC cleanup 代替**: 独自のスコープガード実装を検討（現状は機能的問題なし）
6. **Advanced Filtering**: 否定フィルタ、正規表現、タグベースフィルタ（P1+ spec）

## 技術詳細: setjmp/longjmp スタック破損問題の解決履歴

### 問題の発見 (2025-11-02以前)

**症状**:
- Windows: `STATUS_BAD_STACK (0xC0000028)` でクラッシュ
- macOS/Linux: `SIGBUS` / `SIGSEGV` (環境・最適化により異なる)
- 特定のテスト（`CustomAssert.FailingAssert`）で再現性あり

**原因の特定**:
- C標準: `setjmp()` を呼んだ関数が `return` した後の `longjmp()` は未定義動作
- 関数呼び出し経由の `setjmp()` 実行でスタックフレームがずれる

### 解決プロセス

#### 第1段階: macOS環境での部分的解決 (コミット 03fadaf)

```c
// 試み: マクロ化したが、内部で関数呼び出し
#define att_subtest_scope_protect() \
    att__context_protect_internal()  // ← 関数内でsetjmpを呼ぶ
```

**結果**: macOSでは動作したが、Windowsでは依然としてクラッシュ。

#### 第2段階: Windows環境での完全解決 (コミット fcb4e15)

```c
// 正しい実装: ポインタ取得のみを関数化、setjmpはマクロ展開
sigjmp_buf *att__get_abort_env_ptr(void);  // ← ポインタを返すのみ

#define att_subtest_scope_protect() \
    att_setjmp(*att__get_abort_env_ptr())  // ← 直接setjmpをマクロ展開
```

**キーポイント**:
1. `att__get_abort_env_ptr()` は**ポインタを返すだけ**（`setjmp`を呼ばない）
2. `att_setjmp()` は**マクロ展開で呼び出し元のスタックフレームに埋め込まれる**
3. 関数呼び出しとマクロ展開の境界を正しく設計

### 学んだ教訓

1. **setjmp/longjmp は C標準の厳格な要求に従う必要がある**
   - GCC/Clangは緩い（一部の未定義動作を許容）
   - MSVCは厳格（C標準に忠実）

2. **クロスプラットフォーム開発では最も厳格な環境を基準にする**
   - macOSで動作してもWindowsでクラッシュする可能性
   - 両環境でテストすることが重要

3. **マクロとインライン関数の使い分け**
   - `setjmp`: 必ずマクロ経由で直接展開
   - ヘルパー関数: ポインタ取得やフラグ設定のみ（`setjmp`を呼ばない）

### 関連コミット

- `fcb4e15`: fix(setjmp): resolve Windows stack corruption by inlining setjmp in caller frame
- `03fadaf`: fix(subtest): resolve setjmp/longjmp stack frame issues with macro approach
- `04141b2`: fix(context): use att_get_context() to ensure g_ctx initialization
- `2346116`: fix(context): initialize g_ctx in att_context_begin() before use

## 参考資料

- [C Standard (ISO/IEC 9899): 7.13.2 Nonlocal jumps](https://en.cppreference.com/w/c/program/setjmp)
- [MSVC .CRT$XCU セクション](https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-initialization)
- [C11 aligned_alloc](https://en.cppreference.com/w/c/memory/aligned_alloc)
- [MSVC _aligned_malloc](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/aligned-malloc)
- [C11 threads.h](https://en.cppreference.com/w/c/thread)
- [Parallel Execution Design Document](parallel_execution_design.md)
