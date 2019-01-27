import store from '../store'
import MQTT from 'paho-mqtt'
import AWS from 'aws-sdk'
import { Auth } from 'aws-amplify'
import SigV4Utils from './sigv4'
import * as CCStack from '../aws-stack.json'
import { default as J } from '../vendor/janus.es'

// FIXME: janus needs TS decl
const Janus = J as any

export class IoTClient {
  private mqttClient: MQTT.Client | undefined

  private async getLambdaClient(): Promise<AWS.Lambda> {
    const credentials = await Auth.currentCredentials()
    return new AWS.Lambda({ credentials: Auth.essentialCredentials(credentials) });
  }

  private async invokeLambda(params: AWS.Lambda.InvocationRequest, callback?: (err: AWS.AWSError, data: AWS.Lambda.InvocationResponse) => void): Promise<AWS.Request<AWS.Lambda.InvocationResponse, AWS.AWSError>> {
    // params.FunctionName = `${CCStack.StackName}-${params.FunctionName}`
    const client = await this.getLambdaClient()
    return client.invoke(params, callback)
  }

  isMqttConnected(): boolean {
    if (!this.mqttClient) 
      return false
    return this.mqttClient.isConnected();
  }

  async mqttConnect() {
    return new Promise(async (resolve, reject) => {
      let host = "a23c0yhadolyov-ats.iot.us-west-2.amazonaws.com";
      const credentials = await Auth.currentCredentials()
      let requestUrl = SigV4Utils.getSignedUrl(host, CCStack.Region, credentials);

      console.log("MQTT websocket url: " + requestUrl)
      const clientId = "wss-client-" + Math.random().toString(36).substring(7)
      this.mqttClient = new MQTT.Client(requestUrl, clientId);
      this.mqttClient.onConnectionLost = (responseObject) => {
        if (responseObject.errorCode !== 0) {
          console.log("MQTT onConnectionLost:" + responseObject.errorMessage);
          setTimeout(() => this.mqttConnect(), 1000);
        }
      };
      this.mqttClient.onMessageArrived = (message) => {
        console.log("MQTT message to " + message.destinationName);
        let payload = JSON.parse(message.payloadString);
        console.log(payload);
        let thingName = message.destinationName.match(/\$aws\/things\/([^\\]+)\/shadow\/update\/accepted/);
        if (thingName && thingName[1]) {
          this.shadowUpdateHandler(thingName[1], payload)
        }
      };
      let connectOptions = {
        onSuccess: function () {
          // connect succeeded
          console.log("mqtt connection succeeded");
          resolve();
        },
        useSSL: true,
        timeout: 15,
        onFailure: function (err) {
          console.log("mqtt connection failed: " + JSON.stringify(err))
          reject(err);
        }
      };
      this.mqttClient.connect(connectOptions);
    });
  }

  // request new thumbnails every 10s
  private reqThumbsProcHandle: NodeJS.Timeout
  requestThumbsProcess() {
    if (this.reqThumbsProcHandle)
      clearTimeout(this.reqThumbsProcHandle);
    this.requestThumbs(Object.keys(store.getState().iot.things));
    this.reqThumbsProcHandle = setTimeout(this.requestThumbsProcess.bind(this), 10000);
  }

  // subscibe to mqtt topics where thing shadow updates are published
  subscribeToShadowUpdates(thingName) {
    return new Promise((resolve, reject) => {
      this.mqttClient.subscribe("$aws/things/" + thingName + "/shadow/update/accepted", {
        onSuccess: function () {
          console.log("mqtt subscription to " + thingName + " shadow updates succeeded");
          resolve();
        },
        onFailure: function (err) {
          console.log("mqtt subscription to " + thingName + " shadow updates failed: " + JSON.stringify(err))
          reject(err);
        }
      });
    });
  }

  // called when iot thing shadow is updated
  shadowUpdateHandler(thingName, doc) {
    // if thing uploaded a new thumbnail to S3, get the download url and dispatch redux update for it
    if (doc.state.reported &&
      doc.state.reported.last_uploaded_thumb &&
      doc.state.reported.last_uploaded_thumb.status === "success") {
      console.log("new " + thingName + " thumb available at " + doc.state.reported.last_uploaded_thumb.download_url);
      store.dispatch({
        type: 'iot/thumb', thingName: thingName, thumbUrl: doc.state.reported.last_uploaded_thumb.download_url
      })
    }
  }

  // iot thing shadow document format:
  // {
  //   "state": {
  //     "desired": {
  //       "key": <value>
  //     }
  //   }
  // }

