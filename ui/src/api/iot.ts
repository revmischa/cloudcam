import store from '../store'
import MQTT from 'paho-mqtt'
import SigV4Utils from './sigv4'
import * as CCStack from '../aws-stack.json'
import { Auth } from 'aws-amplify'
import { ccAWS } from './aws'
import { SDPSetupMessage } from './rtc'

// singleton
let sharedClient: IoTClient

export type SDPHandler = (msg: object) => void

export class IoTClient {
  private mqttClient: MQTT.Client | undefined
  private ccAws = ccAWS
  private sdpHandler?: SDPHandler

  public static get shared(): IoTClient {
    if (!sharedClient) sharedClient = new IoTClient()
    return sharedClient
  }

  isMqttConnected(): boolean {
    if (!this.mqttClient) return false
    return this.mqttClient.isConnected()
  }

  public async connectIfNeeded() {
    if (this.isMqttConnected()) return
    this.mqttConnect()
  }

  public async mqttConnect(): Promise<void> {
    return new Promise(async (resolve, reject) => {
      // todo: get endpoint address from a lambda function instead of hard-coding
      let host = 'a23c0yhadolyov-ats.iot.eu-central-1.amazonaws.com'
      const credentials = await Auth.currentCredentials()
      let requestUrl = SigV4Utils.getSignedUrl(host, CCStack.Region, credentials)

      console.log('MQTT websocket url: ' + requestUrl)
      const clientId = 'wss-' + Math.random().toString(36)
      this.mqttClient = new MQTT.Client(requestUrl, clientId)
      this.mqttClient.onConnectionLost = responseObject => {
        if (responseObject.errorCode !== 0) {
          console.log('MQTT onConnectionLost:' + responseObject.errorMessage)
          setTimeout(() => this.mqttConnect(), 1000)
        }
      }
      this.mqttClient.onMessageArrived = (message: MQTT.Message) => {
        console.log('MQTT message to ' + message.destinationName)
        let payload = JSON.parse(message.payloadString)
        console.log(payload)
        this.handleMQTTMessage(message, payload)
      }
      let connectOptions = {
        onSuccess: function() {
          // connect succeeded
          console.log('mqtt connection succeeded')
          resolve()
        },
        useSSL: true,
        timeout: 15,
        onFailure: function(err) {
          console.log('mqtt connection failed: ' + JSON.stringify(err))
          reject(err)
        },
      }
      this.mqttClient.connect(connectOptions)
    })
  }

  private async handleMQTTMessage(message: MQTT.Message, payload) {
    let thingName = message.destinationName.match(/\$aws\/things\/([^\\]+)\/shadow\/get\/accepted/)
    if (thingName && thingName[1]) {
      this.shadowUpdateHandler(thingName[1], payload)
    }

    thingName = message.destinationName.match(/\$aws\/things\/([^\\]+)\/shadow\/update\/accepted/)
    if (thingName && thingName[1]) {
      this.shadowUpdateHandler(thingName[1], payload)
    }

    // WebRTC signalling
    thingName = message.destinationName.match(/cloudcam\/webrtc\/setup\/([^\\]+)/)
    if (thingName && thingName[1]) {
      console.log('webrtc signalling received:', payload)
    }
  }

  public async sendWebRTCSetup(thingName: string, msg: SDPSetupMessage) {
    if (!this.isMqttConnected()) {
      throw new Error('sendWebRTCSetup but MQTT is not connected')
    }

    let mqttMessage = new MQTT.Message(JSON.stringify(msg))
    mqttMessage.destinationName = `cloudcam/${thingName}/webrtc/setup`
    this.mqttClient.send(mqttMessage)
    console.log('sent MQTT setup message: ', mqttMessage, 'to:', mqttMessage.destinationName)
  }

  public registerSDPHandler = (handler: SDPHandler) => {
    if (this.sdpHandler) console.warn('registerSDPHandler called but it is already set')
    this.sdpHandler = handler
  }

  public unregisterSDPHandler = (handler: SDPHandler) => {
    if (!this.sdpHandler) {
      console.warn('unregisterSDPHandler called but none is set')
      return
    }
    this.sdpHandler = undefined
  }

