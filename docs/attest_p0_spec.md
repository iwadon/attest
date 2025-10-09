# attest P0 Specification

## 1. 対象範囲（P0）
- 標準：C11（`_Generic` 使用）。
- 単一スレッド前提。
- 自動登録：GCC/Clang は constructor 属性、その他は代替経路。
- 実行は逐次。TAP/JUnit・フィクスチャ・タイムアウトは P1。

---

## 2. エントリ・CLI・終了コード
### 2.1 エントリポイント
```c
int attest_main(int argc, char** argv);
```
- テスト列挙 → フィルタ → 実行 → 集計 → 出力 → 終了コード返却。

### 2.2 CLI オプション
- `--list`：全テスト名列挙のみ。終了コード 0。
- `--filter=<pattern>[;<pattern>...]`：実行対象を絞り込み。
- `--no-color`：色付け無効化。
- 未知オプション：エラー。終了コード 2。

### 2.3 終了コード
| コード | 意味 |
|--------|------|
| 0 | 全件成功 or `--list` |
| 1 | テスト失敗あり |
| 2 | CLI エラー |
| 3 | 初期化失敗 |

---

## 3. テスト定義・実行モデル
- テスト名形式：`Suite.Name`（重複禁止）。
- 実行順：登録順。
- 各テスト関数は `setjmp/longjmp` で囲い、ASSERT は中断、EXPECT は継続。
- 自然 return は成功。

---

## 4. フィルタ規則
- `Suite.Name` に対して `*`, `?` 使用可。
- 省略形：`Suite` → `Suite.*`, `.Name` → `*.Name`
- 複数指定：セミコロン区切り（OR）。
- マッチ 0 件：実行 0、終了コード 0。

---

## 5. 出力仕様
### 5.1 色付け
- 既定で有効。`--no-color` で無効。
- 緑：成功サマリ、赤：失敗、暗灰：ファイル・行番号。

### 5.2 メッセージ構造
```
[==========] Running N tests from M suites.
[  FAILED  ] Suite.Name
  file.c:123: EXPECT_EQ(a, b) failed.
    expected: 42
      actual: 24
    expr: a=42, b=24
[==========] N tests ran. F failures.
[  PASSED  ] N tests.
```
- `--list` 時は `Suite.Name` のみ出力。

---

## 6. アサーション
### 6.1 種別
- 致命：`ASSERT_TRUE/FALSE/EQ/NE/LT/LE/GT/GE`
- 非致命：`EXPECT_TRUE/FALSE/EQ/NE/LT/LE/GT/GE`
- 文字列：`*_STREQ`, `*_STRNE`
- メモリ：`*_MEMEQ(ptrA, ptrB, n)`
- 浮動小数点：`*_NEAR(a, b, eps)`

### 6.2 型分派
- `_Generic` により int/long/unsigned/pointer/float/double を自動分派。

### 6.3 文字列・メモリ
- `*_STREQ`: `NULL` 同士は真。NULL vs 非NULL は偽。
- 出力は `"(null)"`。
- `*_MEMEQ`: `n==0` は真。NULL かつ `n>0` は偽。

### 6.4 浮動小数点
- 判定：`fabs(a - b) <= eps`
- `eps` 必須。NaN は常に偽。±inf 同符号は真。

### 6.5 出力テンプレート
致命：
```
[  FAILED  ] Suite.Name
  file.c:LINE: ASSERT_<OP>(lhs, rhs) failed (fatal).
    expected: <pretty(lhs)>
      actual: <pretty(rhs)>
    expr: lhs=<pretty>, rhs=<pretty>
```
非致命：`ASSERT_` → `EXPECT_` に変更、`(fatal)` 省略。

---

## 7. 集計
### 7.1 テスト単位
- `assertions_total`：発火数。
- `fail_nonfatal`：EXPECT 失敗数。
- `fail_fatal`：ASSERT 失敗数。
- `status`：`ATT_TEST_OK` / `ATT_TEST_FAILED`

### 7.2 全体サマリ
- `tests_total`, `tests_failed`, `assertions_total`, `failures_total` を保持。

---

## 8. サブテスト API（自己テスト用）
```c
typedef enum { ATT_OK=0, ATT_FAIL=1, ATT_ABORTED=2 } att_status;
typedef struct {
  int total;
  int failed;
  int fatal_failures;
  int nonfatal_failures;
  int skipped;
  att_status status;
} att_result;
att_status att_run_subtest(const char* name, void (*fn)(void*), void* user, att_result* out);
```
- 独立文脈で `fn` 実行。`ASSERT_*` の中断は親に波及しない。
- `out` に結果格納。戻り値は `out->status` と同じ。

### 8.3 期待失敗糖衣
```c
ATT_EXPECT_SUBTEST_FAILS(name, block, min, max)
```
- `failed ∈ [min,max]` なら成功、それ以外は非致命失敗。

---

## 9. 出力捕捉 API
```c
typedef struct { char* data; size_t size; } att_captured;
int att_capture_begin(void);
int att_capture_end(att_captured* out);
```
- `stderr` を内部バッファに切替。ネスト不可。
- `att_capture_end` で文字列取得（呼び出し側が `free` で解放すること）。

---

## 10. エラーハンドリング
| 状況 | 出力 | 終了コード |
|------|------|-------------|
| 未知オプション | `error: unknown option '--xxxx'` | 2 |
| 重複テスト名 | `error: duplicate test name 'Suite.Name'` | 3 |
| 内部不整合 | `error: initialization failed` | 3 |

---

## 11. P1 へ送る項目
- `ATT_SKIP`, `TEST_F`, TAP/JUnit, タイムアウト/シグナル, 並列実行, C89/90 互換, long double, ULP 比較, 複合フィルタ。

---

## 12. P0 完了判定
1. ASSERT/EXPECT 全種が規定文言で出力。
2. 集計が仕様通り。
3. 致命失敗で後続停止。
4. `--list`/`--filter` が動作。
5. `--no-color` 有効。
6. `att_run_subtest` と `att_capture_*` で自己テスト可能。
7. 終了コード仕様通り。
