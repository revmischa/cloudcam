#!/bin/bash
cd /axis/emb-app-sdk_2_0_3 && \
    source init_env && \
    cd /client && \
    eap-install.sh $*