  // request new thumbnails every 10s
  private reqThumbsProcHandle: NodeJS.Timeout
  private async requestThumbsProcess() {
    if (this.reqThumbsProcHandle) clearTimeout(this.reqThumbsProcHandle)

    // stop refreshing if not logged in
    if (!(await this.ccAws.checkAuthExists())) return

    this.requestThumbs(Object.keys(store.getState().iot.things))
    this.reqThumbsProcHandle = setTimeout(this.requestThumbsProcess.bind(this), 10000)
  }

  // subscibe to mqtt topics where thing shadow updates are published
  subscribeToShadowUpdates(thingName) {
    return new Promise((resolve, reject) => {
      this.mqttClient.subscribe('$aws/things/' + thingName + '/shadow/update/accepted', {
        onSuccess: function() {
          console.log('mqtt subscription to ' + thingName + ' shadow updates succeeded')
          resolve()
        },
        onFailure: function(err) {
          console.log('mqtt subscription to ' + thingName + ' shadow updates failed: ' + JSON.stringify(err))
          reject(err)
        },
      })
    })
  }
  subscribeToShadowGet(thingName) {
    return new Promise((resolve, reject) => {
      this.mqttClient.subscribe('$aws/things/' + thingName + '/shadow/get/accepted', {
        onSuccess: function() {
          console.log('mqtt subscription to ' + thingName + ' shadow get succeeded')
          resolve()
        },
        onFailure: function(err) {
          console.log('mqtt subscription to ' + thingName + ' shadow get failed: ' + JSON.stringify(err))
          reject(err)
        },
      })
    })
  }

