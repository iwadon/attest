# attest P1+ Specification (Draft)

この文書は、attest フレームワークの P1 以降の仕様・拡張計画をまとめたものです。P0 実装完了後の機能拡張や設計検討の基礎資料として利用します。

---

## 1. フィクスチャ機構 (`TEST_F`)
### 1.1 目的
- GoogleTest の `TEST_F` と同等の使い勝手を C 言語で実現。
- テストスイート単位で共有される **フィクスチャ構造体** をサポート。

### 1.2 設計方針
- ユーザ定義構造体（例：`typedef struct MyFixture { ... } MyFixture;`）をテストごとに生成。
- `SetUp(MyFixture* fx)` と `TearDown(MyFixture* fx)` を自動呼出。
- 実行順序：`SetUp` → テスト本体 → `TearDown`。
- メモリ割当：既定でスタック上、サイズ超過時はヒープ確保。
- 各 `TEST_F` 関数には `MyFixture*` を渡す。内部的に現在のフィクスチャはスレッドローカル変数で保持。

### 1.3 例
```c
TEST_F(MyFixture, Math, AddsCorrectly) {
    ASSERT_EQ(fx->a + fx->b, 42);
}
```

---

## 2. スキップと条件実行
### 2.1 スキップ API
- `ATT_SKIP("reason")`：現在のテストをスキップとして終了。
- スキップ時：失敗カウントには含めず、サマリで別集計。
- スキップ出力例：
  ```
  [  SKIPPED ] Suite.Name
    reason: unsupported platform
  ```

### 2.2 条件付きスキップ
- 簡易マクロ：
  ```c
  #define ATT_SKIP_IF(cond, reason) do { if (cond) ATT_SKIP(reason); } while (0)
  ```

---

## 3. 出力フォーマット拡張
### 3.1 TAP 13 対応
- CLI オプション：`--format=tap`。
- 出力例：
  ```
  1..3
  ok 1 - SuiteA.Test1
  not ok 2 - SuiteA.Test2 # FAILED
  ok 3 - SuiteB.Test1
  ```

### 3.2 JUnit XML 対応
- CLI オプション：`--format=junit --output=path.xml`
- XML 形式は JUnit 互換スキーマを採用。
- 各 `<testcase>` に `classname`, `name`, `time`, `failure` 等を含む。

---

## 4. 実行制御オプション
| オプション | 内容 |
|-------------|------|
| `--shuffle` | テスト実行順序をシャッフル |
| `--repeat N` | N 回繰り返す |
| `--seed S` | シャッフルの乱数シード指定 |
| `--timeout-ms N` | 各テストの上限実行時間 (ms) |

### 4.1 タイムアウト動作
- タイムアウト発生時は **強制中断 + FAILED** 扱い。
- POSIX: `alarm()` / `timerfd`、Windows: `WaitForSingleObject` 相当。

### 4.2 シグナル捕捉
- SIGSEGV/SIGABRT/SIGFPE を捕捉し、該当テストのみ失敗として継続。
- 出力例：
  ```
  [  FAILED  ] Suite.CrashTest (segmentation fault)
  ```

---

## 5. 比較・検証強化
### 5.1 相対誤差・ULP 比較 (浮動小数点)
- `*_NEAR_REL(a, b, rel_eps)`：相対誤差判定。
- `*_ULP_EQ(a, b, max_ulp)`：ULP 差比較。

### 5.2 型拡張
- `long double` / `_Decimal64` / `_Float128` など拡張浮動小数型の対応。
- C89/90 向け：`EXPECT_INT_EQ` などの明示型付きマクロ群を提供。

---

## 6. 並列実行 (実験的)
- `--jobs=N`：最大同時実行数。
- スレッドプールによりテスト並列化（共有リソース注意）。
- 出力順序は登録順を保証。

---

## 7. 拡張 API
### 7.1 SCOPED_INFO
- コンテキスト文字列をスコープ単位でスタックに積み、失敗時に追加出力。
```c
SCOPED_INFO("iteration=%d", i);
EXPECT_EQ(result, expected);
```
出力：
```
  context: iteration=5
```

### 7.2 カスタムアサーション
- `ATT_ASSERT(expr, message)`：単純な布告型アサート。
- `ATT_EXPECT(predicate_fn, context)`：関数を評価して非致命失敗を判定。

---

## 8. レポート・統計 API
- 内部統計構造体を外部に公開：
  ```c
  const att_summary* attest_summary(void);
  ```
- 含まれる情報：総件数、失敗件数、スキップ件数、実行時間、最長テスト名など。
- P2 以降で JSON 出力 (`--format=json`) を検討。

---

## 9. 互換性層
- C89/90 対応のための明示マクロセット：
  ```c
  EXPECT_INT_EQ, EXPECT_UINT_EQ, EXPECT_STR_EQ, EXPECT_PTR_EQ
  ```
- 自動登録が使用できない環境では `ATT_MANUAL_REGISTER(Suite, Name, Fn)` を用意。

---

## 10. 将来的な拡張候補
- `att_benchmark`：軽量ベンチマーク計測 API。
- `att_mock`：簡易モック・スタブ補助マクロ群。
- `att_trace`：テスト中のログ収集フック。
- Visual Studio / CLion 用プラグイン連携（出力解析用）。

---

## 11. 仕様進行段階
| 段階 | 内容 |
|------|------|
| P1 | フィクスチャ、スキップ、TAP/JUnit、タイムアウト |
| P1.1 | SCOPED_INFO、相対誤差、ULP 比較 |
| P1.2 | 並列実行、カスタムアサーション |
| P2 | JSON 出力、ベンチマーク、モック |

---

## 12. ドキュメント運用
- 本書は P1 以降の設計の基礎仕様として維持。
- 各リリース段階（P1, P1.1, P1.2, P2）ごとに差分を追記する。
- P0 仕様とは独立に改訂可能。

