# APIリファレンス

[English](api.md) | **日本語**

本ドキュメントは attest のすべてのマクロ・関数・CLIオプションを網羅したリファレンスです。

## テスト定義マクロ

### TEST(Suite, Name)

シンプルなテストケースを定義します。

```c
TEST(Math, Addition) {
    ASSERT_EQ(2 + 2, 4);
}
```

### TEST_F(Fixture, Name)

フィクスチャを伴うテストケースを定義します。フィクスチャ型は構造体である必要があり、setup/teardown 関数は省略可能です。

```c
typedef struct {
    int value;
} MyFixture;

ATT_FIXTURE_SETUP(MyFixture) {
    att_fixture->value = 42;
}

ATT_FIXTURE_TEARDOWN(MyFixture) {
    // クリーンアップ
}

TEST_F(MyFixture, Example) {
    ASSERT_EQ(att_fixture->value, 42);
}
```

### ATT_FIXTURE_SETUP(Type) / ATT_FIXTURE_TEARDOWN(Type)

フィクスチャの setup/teardown 関数を定義します。これらの関数内では `att_fixture` がフィクスチャインスタンスへのポインタになります。両フックとも省略可能で、コンストラクタ属性で登録されるため、同じ `Type` を複数の `TEST_F` で再利用できます。

フィクスチャインスタンスは `TEST_F` ごとに新しくゼロ初期化され（各テストが自分専用のコピーを得ます）、teardown は終了時に**必ず**呼ばれます。テスト本体が `ASSERT_*` で中断した場合や、`ATT_SKIP` を呼んだ場合、タイムアウトに当たった場合でも実行されるため、setup で確保したリソースの解放はここで行うのがドキュメント上の正解です。

---

## アサーション

すべてのアサーションは2種類の variant を持ちます。

- **ASSERT_*** — 致命的：失敗時に現在のテストを中断
- **EXPECT_*** — 非致命的：失敗を記録して継続

### 比較アサーション

これらの型汎用比較マクロは、`_Generic` が使える C11 以降でのみ提供されます。
C99 以前では、後述の明示型マクロを使用してください。

| マクロ | 条件 |
|--------|------|
| `ASSERT_EQ(a, b)` / `EXPECT_EQ(a, b)` | `a == b` |
| `ASSERT_NE(a, b)` / `EXPECT_NE(a, b)` | `a != b` |
| `ASSERT_LT(a, b)` / `EXPECT_LT(a, b)` | `a < b` |
| `ASSERT_LE(a, b)` / `EXPECT_LE(a, b)` | `a <= b` |
| `ASSERT_GT(a, b)` / `EXPECT_GT(a, b)` | `a > b` |
| `ASSERT_GE(a, b)` / `EXPECT_GE(a, b)` | `a >= b` |

型ディスパッチには C11 の `_Generic` を使い、符号付き / 符号なし整数、浮動小数点、ポインタを処理します。

### 明示型マクロ（C89互換）

`_Generic` のない C89/C99 環境でも動く、型を明示するアサーションです。

#### 符号付き整数 (`INT`)

- `EXPECT_INT_EQ(lhs, rhs)` / `ASSERT_INT_EQ(lhs, rhs)` — 等しい
- `EXPECT_INT_NE(lhs, rhs)` / `ASSERT_INT_NE(lhs, rhs)` — 等しくない
- `EXPECT_INT_LT(lhs, rhs)` / `ASSERT_INT_LT(lhs, rhs)` — 未満
- `EXPECT_INT_LE(lhs, rhs)` / `ASSERT_INT_LE(lhs, rhs)` — 以下
- `EXPECT_INT_GT(lhs, rhs)` / `ASSERT_INT_GT(lhs, rhs)` — 超過
- `EXPECT_INT_GE(lhs, rhs)` / `ASSERT_INT_GE(lhs, rhs)` — 以上

値は比較のため `long long` にキャストされます。

```c
TEST(Numbers, SignedInteger) {
    int a = 42;
    int b = 10;
    ASSERT_INT_EQ(a, 42);
    EXPECT_INT_GT(a, b);
}
```

#### 符号なし整数 (`UINT`)

