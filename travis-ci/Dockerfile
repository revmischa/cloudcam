FROM ubuntu:19.04

RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections

RUN apt update && apt install -y \
  python3 python3-pip curl git-all build-essential \
  libssl-dev libcurl4-openssl-dev mosquitto

RUN apt install -y \
  libgstreamer1.0-dev \
  gstreamer1.0-plugins-ugly-dbg gstreamer1.0-plugins-bad-dbg gstreamer1.0-plugins-good-dbg gstreamer1.0-plugins-base-dbg \
  gstreamer1.0-plugins-rtp gstreamer1.0-nice libgstreamer1.0-0-dbg gstreamer1.0-python3-dbg-plugin-loader \
  python3-gst-1.0 gir1.2-gst-rtsp-server-1.0 

RUN apt install -y \
  librust-openssl-dev rustc cargo

RUN pip3 install --upgrade \
  awscli yamllint pytest paho-mqtt Pillow cherrypy

