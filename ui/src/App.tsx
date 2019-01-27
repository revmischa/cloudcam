import React, { Component } from 'react';
import './App.css';
import Amplify, { Auth } from 'aws-amplify';
import { withAuthenticator } from 'aws-amplify-react'; // or 'aws-amplify-react-native';
import { BrowserRouter as Router, Route, Link } from "react-router-dom";
import Provision from './pages/Provision'
import Home from './pages/Home'
import {Provider} from 'react-redux'
import store from './store'

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
  logout = () => {
    Auth.signOut()
      .then(data => console.log(data))
      .catch(err => console.log(err));
  }

  render() {
    return (
      <Provider store={store}>
        <Router>
          <div className="App">
            <header className="App-header">
              <button onClick={this.logout}>Log out</button>
              <Link to="/">Home</Link>
              <Link to="/provision">Provision</Link>
            </header>
            
            <Route exact path="/" component={Home} />
            <Route exact path="/provision" component={Provision} />
          </div>
        </Router>
      </Provider>
    );
  }
}

export default withAuthenticator(App, { signUpConfig })
