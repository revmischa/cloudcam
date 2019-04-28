FROM ubuntu:17.10

RUN apt-get update && apt-get install -y \
  python3 python3-pip curl git-all build-essential \
  libssl-dev libcurl4-openssl-dev gstreamer1.0 \
  python-gst-1.0 gir1.2-gst-rtsp-server-1.0 mosquitto

RUN pip3 install --upgrade \
  awscli yamllint pytest paho-mqtt Pillow cherrypy

RUN curl https://sh.rustup.rs -sSf | sh -s -- -y

