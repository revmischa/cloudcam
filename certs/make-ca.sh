#!/usr/bin/env bash
openssl genrsa -out cloudcam-ca.key 2048
openssl req -x509 -new -nodes -key cloudcam-ca.key -sha256 -days 365 -out cloudcam-ca.pem