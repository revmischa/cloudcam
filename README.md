# About
Modern software for manipulating live video streams from IP cameras.  
[Explanation of this project.](http://spiegelmock.com/2016/03/27/diving-into-iot-development-using-aws/)

### How this works
[Description of the architecture.](http://spiegelmock.com/2016/03/27/developing-a-cloud-based-iot-service/)  

Currently working on embedded linux support, including for running on Axis cameras with the [ACAP SDK](http://www.axis.com/us/en/support/developer-support/axis-camera-application-platform). Support for other devices is planned.  
The backend services will be provided by AWS IoT, Lambda and S3.  
This is based on open source as much as humanly possible. Contributions are encouraged and welcome.  
There is full support for embedded linux. Video streaming will be done with gstreamer.  

# Building:
1. Get dependencies
2. Type `make`
3. Report any issues

## Dependencies:
### mbedTLS:
```
wget https://s3.amazonaws.com/aws-iot-device-sdk-embedded-c/linux_mqtt_mbedtls-1.1.0.tar
tar -xf linux_mqtt_mbedtls-1.1.0.tar
```
### AWS IoT Embedded C
`git clone https://github.com/aws/aws-iot-device-sdk-embedded-C.git aws_iot_client`
