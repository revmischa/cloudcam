import * as React from 'react'
import { IoTClient } from '../api/iot'
import LiveStream from '../components/liveStream'
import { withRouter } from 'react-router'

class LiveView extends React.Component<any, any> {
  client = new IoTClient()

  private getThingName = (): string => this.props.match.params.thingName

  render() {
    const thingName = this.getThingName()
    if (!thingName) {
      console.error("Didn't get thingName from route")
      return null
    }

    return (
      <div>
        <section className="section content container">
          <h2 className="title">Camera # </h2>
          <LiveStream thingName={this.getThingName()} />
        </section>
      </div>
    )
  }
}

export default withRouter(LiveView)
