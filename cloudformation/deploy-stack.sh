#!/usr/bin/env bash

MODE=${1:-update}              # create or update
S3_CODE_BUCKET=cloudcam-code   # s3 bucket to upload lambda code to, will be created if doesn't exist
STACK_NAME=cloudcam-test       # stack name
IOT_CA_CERT_ID=dc6763eaf7b543310159727dee5232f973292b174a8cfa251713fbb3294797fe   # IoT CA AWS cert id used for just-in-time thing cert registration

aws s3 mb s3://${S3_CODE_BUCKET}
aws cloudformation package --template-file cloudcam.json --s3-bucket ${S3_CODE_BUCKET} --use-json --output-template-file cloudcam-packaged.json
aws cloudformation ${MODE}-stack --template-body file://cloudcam-packaged.json --stack-name ${STACK_NAME} --capabilities=CAPABILITY_IAM --parameters ParameterKey=iotCaCertId,ParameterValue=${IOT_CA_CERT_ID}
