# ユーザーガイド

[English](guide.md) | **日本語**

本ガイドでは、attest のよく使われる利用パターンと発展的な機能を解説します。

## 基本的なテスト

最も単純なテストは `TEST(Suite, Name)` マクロで書きます。

```c
#include "attest/attest.h"

TEST(Math, Addition) {
    ASSERT_EQ(2 + 2, 4);
    EXPECT_NE(3 + 3, 7);
}

int main(int argc, char **argv) {
    return attest_main(argc, argv);
}
```

テストはコンストラクタ属性によって起動時に自動登録されます。テストリストの手動メンテナンスは不要です。

---

## テストフィクスチャ

フィクスチャは関連するテスト群で共通の setup/teardown を共有する仕組みです。

### フィクスチャの定義

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
    ASSERT_NOT_NULL(att_fixture->buffer);
    ASSERT_EQ(att_fixture->size, 100);
}

TEST_F(BufferFixture, FillWithZeros) {
    memset(att_fixture->buffer, 0, att_fixture->size * sizeof(int));
    EXPECT_EQ(att_fixture->buffer[0], 0);
}
```

### 実行順序

1. フィクスチャインスタンスを確保
2. setup 関数を呼び出し（定義されていれば）
3. テスト本体を実行
4. teardown 関数を呼び出し（定義されていれば）— ASSERT 失敗やスキップ後でも**必ず**実行
5. フィクスチャインスタンスを解放

### Setup/Teardown の失敗

- **setup 失敗**: テスト本体はスキップされ、`(setup)` 失敗としてマークされる
- **teardown 失敗**: テスト結果に `(teardown)` タグ付きで追記される

---

## テストのスキップ

### 無条件スキップ

```c
TEST(Platform, LinuxOnly) {
    #ifndef __linux__
    ATT_SKIP("Linux only");
    #endif
    // Linux 固有のテストコード
}
```

### 条件付きスキップ

```c
TEST(Feature, RequiresEnv) {
    ATT_SKIP_IF(getenv("FEATURE_FLAG") == NULL, "FEATURE_FLAG not set");
    // テストコード
}
```

スキップされたテストは:
- 失敗としてカウントされない
- サマリで `[  SKIPPED ]` と表示される
- 終了コードは 0 のまま（他のテストが失敗しない限り）

---

## スコープ付き情報（Scoped Info）

アサーション失敗時にのみ表示される文脈情報を追加できます。

```c
TEST(DataDriven, Validation) {
    int test_cases[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        SCOPED_INFO("test_case[%d] = %d", i, test_cases[i]);
        EXPECT_GT(test_cases[i], 0);
    }
}
```

失敗時の出力例：

```
[  FAILED  ] DataDriven.Validation
  context: test_case[3] = 4
  file.c:10: EXPECT_GT(...) failed.
```

**特徴：**
- ネスト対応（スタック深さの上限：8）
- スコープ離脱時に自動 pop（GCC/Clang のみ）
- 文脈は失敗時にのみ表示

---

## サブテスト

親テストの状態に影響しない、隔離されたテストを実行します。

```c
static void validate_positive(void *user) {
    int value = *(int *)user;
    ASSERT_GT(value, 0);
}

