\page server_readme サーバー

\tableofcontents{html}

`server/` では、 Bosonic simulator の gRPC サーバ `bosim_server` を実装する。

## How to use

`bosim_server` の使用方法は `--help` オプションで確認できる。

```sh
$ bosim_server --help
bosim_server options:
  -h [ --help ]                Display available options
  --settings arg               Path to the settings json file
  --error_file arg             Path to the error defining json
  --log_level arg              Verbosity level (0: QUIET, 1: WARN, 2: INFO, 3: 
                               DEBUG)
  --log_color                  Colorize output
  --log_file arg               Path to log file
  --log_max_bytes arg          Maximum size of log file before rotation (0 
                               means unlimited)
  --log_backup_count arg       Number of backup log files to keep
  --execution_address arg      Address to the execution service of the 
                               scheduler
  --execution_ca_cert_file arg Path to the credentials file to use the 
                               execution service
  --backup_root_path arg       Path to the root directory of backup files
```

### サーバの起動方法

`bosim_server` を実行すると gRPC サーバを起動する。
シミュレータ層のアドレスは `--execution_address` オプションもしくは設定 JSON ファイル で指定する。

#### 例 1: ローカルで実行中の scheduler_for_sim に接続する場合

```bash
bosim_server --execution_address localhost:8088
```

#### 例 2: AWS で実行中の scheduler_for_sim に接続する場合

`.envs/stag.json` には stag のエンドポイントが記載してある。
設定ファイルを渡すことで stag に接続できる。

```bash
bosim_server --settings server/.envs/stag.json
```
