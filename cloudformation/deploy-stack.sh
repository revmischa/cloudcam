#!/usr/bin/env bash

# SET CONFIG VARS FIRST before calling this script
# (see deploy-stack-dev.sh)

# try to find aws region and account id
if [[ -n "$AWS_DEFAULT_REGION" ]]; then
  AWS_REGION="${AWS_REGION:-$AWS_DEFAULT_REGION}"
fi
if [[ -z "$AWS_REGION" ]]; then
  AWS_REGION=`aws configure get region`
fi
if [[ -z "$AWS_ACCOUNT_ID" ]]; then
  AWS_ACCOUNT_ID=`aws sts get-caller-identity --output text --query 'Account'`
fi

# sanity-check for presence of required configuration variables
MISSING=
for config in STACK_NAME S3_CODE_BUCKET AWS_REGION JANUS_KMS_KEY_USER JANUS_HEALTH_CHECK_ALARMS_TOPIC AWS_ACCOUNT_ID; do
  if [[ -z "${!config}" ]]; then
    echo "ERROR: missing configuration variable: $config"
    MISSING=1
  fi
done
if [[ $MISSING ]]; then
  exit 1
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# note: Route53 health check notifications require that the Cloudwatch alarm and SNS topic reside in the us-east-1 region
#       since multi-region cloudformation templates are complicated, we create them using the aws cli here
JANUS_HEALTH_CHECK_ALARMS_TOPIC_ARN=`aws sns create-topic --region us-east-1 --name ${JANUS_HEALTH_CHECK_ALARMS_TOPIC} --output text`
JANUS_HEALTH_CHECK_ALARMS_ENDPOINT_ARN=arn:aws:lambda:${AWS_REGION}:${AWS_ACCOUNT_ID}:function:janus_scale_lightsail
 
# ensure S3_CODE_BUCKET exists
aws s3 mb s3://${S3_CODE_BUCKET}

# package and deploy the stack
aws cloudformation package --template-file cloudcam.yml --s3-bucket ${S3_CODE_BUCKET} --output-template-file DIR/cloudcam-packaged.yml
aws cloudformation deploy --template-file $DIR/cloudcam-packaged.yml \
    --stack-name ${STACK_NAME} --capabilities CAPABILITY_IAM CAPABILITY_NAMED_IAM \
    --parameter-overrides JanusKmsKeyUser=${JANUS_KMS_KEY_USER}

# add an SNS subscription for Janus gateway Route53 health checks (see above on why is this not a part of the Cloudformation stack)
aws sns subscribe --region us-east-1 --topic-arn ${JANUS_HEALTH_CHECK_ALARMS_TOPIC_ARN} --protocol lambda \
    --notification-endpoint ${JANUS_HEALTH_CHECK_ALARMS_ENDPOINT_ARN} --output text

# upload UI
if [[ -n "$S3_UI_BUCKET" ]]; then
  # upload UI assets to S3_UI_BUCKET
  ( cd ../dev-ui && NODE_ENV=production webpack )
  aws s3 cp --recursive --acl public-read $DEV/../dev-ui/webroot s3://${S3_UI_BUCKET}
  # invalidate cloudfront
  if [[ -n "$CLOUDFRONT_UI_DISTRIBUTION_ID" ]]; then
    aws cloudfront create-invalidation --distribution-id ${CLOUDFRONT_UI_DISTRIBUTION_ID} --paths '/*'
  fi
fi