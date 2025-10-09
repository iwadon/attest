# attest capture ownership refactor (WIP)

最終更新: 2024-XX-XX

## 変更概要
- `att_capture_end` が返す `att_captured.data` の所有権を呼び出し側へ移譲する変更済み。
- API ヘッダ (`include/attest/attest.h`)、自己テスト、P0 スペックには新しい契約を反映済み。
- `ATT_EXPECT_SUBTEST_FAILS` マクロ内でキャプチャ済み出力を必ず `free` するよう改修。
- 調査用に `att_run_tests` へ多数の `DEBUG` 出力を挿入し、保護フローや失敗件数を可視化中。

## 既知の問題
- 自己テスト (`attest_selftest`) が `Subtest.ReportsFailures` / `Subtest.RecordsAbort` 実行後にセグフォ。  
  - `att_context_abort` → `longjmp` 復帰後にスタック状態が壊れている可能性が高い。  
  - `att_subtest_scope` が `jmp_buf` を構造体ごとコピーしている点が疑わしい。
- `att_capture_begin` 再実行時にライブラリ側でバッファを解放しないため、呼び出し側が保持している場合と整合性が取れていない。
- テストや CLI 実行時に `DEBUG:` ログが標準エラーへ多量に出力される。

## 次のステップ（想定）
1. `att_subtest_scope` のコンテキスト保存を安全なスタックベース実装へ置き換える（`jmp_buf` の値コピー排除）。
2. `att_capture` のバッファライフサイクルを明確化し、`att_capture_begin` 再呼び出し時に古いデータへの参照が残らないよう調整。
3. デバッグ用の `DEBUG` 出力を削除して通常ログへ戻す。
4. `ctest` および ASan による再検証。

