\page cmake_readme CMake

\tableofcontents{html}

## 依存ライブラリ

| Library       | Version | License                                                              | Related CMake option                           |
| ------------- | ------- | -------------------------------------------------------------------- | ---------------------------------------------- |
| Boost         | 1.86.0  | Boost Software License                                               |                                                |
| curl          |         |                                                                      | `BOSIM_BUILD_SERVER`, `BOSIM_BUILD_SERVER_LIB` |
| eigen3        | 3.4.0   | MPL-2.0                                                              |                                                |
| fmt           | 11.2.0#0 | [License.rst](https://github.com/fmtlib/fmt/blob/master/LICENSE.rst) |                                                |
| GoogleTest    | 1.15.2  | BSD 3-Clause License                                                 | `BOSIM_BUILD_TESTS`                            |
| grpc          | 1.68.1  | Apache-2.0 License                                                   | `BOSIM_BUILD_SERVER`, `BOSIM_BUILD_SERVER_LIB` |
| nanobind      | 2.8.0   | BSD 3-Clause License                                                 | `BOSIM_BUILD_BINDING`                          |
| nlohmann-json | 3.11.3  | MIT                                                                  |                                                |
| protobuf      | 5.28.3  | BSD 3-Clause License                                                 |                                                |
| taskflow      | 3.8.0   | MIT                                                                  |                                                |
| zlib          |         |                                                                      | `BOSIM_BUILD_SERVER`, `BOSIM_BUILD_SERVER_LIB` |

- Boost (functional, math), eigen3, fmt, nlohmann-json, protobuf, taskflow は常に必要なライブラリである。
    - protobuf は grpc に付属するものではなく、指定バージョンを明示的にインストールする必要がある。
- テストを有効化する際は GoogleTest が必要である。
- サーバをビルドする際は Boost (program-options), curl, grpc, zlib が追加で必要である。

ライブラリのバージョン指定は `deps.cmake` で行っている。

## CMake のオプション

| Option                 | Default | Description                                                           |
| ---------------------- | ------- | --------------------------------------------------------------------- |
| BOSIM_DEV_MODE         | `OFF`   | `ON` の場合、開発者モードでライブラリをビルドする。                   |
| BOSIM_USE_GPU          | `OFF`   | `ON` の場合、GPU を使用する。                                         |
| BOSIM_BUILD_EXAMPLES   | `OFF`   | `ON` の場合、bosim を使用したサンプルコードをビルドする。             |
| BOSIM_BUILD_SERVER     | `OFF`   | `ON` の場合、bosim のサーバをビルドする。                             |
| BOSIM_BUILD_SERVER_LIB | `OFF`   | `ON` の場合、bosim のサーバ（ライブラリのみ）をビルドする。           |
| BOSIM_BUILD_TESTS      | `OFF`   | `ON` の場合、テストをビルドする。                                     |
| BOSIM_BUILD_BENCHMARK  | `OFF`   | `ON` の場合、ベンチマークをビルドする。                               |
| BOSIM_BUILD_BINDING    | `OFF`   | `ON` の場合、Python バインディングをビルドする。                      |
| BOSIM_MEASURE_COVERAGE | `OFF`   | `ON` の場合、テスト実行時に、カバレッジを計測する (Linux のみ対応) 。 |

### Python Binding のビルド

Python Binding をビルドする場合は、次のようにオプションを指定する。

- `BOSIM_BUILD_BINDING=ON`

> [!note]
>
> Python Binding をビルドする場合、以下のオプションはすべて OFF にする必要がある：
> `BOSIM_BUILD_EXAMPLES`, `BOSIM_BUILD_SERVER`, `BOSIM_BUILD_SERVER_LIB`, `BOSIM_BUILD_TESTS`, `BOSIM_BUILD_BENCHMARK`

### サーバのビルド

サーバをビルドする場合は、次のようにオプションを指定する。

- `BOSIM_BUILD_SERVER=ON`
- `CMAKE_BUILD_TYPE=Release`

#### サーバのテスト

サーバに関連するテストをビルドする場合は、次のようにオプションを指定する。

- `BOSIM_BUILD_SERVER_LIB=ON`
- `BOSIM_BUILD_TESTS=ON`

### カバレッジ測定

Linux環境でのみカバレッジ計測を行うことができる。
カバレッジ計測する際は次のようにオプションを指定する。

- `CMAKE_BUILD_TYPE=Debug`
- `BOSIM_USE_GPU=OFF`
- `BOSIM_BUILD_SERVER=OFF`
- `BOSIM_BUILD_SERVER_LIB=OFF`
- `BOSIM_BUILD_TESTS=ON`
- `BOSIM_MEASURE_COVERAGE=ON`
