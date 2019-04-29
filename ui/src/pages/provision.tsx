import * as React from 'react'
import { connect } from 'react-redux'
// import { error, success } from '../api/notification'
import FormProvision from '../components/FormProvision'
import store from '../store'
import { IoTClient } from '../api/iot'

interface IProvisionProps {
  thingConfig: object
}

class Provision extends React.Component<IProvisionProps> {
  private client: IoTClient

  public static route = {
    path: 'provision',
    component: Provision,
  }

  constructor(props) {
    super(props)
    this.client = new IoTClient()
    this.state = {
      thingConfig: null,
    }
  }

  onSubmit = form => {
    return this.client
      .provisionThing(form.thingName)
      .then(data => {
        console.log('Device config:')
        console.log(data.thingConfig)
        store.dispatch({
          type: 'iot/provision',
          thingConfig: data.thingConfig,
        })
        // success('Provisioned a thing.');
      })
      .catch(e => {
        // error(e && e.message ? e.message : 'Could not provision a thing.')
      })
  }

  render() {
    const { thingConfig } = this.props
    return (
      <div className="section form is-large">
        <h2 className="title">Provision</h2>
        <div className="columns">
          <div className="column is-medium">
            <div className="notification">
              {!thingConfig && <p>Provision a thing here.</p>}
              {thingConfig && (
                <p>
                  Device configuration:
                  <br />
                  {JSON.stringify(thingConfig, null, 2)}
                </p>
              )}
              {/* <JSONPretty id="json-pretty" data={JSON.stringify(thingConfig, null, 2)}></JSONPretty>  */}
              {/* <CodeBlock JSON>
                <pre>
                  <code>
                    {JSON.stringify(thingConfig, null, 2)}
                  </code>
                </pre>
              </CodeBlock>              */}
            </div>
          </div>
          <div className="column">
            <FormProvision onSubmit={this.onSubmit} />
          </div>
        </div>
      </div>
    )
  }
}

const mapStateToProps = function(store) {
  return {
    thingConfig: store.iot.thingConfig,
  }
}

export default connect(mapStateToProps)(Provision)
