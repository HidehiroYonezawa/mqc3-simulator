\page examples_readme 使用例

\tableofcontents{html}

## teleportation.cpp

bosim ライブラリの使用例として、量子テレポーテーションを実装した [examples/teleportation.cpp](@ref examples/teleportation.cpp) を紹介する。

### 回路の作成

量子回路は [Circuit](@ref bosim::Circuit) のコンストラクタで生成し、[EmplaceOperation](@ref bosim::Circuit::EmplaceOperation) を使って各操作を追加する。以下の手順で回路を構成する：

<ol start="0">
<li> ビームスプリッターをモード 1, 2 に適用 </li>
<li> ビームスプリッターをモード 0, 1 に適用 </li>
<li> モード 0 を測定角 \f$\theta_0 = \pi/2\f$ でホモダイン測定 </li>
<li> モード 1 を測定角 \f$\theta_1 = 0\f$ でホモダイン測定 </li>
<li> モード 2（テレポート先）を \f$(d_x, d_p)\f$ だけ変位 </li>
</ol>

測定結果に基づいてフィードフォワードを行う：

-   2 番目の測定結果を 4 番目の操作のパラメータ \f$d_x\f$ に反映
-   3 番目の測定結果を 4 番目の操作のパラメータ \f$d_p\f$ に反映

操作を追加する際、\f$(d_x, d_p)\f$ は仮に \f$(0, 0)\f$ としておき、後から [AddFF](@ref bosim::Circuit::AddFF) を使ってフィードフォワードを登録する。

### 初期状態の作成

状態は [State](@ref bosim::State) のコンストラクタで生成する。

テレポートする状態として、\f$(x, p)\f$ だけ変位したコヒーレント状態を用いる。また、回路内のモード 1, 2 の初期状態は以下のように設定する：

-   モード 1：\f$p\f$-スクイーズド状態（スクイージング角 \f$\pi/2\f$）
-   モード 2：\f$x\f$-スクイーズド状態（スクイージング角 \f$0\f$）

どちらのスクイーズド状態もスクイージングレベルは `SqueezingLevel=100` dB とする。

### 回路の実行

1 回のショットを CPU 上でシングルスレッド実行し、シミュレーション後の状態を保存する。実行には、[SimulateSettings](@ref bosim::SimulateSettings) を用いて設定し、これを回路 ([Circuit](@ref bosim::Circuit)) と状態 ([State](@ref bosim::State)) とともに [Simulate](@ref bosim::Simulate) に渡す。シミュレーションの結果は [Result](@ref bosim::Result) 型のオブジェクトとして返される。

最後に、結果オブジェクトを用いて、テレポートされたモード 2 がモード 0 の初期状態と一致するかを確認する。
