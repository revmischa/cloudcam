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
          <h2 className='title'>Cameras</h2>
          <Button  
              variant="contained" 
              color="secondary" 
              onClick={() => client.refreshThings()} 
              >refresh
          </Button>
          <br/>
          <br/>
          <div>
            {Object.entries(things).length === 0 && <div>No things</div>}
          </div>

          <Grid container spacing={40}>
          {Object.entries(things).map(function ([key, thing]) {
              return <Grid item key={thing.name} sm={6} md={4} lg={3}>
                <Card>
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
                    <Button
                      size="small"
                      variant="contained" 
                      color="secondary"  
                      style={{display: thing.isStreaming ? 'none' : 'inline-block'}} href="#"
                        onClick={() => client.requestThumbs([thing.name])}>refresh
                    </Button>
                    <Button
                      size="small"
                      variant="contained" 
                      color="secondary" 
                      onClick={() => {
                        if (!thing.isStreaming) {
                          client.startStreaming(thing.name);
                        } else {
                          client.stopStreaming(thing.name);
                        }
                      }} >{thing.isStreaming ? 'stop' : 'view live'}
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
