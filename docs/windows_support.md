# Windows対応ガイド

## 概要

attestフレームワークはWindows環境でのビルドと実行をサポートしています。この文書では、実装されたクロスプラットフォーム対応の詳細と、既知の制限事項について説明します。

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

タイムアウト機能はPOSIXのシグナルとタイマーに依存：

- **POSIX**: `sigaction()`, `setitimer()` を使用して完全サポート
- **Windows**: 現在は未実装（スタブ関数のみ）

### 6. ビルドシステム

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

### 1. タイムアウト機能（Windows）

現在、Windows版ではタイムアウト機能が実装されていません。

**影響**: `--timeout` オプションは無視されます。

**今後の対応**: Windows APIのスレッドとタイマーを使用した実装を予定。

### 2. SCOPED_INFO の自動クリーンアップ（MSVC）

MSVCは `__attribute__((cleanup))` をサポートしていないため、`SCOPED_INFO` マクロの自動クリーンアップが動作しません。

**影響**: スコープ情報はプッシュされますが、スコープ終了時の自動ポップは行われません。テスト終了時にスタックはリセットされるため、機能的な問題はありません。

**回避策**: 手動で `att_info_scope_pop_impl()` を呼び出すことも可能ですが、通常は不要です。

## 動作確認

Windows環境でのセルフテスト結果：

```
[==========] Running 21 tests from 11 suites.
[  PASSED  ] 18 tests.
[  FAILED  ] 1 tests (出力フォーマットの差異によるテストの失敗)
[  SKIPPED ] 2 tests (タイムアウトテスト)
```

主要な機能は正常に動作しています：

- ✅ TEST() / TEST_F() マクロ
- ✅ ASSERT_* / EXPECT_* アサーション
- ✅ フィクスチャ（setup/teardown）
- ✅ サブテスト
- ✅ --filter によるテストフィルタリング
- ✅ --list によるテスト一覧
- ✅ カラー出力の制御
- ⚠️ タイムアウト機能（未実装）

## テスト実行例

```powershell
# すべてのテストを実行
.\build\Debug\attest_selftest.exe

# 特定のテストスイートのみ実行
.\build\Debug\attest_selftest.exe --filter=Fixture.*

# テスト一覧を表示
.\build\Debug\attest_selftest.exe --list

# 色付きなしで実行
.\build\Debug\attest_selftest.exe --no-color
```

## 今後の改善項目

1. **Windows タイムアウト実装**: CreateThread + WaitForSingleObject を使用
2. **MSVC cleanup 代替**: 独自のスコープガード実装を検討
3. **CI/CD**: GitHub Actions でWindows/Linux両方でのビルド・テストを自動化
4. **ドキュメント**: Windows特有の注意事項をREADME.mdに追記

## 参考資料

- [MSVC .CRT$XCU セクション](https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-initialization)
- [C11 aligned_alloc](https://en.cppreference.com/w/c/memory/aligned_alloc)
- [MSVC _aligned_malloc](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/aligned-malloc)
