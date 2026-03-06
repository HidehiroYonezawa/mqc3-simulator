# ベンチマーク

性能評価を行うための C++ および Python スクリプトから成るディレクトリである。

## セットアップ

*   bosim ベンチマーク用スクリプトのビルド
    ```sh
    $ cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=externals/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_C_COMPILER=<path_to_clang> -DCMAKE_CXX_COMPILER=<path_to_clang++> -DCMAKE_CUDA_COMPILER=<path_to_nvcc> -DCMAKE_BUILD_TYPE=Release -DBOSIM_DEV_MODE=OFF -DBOSIM_USE_GPU=ON -DBOSIM_BUILD_BENCHMARK=ON
    $ cmake --build build/ -- -j 8
    ```
*   Python スクリプトは仮想環境内で実行することを推奨する。仮想環境のシェルはルートディレクトリで
    ```sh
    $ pipenv install --python <Python 実行ファイル> --dev
    $ pipenv shell
    ```
    を実行すると使える。

## bosim ベンチマーク用スクリプト

*   `rotation_op_time.cpp`
    *   位相回転回路を実行。ベンチマーク結果が標準出力およびファイル出力される。
    *   実行コマンド:
        ```sh
        taskset <コア数を指定するマスク> ./build/benchmark/rotation_op_time <cpu/gpu> <指定したコア数> <全モード数> <猫状態モード数> <出力ファイルのパス>
        ```
*   `bs_op_time.cpp`
    *   ビームスプリッター回路を実行。ベンチマーク結果が標準出力およびファイル出力される。
    *   実行コマンド:
        ```sh
        taskset <コア数を指定するマスク> ./build/benchmark/bs_op_time <cpu/gpu> <指定したコア数> <全モード数> <猫状態モード数> <出力ファイルのパス>
        ```
*   `measure_op_time.cpp`
    *   測定回路を実行。ベンチマーク結果が標準出力およびファイル出力される。
    *   実行コマンド:
        ```sh
        taskset <コア数を指定するマスク> ./build/benchmark/measure_op_time <cpu/gpu> <指定したコア数> <全モード数> <猫状態モード数> <出力ファイルのパス>
        ```
*   `benchmark.py`
    *   上記 3 種のスクリプトを複数のパラメータについて繰り返し実行する。
    *   出力ファイルはプロット用スクリプト `plot_bar.py` を用いて可視化できる。
        *   オプションとして、 `--cpu_peak_level` もしくは `--single_gpu_single_thread` を必ず追加する。
    *   実行コマンド:
        ```sh
        python benchmark/benchmark.py
        ```
    *   オプション:
        以下の3種類のオプションのいずれかが指定された場合、対応するベンチマークのみが実行される。複数指定も可能。いずれも指定しない場合は3つ全てが実行される。
        *   `--unary`: 位相回転回路を実行
            *   結果は標準出力およびカレントディレクトリの `benchmark_result_unary.csv` に出力される（ファイルが存在しない場合は作られる）。
        *   `--binary`: ビームスプリッター回路を実行
            *   結果は標準出力およびカレントディレクトリの `benchmark_result_binary.csv` に出力される（ファイルが存在しない場合は作られる）。
        *   `--measurement`: 測定回路を実行
            *   結果は標準出力およびカレントディレクトリの `benchmark_result_measurement.csv` に出力される（ファイルが存在しない場合は作られる）。

        以下の5種類のオプションのいずれかが指定された場合、対応するバックエンドでベンチマークが実行される。複数指定も可能。いずれも指定しない場合は全てのバックエンドで実行される。

        *   `--cpu`: 状況に応じて CPU ショットレベル並列か CPU ピークレベル並列を使い分ける。
        *   `--cpu_shot_level`: CPU ショットレベル並列
        *   `--cpu_peak_level`: CPU ピークレベル並列
        *   `--single_gpu`: 単一GPUでショットレベル並列
        *   `--single_gpu_single_thread`: 単一GPUでシングルスレッド
        *   `--multi_gpu`: 複数GPUでショットレベル並列

        加えて、以下のオプションがある。

        *   `--n_shots`: ショット数（デフォルト: 1）
            * cpu_shot_level, single_gpu, multi_gpu などの主にショットレベルで並列化を行うバックエンドのベンチマークを取る際に大きな値に変更する。
        *   `--n_warmup`: ウォームアップの回数（デフォルト: 0）
        *   `--n_trial`: 試行数（デフォルト: 1）
        *   `--n_cores`: マルチスレッド実行において使用するコア数（デフォルト: 64）
        *   `--timeout`: タイムアウト（デフォルト: 60 秒）
            * 上記3種スクリプトを **一回** 実行する際のタイムアウト
            * `benchmark.py` 全体のタイムアウトではない。
    *   状態と操作の設定についての詳細は `docs/performance.md` に示す。
    *   個々のスクリプトの実行だけでは必要な情報を全て出力しないため、 `plot_bar.py` でプロットを作成前に `benchmark.py` を実行する必要がある。

