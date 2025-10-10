# attest P1 Specification (Draft)

この文書は P1 ステージで導入する attest の機能拡張を定義します。P0 実装の挙動を前提とし、互換性に影響する変更点は段落ごとに明記します。

---

## 1. 追加スコープ
- フィクスチャ付きテスト (`TEST_F`)
- テストスキップ API
- 代替出力フォーマット（TAP 13・JUnit XML）
- 実行タイムアウト制御

P1 ではこれらの機能を既存 CLI/アサーションと統合し、後続の P1.1+ で予定する詳細拡張の基盤を整備します。

---

## 2. フィクスチャ機構 (`TEST_F`)
### 2.1 目的
- GoogleTest の `TEST_F` と同等の使い勝手を C で提供する。
- スイート単位で共有されるフィクスチャ構造体を自動生成・初期化する。

### 2.2 設計要件
- 利用者はフィクスチャ構造体を定義し、`TEST_F(Fixture, Name)` でテストを宣言する。
- attest は各テスト実行時に以下の順序を保証する。
	1. フィクスチャ インスタンス確保（スタック: 小サイズ / ヒープ: サイズ閾値超過）
	2. `Fixture_SetUp(Fixture* fx)` 呼び出し（存在する場合）
	3. テスト本体関数へ `Fixture*` を引数として渡す
	4. `Fixture_TearDown(Fixture* fx)` 呼び出し（存在する場合）
	5. リソース解放
- 現在のフィクスチャポインタはスレッドローカルで保持し、`ATT_CURRENT_FIXTURE(type)` のようなヘルパ公開を検討。
- アサーション失敗（`ASSERT_*`）や `ATT_SKIP` によりテストが中断した場合でも `TearDown` は実行される。

### 2.3 使用例
```c
typedef struct {
	int lhs;
	int rhs;
} MathFx;

void MathFx_SetUp(MathFx* fx) {
	fx->lhs = 40;
	fx->rhs = 2;
}

TEST_F(MathFx, AddsCorrectly) {
	ASSERT_EQ(fx->lhs + fx->rhs, 42);
}
```

---

## 3. スキップと条件実行
### 3.1 API
- `ATT_SKIP(const char* reason)`：現在のテストをスキップとして終了。
- スキップ時は失敗カウントに含めず、サマリで `skipped` 件数を新設。

### 3.2 出力
```
[  SKIPPED ] Suite.Name
  reason: unsupported platform
```
- `reason` が `NULL` の場合は `reason: (none)` を出力。

### 3.3 条件スキップマクロ
```c
#define ATT_SKIP_IF(cond, reason) \
	do { \
		if (cond) { \
			ATT_SKIP(reason); \
		} \
	} while (0)
```
- 条件評価は一度のみ。副作用を持つ式は避ける旨をドキュメント化する。

---

## 4. 出力フォーマット拡張
### 4.1 TAP 13
- CLI 追加フラグ：`--format=tap`
- ヘッダーに `1..N` を出力し、各テストごとに `ok`/`not ok` を記録。
- 失敗・スキップ時には診断コメント (`# FAILED`, `# SKIPPED`) を添える。

### 4.2 JUnit XML
- CLI 追加フラグ：`--format=junit --output=path.xml`
- 生成 XML は JUnit 互換スキーマとし、以下を含む。
	- `<testsuite>`：`tests`, `failures`, `skipped`, `time`
	- `<testcase>`：`classname`, `name`, `time`
	- 失敗時 `<failure message="...">詳細</failure>`
	- スキップ時 `<skipped message="reason"/>`
- CLI で `--output` が未指定の場合はエラー扱い（終了コード 2）。

### 4.3 互換性
- 既定フォーマット（P0 のプレーン出力）は `--format=default` またはフラグ未指定で継続。
- `--format` フラグは将来拡張を見越し、入力検証で未知フォーマットを拒否する。

---

## 5. 実行タイムアウト
### 5.1 CLI オプション
- `--timeout-ms=<duration>`：各テストケース／サブテストの最大実行時間（ミリ秒）。
- 既定値は 0（無制限）。有効時は `att_context_protect` と連携した中断処理を導入する。

### 5.2 挙動
- タイムアウト発生時はテストを強制終了し、`FAILED` としてレポート。
- 出力例：
	```
	[  FAILED  ] Suite.LongRunning
	  reason: timeout after 500 ms
	```
- サマリでは失敗として集計し、追加で `timeouts` 指標を保持する。
- POSIX 環境：`timerfd` または `pthread` タイマーで中断を実装。Windows は `WaitForSingleObject` 等で模倣。

### 5.3 シグナル/例外連携
- タイムアウトおよび `SIGSEGV` 等の致命シグナルは同一の失敗パスに流し、テスト名とエラー種別を含むメッセージを出力する。
- 将来的な P1.1+ のシグナル詳細レポートに備え、内部 API を抽象化しておく。

---

## 6. 実装・移行指針
- P1 実装時は `docs/attest_p0_spec.md` に影響する振る舞いを確認し、互換性破壊がないことを検証する。
- 新規 CLI フラグは `att_cli_parse` に追加し、既存テストに加えて回帰テスト（自己テスト or integration）を整備する。
- `ATT_SKIP` や `TEST_F` 導入によるサマリ変更は、既存の P0 テキストにも明記して差分を共有する。