TEST(Validation, MultipleInputs) {
    int values[] = {42, -1, 100};
    att_result result;

    for (int i = 0; i < 3; i++) {
        SCOPED_INFO("value=%d", values[i]);
        att_run_subtest("check", validate_positive, &values[i], &result);

        if (values[i] > 0) {
            EXPECT_EQ(result.status, ATT_STATUS_OK);
        } else {
            EXPECT_EQ(result.status, ATT_STATUS_FAIL);
        }
    }
}
```

### 期待された失敗

コードが期待通りに失敗することを検証します。

```c
TEST(ErrorHandling, InvalidInput) {
    ATT_EXPECT_SUBTEST_FAILS("null_check", {
        char *ptr = NULL;
        ASSERT_NOT_NULL(ptr);
    }, 1, 1);  // ちょうど 1 件の失敗を期待
}
```

マクロのパラメータ：
- `name`: レポート用のサブテスト名
- `block`: 実行するコードブロック
- `min`: 期待する失敗の最小件数
- `max`: 期待する失敗の最大件数

---

## 浮動小数点比較

### 絶対誤差（NEAR）

```c
TEST(Float, AbsoluteError) {
    double computed = 3.14159;
    double expected = 3.14;
    EXPECT_NEAR(computed, expected, 0.01);  // |diff| <= 0.01
}
```

### 相対誤差（NEAR_REL）

桁が大きく変動する値に向いています。

```c
TEST(Float, RelativeError) {
    EXPECT_NEAR_REL(100.0, 101.0, 0.02);     // 1% 差、2% 許容
    EXPECT_NEAR_REL(1e10, 1e10 + 1e8, 0.02); // 大きな値でも機能
}
```

### ULP 比較（ULP_EQ）

精密な浮動小数点比較が必要なとき。

```c
TEST(Float, ULPDistance) {
    EXPECT_ULP_EQ(1.0, 1.0, 0);              // 完全一致
    EXPECT_ULP_EQ(1.0, nextafter(1.0, 2.0), 1);  // 1 ULP 差
    EXPECT_ULP_EQ(0.0, -0.0, 0);             // +0 と -0 は 0 ULP 差
}
```

---

## 並列実行

テストを並列に走らせて高速化できます。

```bash
# 4 ワーカースレッドを使用
./test_runner --jobs=4

# CPU コア数を自動検出
./test_runner --jobs=auto
```

**注意：**
- テスト同士が独立していることが前提（詳細は後述の [共有状態とデータ競合](#共有状態とデータ競合) を参照）
- 出力はテストごとに収集され、登録順に出力されます
- **フォーマット制約：** 並列実行は現状デフォルトの人間向けフォーマットしか出力しません。TAP のテスト行（`ok N` / `not ok N`）と JUnit XML レポートは `--jobs > 1` では生成されないため、これらの形式が必要なら `--jobs=1`（デフォルト）で逐次実行してください。
- **プラットフォーム対応：** POSIX スレッド（Linux、macOS）が必要です。スレッドサポートのないプラットフォーム（例：Human68k）や Windows では、`--jobs` は黙って逐次実行へフォールバックします。

### 共有状態とデータ競合

`--jobs > 1` では、別々のテストケースが別々のワーカースレッドで同時に走ります。**テスト間で共有される状態**（グローバル変数、ファイルスコープの `static` 変数、環境変数、ディスク上のファイルなど）はすべてスレッド間の共有可変状態となり、自分で同期しない限り、並行読み書きはデータ競合（未定義動作）になります。

具体的には：

- **フィクスチャインスタンス自体は共有されません。** `TEST_F` はテストごとに新しいゼロ初期化されたフィクスチャ構造体を渡し、`setup` / `teardown` をテストごとに呼ぶので、フィクスチャ構造体そのものは安全です。
- **フィクスチャを定義したファイルの `static` 変数は共有されます。** よくあるパターンとして、ファイルスコープの `static int g_calls;` にカウンタやキャッシュを置き、`ATT_FIXTURE_SETUP` から加算する書き方があります。このカウンタはプロセス全体で共有されるので、同じフィクスチャを使う `TEST_F` が 2 つのワーカーで同時に走るとインクリメントが競合します。インクリメント自体がレースですし、そのカウンタの値に対するアサーションは flaky になります。
- **外部リソースは共有です。** 固定パスのファイル、カレントディレクトリ、環境変数、共有データベースに触るテストは、自分で調停するか、逐次実行する必要があります。

最もきれいな解決策は、テストごとに独立した状態を持たせることです。たとえばカウンタ系の検証では「ただ 1 つのテストだけが使う専用のフィクスチャ型」を定義する、あるいはファイルスコープの記憶ではなくフィクスチャ構造体経由で状態を渡す、という方法があります。本質的に並列化できないテスト（例：`chdir` するもの）がある場合は、そのテストだけ `--filter=Suite.Name --jobs=1` で実行するか、CI で別プロセスから呼ぶよう組んでください。

---

## CMake 連携

### サブプロジェクトとして利用

```cmake
add_subdirectory(external/attest)

