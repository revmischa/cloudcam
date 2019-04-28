import * as React from 'react'
import { createStyles, withStyles, WithStyles } from '@material-ui/core/styles'
import RTC from '../api/rtc'

const styles = () => createStyles({})

interface ILiveStreamProps extends WithStyles<typeof styles> {
  thingName: string
}
interface ILiveStreamState {
  textMessage?: string
}

export class LiveStream extends React.Component<ILiveStreamProps, ILiveStreamState> {
  public state: Partial<ILiveStreamState> = {}
  private rtc: RTC = undefined

  private handleConnected = () => {}

  mediaStreamConnected = () => {
    console.log('stream connected')
  }

  mediaStreamRemoved = () => {
    console.log('stream removed')
  }

  mediaStreamRemoteRemoved = () => {
    console.log('remote stream removed')
  }

  datachannelOpen = channel => {
    console.log('datachannel open:', channel)
  }

  datachannelMessage = (channel, message) => {
    console.log('datachannel message:', channel, message)
    this.setState({
      textMessage: JSON.stringify(message),
    })
  }

  datachannelError = channel => {
    console.log('datachannel error:', channel)
  }

  datachannelClose = channel => {
    console.log('datachannel close:', channel)
  }

  startCamera = () => {
    this.rtc.media('start')
  }

  stopCamera = () => {
    this.rtc.media('stop')
  }

  stopRemoteCamera = () => {
    this.rtc.media('stopRemote')
    console.log('1')
  }

  sendText = () => {
    const time = new Date().toTimeString().slice(0, 8)
    this.rtc.send('text', { time })
  }

  constructor(props) {
    super(props)

    const config = {
      devMode: true,
      videoIdLocal: 'localVideo',
      videoIdRemote: 'remoteVideo',
      connected: this.handleConnected,
      mediaStreamConnected: this.mediaStreamConnected,
      mediaStreamRemoved: this.mediaStreamRemoved,
      mediaStreamRemoteRemoved: this.mediaStreamRemoteRemoved,
      datachannels: [
        {
          name: 'text',
          callbacks: {
            open: this.datachannelOpen,
            message: this.datachannelMessage,
            error: this.datachannelError,
            close: this.datachannelClose,
          },
        },
      ],
    }
    this.rtc = new RTC(this.props.thingName, config)

    // signaling(message => {
    //   this.rtc.handleSignaling(message);
    // });

    this.state = {
      textMessage: '...',
    }
  }

  public componentDidMount() {
    this.connectRTC()
  }

  public componentWillUnmount() {
    if (this.rtc) this.rtc.disconnect()
  }

  private async connectRTC() {
    this.rtc.connect()
  }

  public render() {
    // const { classes } = this.props
    return (
      <React.Fragment>
        <div className="message-container">
          <h2>Datachannel message</h2>
          <h3>{this.state.textMessage}</h3>
          <button onClick={this.sendText}>Send message</button>
        </div>
        <div className="local-container">
          <h2>Local</h2>
          <video id="localVideo" width="300" height="200" />
          <button onClick={this.startCamera}>Start camera</button>
          <button onClick={this.stopCamera}>Stop camera</button>
        </div>
        <div className="remote-container">
          <h2>Remote</h2>
          <video id="remoteVideo" width="300" height="200" />
          <button onClick={this.stopRemoteCamera}>Stop remote stream</button>
        </div>
      </React.Fragment>
    )
  }
}

export default withStyles(styles)(LiveStream)