- `EXPECT_UINT_EQ(lhs, rhs)` / `ASSERT_UINT_EQ(lhs, rhs)` — 等しい
- `EXPECT_UINT_NE(lhs, rhs)` / `ASSERT_UINT_NE(lhs, rhs)` — 等しくない
- `EXPECT_UINT_LT(lhs, rhs)` / `ASSERT_UINT_LT(lhs, rhs)` — 未満
- `EXPECT_UINT_LE(lhs, rhs)` / `ASSERT_UINT_LE(lhs, rhs)` — 以下
- `EXPECT_UINT_GT(lhs, rhs)` / `ASSERT_UINT_GT(lhs, rhs)` — 超過
- `EXPECT_UINT_GE(lhs, rhs)` / `ASSERT_UINT_GE(lhs, rhs)` — 以上

値は比較のため `unsigned long long` にキャストされます。

```c
TEST(Numbers, UnsignedInteger) {
    unsigned int size = 256;
    ASSERT_UINT_EQ(size, 256u);
    EXPECT_UINT_GT(size, 100u);
}
```

#### ポインタ (`PTR`)

- `EXPECT_PTR_EQ(lhs, rhs)` / `ASSERT_PTR_EQ(lhs, rhs)` — 等しい
- `EXPECT_PTR_NE(lhs, rhs)` / `ASSERT_PTR_NE(lhs, rhs)` — 等しくない
- `EXPECT_PTR_LT(lhs, rhs)` / `ASSERT_PTR_LT(lhs, rhs)` — 未満
- `EXPECT_PTR_LE(lhs, rhs)` / `ASSERT_PTR_LE(lhs, rhs)` — 以下
- `EXPECT_PTR_GT(lhs, rhs)` / `ASSERT_PTR_GT(lhs, rhs)` — 超過
- `EXPECT_PTR_GE(lhs, rhs)` / `ASSERT_PTR_GE(lhs, rhs)` — 以上

ポインタは `uintptr_t` 値として比較されます。

```c
TEST(Pointer, Comparison) {
    int arr[10];
    int *p1 = &arr[0];
    int *p2 = &arr[5];
    ASSERT_PTR_NE(p1, p2);
    EXPECT_PTR_LT(p1, p2);
}
```

### 真偽値アサーション

| マクロ | 条件 |
|--------|------|
| `ASSERT_TRUE(cond)` / `EXPECT_TRUE(cond)` | `cond` が真 |
| `ASSERT_FALSE(cond)` / `EXPECT_FALSE(cond)` | `cond` が偽 |

### ポインタアサーション

| マクロ | 条件 |
|--------|------|
| `ASSERT_NULL(ptr)` / `EXPECT_NULL(ptr)` | `ptr == NULL` |
| `ASSERT_NOT_NULL(ptr)` / `EXPECT_NOT_NULL(ptr)` | `ptr != NULL` |

### 文字列アサーション

| マクロ | 条件 |
|--------|------|
| `ASSERT_STREQ(s1, s2)` / `EXPECT_STREQ(s1, s2)` | `strcmp(s1, s2) == 0` |
| `ASSERT_STRNE(s1, s2)` / `EXPECT_STRNE(s1, s2)` | `strcmp(s1, s2) != 0` |

- `NULL == NULL` は真
- `NULL` と非NULL は偽
- 出力では NULL ポインタを `"(null)"` と表示

### メモリアサーション

| マクロ | 条件 |
|--------|------|
| `ASSERT_MEMEQ(p1, p2, n)` / `EXPECT_MEMEQ(p1, p2, n)` | `memcmp(p1, p2, n) == 0` |

- `n == 0` は常に成功
- NULL ポインタで `n > 0` の場合は失敗

### 浮動小数点アサーション

| マクロ | 条件 |
|--------|------|
| `ASSERT_NEAR(a, b, eps)` / `EXPECT_NEAR(a, b, eps)` | `|a - b| <= eps` |
| `ASSERT_NEAR_REL(a, b, rel)` / `EXPECT_NEAR_REL(a, b, rel)` | `|a - b| <= rel * max(|a|, |b|)` |
| `ASSERT_ULP_EQ(a, b, ulp)` / `EXPECT_ULP_EQ(a, b, ulp)` | ULP 距離 <= ulp |

**特殊ケース：**
- NaN は常に失敗
- ±Infinity は同符号で完全一致した場合のみ一致
- `NEAR_REL`: `|lhs|` と `|rhs|` がいずれも `1e-15` 未満のときは、絶対比較に切り替わり、`rel_eps` をそのまま絶対許容誤差として `|lhs - rhs| <= rel_eps` で判定します。実質ゼロのスケールで除算するのを避けるためです。
- `ULP_EQ`: 非正規化数や符号遷移を正しく扱います

### カスタムアサーション

