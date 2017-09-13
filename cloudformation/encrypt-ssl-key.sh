#!/usr/bin/env bash

# this encrypts the janus ssl private key with the alias/janus kms key so the result can be stored insecurely (e.g. in a source file)
aws kms encrypt --key-id alias/janus --plaintext fileb://../certs/cloudcam.space/server.key