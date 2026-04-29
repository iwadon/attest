# attest

[English](README.md) | **日本語**

GoogleTestにインスパイアされた、シンプルさと自動テスト登録を重視した軽量なC11ユニットテストフレームワークです。

## 特徴

- GoogleTest風の `TEST()` / `TEST_F()` マクロ
- 自動テスト登録（手動でのリスト管理不要）
- C11 `_Generic` による型汎用アサーション
- 致命的（`ASSERT_*`）/ 非致命的（`EXPECT_*`）アサーション
- フィクスチャ、サブテスト、スキップ、スコープ付き情報
- CLIフィルタリング、カラー出力、TAP/JUnit出力
- `--jobs=N` による並列実行
- 外部依存なし

## クイックスタート

```bash
# ビルド
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# テスト実行
./build/attest_selftest
```

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

## ドキュメント

- **[APIリファレンス](docs/api.md)** — 全アサーション、マクロ、CLIオプション
- **[ユーザーガイド](docs/guide.md)** — フィクスチャ、サブテスト、スキップ等の使い方
- **[内部設計](docs/internals.md)** — アーキテクチャ、設計判断、プラットフォームサポート
- **[ロードマップ](docs/roadmap.md)** — 将来計画（P1+）

## コンパイラサポート

| コンパイラ | バージョン | 備考 |
|-----------|-----------|------|
| GCC | 5.0+ | ⚠️ GCC 14.2.0 ARM64は既知の問題あり、Clang推奨 |
| Clang | 3.1+ | ARM64で推奨 |
| MSVC | 2015+ | 自動登録に `.CRT$XCU` セクションを使用 |

## Human68k（Sharp X680x0）向けクロスビルド

[elf2x68k](https://github.com/yunkya2/elf2x68k) ツールチェインを使って Human68k 向けにクロスコンパイルできます。

```bash
cmake -S . -B build-h68k -DCMAKE_TOOLCHAIN_FILE=cmake/human68k.cmake
cmake --build build-h68k
```

ツールチェインのインストール先は以下の順で自動検出されます。

1. `-DELF2X68K_ROOT=<path>`（CMakeオプション）
2. 環境変数 `ELF2X68K_ROOT`
3. `brew --prefix elf2x68k`（Homebrewユーザはゼロ設定で動作）
4. `PATH` 上の `m68k-xelf-gcc` の親ディレクトリ

いずれも見つからない場合、インストール手順を示すエラーメッセージで configure が失敗します。

## ライセンス

MIT No Attribution (MIT-0)。詳細は [LICENSE](LICENSE) を参照してください。