| マクロ | 説明 |
|--------|------|
| `ATT_ASSERT(expr, fmt, ...)` | 致命的：printf 形式のカスタムメッセージ |
| `ATT_EXPECT(expr, fmt, ...)` | 非致命的：printf 形式のカスタムメッセージ |

```c
TEST(Validation, Custom) {
    int code = get_error_code();
    ATT_ASSERT(code == 0, "unexpected error code: %d", code);
}
```

---

## テスト制御

### ATT_SKIP(reason)

理由メッセージを添えて現在のテストをスキップします。

```c
TEST(Platform, LinuxOnly) {
    #ifndef __linux__
    ATT_SKIP("Linux only");
    #endif
    // テストコード
}
```

### ATT_SKIP_IF(condition, reason)

条件が真であれば現在のテストをスキップします。

```c
TEST(Feature, RequiresEnv) {
    ATT_SKIP_IF(getenv("CI") != NULL, "Skipped in CI");
    // テストコード
}
```

### SCOPED_INFO(fmt, ...)

失敗時のみ表示される文脈情報を追加します。GCC/Clang の `cleanup` 属性を使ってスコープ管理を自動化しています。

```c
TEST(Loop, Example) {
    for (int i = 0; i < 100; i++) {
        SCOPED_INFO("iteration=%d", i);
        EXPECT_GT(compute(i), 0);
    }
}
```

**注意：** MSVC は `cleanup` 属性をサポートしないため、情報は push されますが自動 pop されません。テスト終了時にスタックがリセットされるため、機能的には等価です。

---

## サブテスト API

### att_run_subtest(name, fn, user, result)

親テストの状態に影響しない、隔離されたサブテストを実行します。

```c
static void my_subtest(void *user) {
    int val = *(int *)user;
    ASSERT_GT(val, 0);
}

TEST(Validation, Subtest) {
    int value = 42;
    att_result result;
    att_status status = att_run_subtest("check_positive", my_subtest, &value, &result);
    EXPECT_EQ(status, ATT_STATUS_OK);
    EXPECT_EQ(result.failed, 0);
}
```

**シグネチャ：**

```c
att_status att_run_subtest(const char *name,
                           void (*fn)(void *), void *user,
                           att_result *out);
```

戻り値は `out->status` をそのまま返すため、どちらの形式でも問題ありません。詳細カウンタが必要なら `out` を非NULLで渡し、ステータスのみ必要なら `NULL` を渡してください。

### ATT_EXPECT_SUBTEST_FAILS(name, block, min, max)

コードブロックが期待通り失敗することを検証します。

```c
TEST(Error, ExpectedFailure) {
    ATT_EXPECT_SUBTEST_FAILS("should_fail", {
        ASSERT_TRUE(false);
    }, 1, 1);  // ちょうど 1 件の失敗を期待
}
```

### サブテストスコープ API

サブテスト実行をきめ細かく制御したい場合に使用します。

```c
TEST(Scope, Example) {
    att_subtest_scope *scope = att_subtest_scope_enter("my_scope");
    if (att_subtest_scope_protect(scope) == 0) {
        ASSERT_TRUE(some_condition());
    }
    att_result result;
    att_subtest_scope_leave(scope, &result);
    EXPECT_EQ(result.status, ATT_STATUS_OK);
}
```

- `att_subtest_scope_enter(name)` — サブテストスコープを生成して返します
- `att_subtest_scope_protect(scope)` — 致命的アサーション処理用に呼び出し元のスタックフレームで `setjmp` を呼ぶマクロ
- `att_subtest_scope_leave(scope, result)` — スコープをクリーンアップし、結果を埋めます

---

## 出力キャプチャ

### att_capture_begin() / att_capture_end(out)

検証目的で stderr 出力をキャプチャします。非リエントラント（ネスト不可）です。両関数とも成功時 `0`、エラー時または現在のプラットフォームでキャプチャ非対応の場合は `-1` を返します。

```c
att_captured cap;
att_capture_begin();
fprintf(stderr, "test output");
att_capture_end(&cap);
// cap.data に "test output" が入る、cap.size はその長さ
free(cap.data);
```

**プラットフォームサポート：** キャプチャは `dup` / `dup2` に依存しています。Human68k ではこれらの API が使えないため、`att_capture_begin()` / `att_capture_end()` は `-1` を返すだけの no-op としてコンパイルされます。`ATT_EXPECT_SUBTEST_FAILS`（内部でキャプチャを使い、想定された失敗のノイズを抑える）に依存するテストは、Human68k 上でも失敗件数の検証は行われますが、キャプチャ済み出力のリプレイステップはスキップされます。

