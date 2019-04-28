import * as React from 'react'
import {connect} from 'react-redux'
import {IoTClient} from '../api/iot'
// import Card from '@material-ui/core/Card';
// import CardActions from '@material-ui/core/CardActions';
// import CardContent from '@material-ui/core/CardContent';
// import CardMedia from '@material-ui/core/CardMedia';
// import CssBaseline from '@material-ui/core/CssBaseline';
// import Grid from '@material-ui/core/Grid';
// import Toolbar from '@material-ui/core/Toolbar';
// import Typography from '@material-ui/core/Typography';
// import Button from '@material-ui/core/Button';


interface ICamera {
  name: string
  thumbUrl?: string
  isStreaming: boolean
}

interface ICameraListProps {
  things: ICamera[]
}

class LiveView extends React.Component<ICameraListProps, any> {
  client = new IoTClient()

  constructor(props) {
    super(props);
  }

  public componentDidMount() {
    this.client.refreshThings()
  }

  render() {
    // const client = this.client
    // const {things} = this.props;
    return (
      <div>
        <section className='section content container'>
          <h2 className='title'>Camera # </h2>
          
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

export default connect(mapStateToProps)(LiveView)
