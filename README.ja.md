# attest

GoogleTestにインスパイアされた、シンプルさと自動テスト登録を重視した軽量なC11ユニットテストフレームワークです。

## 特徴

- **シンプルなAPI**: GoogleTest風の構文で `TEST()` と `TEST_F()` マクロを提供
- **自動登録**: テストが起動時に自動的に登録される（手動でのテストリスト管理不要）
- **型汎用アサーション**: C11の `_Generic` を使用した型安全な比較
- **豊富なアサーション**: 比較、文字列、メモリ、浮動小数点アサーションを包括的に提供
- **テストフィクスチャ**: テスト分離のためのセットアップ/ティアダウンフック
- **致命的/非致命的**: `ASSERT_*` はテストを中断、`EXPECT_*` は失敗後も継続
- **サブテスト**: 独立したサブテスト実行と期待される失敗の検証
- **CLIフィルタリング**: 柔軟なワイルドカードベースのテストフィルタリング
- **カラー出力**: 明瞭で読みやすいテスト結果（`--no-color` オプションあり）
- **スキップサポート**: `ATT_SKIP()` と `ATT_SKIP_IF()` による条件付きテストスキップ
- **スコープ付き情報**: 失敗時のみ表示されるコンテキストメッセージ
- **ゼロ依存**: 純粋なC11で外部ライブラリ不要

## クイックスタート

### ビルド方法

```bash
# 設定
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# ビルド
cmake --build build

# テスト実行
./build/attest_selftest
```

### 最初のテストを書く

```c
#include "attest/attest.h"

TEST(Math, Addition) {
    ASSERT_EQ(2 + 2, 4);
    EXPECT_NE(3 + 3, 7);
}

TEST(String, Comparison) {
    ASSERT_STREQ("hello", "hello");
    EXPECT_STRNE("world", "WORLD");
}

int main(int argc, char **argv) {
    return attest_main(argc, argv);
}
```

### フィクスチャの使用

```c
typedef struct {
    int *buffer;
    size_t size;
} BufferFixture;

ATT_FIXTURE_SETUP(BufferFixture) {
    att_fixture->size = 100;
    att_fixture->buffer = malloc(att_fixture->size * sizeof(int));
}

ATT_FIXTURE_TEARDOWN(BufferFixture) {
    free(att_fixture->buffer);
}

TEST_F(BufferFixture, Initialization) {
    ASSERT_NE(att_fixture->buffer, NULL);
    ASSERT_EQ(att_fixture->size, 100);
}
```

## アサーション

### 比較アサーション

| マクロ | 致命的 | 説明 |
|-------|-------|------|
| `ASSERT_EQ(a, b)` | はい | `a == b` |
| `EXPECT_EQ(a, b)` | いいえ | `a == b` |
| `ASSERT_NE(a, b)` | はい | `a != b` |
| `EXPECT_NE(a, b)` | いいえ | `a != b` |
| `ASSERT_LT(a, b)` | はい | `a < b` |
| `EXPECT_LT(a, b)` | いいえ | `a < b` |
| `ASSERT_LE(a, b)` | はい | `a <= b` |
| `EXPECT_LE(a, b)` | いいえ | `a <= b` |
| `ASSERT_GT(a, b)` | はい | `a > b` |
| `EXPECT_GT(a, b)` | いいえ | `a > b` |
| `ASSERT_GE(a, b)` | はい | `a >= b` |
| `EXPECT_GE(a, b)` | いいえ | `a >= b` |

### ブールアサーション

- `ASSERT_TRUE(condition)` / `EXPECT_TRUE(condition)`
- `ASSERT_FALSE(condition)` / `EXPECT_FALSE(condition)`

### 文字列アサーション

- `ASSERT_STREQ(str1, str2)` / `EXPECT_STREQ(str1, str2)` - 文字列の等価性
- `ASSERT_STRNE(str1, str2)` / `EXPECT_STRNE(str1, str2)` - 文字列の不等性

### メモリアサーション

- `ASSERT_MEMEQ(ptr1, ptr2, size)` / `EXPECT_MEMEQ(ptr1, ptr2, size)` - メモリの等価性

### 浮動小数点アサーション

- `ASSERT_NEAR(a, b, epsilon)` / `EXPECT_NEAR(a, b, epsilon)` - 許容誤差を持つ浮動小数点比較

## コマンドラインオプション

```bash
# すべてのテストをリスト表示
./build/attest_selftest --list

# 特定のテストを実行（ワイルドカードサポート）
./build/attest_selftest --filter='Math.*'
./build/attest_selftest --filter='*.Addition'

# 複数フィルタ（OR論理）
./build/attest_selftest --filter='Math.*;String.Comparison'

# カラー出力を無効化
./build/attest_selftest --no-color

# 各テストのタイムアウトを設定（ミリ秒単位）
./build/attest_selftest --timeout-ms=5000
```

### 手動タイムアウト検証

`attest_timeout_test` バイナリには、タイムアウト機能を検証するために特別に設計されたテストが含まれています。これらのテストは明示的なタイムアウト設定が必要で、失敗することが期待されているため、自動テストスイートの一部では**ありません**：

