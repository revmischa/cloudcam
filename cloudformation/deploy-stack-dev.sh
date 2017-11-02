#!/usr/bin/env bash

STACK_NAME=cloudcamdev            # stack name
S3_CODE_BUCKET=cloudcam-code      # s3 bucket to upload lambda code to, will be created if doesn't exist
S3_UI_BUCKET=beta.cloudcam.space  # ui bucket
CLOUDFRONT_UI_DISTRIBUTION_ID=E3RT8HOQAAG0IG   # ui cloudfront distribution id
JANUS_HOSTED_ZONE_ID=Z1MUZ3ZP15MONF            # aws hosted zone id where janus instances will receive dns names
JANUS_HOSTED_ZONE_DOMAIN=cloudcam.space        # aws hosted zone domain where janus instances will receive dns names
JANUS_HEALTH_CHECK_ALARMS_TOPIC=JanusHealthCheckAlarms   # topic for janus gateway health check alarms

source deploy-stack.sh