add_executable(my_tests tests/my_tests.c)
target_link_libraries(my_tests PRIVATE attest)
```

サブプロジェクトとして使う場合、attest の selftest は自動的に無効化されます。有効にするには：

```bash
cmake -S . -B build -DATTEST_BUILD_TESTING=ON
```

### FetchContent を利用

```cmake
include(FetchContent)
FetchContent_Declare(
    attest
    GIT_REPOSITORY https://github.com/user/attest.git
    GIT_TAG main
)
FetchContent_MakeAvailable(attest)

target_link_libraries(my_tests PRIVATE attest)
```

### 同梱の `attest_runner`

トップレベルの CMake ビルドは `attest_runner` 実行ファイル（`src/attest_main.c`）も生成します。これは単に `attest_main()` を呼ぶだけのもので、ライブラリのリンク面が単独で検証可能であることを保証する目的と、`main()` のコピー元として利用できる最小例の役割を持ちます。テストは含まれていないので、自分の翻訳単位をリンクして駆動するか、自前の `main()` を書いてランナーを使わない選択もできます。

---

## 出力フォーマット

### デフォルト（人間向け）

`--no-color` でカラー出力を無効化できます（CI 環境やログファイル向け）。

```bash
./test_runner
```

デフォルトのフォーマッタは意図的に控えめで、合格テストはテストごとの出力を出しません。失敗とスキップのみ報告し、最後にサマリを出します。

```
[==========] Running 10 tests from 3 suites.
[  FAILED  ] Math.Division
  test.c:15: EXPECT_EQ(result, 2) failed.
    expected: 2
      actual: 3
    expr: result=3, 2=2
[  SKIPPED ] Platform.LinuxOnly
  reason: Linux only
[==========] 10 tests ran. 1 failures, 1 skipped.
[  FAILED  ] 1 tests.
[  PASSED  ] 8 tests.
[  SKIPPED ] 1 tests.
```

並列実行（`--jobs=N`、`N > 1`）の場合は、ワーカーの進捗を可視化するためテスト完了ごとに `[ RUN      ]` / `[       OK ]` / `[  FAILED  ]` マーカも出力します。逐次実行では簡潔さのためこれらのマーカは省略されます。

### TAP 13

```bash
./test_runner --format=tap
```

```
1..10
ok 1 - Math.Addition
not ok 2 - Math.Division
  ---
  message: EXPECT_EQ failed
  ...
```

### JUnit XML

```bash
./test_runner --format=junit --output=results.xml
```

Jenkins や GitHub Actions などの CI が解釈できる XML を生成します。

---

## タイムアウトの扱い

テストごとのタイムアウトを設定できます。

```bash
./test_runner --timeout-ms=5000
```

**プラットフォーム差：**
- **POSIX**: シグナルベースの中断（無限ループにも有効）
- **Windows**: ポーリングベース（タイムアウト検出にはアサーションマクロの実行が必要）

Windows では、長時間動作するコードに定期的なアサーションを挟んでください。

```c
TEST(LongRunning, WithProgress) {
    for (int i = 0; i < 1000000; i++) {
        // 処理...
        if (i % 10000 == 0) {
            EXPECT_TRUE(true);  // タイムアウトチェックの機会
        }
    }
}
```

---

## 手動登録

`__attribute__((constructor))` 非対応コンパイラ向け：

```c
// 通常通りテストを定義
TEST(Suite, Test1) { ... }
TEST(Suite, Test2) { ... }

// 手動で登録
ATT_REGISTER_TESTS(
    &test_Suite_Test1_register,
    &test_Suite_Test2_register
);

int main(int argc, char **argv) {
    return attest_main(argc, argv);
}
```

登録関数名のパターンは `test_<Suite>_<Name>_register` です。