  // updates the specified thing shadow
  updateShadow(thingName, doc) {
    let message = new MQTT.Message(JSON.stringify(doc));
    message.destinationName = "$aws/things/" + thingName + "/shadow/update";
    this.mqttClient.send(message);
  }

  // takes over a specified thing
  attachThingPolicy(thingName) {
    return new Promise((resolve, reject) => {
      console.log('invoking iot_attach_thing_policy:');
      this.invokeLambda({
        FunctionName: CCStack.IoTAttachThingPolicyLambdaFunctionQualifiedArn,
        Payload: JSON.stringify({
          thingName: thingName
        })
      }, (err, data) => {
        console.log('iot_attach_thing_policy result:');
        if (err) {
          console.log(err);
          reject(err);
        }
        else {
          console.log(data);
          resolve(data);
        }
      });
    });
  }

  // provisions a new thing
  provisionThing(thingName) {
    return new Promise((resolve, reject) => {
      console.log('invoking iot_provision_thing:');
      this.invokeLambda({
        FunctionName: CCStack.IoTProvisionThingLambdaFunctionQualifiedArn,
        Payload: JSON.stringify({
          thingName: thingName
        })
      }, (err, data) => {
        console.log('iot_provision_thing result:');
        if (err) {
          console.log(err);
          reject(err);
        }
        else {
          console.log(data);
          this.refreshThings();
          resolve(JSON.parse(data.Payload as string));
        }
      });
    });
  }

  // getsa a list of things current Cognito identity has access to
  refreshThings() {
    return new Promise((resolve, reject) => {
      console.log('invoking iot_list_things:');
      this.invokeLambda({
        FunctionName: CCStack.IoTListThingsLambdaFunctionQualifiedArn,
        Payload: ""
      }, (err, data) => {
        if (err) {
          console.log(err);
          reject(err);
        }
        else {
          console.log(data);
          let result = JSON.parse(data.Payload as string);
          store.dispatch({
            type: 'iot/things', thingNames: result.thingNames
          });
          this.requestThumbsProcess();
          var subscribeFn = () => {
            result.thingNames.map((name) => {
              this.subscribeToShadowUpdates(name)
                .catch(e => {
                  console.log(e);
                });
            });
          };
          if (!this.isMqttConnected()) {
            this.mqttConnect().then(() => {
              subscribeFn();
            });
          }
          else {
            subscribeFn()
          }
          resolve(data);
        }
      });
    });
  }

  // requests new thumbnails from the specified things (upload notification will be received separately via MQTT thing shadow update)
  requestThumbs(thingNames) {
    return new Promise((resolve, reject) => {
      console.log('invoking request_thumb: ' + thingNames);
      let params = {
        FunctionName: CCStack.ThumbStoreLambdaFunctionQualifiedArn,
        Payload: JSON.stringify({
          thingNames: thingNames
        })
      };
      this.invokeLambda(params, function (err, data) {
        if (err) {
          console.log(err);
          reject(err);
        }
        else {
          console.log(data);
          resolve(data);
        }
      })
    })
  }

