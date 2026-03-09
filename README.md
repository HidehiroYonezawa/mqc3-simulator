# Bosonic simulator {#mainpage}

\tableofcontents{html}

Bosonic simulator (bosim) は連続量量子回路を高速にシミュレートするライブラリである。

## ドキュメント

本ページでは主にライブラリの使用方法を説明する。本ライブラリの概要やアルゴリズムについてのドキュメントは[こちら](\ref docs_readme)。

## 環境構築

### サブモジュールの初期化

本リポジトリは外部ライブラリを Git Submodule として管理している。
ビルド前に必ず以下を実行する。

```sh
$ git submodule update --init --recursive
```

### C++/CUDA

このライブラリは C++20, CUDA12.4 で実装している。

### 依存ライブラリ

本ライブラリが使用しているライブラリのリストは[こちら](\ref cmake_readme)。

### ビルド

依存ライブラリは [Vcpkg](https://github.com/microsoft/vcpkg) を用いてインストールすることを推奨する。
`externals/vcpkg` にある Vcpkg を使用する場合は次のようにコマンドを実行する。

```sh
$ cmake -S . -B build/ -DCMAKE_TOOLCHAIN_FILE=externals/vcpkg/scripts/buildsystems/vcpkg.cmake
$ cmake --build build/ -- -j 8
```

CMake のオプションは[こちら](\ref cmake_readme)にまとめている。

### Docker

Docker を使う場合は次のコマンドを実行してコンテナ内でビルドする。

````{note}
 `.dockerignore` はリリースビルド用に `externals/` を ignore するよう設定している。
開発時には便利なライブラリもあるため、 `externals/` の ignore を外してからコンテナのビルドを推奨する。
````

```sh
# コンテナに入る
$ docker run --rm --gpus all -it -v $(pwd):/workspace -w /workspace bosim:gpu bash

# コンテナ内で次のコマンドを実行する
$ cmake -S . -B build -G Ninja \
    -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc \
    -DCMAKE_TOOLCHAIN_FILE=/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release \
    -DBOSIM_DEV_MODE=OFF -DBOSIM_USE_GPU=ON -DBOSIM_BUILD_EXAMPLES=ON \
    -DBOSIM_BUILD_SERVER=ON -DBOSIM_BUILD_TESTS=ON -DBOSIM_BUILD_BENCHMARK=OFF -DBOSIM_MEASURE_COVERAGE=OFF
$ cmake --build build -- -j 8
```

## プロジェクト構成

| Name                              | Description                                          |
| --------------------------------- | ---------------------------------------------------- |
| `.clang-format`                   | Clang フォーマッタの設定                             |
| `.clang-tidy`                     | Clang の静的解析ツールの設定                         |
| `.clangd`                         | Clang 言語サーバの設定                               |
| `CMakeLists.txt`                  | CMake のビルド設定を定義するエントリーポイント       |
| `.dockerignore`                   | コンテナビルド時に ignore するファイルを指定         |
| `Dockerfile`                      | ビルド用コンテナの設定                               |
| `Dockerfile.wheel`                | Pythonバインディングのwheelビルド用コンテナの設定    |
| `Doxyfile`                        | Doxygen によるドキュメント生成の設定                 |
| `Pipfile`                         | テストで使用する Python ライブラリを定義             |
| `pyrightconfig.json`              | Python コードの型検査設定                            |
| `README.md`                       | プロジェクトの概要や使用方法を記載する               |
| `ruff.toml`                       | Python コードのフォーマット・Lint 設定               |
| `vcpkg.json`                      | vcpkg の依存ライブラリを管理するマニフェストファイル |
| `Version.txt`                     | ライブラリのバージョン情報を記載                     |
| `benchmark/`                      | ベンチマークスクリプトを配置                         |
| [cmake/](\ref cmake_readme)       | CMake 用のユーティリティ関数を実装                   |
| `codegen/`                        | 自動生成されたコードを配置                           |
| [docs/](\ref docs_readme)         | プロジェクトのドキュメントを配置                     |
| [examples/](\ref examples_readme) | ライブラリの使用例を実装                             |
| `externals/`                      | 外部ライブラリを配置                                 |
| `proto/`                          | サーバーの通信を定義する proto files を配置          |
| `scripts/`                        | デプロイに関わるスクリプトを配置                     |
| [server/](\ref server_readme)     | サーバの実装コードを配置                             |
| [src/](\ref bosim_readme)         | ライブラリ本体を実装                                 |
| `tests/`                          | テストコードを実装                                   |

## テスト

一部のテストは Python (>=3.12) を使用するため、 `Pipfile` で環境構築してからテストする必要がある。

```sh
$ pipenv install
$ pipenv shell
```

CMake のオプションを `BOSIM_BUILD_TESTS=ON` と設定すると、テストをビルドする。
`ctest` コマンドですべてのテストを一括で実行できる。

```sh
$ cmake -S . -B build/ -DBOSIM_BUILD_TESTS=ON
$ cmake --build build/ -- -j 8
$ ctest --test-dir build/
```

## ドキュメント作成

ドキュメントは[docs/](docs/)に配置している。Doxygen を使用してドキュメントをビルドする。
`docs/` の画像は別途コピーする必要があることに注意する。

```sh
$ docker run --rm -v $(pwd):/data -it hrektts/doxygen /bin/bash -c "doxygen Doxyfile"
$ cp -r docs html
```

## Python バインディング

Python バインディングをインストールする場合は次のコマンドを実行する。

```sh
SKBUILD_CMAKE_DEFINE="CMAKE_TOOLCHAIN_FILE=externals/vcpkg/scripts/buildsystems/vcpkg.cmake;CMAKE_BUILD_TYPE=Release;BOSIM_BUILD_BINDING=ON;BOSIM_USE_GPU=OFF" pip install .
```

上のコマンドは以下を仮定している。環境に応じてこれらの値は変更すること。

- Vcpkg は `externals/` 以下のものを使用する
  - 別の場所にインストールされた Vcpkg を使用する場合は適切にパスを設定する
- GPU を使用しない
  - 使用する場合は `BOSIM_USE_GPU=ON` とする

wheel をビルドする場合は次のコマンドを実行する。

```sh
SKBUILD_CMAKE_DEFINE="BOSIM_BUILD_BINDING=ON" pip wheel -v -w mqc3-simulator .
```

### テスト

Python バインディングのテストを行う場合は `pytest` を使用する。

```sh
$ pytest
```


## 運用

[scripts/](scripts/) を参照。
