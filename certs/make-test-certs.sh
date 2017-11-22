#!/usr/bin/env bash

openssl genrsa -out cloudcam-client-test.key 2048
openssl req -new -key cloudcam-client-test.key -out cloudcam-client-test.csr -subj "/CN=cloudcam-client-test/"
openssl x509 -req -in cloudcam-client-test.csr -CA cloudcam-test-ca.pem -CAkey cloudcam-test-ca.key -CAcreateserial -out cloudcam-client-test.crt -days 3650 -sha256

openssl genrsa -out cloudcam-server-test.key 2048
openssl req -new -key cloudcam-server-test.key -out cloudcam-server-test.csr -subj "/CN=127.0.0.1/"
openssl x509 -req -in cloudcam-server-test.csr -CA cloudcam-test-ca.pem -CAkey cloudcam-test-ca.key -CAcreateserial -out cloudcam-server-test.crt -days 3650 -sha256
