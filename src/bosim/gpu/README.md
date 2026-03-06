# bosim/gpu {#bosim_gpu}

GPU 関連のモジュールを実装する。

## コードマップ

* `simulate_gpu.[h|cu]`: GPU 版のシミュレーション実行関数を実装
* `simulator_gpu.cuh`：状態を回路に通して結果を保持する GPU 版のシミュレータクラスを実装
* `state_gpu.cuh`：各種演算をメソッドとして持つ GPU 版の状態クラスを実装
