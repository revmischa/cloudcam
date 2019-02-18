import * as React from 'react'
import {connect} from 'react-redux'
import {IoTClient} from '../api/iot'
import Card from '@material-ui/core/Card';
import CardActions from '@material-ui/core/CardActions';
import CardContent from '@material-ui/core/CardContent';
import CardMedia from '@material-ui/core/CardMedia';
import CssBaseline from '@material-ui/core/CssBaseline';
import Grid from '@material-ui/core/Grid';
import Toolbar from '@material-ui/core/Toolbar';
import Typography from '@material-ui/core/Typography';
import Button from '@material-ui/core/Button';


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
            {/* <ul>
              {Object.entries(things).map(function ([key, thing]) {
                return <li key={thing.name}>
                  <div>
                    <div className="thumb-block">
                      <img id={thing.name} className='thumb' style={{display: thing.isStreaming ? 'none' : 'block'}}
                           src={thing.isStreaming ? thing.thumbUrl : 'images/no-video.png'}/>
                      <video id={thing.name} style={{display: thing.isStreaming ? 'block' : 'none'}} autoPlay/>
                    </div>
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
            </ul> */}
          </div>

          <Grid container spacing={40}>
          {Object.entries(things).map(function ([key, thing]) {
              return <Grid item key={thing.name} sm={6} md={4} lg={3}>
                <Card>
                  {/* <CardMedia
                    className={classes.cardMedia}
                    image="data:image/svg+xml;charset=UTF-8,%3Csvg%20width%3D%22288%22%20height%3D%22225%22%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20viewBox%3D%220%200%20288%20225%22%20preserveAspectRatio%3D%22none%22%3E%3Cdefs%3E%3Cstyle%20type%3D%22text%2Fcss%22%3E%23holder_164edaf95ee%20text%20%7B%20fill%3A%23eceeef%3Bfont-weight%3Abold%3Bfont-family%3AArial%2C%20Helvetica%2C%20Open%20Sans%2C%20sans-serif%2C%20monospace%3Bfont-size%3A14pt%20%7D%20%3C%2Fstyle%3E%3C%2Fdefs%3E%3Cg%20id%3D%22holder_164edaf95ee%22%3E%3Crect%20width%3D%22288%22%20height%3D%22225%22%20fill%3D%22%2355595c%22%3E%3C%2Frect%3E%3Cg%3E%3Ctext%20x%3D%2296.32500076293945%22%20y%3D%22118.8%22%3EThumbnail%3C%2Ftext%3E%3C%2Fg%3E%3C%2Fg%3E%3C%2Fsvg%3E" // eslint-disable-line max-len
                    title="Image title"
                  /> */}
                  <CardContent>
                    <Typography gutterBottom variant="h5" component="h2">
                      {thing.name}
                    </Typography>
                    <Typography>
                      Thing description to be added.
                    </Typography>
                    <div className="thumb-block">
                      <img id={thing.name} className='thumb' style={{display: thing.isStreaming ? 'none' : 'block'}}
                           src={thing.thumbUrl ? thing.thumbUrl : 'images/no-video.png'}/>
                      <video id={thing.name} style={{display: thing.isStreaming ? 'block' : 'none'}} autoPlay/>
                    </div>
                  </CardContent>
                  <CardActions>
                    <Button size="small" color="primary">
                      <a style={{display: thing.isStreaming ? 'none' : 'inline-block'}} href="#"
                        onClick={() => client.requestThumbs([thing.name])}>refresh</a>
                    </Button>
                    <Button size="small" color="primary">
                      <a onClick={() => {
                        if (!thing.isStreaming) {
                          client.startStreaming(thing.name);
                        } else {
                          client.stopStreaming(thing.name);
                        }
                      }} href="#">{thing.isStreaming ? 'stop' : 'view live'}</a>
                    </Button>
                  </CardActions>
                </Card>
              </Grid>
            })}
          </Grid>




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
