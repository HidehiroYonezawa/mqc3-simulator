#!/bin/bash

# 必須の引数をチェック
if [ $# -ne 1 ]; then
  echo "Usage: $0 <ACTION>"
  exit 1
fi

# デプロイ環境名を設定
export DEPLOY_ENV="prod"
export IMAGE_TAG=$(TZ=Asia/Tokyo date +%Y%m%d)

# アクション名を引数から取得 (up/down)
ACTION=$1
export ACTION=${ACTION,,}

# アクション名の値をチェック
case ${ACTION} in
  up|down)
    echo "Starting action: ${ACTION} for environment: ${DEPLOY_ENV}"
    ;;
  *)
    echo "Invalid ACTION: ${ACTION}. Please use 'up' or 'down'."
    exit 1
    ;;
esac

# プロジェクトのルートに移動する
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
pushd "${SCRIPT_DIR}/../.."

# SSMからパラメータを取得する関数
get_ssm_parameter() {
  local parameter_name="$1"
  local variable_name="$2"

  # SSMエージェントの認証情報を利用するため sudo が必要
  export "$variable_name"=$(sudo aws ssm get-parameter \
      --name "$parameter_name" \
      --query Parameter.Value --output text)

  # エラーハンドリング: get-parameter が失敗した場合
  if [ $? -ne 0 ]; then
    echo "Error: Failed to get parameter '$parameter_name'. Check permissions and parameter existence."
    return 1
  fi
  return 0
}

# S3バケット名を取得
PARAMETER_NAME="/${DEPLOY_ENV}/Physsimulator/S3LogBucketName"
if get_ssm_parameter "$PARAMETER_NAME" "S3_BUCKET_NAME"; then
  echo "S3_BUCKET_NAME: $S3_BUCKET_NAME"
else
  echo "Failed to get S3 bucket name."
  exit 1
fi

# CloudWatch Logsのロググループ名を取得
PARAMETER_NAME="/${DEPLOY_ENV}/Physsimulator/CWLLogGroupName"
if get_ssm_parameter "$PARAMETER_NAME" "LOG_GROUP_NAME"; then
  echo "LOG_GROUP_NAME: $LOG_GROUP_NAME"
else
  echo "Failed to get CloudWatch Logs group name."
  exit 1
fi

export GIT_COMMIT_HASH=$(git rev-parse --short HEAD)

PROJECT_NAME=bosim-${DEPLOY_ENV}
COMPOSE_FILE="server/compose.${DEPLOY_ENV}.yaml"

if [ "$ACTION" == "down" ]; then
  echo "Stopping simulator server..."
  docker compose -f ${COMPOSE_FILE} -p ${PROJECT_NAME} down
  exit 0
elif [ "$ACTION" == "up" ]; then
  echo "Starting simulator server..."
  docker compose -f ${COMPOSE_FILE} -p ${PROJECT_NAME} build
  docker compose -f ${COMPOSE_FILE} -p ${PROJECT_NAME} up -d
fi

popd
