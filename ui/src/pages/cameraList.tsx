import * as React from 'react'
import { connect } from 'react-redux'
import { IoTClient } from '../api/iot'
import Card from '@material-ui/core/Card'
import CardActions from '@material-ui/core/CardActions'
import CardContent from '@material-ui/core/CardContent'
import Grid from '@material-ui/core/Grid'
import Typography from '@material-ui/core/Typography'
import Button from '@material-ui/core/Button'
import { Link } from 'react-router-dom'
import { ICamera } from '../model/camera'

interface ICameraListProps {
  things: ICamera[]
}

class CameraList extends React.Component<ICameraListProps, any> {
  public componentDidMount() {
    IoTClient.shared.refreshThings()
  }

  render() {
    const client = IoTClient.shared
    const { things } = this.props
    // This resolves to nothing and doesn't affect browser history
    // const dudUrl = 'javascript:;'
    // const CameraLink = props => <Link to="/live-view" {...props} />
    return (
      <div>
        <section className="section content container">
          <h2 className="title">Cameras</h2>
          <Button variant="contained" color="secondary" onClick={() => client.refreshThings()}>
            refresh
          </Button>
          <br />
          <br />
          <div>{Object.entries(things).length === 0 && <div>No things</div>}</div>

          <Grid container spacing={40}>
            {Object.entries(things).map(function([key, thing]) {
              return (
                <Grid item key={thing.name} sm={6} md={4} lg={3}>
                  <Card>
                    <CardContent>
                      <Typography gutterBottom variant="h5" component="h2">
                        {thing.name}
                      </Typography>
                      <Typography>Thing description to be added.</Typography>
                      <div 
                        className="thumb-block"
                        style={{ maxWidth: '400px'}}
                        >
                        <img
                          id={thing.name}
                          className="thumb"
                          style={{ display: thing.isStreaming ? 'none' : 'block', width: '100%' }}
                          src={thing.thumbUrl ? thing.thumbUrl : 'images/no-video.png'}
                          alt="Camera Thumbnail"
                        />
                        <video id={thing.name} style={{ display: thing.isStreaming ? 'block' : 'none' }} autoPlay />
                      </div>
                    </CardContent>
                    <CardActions>
                      <Button
                        size="small"
                        variant="contained"
                        color="secondary"
                        style={{ display: thing.isStreaming ? 'none' : 'inline-block' }}
                        href="#"
                        onClick={() => client.requestThumbs([thing.name])}
                      >
                        refresh
                      </Button>
                      <Button
                        size="small"
                        variant="contained"
                        color="secondary"
                        onClick={() => {
                          // if (!thing.isStreaming) {
                          //   client.startStreaming(thing.name)
                          // } else {
                          //   client.stopStreaming(thing.name)
                          // }
                        }}
                      >
                        {thing.isStreaming ? 'stop' : 'view live'}
                      </Button>
                      <Link to={`/cameras/${thing.name}/live`}>{thing.name}</Link>
                    </CardActions>
                  </Card>
                </Grid>
              )
            })}
          </Grid>
        </section>
      </div>
    )
  }
}

const mapStateToProps = function(store) {
  return {
    things: store.iot.things,
  }
}

export default connect(mapStateToProps)(CameraList)