---

## 手動登録

コンストラクタ属性に対応していないコンパイラ向け：

```c
ATT_REGISTER_TESTS(
    &test_Suite_Name_register,
    &test_Other_Test_register
);
```

---

## 型

### att_status

サブテスト操作の戻り値型。

```c
typedef enum {
    ATT_STATUS_OK = 0,
    ATT_STATUS_FAIL = 1,
    ATT_STATUS_ABORTED = 2
} att_status;
```

### att_result

`att_run_subtest()` が埋める結果構造体。

```c
typedef struct att_result {
    int total;
    int failed;
    int fatal_failures;
    int nonfatal_failures;
    int skipped;
    att_status status;
} att_result;
```

### attest_summary

`attest_get_summary()` が返すサマリ。

```c
typedef struct attest_summary {
    int total;
    int passed;
    int failed;
    int skipped;
} attest_summary;
```

---

## エントリポイント

### attest_main(argc, argv)

メインエントリポイント。終了コードを返します。

```c
int main(int argc, char **argv) {
    return attest_main(argc, argv);
}
```

### attest_get_summary()

`attest_main()` 完了後にテスト実行のサマリを返します。

```c
int main(int argc, char **argv) {
    int ret = attest_main(argc, argv);
    attest_summary s = attest_get_summary();
    printf("Ran %d tests, %d passed\n", s.total, s.passed);
    return ret;
}
```

---

## コマンドラインオプション

| オプション | 説明 |
|-----------|------|
| `-h`, `--help` | 使用方法を表示して終了 |
| `--list` | すべてのテスト名を列挙して終了 |
| `--filter=<pattern>` | 一致するテストのみ実行 |
| `--no-color` | カラー出力を無効化 |
| `--timeout-ms=<ms>` | テストごとのタイムアウトをミリ秒で指定（POSIX：シグナルベース、Windows：各アサーション時にポーリング） |
| `--shuffle` | テスト実行順をランダム化（シードは `time(NULL)` 由来） |
| `--shuffle=<seed>` | 再現性のためシードを固定してシャッフル |
| `--jobs=<N>` | N 個のワーカーで並列実行（`--jobs=1` は逐次ランナーのまま） |
| `--jobs=auto`, `--jobs=0` | 検出したCPUコア数で並列実行 |
| `--format=default` | 人間向けフォーマット（デフォルト） |
| `--format=tap` | TAP 13 形式で出力 |
| `--format=junit` | JUnit XML 形式で出力 |
| `--output=<path>` | JUnit XML をファイルに書き出し（デフォルト：`test_detail.xml`、`--format=junit` 必須） |

**並列ランナーの注意：** 並列実行は POSIX スレッド対応プラットフォーム（Linux、macOS）でのみコンパイルされます。Windows や Human68k では `--jobs > 1` でも黙って逐次ランナーにフォールバックします。POSIX 上であっても、現状の並列ランナーはデフォルトの人間向けフォーマットしか出力しません。`--format=tap` や `--format=junit` と組み合わせるとテストごとの出力が抑制され、JUnit XML の書き出しもスキップされるため、これらの形式が必要なら `--jobs=1`（デフォルト）で実行してください。

### フィルタ構文

- `Suite.Name` — 完全一致
- `Suite.*` — スイート内の全テスト
- `*.Name` — 任意のスイート内の指定名のテスト
- `Suite` — `Suite.*` の省略形
- `.Name` — `*.Name` の省略形
- `*`, `?` — ワイルドカード
- `Pattern1;Pattern2` — 複数パターン（OR ロジック）
- `-Pattern` — 否定フィルタ：一致するテストを除外
- `Pattern1;-Pattern2` — 包含と除外を組み合わせ（Pattern1 のうち Pattern2 を除外）

**例：**
- `--filter=Math.*` — Math スイートのテストをすべて実行
- `--filter='-Slow*'` — "Slow" で始まるテスト以外を実行
- `--filter='Math.*;-Math.Slow*'` — Math.Slow* を除く Math スイートを実行
- `--filter='*.Fast'` — 任意のスイートの Fast テストを実行
- `--filter='*.Fast;-Skip.*'` — Skip スイートを除く Fast テストを実行

---

## 終了コード

| コード | 意味 |
|--------|------|
| 0 | 全テスト合格（または `--list` モード） |
| 1 | 1 件以上のテスト失敗 |
| 2 | CLI パースエラー |
| 3 | 初期化失敗（テスト名重複、内部エラー） |
