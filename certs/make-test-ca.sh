#!/usr/bin/env bash
openssl genrsa -out cloudcam-test-ca.key 2048
openssl req -x509 -new -nodes -key cloudcam-test-ca.key -sha256 -days 3650 -out cloudcam-test-ca.pem