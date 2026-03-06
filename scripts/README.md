# デプロイ用スクリプト

## prod/start_simulator_prod.sh

本番環境向けにシミュレータのコンテナを起動するためのスクリプトです。

サブコマンドには `up`, `down` を指定できます。
`up` はコンテナを起動、`down` はコンテナを停止するコマンドです。

```sh
$ ./scripts/prod/start_simulator_prod.sh up
```

前提として、AWS CLIのインストールとaws_access_key_id などのAWSの認証情報を事前に設定しておく必要があります。
また、本スクリプトの動作には `riken_dev` という名前のプロファイルを事前に作成しておく必要があります。
