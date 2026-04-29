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

attest 本体は C99 で動作し、C11 の `_Generic` は `__STDC_VERSION__` ガードで適応的に使用します。

| コンパイラ | 最低バージョン | 備考 |
|-----------|---------------|------|
| GCC | 5.0+ | `__attribute__((constructor))` を要求 |
| Clang | 3.1+ | 同上 |
| MSVC | 2015+ | 自動登録に `.CRT$XCU` セクションを使用 |

### 動作確認済みプラットフォーム

#### Apple Silicon, macOS 26

`arm64-apple-darwin` 上で `Release`（`-O3`）ビルドし、selftest（74 件）を通したものを以下に列挙します。

| コンパイラ | バージョン | 結果 | 備考 |
|-----------|-----------|------|------|
| Apple Clang | 21（Xcode 同梱） | ✅ パス | リファレンスとなる toolchain |
| Homebrew Clang | 16 / 17 / 18 / 19 / 20 / 21 / 22 | ✅ パス | |
| Homebrew Clang | 14 / 15 | ⚠️ リンク失敗 | Homebrew のボトルが `--syslibroot=…/CommandLineTools/SDKs/MacOSX14.sdk` をハードコードしているため。Command Line Tools を更新するか、`-Wl,-syslibroot,$(xcrun --show-sdk-path)` を渡すと解消する。コンパイラ自体は健全。 |
| Homebrew GCC | 12 / 13 / 14 / 15 | ✅ パス | |
| Homebrew GCC | 11 | ⚠️ リンク失敗 | Clang 14/15 と同じ SDK 固定問題。 |

過去のリリースでは ARM64 上の GCC 14.2 で sigsetjmp/longjmp の誤コンパイルを避けるため
`-O1` ワークアラウンドを必要としていましたが、根本原因は attest 側の
クロスフレーム `setjmp`（呼び出し元のフレームと異なる関数で `setjmp` を行っていたこと）でした。
現在はテストランナーの `setjmp` を呼び出し元フレームへマクロ展開する形で根治しており、
動作確認済みのいずれの toolchain でも最適化フラグの上書きは不要です。

#### Ubuntu 26.04 (aarch64)

Parallels Desktop の VM 上で `Debug` / `Release` 両方のビルドを確認しています。
`attest_selftest` は 74 件（73 passed / 1 skipped）、`attest_selftest_c99` は
12 件（11 passed / 1 skipped）でパスします。

| コンパイラ | バージョン | 結果 |
|-----------|-----------|------|
| GCC | 15.2.0 | ✅ パス |
| Clang | 21.1.8 | ✅ パス |

#### Windows 11 (arm64)

Parallels Desktop の VM 上で Visual Studio Community 2026
（18.2.111415.280）の `Debug` / `Release` 両構成を確認しています。MSYS2 は
未確認です。`attest_selftest` は 74 件（73 passed / 1 skipped）、
`attest_selftest_c99` は 12 件（11 passed / 1 skipped）でパスします。

| コンパイラ | バージョン | スレッディング | 結果 |
|-----------|-----------|---------------|------|
| MSVC `cl` | 19.50.35723.0（toolset 14.50.35717） | Win32 threads + `__declspec(thread)` | ✅ パス |
| `clang-cl` | 20.1.8（`aarch64-pc-windows-msvc`） | C11 `<threads.h>` + `_Thread_local` | ✅ パス |

動作確認の過程で CP932 ロケール固有の問題を 2 件解消しています。MSVC の
`/utf-8` を `attest` ターゲットに PUBLIC で付与して日本語版 Windows 上の
C4819 を抑制し、`Fixture.SetupTeardownCounters` セルフテストはテスト登録順に
依存しない形へ書き換えて `--shuffle` でも安定するようにしました。

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
