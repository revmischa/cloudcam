import React, { Component } from 'react'
import { withAuthenticator } from 'aws-amplify-react' // or 'aws-amplify-react-native';
import { BrowserRouter as Router, Route } from 'react-router-dom'
import Provision from './pages/provision'
import CameraList from './pages/cameraList'
import { Provider } from 'react-redux'
import store from './store'
import DefaultLayout from './layout'
import { MuiThemeProvider } from '@material-ui/core/styles'
import theme from './theme'
import liveView from './pages/liveView'

const signUpConfig = {
  signUpFields: [
    {
      label: 'Email',
      key: 'username',
      required: true,
      type: 'email',
      displayOrder: 1,
    },
    {
      label: 'Password',
      key: 'password',
      required: true,
      type: 'password',
      displayOrder: 2,
    },
  ],
  hiddenDefaults: ['email', 'phone_number'],
}

class App extends Component {
  render() {
    return (
      <MuiThemeProvider theme={theme}>
        <Provider store={store}>
          <Router>
            <DefaultLayout>
              <Route exact path="/" component={CameraList} />
              <Route exact path="/provision" component={Provision} />
              <Route exact path="/cameras/:thingName/live" component={liveView} />
            </DefaultLayout>
          </Router>
        </Provider>
      </MuiThemeProvider>
    )
  }
}

export default withAuthenticator(App, { signUpConfig })