```bash
# クイックタイムアウトテスト（50ms後にタイムアウトで失敗するべき）
./build/attest_timeout_test --timeout-ms=50

# アサーションループでのタイムアウト（環境変数 + タイムアウトが必要）
# Windows: $env:ATTEST_ENABLE_TIMEOUT_WITH_ASSERTS_TEST="1"
# Unix: export ATTEST_ENABLE_TIMEOUT_WITH_ASSERTS_TEST=1
./build/attest_timeout_test --timeout-ms=100 --filter=Timeout.WithAsserts

# 無限ループタイムアウトテスト（環境変数 + タイムアウトが必要）
# Windows: $env:ATTEST_ENABLE_TIMEOUT_TEST="1"
# Unix: export ATTEST_ENABLE_TIMEOUT_TEST=1
./build/attest_timeout_test --timeout-ms=50 --filter=Timeout.InfiniteLoop
```

**注意**: これらのテストはタイムアウトによって**失敗**するように設計されています。これにより、タイムアウトメカニズムが正しく動作していることを検証します。

## 高度な機能

### サブテスト

親テストに影響を与えない独立したサブテストを実行：

```c
static void validate_input(void *user) {
    int value = *(int *)user;
    ASSERT_GT(value, 0);
}

TEST(Validation, SubtestExample) {
    int positive = 42;
    int negative = -1;

    att_result result;
    att_run_subtest("positive", validate_input, &positive, &result);
    EXPECT_EQ(result.failed, 0);

    att_run_subtest("negative", validate_input, &negative, &result);
    EXPECT_EQ(result.failed, 1);
}
```

### 期待される失敗

特定のコードブロックが期待通りに失敗することを検証：

```c
TEST(ErrorHandling, ExpectedFailure) {
    ATT_EXPECT_SUBTEST_FAILS("invalid_input", {
        att_subtest_fatal(NULL);
    }, 1, 1);  // 正確に1つの失敗を期待
}
```

### テストのスキップ

```c
TEST(Platform, SkipExample) {
    ATT_SKIP_IF(getenv("CI") != NULL, "CI環境ではスキップ");
    // CIで実行すべきでないテストコード
}
```

### スコープ付き情報

テストが失敗したときのみ表示される文脈情報を追加：

```c
TEST(Loop, ScopedInfo) {
    for (int i = 0; i < 100; i++) {
        SCOPED_INFO("Iteration: %d", i);
        EXPECT_LT(i, 100);
    }
}
```

## 統合

### サブプロジェクトとして使用

CMakeプロジェクトにattestを追加：

```cmake
add_subdirectory(external/attest)

add_executable(my_tests tests/my_tests.c)
target_link_libraries(my_tests PRIVATE attest)
```

デフォルトでは、サブプロジェクトとして使用される場合、attestのセルフテストは無効化されます。有効にするには：

```bash
cmake -S . -B build -DATTEST_BUILD_TESTING=ON
```

## 終了コード

- **0**: すべてのテストが成功、または `--list` モード
- **1**: 1つ以上のテスト失敗
- **2**: CLI解析エラー（未知のオプション）
- **3**: 初期化失敗（重複したテスト名、内部エラー）

## コンパイラサポート

- **GCC**: 5.0以上（C11サポート必須）
  - ⚠️ **既知の問題**: GCC 14.2.0 on ARM64/aarch64（Ubuntu 25.04）には `sigsetjmp/siglongjmp` のバグがありクラッシュします。**回避策**: ARM64プラットフォームではClangまたはGCC 13.xを使用してください。
- **Clang**: 3.1以上（C11サポート必須） - **ARM64では推奨**
- **MSVC**: 2015以上（部分的サポート、自動登録に `.CRT$XCU` セクションを使用）

### 既知の問題

- **ARM64 Linux with GCC 14.2.0**: `sigjmp_buf`/`setjmp`/`longjmp` に関するコンパイラ固有の問題により、ランタイムクラッシュが発生します。回避策としてClangを使用してください：
  ```bash
  cmake -S . -B build -DCMAKE_C_COMPILER=clang
  ```

コンストラクタ属性をサポートしないコンパイラの場合、手動登録を使用：

```c
ATT_REGISTER_TESTS(
    &test_register_fn1,
    &test_register_fn2
);
```

## プロジェクト構造

```
attest/
├── include/attest/        # 公開APIヘッダー
├── src/                   # 実装
│   ├── attest.c          # メイン実行エンジン
│   ├── attest_cli.c      # コマンドライン解析
│   ├── attest_assert.c   # アサーション実装
│   ├── attest_capture.c  # 出力キャプチャ
│   ├── attest_fixture.c  # フィクスチャサポート
│   └── internal/         # 内部レジストリ
├── tests/                 # セルフテスト
├── docs/                  # 仕様書（P0、P1、P1+）
└── CMakeLists.txt        # ビルド設定
```

## ロードマップ

プロジェクトは段階的な開発アプローチに従います：

- **P0（現在）**: コア機能 - C11、シングルスレッド、基本的なアサーション、CLI
- **P1（予定）**: TAP/JUnit出力、テストタイムアウト、高度なフィクスチャ
- **P1+（将来）**: C89/90サポート、並列実行、高度なフィルタリング

詳細な仕様については `docs/` ディレクトリを参照してください。

## コントリビューション

1. WebKitコードスタイルに従う（タブ、幅4、右ポインタアライメント）
2. コミット前に `clang-format -i <file>` を実行
3. 新機能については `tests/selftest_main.c` にセルフテストを追加
4. 完全なテストスイートを実行: `./build/attest_selftest`
5. コミットメッセージはConventional Commitsに従う

## ライセンス

詳細については `LICENSE` ファイルを参照してください。

## 謝辞

GoogleTestの人間工学的なAPI設計にインスパイアされながら、組み込み環境やリソース制約のある環境に適した最小限のシングルファイルフレンドリーな実装を維持しています。