  // starts webrtc streaming from the specified thing
  startStreaming(thingName) {
    return new Promise((resolve, reject) => {
      console.log('startStreaming: ' + thingName);
      if (!Janus.isWebrtcSupported()) {
        reject("No WebRTC support... ");
      }
      // allocate primary/standby streams on the Janus gateways via lambda function
      let params = {
        FunctionName: CCStack.JanusStartStreamLambdaFunctionQualifiedArn,
        Payload: JSON.stringify({
          thingName: thingName
          // forceReallocate: true
        })
      };
      this.invokeLambda(params, (err, data) => {
        if (err) {
          console.log(err);
          reject(err);
        }
        else {
          console.log(data);
          // setup the webrtc connection
          let result = JSON.parse(data.Payload as string);
          let primary = result.primary;
          let standby = result.standby;
          // todo: implement primary/standby track switching
          let opaqueId = "cloudcam-dev-ui-" + Janus.randomString(12);
          var streaming = null;
          let stopStream = () => {
            var body = { "request": "stop" };
            streaming.send({ "message": body });
            streaming.hangup();
          };
          var janus = new Janus({
            server: primary.gateway_url,
            success: () => {
              // Attach to streaming plugin
              janus.attach({
                plugin: "janus.plugin.streaming",
                opaqueId: opaqueId,
                success: (pluginHandle) => {
                  streaming = pluginHandle;
                  Janus.log("Plugin attached! (" + streaming.getPlugin() + ", id=" + streaming.getId() + ")");
                  var body = {
                    request: "watch",
                    id: primary.stream_id
                    // pin: primary.stream_pin
                  };
                  streaming.send({ "message": body });
                },
                error: function (error) {
                  Janus.error("  -- Error attaching plugin... ", error);
                  reject(error);
                },
                onmessage: function (msg, jsep) {
                  console.debug(" ::: Got a message :::");
                  console.debug(JSON.stringify(msg));
                  var result = msg["result"];
                  if (result !== null && result !== undefined) {
                    if (result["status"] !== undefined && result["status"] !== null) {
                      var status = result["status"];
                      if (status === 'stopped')
                        stopStream();
                    }
                  } else if (msg["error"] !== undefined && msg["error"] !== null) {
                    console.debug(msg["error"]);
                    reject(msg["error"]);
                    return;
                  }
                  if (jsep !== undefined && jsep !== null) {
                    console.debug("Handling SDP as well...");
                    console.debug(jsep);
                    // Answer
                    streaming.createAnswer(
                      {
                        jsep: jsep,
                        media: { audioSend: false, videoSend: false },	// We want recvonly audio/video
                        success: function (jsep) {
                          console.debug("Got SDP!");
                          console.debug(jsep);
                          var body = { "request": "start" };
                          streaming.send({ "message": body, "jsep": jsep });
                        },
                        error: function (error) {
                          Janus.error("WebRTC error:", error);
                          reject(error);
                        }
                      });
                  }
                },
                onremotestream: function (stream) {
                  console.debug(" ::: Got a remote stream :::");
                  console.debug(JSON.stringify(stream));
                  store.dispatch({
                    type: 'iot/startStreaming', thingName: thingName, stopStreamFn: stopStream
                  });
                  Janus.attachMediaStream(document.querySelector("video#" + thingName), stream);
                  resolve({});
                },
                oncleanup: function () {
                  Janus.log(" ::: Got a cleanup notification :::");
                  store.dispatch({
                    type: 'iot/stopStreaming', thingName: thingName
                  });
                }
              });
            },
            error: function (error) {
              Janus.error(error);
              resolve(error);
            },
            destroyed: function () {
            }
          })
        }
      })
    })
  }

  // stops webrtc streaming for the specified thing
  stopStreaming(thingName) {
    return new Promise((resolve, reject) => {
      console.log('stopStreaming: ' + thingName);
      if (store.getState().iot.things[thingName] && store.getState().iot.things[thingName]['stopStreamFn']) {
        store.getState().iot.things[thingName]['stopStreamFn']();
      }
      let params = {
        FunctionName: CCStack.JanusStopStreamLambdaFunctionQualifiedArn,
        Payload: JSON.stringify({
          thingName: thingName
        })
      };
      this.invokeLambda(params, (err, data) => {
        if (err) {
          console.log(err);
          reject(err);
        }
        else {
          console.log(data);
          store.dispatch({
            type: 'iot/stopStreaming', thingName: thingName
          });
        }
      })
    })
  }

  static reducer(state = { things: {} }, action) {
    let newState;
    switch (action.type) {
      // update thing list
      case 'iot/things':
        newState = JSON.parse(JSON.stringify(state));
        newState.things = action.thingNames.reduce((o, name) => {
          o[name] = Object.assign({ name: name }, state.things[name]);
          return o
        }, {});
        console.log("iot/things new state: " + JSON.stringify(newState));
        return newState;
      // start thing webrtc stream
      case 'iot/startStreaming':
        newState = JSON.parse(JSON.stringify(state));
        newState.things[action.thingName].isStreaming = true;
        newState.things[action.thingName].stopStreamFn = action.stopStreamFn;
        console.log("iot/startStreaming new state: " + JSON.stringify(newState));
        return newState;
      // stop thing webrtc stream
      case 'iot/stopStreaming':
        newState = JSON.parse(JSON.stringify(state));
        newState.things[action.thingName].isStreaming = false;
        console.log("iot/stopStreaming new state: " + JSON.stringify(newState));
        return newState;
      // update current thing thumbnail image
      case 'iot/thumb':
        newState = JSON.parse(JSON.stringify(state));
        newState.things[action.thingName] = Object.assign({}, newState.things[action.thingName], { thumbUrl: action.thumbUrl });
        console.log("iot/thumb new state: " + JSON.stringify(newState));
        return newState;
      // provision a new thing
      case 'iot/provision':
        newState = JSON.parse(JSON.stringify(state));
        newState.thingConfig = action.thingConfig;
        console.log("iot/provision new state: " + JSON.stringify(newState));
        return newState;
      default:
        return state
    }
  }
}
