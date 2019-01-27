import React, { Component } from 'react';
import './App.css';
import { withAuthenticator } from 'aws-amplify-react'; // or 'aws-amplify-react-native';
import { BrowserRouter as Router, Route, Link } from "react-router-dom";
import Provision from './pages/Provision'
import Home from './pages/Home'
import {Provider} from 'react-redux'
import store from './store'
import DefaultLayout from './layout'

const signUpConfig = {
  signUpFields: [
    {
      label: "Email",
      key: "username",
      required: true,
      type: 'email',
      displayOrder: 1,
    },
    {
      label: "Password",
      key: "password",
      required: true,
      type: 'password',
      displayOrder: 2,
    },
  ],
  hiddenDefaults: ['email', 'phone_number']
}

class App extends Component {
  render() {
    return (
      <Provider store={store}>
        <Router>
          <DefaultLayout>
            
            <Route exact path="/" component={Home} />
            <Route exact path="/provision" component={Provision} />
          </DefaultLayout>
        </Router>
      </Provider>
    );
  }
}

export default withAuthenticator(App, { signUpConfig })