  // called when iot thing shadow is updated
  shadowUpdateHandler(thingName, doc) {
    // if thing uploaded a new thumbnail to S3, get the download url and dispatch redux update for it
    if (
      doc.state.reported &&
      doc.state.reported.last_uploaded_thumb &&
      doc.state.reported.last_uploaded_thumb.status === 'success'
    ) {
      console.log('new ' + thingName + ' thumb available at ' + doc.state.reported.last_uploaded_thumb.download_url)
      store.dispatch({
        type: 'iot/thumb',
        thingName: thingName,
        thumbUrl: doc.state.reported.last_uploaded_thumb.download_url,
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

  // get thing shadows
  private getShadows() {
    Object.keys(store.getState().iot.things).forEach(thingName => {
      console.log('thing', thingName)
      this.getShadow(thingName)
    })
  }

  // updates the specified thing shadow
  updateShadow(thingName, doc) {
    let message = new MQTT.Message(JSON.stringify(doc))
    message.destinationName = '$aws/things/' + thingName + '/shadow/update'
    this.mqttClient.send(message)
  }

  // updates the specified thing shadow
  getShadow(thingName) {
    let message = new MQTT.Message('')
    message.destinationName = '$aws/things/' + thingName + '/shadow/get'
    console.log('getting shadow', message.destinationName)
    this.mqttClient.send(message)
  }

  // takes over a specified thing
  // unused
  attachThingPolicy(thingName) {
    return this.ccAws.invokeLambda({
      FunctionName: CCStack.IoTAttachCameraPolicyLambdaFunctionQualifiedArn,
      Payload: JSON.stringify({
        thingName: thingName,
      }),
    })
  }

  // provisions a new thing
  public async provisionThing(thingName) {
    console.log('invoking iot_provision_thing:')
    const data = await this.ccAws.invokeLambda({
      FunctionName: CCStack.IoTProvisionThingLambdaFunctionQualifiedArn,
      Payload: JSON.stringify({
        thingName: thingName,
      }),
    })

    console.log('iot_provision_thing result:', data)
    this.refreshThings()
    return JSON.parse(data.Payload as string)
  }

  public async attachUserPolicy() {
    console.log('invoking attachIoTPolicy')
    const data = await this.ccAws.invokeLambda({
      FunctionName: CCStack.IoTAttachUserPolicyLambdaFunctionQualifiedArn,
    })

    console.debug('attached user to IoT policy:', data)
    store.dispatch({ type: 'iot/attachedUserPolicy', isAttached: true })
  }

  // gets a list of things current Cognito identity has access to
  async refreshThings(): Promise<any> {
    console.log('invoking iot_list_things:')
    const res = await this.ccAws.invokeLambda({
      FunctionName: CCStack.IoTListThingsLambdaFunctionQualifiedArn,
    })
    console.log('refreshed things: ', res)
    let result = JSON.parse(res.Payload as string)
    store.dispatch({
      type: 'iot/things',
      thingNames: result.thingNames,
    })
    this.requestThumbsProcess()

    var subscribeFn = () => {
      if (!result.thingNames) return

      result.thingNames.map(async name => {
        await this.subscribeToShadowUpdates(name)
        await this.subscribeToShadowGet(name)
        await this.getShadow(name)
      })
    }
    if (!this.isMqttConnected()) {
      await this.mqttConnect()
      subscribeFn()
    } else {
      subscribeFn()
    }
    return result
  }

  // requests new thumbnails from the specified things (upload notification will be received separately via MQTT thing shadow update)
  public async requestThumbs(thingNames: string[]): Promise<any> {
    if (!thingNames || !thingNames.length) return

    console.log('invoking request_thumb: ' + thingNames)
    return this.ccAws.invokeLambda({
      FunctionName: CCStack.ThumbStoreLambdaFunctionQualifiedArn,
      Payload: JSON.stringify({
        thingNames: thingNames,
      }),
    })
  }

  // starts webrtc streaming from the specified thing
  // startStreaming(thingName) {
  //   return new Promise((resolve, reject) => {
  //     console.log('startStreaming: ' + thingName)
  //     if (!Janus.isWebrtcSupported()) {
  //       reject('No WebRTC support... ')
  //     }
  //     // allocate primary/standby streams on the Janus gateways via lambda function
  //     let params = {
  //       FunctionName: CCStack.JanusStartStreamLambdaFunctionQualifiedArn,
  //       Payload: JSON.stringify({
  //         thingName: thingName,
  //         // forceReallocate: true
  //       }),
  //     }
  //     this.ccAws.invokeLambda(params, (err, data) => {
  //       if (err) {
  //         console.log(err)
  //         reject(err)
  //       } else {
  //         console.log(data)
  //         // setup the webrtc connection
  //         let result = JSON.parse(data.Payload as string)
  //         let primary = result.primary
  //         // let standby = result.standby
  //         // todo: implement primary/standby track switching
  //         let opaqueId = 'cloudcam-dev-ui-' + Janus.randomString(12)
  //         var streaming = null
  //         let stopStream = () => {
  //           var body = { request: 'stop' }
  //           streaming.send({ message: body })
  //           streaming.hangup()
  //         }
  //         var janus = new Janus({
  //           server: primary.gateway_url,
  //           success: () => {
  //             // Attach to streaming plugin
  //             janus.attach({
  //               plugin: 'janus.plugin.streaming',
  //               opaqueId: opaqueId,
  //               success: pluginHandle => {
  //                 streaming = pluginHandle
  //                 Janus.log('Plugin attached! (' + streaming.getPlugin() + ', id=' + streaming.getId() + ')')
  //                 var body = {
  //                   request: 'watch',
  //                   id: primary.stream_id,
  //                   // pin: primary.stream_pin
  //                 }
  //                 streaming.send({ message: body })
  //               },
  //               error: function(error) {
  //                 Janus.error('  -- Error attaching plugin... ', error)
  //                 reject(error)
  //               },
  //               onmessage: function(msg, jsep) {
  //                 console.debug(' ::: Got a message :::')
  //                 console.debug(JSON.stringify(msg))
  //                 var result = msg['result']
  //                 if (result !== null && result !== undefined) {
  //                   if (result['status'] !== undefined && result['status'] !== null) {
  //                     var status = result['status']
  //                     if (status === 'stopped') stopStream()
  //                   }
  //                 } else if (msg['error'] !== undefined && msg['error'] !== null) {
  //                   console.debug(msg['error'])
  //                   reject(msg['error'])
  //                   return
  //                 }
  //                 if (jsep !== undefined && jsep !== null) {
  //                   console.debug('Handling SDP as well...')
  //                   console.debug(jsep)
  //                   // Answer
  //                   streaming.createAnswer({
  //                     jsep: jsep,
  //                     media: { audioSend: false, videoSend: false }, // We want recvonly audio/video
  //                     success: function(jsep) {
  //                       console.debug('Got SDP!')
  //                       console.debug(jsep)
  //                       var body = { request: 'start' }
  //                       streaming.send({ message: body, jsep: jsep })
  //                     },
  //                     error: function(error) {
  //                       Janus.error('WebRTC error:', error)
  //                       reject(error)
  //                     },
  //                   })
  //                 }
  //               },
  //               onremotestream: function(stream) {
  //                 console.debug(' ::: Got a remote stream :::')
  //                 console.debug(JSON.stringify(stream))
  //                 store.dispatch({
  //                   type: 'iot/startStreaming',
  //                   thingName: thingName,
  //                   stopStreamFn: stopStream,
  //                 })
  //                 Janus.attachMediaStream(document.querySelector('video#' + thingName), stream)
  //                 resolve({})
  //               },
  //               oncleanup: function() {
  //                 Janus.log(' ::: Got a cleanup notification :::')
  //                 store.dispatch({
  //                   type: 'iot/stopStreaming',
  //                   thingName: thingName,
  //                 })
  //               },
  //             })
  //           },
  //           error: function(error) {
  //             Janus.error(error)
  //             resolve(error)
  //           },
  //           destroyed: function() {},
  //         })
  //       }
  //     })
  //   })
  // }

  // // stops webrtc streaming for the specified thing
  // stopStreaming(thingName) {
  //   return new Promise((resolve, reject) => {
  //     console.log('stopStreaming: ' + thingName)
  //     if (store.getState().iot.things[thingName] && store.getState().iot.things[thingName]['stopStreamFn']) {
  //       store.getState().iot.things[thingName]['stopStreamFn']()
  //     }
  //     let params = {
  //       FunctionName: CCStack.JanusStopStreamLambdaFunctionQualifiedArn,
  //       Payload: JSON.stringify({
  //         thingName: thingName,
  //       }),
  //     }
  //     this.ccAws.invokeLambda(params, (err, data) => {
  //       if (err) {
  //         console.log(err)
  //         reject(err)
  //       } else {
  //         console.log(data)
  //         store.dispatch({
  //           type: 'iot/stopStreaming',
  //           thingName: thingName,
  //         })
  //       }
  //     })
  //   })
  // }

  static reducer(state = { things: {} }, action) {
    let newState
    switch (action.type) {
      // update thing list
      case 'iot/things':
        newState = JSON.parse(JSON.stringify(state))
        newState.things = action.thingNames
          ? action.thingNames.reduce((o, name) => {
              o[name] = { name: name, ...state.things[name] }
              return o
            }, {})
          : {}
        console.log('iot/things new state: ' + JSON.stringify(newState))
        return newState
      // start thing webrtc stream
      case 'iot/startStreaming':
        newState = JSON.parse(JSON.stringify(state))
        newState.things[action.thingName].isStreaming = true
        newState.things[action.thingName].stopStreamFn = action.stopStreamFn
        console.log('iot/startStreaming new state: ' + JSON.stringify(newState))
        return newState
      // stop thing webrtc stream
      case 'iot/stopStreaming':
        newState = JSON.parse(JSON.stringify(state))
        newState.things[action.thingName].isStreaming = false
        console.log('iot/stopStreaming new state: ' + JSON.stringify(newState))
        return newState
      // update current thing thumbnail image
      case 'iot/thumb':
        newState = JSON.parse(JSON.stringify(state))
        newState.things[action.thingName] = Object.assign({}, newState.things[action.thingName], {
          thumbUrl: action.thumbUrl,
        })
        console.log('iot/thumb new state: ' + JSON.stringify(newState))
        return newState
      // provision a new thing
      case 'iot/provision':
        newState = JSON.parse(JSON.stringify(state))
        newState.thingConfig = action.thingConfig
        console.log('iot/provision new state: ' + JSON.stringify(newState))
        return newState
      case 'iot/attachedUserPolicy':
        return { ...state, attachedUserPolicy: action.isAttached }
      default:
        return state
    }
  }
}
