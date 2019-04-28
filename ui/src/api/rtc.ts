import NeatRTC from 'neat-rtc'
import { IoTClient } from './iot'

export type SDPSetupMessage = object

/**
 * WebRTC client wrapping NeatRTC, using AWSIoT MQTT for signalling and SDP setup
 * See: https://www.npmjs.com/package/neat-rtc
 */
export default class RTC {
  private thingName: string

  private neat: NeatRTC // encapsulated NeatRTC client
  public get media() {
    return this.neat.media
  }
  public send = (type: string, msg: SDPSetupMessage) => this.neat.send(msg)

  constructor(thingName: string, config: object) {
    this.neat = new NeatRTC(config, msg => this.sendSignalingMessage(msg))
    this.thingName = thingName
  }

  private sendSignalingMessage = (msg: SDPSetupMessage): void => {
    console.log('send setup message', msg)
    const iot = IoTClient.shared
    if (!iot.isMqttConnected()) {
      console.warn('Cannot send RTC signalling message because our IoT transport is gone')
      return
    }
    iot.sendWebRTCSetup(this.thingName, msg)
  }

  public async connect() {
    const iot = IoTClient.shared
    if (!iot.isMqttConnected()) {
      console.log('Connecting MQTT before beginning RTC setup...')
      await iot.mqttConnect()
      console.log('Connected.')
    }
    console.debug('Beginning RTC SDP setup')
    await this.neat.connect()

    IoTClient.shared.registerSDPHandler(this.handleSDPMessage)
  }

  public async disconnect() {
    IoTClient.shared.unregisterSDPHandler(this.handleSDPMessage)
  }

  private handleSDPMessage = (msg: SDPSetupMessage): void => {
    console.log('got SDP:', msg)
  }
}