*   `initial_state_time.cpp`
    *   初期状態準備の時間のみを計測。結果が標準出力およびファイル出力される。
    *   実行コマンド:
        ```sh
        taskset <コア数を指定するマスク> ./build/benchmark/initial_state_time <指定したコア数> <全モード数> <猫状態モード数> <出力ファイルのパス>
        ```
*   `table_header.cpp`
    *   時間計測する範囲（セクション）を表すヘッダを標準出力およびファイル出力
    *   実行コマンド:
        ```sh
        ./build/benchmark/table_header <出力ファイルのパス>
        ```
*   `utility.py`
    *   ベンチマークのためのユーティリティ関数を持つ。

## プロット用スクリプト

*   `plot.py`
    *   時間計測結果のコア数依存性を表す図を作成し `plot.png` に出力
    *   実行コマンド:
        ```sh
        python plot.py <入力 CSV ファイルのパス> <ベンチマークの種類（例: rotation_op_time）>
        ```
*   `plot_change.py`
    *   複数の時間計測結果の差のコア数依存性を表す図を作成し `plot.png` に出力（ソースファイルに変更を加えた際に差分の評価に使用）
    *   実行コマンド:
        ```sh
        python plot_change.py <入力 CSV ファイルのパス0（比較対象）> <入力 CSV ファイルのパス1> ...
        ```
    *   スクリプト内では例として測定回路の回路実行時間をプロットするように設定している。
*   `plot_bar.py`
    *   `docs/performance.md` に示す図を作成しSVGファイルに出力
    *   `benchmark.py` における `cpu_peak_level` および `single_gpu_single_thread` の結果をプロットする。
    *   実行コマンド:
        ```sh
        python plot_bar.py
        ```
    *   オプション:
        以下の3種類のオプションのいずれかが指定された場合、対応するベンチマーク結果の図が作成される。複数指定も可能。いずれも指定しない場合は3つ全てが実行される。
        *   `--unary`: 位相回転回路の結果をプロット
            結果は標準出力およびカレントディレクトリの `plot_bar_unary.svg` に出力される。
        *   `--binary`: ビームスプリッター回路の結果をプロット
            *   結果は標準出力およびカレントディレクトリの `plot_bar_binary.svg` に出力される。
        *   `--measurement`: 測定回路の結果をプロット
            *   結果は標準出力およびカレントディレクトリの `plot_bar_measurement.svg` に出力される。

        上記3種類のプロットに対応して入力ファイルを指定するオプションがある。
        *   `--unary-input`: `benchmark.py` による位相回転回路のベンチマークの出力CSVファイルを指定（デフォルト: `<カレントディレクトリ>/benchmark_result_unary.csv`）
        *   `--binary-input`: `benchmark.py` によるビームスプリッター回路のベンチマークの出力CSVファイルを指定（デフォルト: `<カレントディレクトリ>/benchmark_result_binary.csv`）
        *   `--measurement-input`: `benchmark.py` による測定回路のベンチマークの出力CSVファイルを指定（デフォルト: `<カレントディレクトリ>/benchmark_result_measurement.csv`）

        また、以下のオプションがある。
        *   `--n_cores`: `benchmark.py` で使用したコア数（デフォルト: 64）
        *   `--output_table_csv`: テーブル形式のベンチマーク結果をCSVファイルに出力する（デフォルトでは出力しない）
        *   `--output_table_md`: テーブル形式のベンチマーク結果をMarkdown形式で出力する（デフォルトでは出力しない）

