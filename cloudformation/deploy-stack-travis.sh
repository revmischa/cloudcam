#!/bin/bash

# This deploys the cloudformation stack from Travis-CI

# expect travis environment
if [[ -z "$TRAVIS_BRANCH" ]]; then
  echo "ERROR: no travis environment detected"
  exit 1
fi

configure_dev() {
  STACK_NAME=cloudcam-dev               # stack name
  S3_CODE_BUCKET=cloudcam-cf-dev        # s3 bucket to upload lambda code to, will be created if doesn't exist
  S3_UI_BUCKET=ccdev.mvstg.biz          # ui bucket
  JANUS_KMS_KEY_USER=cloudcam-ops-dev   # user which is granted permission to encrypt Janus SSL key via encrypt-ssl-key.sh
  JANUS_HEALTH_CHECK_ALARMS_TOPIC=JanusHealthCheckAlarms-Dev   # topic for janus gateway health check alarms
  # CLOUDFRONT_UI_DISTRIBUTION_ID=EXXXXXXX   # ui cloudfront distribution id
}

# check where we're deploying to
if [[ "$TRAVIS_BRANCH" -eq "master" ]]; then
  echo "* on master branch"
  echo "* deploying dev CloudFormation stack..."
  configure_dev
else
  echo "ERROR: unknown branch for deployment: $TRAVIS_BRANCH"
  exit 1
fi

./deploy-stack.sh
