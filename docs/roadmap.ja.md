# ロードマップ

[English](roadmap.md) | **日本語**

本ドキュメントは現状の実装（P0、P1、P1.1）を超える将来計画をまとめたものです。

## 現在のステータス

| フェーズ | 状態 | 機能 |
|---------|------|------|
| P0 | 完了 | コアアサーション、CLI、終了コード、サブテスト、出力キャプチャ |
| P1 | 完了 | フィクスチャ、スキップ API、TAP/JUnit 出力、タイムアウト、並列実行 |
| P1.1 | 完了 | `NEAR_REL`、`ULP_EQ`、`SCOPED_INFO` |
| P1.2 | 完了 | 否定フィルタ（`--filter=-Pattern`） |

---

## P1+（将来）

### 型の拡張

- ~~`long double` サポート~~（実装済み：`att_handle_compare_long_double` が `_Generic` に組み込まれ、`LongDouble.*` の selftest で検証されている）
- `_Decimal64` / `_Float128`（利用可能な環境で）
- ~~C89/90 向けの明示型マクロ：`EXPECT_INT_EQ`、`EXPECT_UINT_EQ`、`EXPECT_PTR_EQ`~~（実装済み）

### カスタムアサーション ✓

`ATT_ASSERT(expr, fmt, ...)` および `ATT_EXPECT(expr, fmt, ...)` として printf 形式で実装済み。

```c
ATT_ASSERT(code == 0, "unexpected error code: %d", code);
ATT_EXPECT(is_valid(ptr), "ptr %p is not valid", ptr);
```

### 高度なフィルタリング

- ~~否定フィルタ：`--filter=-Slow.*`~~（P1.2 で実装済み）
- 正規表現サポート：`--filter=/Math\\..*Add/`
- タグベースのフィルタ：`[slow]`、`[integration]`

### 統計 API ✓

`attest_get_summary()` として実装済み。`total`、`passed`、`failed`、`skipped` フィールドを持つ `attest_summary` 構造体を返す。

```c
attest_summary s = attest_get_summary();
printf("Ran %d tests, %d passed\n", s.total, s.passed);
```

---

## P2（長期）

### JSON 出力

```bash
./test_runner --format=json --output=results.json
```

### パラメタライズドテスト

```c
ATT_PARAM_TEST(Math, Addition, int, a, int, b, int, expected) {
    ASSERT_EQ(a + b, expected);
}

ATT_PARAM_VALUES(Math, Addition,
    {1, 2, 3},
    {0, 0, 0},
    {-1, 1, 0}
);
```

### 軽量ベンチマーク

```c
ATT_BENCHMARK(Sort, QuickSort) {
    int data[1000];
    // setup...
    ATT_BENCH_START();
    quicksort(data, 1000);
    ATT_BENCH_END();
}
```

### Mock/Stub ヘルパー

```c
ATT_MOCK(file_read, char*, const char* path) {
    return "mocked content";
}
```

### IDE 連携

- Visual Studio プラグインによるテスト発見
- CLion の run configuration
- IDE が解釈しやすい出力フォーマットの拡張

---

## 互換性目標

### C89/90 対応

C11 が使えない組み込み / レガシー環境向け：

- ~~明示型マクロ（`_Generic` 不使用）~~（実装済み：`EXPECT_INT_*`、`EXPECT_UINT_*`、`EXPECT_PTR_*`）
- ~~手動登録のフォールバック~~（実装済み：`ATT_REGISTER_TESTS(...)`）
- 任意のヘッダオンリーモード

### 最小フットプリント

- 単純な統合のためのシングルヘッダ版
- コンパイルフラグで機能セットを設定可能に
- 組み込み用途向けにメモリ確保を削減

---

## 貢献アイデア

機能要望や実装アイデアを歓迎します。新機能を提案する際は次の点を考慮してください。

1. **互換性**：サポート対象のすべてのプラットフォームで動作するか？
2. **シンプルさ**：attest の軽量な思想に合致するか？
3. **テスト**：その機能を selftest できるか？

実装にあたっては [internals.md](internals.md) のアーキテクチャ詳細を参照してください。
