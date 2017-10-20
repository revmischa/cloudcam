#!/usr/bin/env bash

STACK_NAME=cloudcamdev            # stack name
S3_CODE_BUCKET=cloudcam-code      # s3 bucket to upload lambda code to, will be created if doesn't exist
S3_UI_BUCKET=beta.cloudcam.space  # ui bucket
CLOUDFRONT_UI_DISTRIBUTION_ID=E3RT8HOQAAG0IG   # ui cloudfront distribution id
JANUS_KMS_KEY_USER=cloudcam-ops   # user which is granted permission to encrypt Janus SSL key via encrypt-ssl-key.sh
JANUS_HEALTH_CHECK_ALARMS_TOPIC=JanusHealthCheckAlarms   # topic for janus gateway health check alarms

source deploy-stack.sh