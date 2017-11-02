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
  JANUS_HOSTED_ZONE_ID=Z1MUZ3ZP15MONF            # aws hosted zone id where janus instances will receive dns names
  JANUS_HOSTED_ZONE_DOMAIN=cloudcam.space        # aws hosted zone domain where janus instances will receive dns names
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

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $DIR/deploy-stack.sh
