#!/usr/bin/env bash

STACK_NAME=cloudcamdev            # stack name
S3_CODE_BUCKET=cloudcam-code      # s3 bucket to upload lambda code to, will be created if doesn't exist
S3_UI_BUCKET=beta.cloudcam.space  # ui bucket
CLOUDFRONT_UI_DISTRIBUTION_ID=E3RT8HOQAAG0IG   # ui cloudfront distribution id
JANUS_KMS_KEY_USER=cloudcam-ops   # user which is granted permission to encrypt Janus SSL key via encrypt-ssl-key.sh
JANUS_HEALTH_CHECK_ALARMS_TOPIC=JanusHealthCheckAlarms   # topic for janus gateway health check alarms

AWS_REGION=`aws configure get region`
AWS_ACCOUNT_ID=`aws sts get-caller-identity --output text --query 'Account'`

# note: Route53 health check notifications require that the Cloudwatch alarm and SNS topic reside in the us-east-1 region
#       since multi-region cloudformation templates are complicated, we create them using the aws cli here
JANUS_HEALTH_CHECK_ALARMS_TOPIC_ARN=`aws sns create-topic --region us-east-1 --name ${JANUS_HEALTH_CHECK_ALARMS_TOPIC} --output text`
JANUS_HEALTH_CHECK_ALARMS_ENDPOINT_ARN=arn:aws:lambda:${AWS_REGION}:${AWS_ACCOUNT_ID}:function:janus_scale_lightsail

# ensure S3_CODE_BUCKET exists
aws s3 mb s3://${S3_CODE_BUCKET}

# package and deploy the stack
aws cloudformation package --template-file cloudcam.yml --s3-bucket ${S3_CODE_BUCKET} --output-template-file cloudcam-packaged.yml
aws cloudformation deploy --template-file cloudcam-packaged.yml \
    --stack-name ${STACK_NAME} --capabilities CAPABILITY_IAM CAPABILITY_NAMED_IAM \
    --parameter-overrides JanusKmsKeyUser=${JANUS_KMS_KEY_USER}

# add an SNS subscription for Janus gateway Route53 health checks (see above on why is this not a part of the Cloudformation stack)
aws sns subscribe --region us-east-1 --topic-arn ${JANUS_HEALTH_CHECK_ALARMS_TOPIC_ARN} --protocol lambda \
    --notification-endpoint ${JANUS_HEALTH_CHECK_ALARMS_ENDPOINT_ARN} --output text

# upload UI assets to S3_UI_BUCKET
( cd ../dev-ui && NODE_ENV=production webpack )
aws s3 cp --recursive --acl public-read ../dev-ui/webroot s3://${S3_UI_BUCKET}
aws cloudfront create-invalidation --distribution-id ${CLOUDFRONT_UI_DISTRIBUTION_ID} --paths '/*'
