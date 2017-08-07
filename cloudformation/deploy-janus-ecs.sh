#!/usr/bin/env bash

STACK_NAME=cloudcamdev-janus-ecs-1       # stack name
MODE=${1:-update}                        # create or update

aws cloudformation ${MODE}-stack --template-body file://janus-ecs.yml --stack-name ${STACK_NAME} --capabilities CAPABILITY_IAM CAPABILITY_NAMED_IAM