## グラフ表現シミュレータ ベンチマーク用スクリプト

*   `graph/graph_repr.py`
    *   「初期化 → `n_steps` - 2個の位相回転 → 測定」を`n_local_macronodes`個並べたグラフ表現のオブジェクトを作成する関数を持つ。
*   `graph/graph_simulator_time.cpp`
    *   protoテキスト形式のグラフ表現を読み込んで実行。ベンチマーク結果が標準出力およびファイル出力される。
    *   実行コマンド:
        ```sh
        taskset <コア数を指定するマスク> ./build/benchmark/graph/graph_simulator_time <グラフ表現protoテキストファイルのパス> <出力ファイルのパス> <cpu/gpu> <指定したコア数> <n_local_macronodes> <n_steps> <ショット数>
        ```
*   `graph/graph_table_header.cpp`
    *   時間計測する範囲（セクション）を表すヘッダを標準出力およびファイル出力
    *   実行コマンド:
        ```sh
        ./build/benchmark/graph/graph_table_header <出力ファイルのパス>
        ```
*   `graph/benchmark.py`
    *   ローカルマクロノード数とステップ数を変えて回路を実行。ベンチマーク結果が標準出力およびカレントディレクトリの `graph_benchmark_result.csv` に出力される（ファイルが存在しない場合は作られる）。
    *   実行コマンド:
        ```sh
        python benchmark/graph/benchmark.py
        ```
*   `graph/plot.py`
    *   グラフ表現シミュレータのベンチマーク結果をプロット。
    *   実行コマンド:
        ```sh
        python benchmark/graph/plot.py <ベンチマーク結果のCSVファイルのパス> <出力画像ファイルのパス>
        ```

## 実機表現シミュレータ ベンチマーク用スクリプト

*   `machinery/machinery_simulator_time.cpp`
    *   protoテキスト形式の実機表現を読み込んで実行。ベンチマーク結果が標準出力およびファイル出力される。
    *   実行コマンド:
        ```sh
        taskset <コア数を指定するマスク> ./build/benchmark/machinery/machinery_simulator_time <実機表現protoテキストファイルのパス> <出力ファイルのパス> <cpu/gpu> <指定したコア数> <n_local_macronodes> <n_steps> <ショット数>
        ```
*   `machinery/machinery_table_header.cpp`
    *   時間計測する範囲（セクション）を表すヘッダを標準出力およびファイル出力
    *   実行コマンド:
        ```sh
        ./build/benchmark/machinery/machinery_table_header <出力ファイルのパス>
        ```
*   `machinery/benchmark.py`
    *   ローカルマクロノード数とステップ数を変えて回路を実行。ベンチマーク結果が標準出力およびカレントディレクトリの `machinery_benchmark_result.csv` に出力される（ファイルが存在しない場合は作られる）。
    *   実行コマンド:
        ```sh
        python benchmark/machinery/benchmark.py
        ```
*   `machinery/plot.py`
    *   実機表現シミュレータのベンチマーク結果をプロット。
    *   実行コマンド:
        ```sh
        python benchmark/machinery/plot.py <ベンチマーク結果のCSVファイルのパス> <出力画像ファイルのパス>
        ```

## Strawberry Fields による回路実行の時間計測スクリプト

*   `sf_rotation_op_time.py`
    *   位相回転回路を時間測定
    *   実行コマンド:
        ```sh
        python sf_rotation_op_time.py
        ```
*   `sf_bs_op_time.py`
    *   ビームスプリッター回路を時間測定
    *   実行コマンド:
        ```sh
        python sf_bs_op_time.py
        ```
*   `sf_measure_op_time.py`
    *   測定回路を時間測定
    *   実行コマンド:
        ```sh
        python sf_measure_op_time.py
        ```
