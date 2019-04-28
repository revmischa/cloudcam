Modern software for manipulating live video streams from IP cameras.

Based on Serverless, AWS Lambda and AWS IoT.


## Project Info
* [Explanation of this project](http://spiegelmock.com/2016/03/27/diving-into-iot-development-using-aws/)
* [Digital video introduction](https://github.com/leandromoreira/digital_video_introduction) (technical background info).
* [How it uses AWS IoT](http://spiegelmock.com/2016/03/27/developing-a-cloud-based-iot-service/)

Currently working on embedded linux support, including for running on Axis cameras with the [ACAP SDK](http://www.axis.com/us/en/support/developer-support/axis-camera-application-platform). Support for other devices is planned.
The backend services will be provided by AWS IoT, Lambda and S3.
This is based on open source as much as humanly possible. Contributions are encouraged and welcome.
There is full support for embedded linux. Video streaming will be done with gstreamer.

### Building Client:
* Install Dependencies:
  * Debian/Ubuntu: `sudo apt install libgstreamer1.0-dev python3-pip yarn`
  * macOS: `brew install gstreamer  libsoup json-glib sqlite3`
* Type `make`

### Deploy:
1. Install AWS CLI and authenticate
1. Install [serverless](https://serverless.com/)
1. Run `yarn` to install serverless plugins
1. Run `sls deploy` to deploy lambdas and CloudFormation stack

### Frontend:
```
cd ui
yarn
yarn start
```

> (optional) Run `cd cloudformation && ./encrypt-ssl-key.sh` to encrypt the SSL private key (`certs/$domain/server.key`) for the Janus gateway subdomain with the key managed by the AWS KMS and replace the `janus_encrypted_ssl_key_pem` value in `janus_scale_lightsail.py` with the result (note: this must be run as either as AWS root user or user specified in JANUS_KMS_KEY_USER_ARN in step 2)


## Architecture
There are several distinct modules that must interact to permit live streaming video to flow from an input device to a client using a mobile app or web browser.

A video source generates video and possibly audio from either an IP camera attached to a network, a local camera (built-in camera or USB webcam), or a file. While we'd all love to use a patent-unencumbered video codec that works on all platforms [this is not possible](https://spiegelmock.com/2015/07/24/flash/) so we must use h264.

Once the streaming source has video codec data it must create a streaming session and mountpoint on a video server and then begin transmitting the video and/or audio codec data via [RTP](https://en.wikipedia.org/wiki/Real-time_Transport_Protocol) over UDP. This video server can use WebSockets or HTTP for stream session creation on the back-end, and can deliver it via [WebRTC](https://webrtc.org/) to client devices. This server with two faces as it were is known as [Janus](https://janus.conf.meetecho.com/). As long as the streaming source is generating h264 video (all current-generation IP cameras do) it can be passed through untouched all the way to the client without any transcoding. This means there is a very low CPU requirement for the intermediate software. WebRTC is a new standard for live media streaming in web browsers and mobile applications and it is already supported in most browsers with the notable exception of Safari, though Apple has [announced](http://www.nojitter.com/post/240171589/apple-jumps-on-the-webrtc-bandwagon) WebRTC support will be forthcoming soon.

By using gstreamer, Janus, and WebRTC we can construct a fully open-source, standards-compliant free live video streaming service. Incorporating our Axis camera app using their (propriatary, unfortunately) ACAP SDK with the AWS IoT API and Lambda allows us to run the application services and signalling entirely in the cloud without servers or even virtual machine instances. The Janus streaming media server can be set up separately including on premises for organizations that do not want their sensitive video leaving their network.


## Contributing
If this sounds like an interesting project that you would like to help build, please drop us a line: [cloudcam@int80.biz](mailto:cloudcam@int80.biz).

### Related projects
* [Janus-panoptic](https://github.com/revmischa/janus-panoptic)
* [Janus-docker](https://github.com/revmischa/docker-janus)

### Old projects
This design has gone through many incarnations and revisions. Some previous attempts were:
* [RTSP client](https://github.com/revmischa/rtsp-client)
* [RTSP server](https://github.com/revmischa/rtsp-server)
* [RTSP proxy](https://github.com/revmischa/rtsp-proxy)
* [Perl libav/ffmpeg bindings](https://github.com/revmischa/av-streamer)
* [Perl panoptic](https://github.com/revmischa/panoptic-perl)
* [AS3 panoptic](https://github.com/revmischa/as3-panoptic)
* [RTOS panoptic client](https://github.com/revmischa/keil-panoptic-client)
* [WebApp rapid development framework for panoptic](https://github.com/revmischa/rapid)
