#!/usr/bin/env bash

STACK_NAME=cloudcamdev            # stack name
S3_CODE_BUCKET=cloudcam-code      # s3 bucket to upload lambda code to, will be created if doesn't exist
S3_UI_BUCKET=beta.cloudcam.space  # ui bucket
JANUS_KMS_KEY_USER=cloudcam-ops   # user which is granted permission to encrypt Janus SSL key via encrypt-ssl-key.sh

aws s3 mb s3://${S3_CODE_BUCKET}
aws cloudformation package --template-file cloudcam.yml --s3-bucket ${S3_CODE_BUCKET} --output-template-file cloudcam-packaged.yml
aws cloudformation deploy --template-file cloudcam-packaged.yml \
    --stack-name ${STACK_NAME} --capabilities CAPABILITY_IAM CAPABILITY_NAMED_IAM \
    --parameter-overrides JanusKmsKeyUser=${JANUS_KMS_KEY_USER}
( cd ../dev-ui && NODE_ENV=production webpack )
aws s3 cp --recursive --acl public-read ../dev-ui/webroot s3://${S3_UI_BUCKET}
