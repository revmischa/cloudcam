import * as React from 'react'
import {connect} from 'react-redux'
import {IoTClient} from '../api/iot'

interface ICamera {
  name: string
  thumbUrl?: string
  isStreaming: boolean
}

interface ICameraListProps {
  things: ICamera[]
}

class CameraList extends React.Component<ICameraListProps, any> {
  client = new IoTClient()

  constructor(props) {
    super(props);
  }

  public componentDidMount() {
    this.client.refreshThings()
  }

  render() {
    const client = this.client
    const {things} = this.props;
    return (
      <div>
        <section className='section content container'>
          <h2 className='title'>Cameras <a onClick={() => client.refreshThings()} href="#">refresh</a>
          </h2>
          <div>
            {Object.entries(things).length === 0 && <div>No things</div>}
            <ul>
              {Object.entries(things).map(function ([key, thing]) {
                return <li key={thing.name}>
                  <div>
                    <p>
                      <img id={thing.name} className='thumb' style={{display: thing.isStreaming ? 'none' : 'block'}}
                           src={thing.thumbUrl}/>
                      <video id={thing.name} style={{display: thing.isStreaming ? 'block' : 'none'}} autoPlay/>
                    </p>
                    {thing.name}&nbsp;
                    <a style={{display: thing.isStreaming ? 'none' : 'inline-block'}} href="#"
                       onClick={() => client.requestThumbs([thing.name])}>refresh</a>&nbsp;
                    <a onClick={() => {
                      if (!thing.isStreaming) {
                        client.startStreaming(thing.name);
                      } else {
                        client.stopStreaming(thing.name);
                      }
                    }} href="#">{thing.isStreaming ? 'stop' : 'view live'}</a>&nbsp;
                  </div>
                </li>
              })}
            </ul>
          </div>
        </section>
      </div>
    )
  }
}

const mapStateToProps = function (store) {
  return {
    things: store.iot.things,
  }
};

export default connect(mapStateToProps)(CameraList)
