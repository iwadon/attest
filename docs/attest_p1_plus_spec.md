# attest P1+ Specification (Draft)

この文書は attest フレームワークの P1.1 以降に向けた仕様・拡張計画をまとめたものです。P1 の基本機能については `docs/attest_p1_spec.md` を参照してください。本書は P1 実装後に追加する機能群の検討資料として利用します。

---

## 1. 比較・検証強化
### 1.1 型拡張
- `long double` / `_Decimal64` / `_Float128` など拡張浮動小数型の対応。
- C89/90 向け：`EXPECT_INT_EQ` などの明示型付きマクロ群を提供。

---

## 2. 並列実行 (実験的)
- `--jobs=N`：最大同時実行数。
- スレッドプールによりテスト並列化（共有リソース注意）。
- 出力順序は登録順を保証。

---

## 3. 拡張 API
### 3.1 カスタムアサーション
- `ATT_ASSERT(expr, message)`：単純な布告型アサート。
- `ATT_EXPECT(predicate_fn, context)`：関数を評価して非致命失敗を判定。

---

## 4. レポート・統計 API
- 内部統計構造体を外部に公開：
  ```c
  const att_summary* attest_summary(void);
  ```
- 含まれる情報：総件数、失敗件数、スキップ件数、実行時間、最長テスト名など。
- P2 以降で JSON 出力 (`--format=json`) を検討。

---

## 5. 互換性層
- C89/90 対応のための明示マクロセット：
  ```c
  EXPECT_INT_EQ, EXPECT_UINT_EQ, EXPECT_STR_EQ, EXPECT_PTR_EQ
  ```
- 自動登録が使用できない環境では `ATT_MANUAL_REGISTER(Suite, Name, Fn)` を用意。

---

## 6. 将来的な拡張候補
- `att_benchmark`：軽量ベンチマーク計測 API。
- `att_mock`：簡易モック・スタブ補助マクロ群。
- `att_trace`：テスト中のログ収集フック。
- Visual Studio / CLion 用プラグイン連携（出力解析用）。

---

## 7. 仕様進行段階
| 段階 | 内容 |
|------|------|
| P1 | フィクスチャ、スキップ、TAP/JUnit、タイムアウト（詳細は `docs/attest_p1_spec.md`） |
| P1.1 | 詳細は `docs/attest_p1.1_spec.md` を参照 |
| P1.2 | 並列実行、カスタムアサーション |
| P2 | JSON 出力、ベンチマーク、モック |

---

## 8. ドキュメント運用
- 本書は P1.1 以降の設計の基礎仕様として維持。
- 各リリース段階（P1.1, P1.2, P2）ごとに差分を追記する。
- P0 仕様とは独立に改訂可能。
