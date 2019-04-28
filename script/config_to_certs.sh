#!/bin/bash

CONFIG=$1
if [ -z "$CONFIG" ]; then
    CONFIG='config.json'
fi
if [ ! -e "$CONFIG" ]; then
    echo "$CONFIG file not present"
    exit 1
fi

command -v jq >/dev/null 2>&1 || {
    echo "jq not installed"
    exit 1
}

set -e

JQSELECT() {
    cat $CONFIG | jq -r $1
}
JQSELECT .certificatePrivateKey > key.pem
JQSELECT .certificatePem > cert.pem
JQSELECT .caPem > cacert.pem

echo "Done"
