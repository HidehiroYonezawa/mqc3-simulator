\page algo_readme アルゴリズム

\tableofcontents{html}

本シミュレータでは連続量量子計算を行う。状態は一般に多モード状態として扱われ、各モードに対してガウスユニタリ操作またはホモダイン測定を行い、状態を更新してゆく。状態の表現と操作（測定含む）のアルゴリズムは [Phys. Rev. X Quantum 2, 040315 (2021)](https://link.aps.org/doi/10.1103/PRXQuantum.2.040315) に基づいている。

@anchor state_rep
## 状態の表現

シミュレーションにおいては、回路の全\f$N\f$モード分の状態を\f$N\f$モード状態として扱う。状態は「ピーク」の和で表される。ピークとはガウス関数の平均と重みを複素数に拡張した関数で表されるウィグナー関数のことであり、以下の3種類の要素の組で表現される。

-   係数\f$c\f$

    複素数。

-   平均ベクトル\f$\boldsymbol{\mu}\f$

    \f$2N\f$次元複素ベクトル。

    \f[
    \boldsymbol{\mu}=
    \begin{pmatrix}
    x_0 \\
    x_1 \\
    \vdots \\
    x_{N-1} \\
    p_0 \\
    p_1 \\
    \vdots \\
    p_{N-1}
    \end{pmatrix}
    \f]

    の形で要素を保持する。

-   共分散行列\f$\boldsymbol{\Sigma}\f$

    \f$2N\f$次元半正定値実行列。要素の下付き添え字をモードインデックスとして

    \f[
    \boldsymbol{\Sigma}=
    \begin{pmatrix}
    \sigma_{x_0}^2       & \sigma_{x_0 x_1}     & \cdots & \sigma_{x_0 x_{N-1}} & \sigma_{x_0 p_0}         & \sigma_{x_0 p_1}     & \cdots & \sigma_{x_0 p_{N-1}}     \\
    \sigma_{x_0 x_1}     & \sigma_{x_1}^2       & \cdots & \sigma_{x_1 x_{N-1}} & \sigma_{x_1 p_0}         & \sigma_{x_1 p_1}     & \cdots & \sigma_{x_1 p_{N-1}}     \\
    \vdots               & \vdots               & \ddots &                      &                          &                      &        & \vdots                   \\
    \sigma_{x_0 x_{N-1}} & \sigma_{x_1 x_{N-1}} &        & \sigma_{x_{N-1}}^2   & \sigma_{x_{N-1} p_0}     & \sigma_{x_{N-1} p_1} &        & \sigma_{x_{N-1} p_{N-1}} \\
    \sigma_{x_0 p_0}     & \sigma_{x_1 p_0}     &        & \sigma_{x_{N-1} p_0} & \sigma_{p_0}^2           & \sigma_{p_0 p_1}     &        & \sigma_{p_0 p_{N-1}}     \\
    \sigma_{x_0 p_1}     & \sigma_{x_1 p_1}     &        & \sigma_{x_{N-1} p_1} & \sigma_{p_0 p_1}         & \sigma_{p_1}^2       &        & \sigma_{p_1 p_{N-1}}     \\
    \vdots               & \vdots               &        &                      &                          &                      & \ddots & \vdots                   \\
    \sigma_{x_0 p_{N-1}} & \sigma_{x_1 p_{N-1}} & \cdots & \sigma_{x_{N-1} p_{N-1}} & \sigma_{p_0 p_{N-1}} & \sigma_{p_1 p_{N-1}} & \cdots & \sigma_{p_{N-1}}^2
    \end{pmatrix}
    \f]

    の形で要素を保持する。

状態は各ピークインデックス\f$m\f$に対するこの組\f$\left\{ c_m, \boldsymbol{\mu}_m, \boldsymbol{\Sigma}_m \right\}_{m \in \mathcal{M}}\f$の列として表される（\f$\mathcal{M}\f$はピークインデックスの集合）。


## 操作のアルゴリズム

### ガウスユニタリ操作

ガウスユニタリ操作の行列を\f$ \mathbf{S} \f$とおくと、ピーク\f$m\f$の平均ベクトル\f$ \boldsymbol{\mu}_m \f$と共分散行列\f$ \boldsymbol{\Sigma}_m \f$はそれぞれ

\f[
\begin{align}
\boldsymbol{\mu}_m     &\mapsto \mathbf{S} \boldsymbol{\mu}_m \\
\boldsymbol{\Sigma}_m  &\mapsto \mathbf{S} \boldsymbol{\Sigma}_m \mathbf{S}^\text{T}
\end{align}
\f]

と変換される。

ここで全モードを操作対象モード\f$B\f$とそれ以外のモード\f$A\f$に分割し、末尾に\f$B\f$が集まるように平均ベクトルに要素の置換を、共分散行列に行と列それぞれの置換を施す。このときピーク\f$m\f$の平均ベクトルと共分散行列は

\f[
\begin{align}
\boldsymbol{\mu}_m&=\binom{\boldsymbol{\mu}_{m, A}}{\boldsymbol{\mu}_{m, B}} \\
\boldsymbol{\Sigma}_m&=\left(\begin{array}{cc} \boldsymbol{\Sigma}_{m, A} & \boldsymbol{\Sigma}_{m, AB} \\ \boldsymbol{\Sigma}_{m, AB}^\text{T} & \boldsymbol{\Sigma}_{m, B} \end{array}\right)
\end{align}
\f]

と分割できる。また、操作行列は

\f[
\mathbf{S}=\left(\begin{array}{cc} \mathbf{1} & \mathbf{0} \\ \mathbf{0} & \mathbf{S}_B \end{array}\right)
\f]

と分割できる。操作対象モード数を\f$n\f$とすると、\f$\boldsymbol{\Sigma}_{m, B}\f$と\f$\mathbf{S}_B\f$は\f$2n\f$次元ユニタリ行列である。操作オブジェクトが保持する操作行列は\f$\mathbf{S}_B\f$であって\f$ \mathbf{S} \f$ではない。上記の分割を用いると、操作による平均ベクトルと共分散行列の変換は

\f[
\begin{align}
\boldsymbol{\mu}_m     &\mapsto \mathbf{S} \boldsymbol{\mu}_m = \binom{\boldsymbol{\mu}_{m, A}}{\mathbf{S}_B \boldsymbol{\mu}_{m, B}}\\
\boldsymbol{\Sigma}_m  &\mapsto \mathbf{S} \boldsymbol{\Sigma}_m \mathbf{S}^\text{T} = \left(\begin{array}{cc} \boldsymbol{\Sigma}_{m, A} & \boldsymbol{\Sigma}_{m, AB} \mathbf{S}_B^\text{T} \\ \mathbf{S}_B \boldsymbol{\Sigma}_{m, AB}^\text{T} & \mathbf{S}_B \boldsymbol{\Sigma}_{m, B} \mathbf{S}_B^\text{T} \end{array}\right)
\end{align}
\f]

と計算できる。この変換は全モード数\f$N\f$に対して\f$O(N)\f$の計算量で実行できる。実装としては平均ベクトルの要素と共分散行列の行と列の置換は行わず、直接要素にアクセスして上記の高速な変換を行っている。

### 測定

1モードのホモダイン測定が利用可能である。測定角\f$\theta\f$がパラメータであり、測定対象モード\f$i\f$に対して
\f$x_i \sin \theta+p_i \cos \theta\f$をサンプリングする。実装としては\f$x\f$方向から\f$p\f$方向に向かって\f$\phi=\theta-\pi/2\f$だけ位相回転を行ってから\f$x\f$基底ホモダイン測定を行う。\f$x\f$基底ホモダイン測定は\f$x\f$のサンプリングとその結果に基づいた状態の更新から成る。下図に測定アルゴリズムの概要を示す。

![Peak-level multi-thread execution](docs/images/measurement_algorithm.svg)

以下では位相回転後の処理のアルゴリズムを詳しく述べる。

#### サンプリング

サンプリング対象の確率密度分布は一般に複数のピークからなり、各ピークの重みと平均ベクトルの要素は複素数であるため、直接サンプリングを行うことは難しい。そこでピークの重みと平均ベクトルの要素が全て実数であるような提案分布を作って棄却サンプリングを行う。

##### サンプリング対象の確率密度分布

測定対象モードを\f$B\f$とし、それ以外の系を\f$A\f$とする。全体の系を\f$A, B\f$に分割し、それに従って平均を

\f[
\left\{ \boldsymbol{\mu}_{m,A} \right\}_m, \left\{ \boldsymbol{\mu}_{m,B} \right\}_m
\f]

共分散行列を

\f[
\left\{ \boldsymbol{\Sigma}_{m,A} \right\}_m, \left\{ \boldsymbol{\Sigma}_{m,AB} \right\}_m, \left\{ \boldsymbol{\Sigma}_{m,B} \right\}_m
\f]

と分割する。測定対象モードのみ抽出した状態は

\f[
\left\{ c_m, \boldsymbol{\mu}_{m,B}, \boldsymbol{\Sigma}_{m,B} \right\}_{m \in \mathcal{M}}=
\left\{ c_m,
\begin{pmatrix}
\mu_{m,x} \\
\mu_{m,p} \\
\end{pmatrix},
\begin{pmatrix}
\sigma_{m,x}^2 & \sigma_{m,xp} \\
\sigma_{m,xp} & \sigma_{m,p}^2 \\
\end{pmatrix}
\right\}_{m \in \mathcal{M}}
\f]

である。

一般の1次元ガウス分布を

\f[
G_{\mu, \sigma^2}(x) \equiv \frac{1}{\sigma \sqrt{2 \pi}} e^{-\frac{1}{2}\left(\frac{x-\mu}{\sigma}\right)^2}
\f]

とすると、測定結果\f$x_M\f$を得る確率密度関数は

\f[
p\left(x_M ; \hat{\rho}\right)
=\sum_{m \in \mathcal{M}} c_m G_{\mu_{m,x}, \sigma_{m,x}^2}\left(x_M\right)
\f]

となる。これを規格化したものがサンプリング対象の確率密度分布である。

##### 提案分布

ここで棄却サンプリングの提案分布を作るためにピークインデックス集合\f$\mathcal{M}\f$の部分集合をいくつか定義する。

-   \f$\mathcal{M}_{c<0} = \left\{m\in\mathcal{M} \mid c_m<0\right\}\f$
-   \f$\mathcal{M}_{\boldsymbol{\mu}\in\mathbb{R}^{2}} = \left\{m\in\mathcal{M} \mid \operatorname{Im}\left(\boldsymbol{\mu}_m\right)=0 \right\}\f$
-   \f$\mathcal{M}^{-} = \mathcal{M}_{c<0} \cap \mathcal{M}_{\mu\in\mathbb{R}^{2}}\f$

確率密度関数を\f$\mathcal{M}^{-}\f$と\f$\mathcal{M}^{+}=\mathcal{M}\setminus\mathcal{M}^{-}\f$で分割する。

\f[
p\left(x_M ; \hat{\rho}\right)=
\sum_{m \in \mathcal{M}^{-}} c_m G_{\mu_{m,x}, \sigma_{m,x}^2}\left(x_M\right)
+\sum_{m \in \mathcal{M}^{+}} c_m G_{\mu_{m,x}, \sigma_{m,x}^2}\left(x_M\right)
\f]

ここで\f$\mathcal{M}^{+}\f$の各項

\f[
c_{m} G_{\mu_{m,x}, \sigma_{m,x}^2}\left(x_M\right)
=
c_{m} e^{\left(\operatorname{Im}\left(\mu_{m,x}\right)\right)^2 / 2\sigma_{m,x}^2}
G_{\operatorname{Re}\left(\mu_{m,x}\right), \sigma_{m,x}^2}\left(x_M\right)
e^{i\left[x_M-\operatorname{Re}\left(\mu_{m,x}\right)\right] \operatorname{Im}\left(\mu_{m,x}\right) / \sigma_{m,x}^2}
\f]

の実部のノルムをとって足し直したもの

\f[
g\left(x_M\right)=\sum_{m \in \mathcal{M}^{+}} \tilde{c}_m G_{\operatorname{Re}\left(\mu_{m,x}\right), \sigma_{m,x}^2}\left(x_M\right)
\f]

\f[
\left(
  \tilde{c}_m=\left|c_m\right| e^{\left(\operatorname{Im}\left(\mu_{m,x}\right)\right)^2 / 2\sigma_{m,x}^2}
\right)
\f]

は必ず元の確率密度関数の値以上となるため提案分布として使う（\f$p\left(x_M ; \hat{\rho}\right) \leq g\left(x_M\right)\f$）。

##### 棄却サンプリング

サンプルが採用されるまで以下を繰り返す。

まず\f$\mathcal{M}^{+}\f$からピーク
\f[
m_0: \tilde{c}_{m_0} G_{\operatorname{Re}\left(\mu_{m_0,x}\right), \sigma_{m_0,x}^2}\left(x_M\right)
\f]
を重み\f$\tilde{c}_m\f$に従ってランダムに選択する（ピークサンプリング）。

次にピーク\f$m_0\f$からサンプリングして測定値\f$x_0\f$を得る。

最後に\f$\left[0, g\left(x_0\right) \right]\f$の範囲から一様分布に従いサンプリングし、\f$p\left(x_0 ; \hat{\rho}\right)\f$以下の値であれば測定値\f$x_0\f$を採用する。

#### 状態の更新

モード\f$B\f$を測定して得られた値\f$x_0\f$を用いて、全てのピーク\f$m\f$について次のような変更を適用して状態\f$\left\{ c_m, \boldsymbol{\mu}_m, \boldsymbol{\Sigma}_m \right\}_{m \in \mathcal{M}}\f$を更新する。

-   \f$c_m \rightarrow \frac{c_m G_{\mu_{m,x}, \sigma_{m,x}^2}\left(x_0\right)}{\sum_{m \in \mathcal{M}} c_m G_{\mu_{m,x}, \sigma_{m,x}^2}\left(x_0\right)}\f$
-   \f$\boldsymbol{\mu}_{m, A} \rightarrow \boldsymbol{\mu}_{m, A}+\boldsymbol{\Sigma}_{m, A B} \begin{pmatrix}
\frac{x_0-\mu_{m,x}}{\sigma_{m,x}^2} \\
0 \\
\end{pmatrix}
\f$
-   \f$\boldsymbol{\mu}_{m, B} \rightarrow \boldsymbol{0}\f$
-   \f$\boldsymbol{\Sigma}_{m, A} \rightarrow \boldsymbol{\Sigma}_{m, A}-\boldsymbol{\Sigma}_{m, A B}\begin{pmatrix}
\frac{1}{\sigma_{m,x}^2} & 0 \\
0 & 0 \\
\end{pmatrix} \boldsymbol{\Sigma}_{m, A B}^\text{T}\f$
-   \f$\boldsymbol{\Sigma}_{m,AB} \rightarrow \boldsymbol{0}\f$
-   \f$\boldsymbol{\Sigma}_{m,B} \rightarrow \frac{\hbar}{2}\boldsymbol{1}\f$

状態の更新は全モード数\f$N\f$に対して\f$O(N^2)\f$の計算量で実行される。

### フィードフォワード

フィードフォワード (FF) は、一般に複数の測定操作から得られた測定値の組を入力とした関数（FF関数）の値を、それらの測定操作の後の特定の操作のパラメータとして設定することである。どの測定操作の測定値をどの操作のどのパラメータにどのようなFF関数を通して適用するかは回路実行前に予め設定される。フィードフォワードは測定操作に付随し、以下の手順で行われる。

1.  注目する測定操作の測定値を最後の入力とするFF関数の列を取得する
    -   回路の中で注目する測定操作よりも後には、測定値がFF関数の入力となるような測定操作が無い
2.  取得したFF関数を順番に実行する

## バックエンド

### CPUシングルスレッド実行

行われる処理はショット毎に独立であり、下図のように各ショットにおいて全モード状態が一般に複数個のユニタリ操作 \f$(U)\f$ と測定 \f$(M)\f$ （フィードフォワードも含む）を受ける。測定において用いるランダム関数のシードがショット間の違いである。シングルスレッド実行では小さいシードから順番にショットを実行していく。

![Shot-level multi-thread execution](docs/images/shot_level.svg)

### CPUショットレベルマルチスレッド実行

ショットレベルマルチスレッド実行では上図の各ショットを並列に実行する。

### CPUピークレベルマルチスレッド実行

ピークレベルマルチスレッド実行ではショットレベルの並列化に加えてピークレベルの並列化も行う。ユニタリ操作はピーク毎に独立に実行できるため、下図に示すように各ショット内で並列に実行される。また、測定は位相回転 \f$(R)\f$ 、サンプリング、フィードフォワード、状態の更新に分解され、そのうち位相回転と状態の更新はユニタリ操作と同様にピークレベルで並列に実行される。測定操作の前後（位相回転の前と状態の更新の後）では同期を行わない。一方、サンプリングにおいては全ピークを用いた処理を行うため同期を行う。フィードフォワードは状態の更新とは独立に実行可能であるが、処理が速いためサンプリングの直後に同期したまま行う。

![Peak-level multi-thread execution](docs/images/peak_level.svg)

### GPU実行

状態をGPU上で保持し、操作オブジェクトや回路オブジェクト等その他のオブジェクトはCPU上で管理する。ショット・状態の更新（ガウスユニタリ操作や測定による）毎に、ピークを表す共分散行列等を前述の通りにブロック行列に分けて操作を複数ステップに分割し、逐次的に実行していく。各ステップにおいて各ピークに対応する行列は1次元ベクトルへと変換され、これらをさらにピークについて結合したものがGPU上で扱われる。例えば更新される部分行列が8次元正方行列である場合、下図に示すようにして変形したデータがGPUで扱われる。このとき、このデータに対して行う操作を表す操作行列がCPUからGPUへ送信される。

![Structure of state matrices in GPU](docs/images/cov_matrix_gpu.svg)

平均ベクトルは元々1次元であるためGPUにおける計算のために非自明な変形を受けることはないが、行列の場合と同様にピークについて結合される。

測定におけるサンプリングとフィードフォワードはCPUで行う。実装されている測定操作は1次元ホモダイン測定であるため、モード\f$i\f$の測定におけるサンプリングを行うためには各ピークの係数、平均ベクトルの第\f$i\f$成分、共分散行列の\f$(i,i)\f$成分をGPUからCPUへ送り、CPU上のランダム関数を用いて1次元ガウス関数からのサンプリングを繰り返す。

## グラフ表現シミュレータのアルゴリズム

グラフ表現シミュレータでは、与えられたグラフ表現を前の数セクションで述べたアルゴリズムで実行できるように、シミュレータが直接扱える回路表現に変換して実行する。

### 状態の扱い方

ステップあたりのマクロノード数を\f$N\f$とすると、グラフ表現シミュレータは初期状態に使う分のモード数、すなわちマクロノード0の入力モード2つとマクロノード1~\f$N-1\f$の入力モード1つずつの計\f$N+1\f$モードのみを使いまわす。全てのモードの初期状態はスクイージングレベル\f$s\f$ dBのxスクイーズド状態である。これがパラメータ\f$\theta\f$の`Initialization`操作を受けると、スクイージングレベル\f$(s-10\log_{10}2)\f$ dB、スクイージング角\f$\phi=\theta＋\pi/2\f$のスクイーズド状態として初期化される。

### マクロノードあたりの操作

マクロノード操作の種類によって、マクロノードあたりに行われる操作が異なる。マクロノード\f$k\f$において\f$k-1\f$からの入力モードを\f$\psi_{k}^\text{up}\f$、\f$k-N\f$からの入力モードを\f$\psi_{k}^\text{left}\f$と書くと、操作の列は以下のようになる。

#### Wiring

| マクロノード\f$k\f$における操作インデックス | 操作           | 対象モード  |
|---------------------|--------------|--------|
| 0                   | Displacement | \f$\psi_{k}^\text{up}\f$   |
| 1                   | Displacement | \f$\psi_{k}^\text{left}\f$ |

#### 測定・初期化以外の1モード操作

| マクロノード\f$k\f$における操作インデックス | 操作           | 対象モード  |
|---------------------|--------------|--------|
| 0                   | Displacement | \f$\psi_{k}^\text{up}\f$   |
| 1                   | 所与の1モード操作 | \f$\psi_{k}^\text{up}\f$ |
| 2                   | Displacement | \f$\psi_{k}^\text{left}\f$ |
| 3                   | 所与の1モード操作 | \f$\psi_{k}^\text{left}\f$ |

#### 2モード操作

| マクロノード\f$k\f$における操作インデックス | 操作           | 対象モード  |
|---------------------|--------------|--------|
| 0                   | Displacement | \f$\psi_{k}^\text{up}\f$   |
| 1                   | Displacement | \f$\psi_{k}^\text{left}\f$ |
| 2                   | 所与の2モード操作 | \f$\psi_{k}^\text{up}\f$, \f$\psi_{k}^\text{left}\f$ |

#### 測定・初期化

| マクロノード\f$k\f$における操作インデックス | 操作                     | 対象モード  | 意味                               |
|---------------------|------------------------|--------|----------------------------------|
| 0                   | Displacement           | \f$\psi_{k}^\text{up}\f$   | 変位操作                             |
| 1                   | HomodyneSingleMode     | \f$\psi_{k}^\text{up}\f$   | ホモダイン測定                          |
| 2                   | ReplaceBySqueezedState | \f$\psi_{k}^\text{up}\f$   | \f$\psi_{k}^\text{up}\f$を捨てて\f$\psi_{k+1}^\text{up}\f$をスクイーズド状態として作る     |
| 3                   | Displacement           | \f$\psi_{k}^\text{left}\f$ | 変位操作                             |
| 4                   | HomodyneSingleMode     | \f$\psi_{k}^\text{left}\f$ | ホモダイン測定                          |
| 5                   | ReplaceBySqueezedState | \f$\psi_{k}^\text{left}\f$ | \f$\psi_{k}^\text{left}\f$を捨てて\f$\psi_{k+N}^\text{left}\f$をスクイーズド状態として作る |

## 実機表現シミュレータのアルゴリズム

実機表現シミュレータでは、与えられた実機表現を前の数セクションで述べたアルゴリズムで実行できるように、シミュレータが直接扱える回路表現に変換して実行する。

### 状態の扱い方

例として、以下にステップあたりのマクロノード数\f$N=4\f$, ステップ数\f$S=3\f$のグラフの実機表現におけるマクロノード毎の操作に関与するミクロノードを示す。楕円がマクロノードを示し、その中の4つの円が左から順にミクロノード\f$b,d,c,a\f$を示す。1行毎に1つのマクロノードにおける操作を表し、各行で当該マクロノードにおける操作に関わるミクロノードを赤色でハイライトしている。また、ハイライトしたミクロノードの数が行の左端に書かれている。

![Micronodes involved in each macronode operation](docs/images/micronodes_in_macronode_op.svg)

図が示すように、各マクロノードにおける操作で最大\f$5+N\f$モードが必要となる。

-   現在のマクロノード\f$k\f$のミクロノード：4個
-   マクロノード\f$k+1\f$のミクロノード\f$b\f$：1個
-   マクロノード\f$k+1\f$~\f$k+N\f$のミクロノード\f$d\f$：\f$N\f$個

実機表現における状態オブジェクトで全ミクロノード分のモードを個別に用いない。グレーのミクロノードが示すように、各マクロノードにおいて操作が終わると不要になるミクロノードが発生するため、これを次のマクロノードにおける操作で新たに使われるモードとして使いまわす。そのため、状態オブジェクトの生成時に用意しておくモード数は必要最小限の\f$5+N\f$に固定する。各ミクロノードの下の赤い数字は、そのミクロノードが状態オブジェクトにおけるどのモードインデックスに対応するかを示す。不要になったモードの使いまわしは赤い破線の矢印が示す。

上図において各行\f$k\f$で赤いミクロノードを左から（0から）数えたときの\f$i\f$番目のミクロノードに対応する赤い数字（状態オブジェクトにおけるモードインデックス）は以下のようになる。

| マクロノード\f$k\f$の操作における\f$i\f$ | 赤い数字（状態オブジェクトにおけるモードインデックス） |
|-----|-----|
| 0, 1 | \f$k-1\f$における\f$i+4\f$番目の赤い数字（状態オブジェクトにおけるモードインデックス） |
| 2, 3, 4 | \f$k-1\f$における\f$i-2\f$番目の赤い数字（状態オブジェクトにおけるモードインデックス） |
| 5, …, \f$5+N-2\f$ | \f$k-1\f$における\f$i+1\f$番目の赤い数字（状態オブジェクトにおけるモードインデックス） |
| \f$5+N-1\f$ | \f$k-1\f$における3番目の赤い数字（状態オブジェクトにおけるモードインデックス） |

\f$k-1\f$から\f$k\f$へマクロノードインデックスが1つ増える際のこのインデックスの更新を置換\f$P(i)\f$として見ると、

-   (0 4 2)
-   (1 5 6 ... \f$N+3\f$ \f$N+4\f$ 3)

という2つのサイクルに分解できるため、\f$P^k (i)\f$は2つのベクトル

-   \f$v=(0, 4, 2)\f$
-   \f$w=(1, 5, 6, ..., N+3, N+4, 3)\f$

を用いて以下の値として計算できる。

| \f$i\f$   | \f$P^k (i)\f$             |
|-----|-------------------|
| 0   | \f$v_{k \ (\text{mod } 3)}\f$           |
| 1   | \f$w_{k \ (\text{mod } N+2)}\f$       |
| 2   | \f$v_{2+k \ (\text{mod } 3)}\f$       |
| 3   | \f$w_{k+N+1 \ (\text{mod } N+2)}\f$ |
| 4   | \f$v_{1+k \ (\text{mod } 3)}\f$       |
| 5   | \f$w_{k+1 \ (\text{mod } N+2)}\f$   |
| ...   | ...                 |
| \f$N+3\f$ | \f$w_{k+N-1 \ (\text{mod } N+2)}\f$ |
| \f$N+4\f$ | \f$w_{k+N \ (\text{mod } N+2)}\f$   |

ただしベクトルの添え字は0から始まる。

全てのマクロノードにおいて、与えられた実機表現シミュレーションのスクイージングレベル\f$s\f$に対してミクロノード\f$a, c\f$は\f$-s\f$ dBの、\f$b, d\f$は\f$s\f$ dBのスクイーズド状態を初期状態とする。（0から数えて）1番目以降のマクロノード操作のためのモードの再初期化（使いまわし）には操作`ReplaceBySqueezedState`を用いる。

### マクロノードあたりの操作

マクロノードあたり18個の操作を行う。

| マクロノード\f$k\f$における操作インデックス | 操作                     | 対象モード      | 意味                         |
|---------------------|------------------------|------------|----------------------------|
| 0                   | `Displacement`           | \f$b_k\f$         | 非線形FFを受け得る変位操作   |
| 1                   | `Displacement`           | \f$d_k\f$         | 非線形FFを受け得る変位操作   |
| 2                   | `BeamSplitter50`         | \f$b_k\f$, \f$d_k\f$     | 50:50ビームスプリッター             |
| 3                   | `BeamSplitter50`         | \f$a_k\f$, \f$b_{k+1}\f$   | 50:50ビームスプリッター             |
| 4                   | `BeamSplitter50`         | \f$c_k\f$, \f$d_{k+N}\f$   | 50:50ビームスプリッター             |
| 5                   | `BeamSplitter50`         | \f$b_k\f$, \f$a_k\f$     | 50:50ビームスプリッター             |
| 6                   | `BeamSplitter50`         | \f$d_k\f$, \f$c_k\f$     | 50:50ビームスプリッター             |
| 7                   | `HomodyneSingleMode`     | \f$a_k\f$         | ホモダイン測定                    |
| 8                   | `HomodyneSingleMode`     | \f$b_k\f$         | ホモダイン測定                    |
| 9                   | `HomodyneSingleMode`     | \f$c_k\f$         | ホモダイン測定                    |
| 10                  | `HomodyneSingleMode`     | \f$d_k\f$         | ホモダイン測定                    |
| 11                  | `BeamSplitter50`         | \f$d_{k+N}\f$, \f$b_{k+1}\f$ | 50:50ビームスプリッター             |
| 12                  | `Displacement`           | \f$b_{k+1}\f$       | 線形FF                       |
| 13                  | `Displacement`           | \f$d_{k+N}\f$       | 線形FF                       |
| 14                  | `ReplaceBySqueezedState` | \f$b_k\f$         | \f$b_k\f$を捨てて\f$c_{k+1}\f$をスクイーズド状態として作る   |
| 15                  | `ReplaceBySqueezedState` | \f$d_k\f$         | \f$d_k\f$を捨てて\f$a_{k+1}\f$をスクイーズド状態として作る   |
| 16                  | `ReplaceBySqueezedState` | \f$c_k\f$         | \f$c_k\f$を捨てて\f$b_{k+2}\f$をスクイーズド状態として作る   |
| 17                  | `ReplaceBySqueezedState` | \f$a_k\f$         | \f$a_k\f$を捨てて\f$d_{k+N+1}\f$をスクイーズド状態として作る |
