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
- セットアップ／ティアダウンはそれぞれ `ATT_FIXTURE_SETUP(Fixture)`、`ATT_FIXTURE_TEARDOWN(Fixture)` マクロで宣言し、マクロが展開する内部名・可視性に従う。
- 各マクロは `static void Fixture_SetUp(Fixture* att_fixture)` / `static void Fixture_TearDown(Fixture* att_fixture)` を生成し、マクロ本体は `att_fixture` を通じてインスタンスへアクセスする。マクロを宣言しなかった場合は対応するフェーズをスキップする。
- attest は各テスト実行時に以下の順序を保証する。
	1. フィクスチャ インスタンス確保（ヒープ上に確保）
	2. セットアップ関数（存在する場合）の呼び出し
	3. テスト本体関数へ `Fixture*` を引数として渡す
	4. ティアダウン関数（存在する場合）の呼び出し
	5. リソース解放
- テスト本体では `Fixture* att_fixture` という識別子が暗黙に定義され、`ATT_FIXTURE(type)` マクロで型を明示して取得できる。
- 現在のフィクスチャポインタはスレッドローカルで保持し、`ATT_FIXTURE` はそこから参照を取り出す。
- アサーション失敗（`ASSERT_*`）や `ATT_SKIP` によりテストが中断した場合でも `TearDown` は実行される。
- フィクスチャは常にヒープ確保とし、スタック上への配置を強制する API は提供しない。
- `ASSERT_*` が SetUp 内で失敗した場合、テスト本体は実行せずに「fixture setup failure」としてレポートする。出力には `(setup)` タグを付与し、従来の `(fatal)` 表記に倣って強調する。
- TearDown 内の `ASSERT_*` 失敗はテストの失敗結果に追記され、既存の失敗情報を上書きしない。出力には `(teardown)` タグを付与し、テスト終了ログ直後に詳細を追加する。

### 2.3 使用例
```c
typedef struct {
	int lhs;
	int rhs;
} MathFx;

ATT_FIXTURE_SETUP(MathFx) {
	att_fixture->lhs = 40;
	att_fixture->rhs = 2;
}

TEST_F(MathFx, AddsCorrectly) {
	MathFx* fx = ATT_FIXTURE(MathFx);
	ASSERT_EQ(fx->lhs + fx->rhs, 42);
}
```

ティアダウン失敗時の出力例（プレーンフォーマット）：
```
[ RUN      ] Suite.LeavesResource
[  FAILED  ] Suite.LeavesResource (teardown)
  (teardown) ASSERT_EQ(resource_refcount, 0)
             expected 0
             actual   1
```

---

## 3. スキップと条件実行
### 3.1 API
- `ATT_SKIP(const char* reason)`：現在のテストをスキップとして終了。
- スキップ時は失敗カウントに含めず、サマリで `skipped` 件数を新設。
- 全テストがスキップされた場合でも終了コードは成功（0）とし、失敗が一件でもあれば既存と同様に非 0 とする。

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
- スキップは `ok <index> <name> # SKIP <reason>` の TAP 13 指定に従い、理由が無い場合は `(none)` を出力する。
- TAP フォーマット選択時は既定のテキストサマリを抑制し、TAP 出力のみを標準出力へ書き出す。

### 4.2 JUnit XML
- CLI 追加フラグ：`--format=junit --output=path.xml`
- 生成 XML は JUnit 互換スキーマとし、以下を含む。
	- `<testsuite>`：`tests`, `failures`, `skipped`, `time`
	- `<testcase>`：`classname`, `name`, `time`
	- 失敗時 `<failure message="...">詳細</failure>`
	- スキップ時 `<skipped message="reason"/>` を付与し、`<testsuite skipped>` 属性でスキップ数を集計する
- CLI で `--output` が未指定の場合は `test_detail.xml` をカレントディレクトリに生成する（GoogleTest に準拠）。
- JUnit フォーマット選択時も既定サマリは出力せず、XML ファイルのみを生成する。

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
- `--timeout-ms` を有効にした状態でユーザーコードが `signal` / `sigaction` などでシグナルハンドラを再設定すると、attest が仕込む監視ハンドラが動作しない場合がある。仕様としてその際のタイムアウト挙動は保証外であることを明記し、ドキュメントおよび CLI ヘルプで注意喚起する。

---

## 6. 実装・移行指針
- P1 実装時は `docs/attest_p0_spec.md` に影響する振る舞いを確認し、互換性破壊がないことを検証する。
- 新規 CLI フラグは `att_cli_parse` に追加し、既存テストに加えて回帰テスト（自己テスト or integration）を整備する。
- `ATT_SKIP` や `TEST_F` 導入によるサマリ変更は、既存の P0 テキストにも明記して差分を共有する。プレーン出力の最終サマリには `skipped` カウントを追加する。
