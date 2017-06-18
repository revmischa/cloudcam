import React from 'react'
import ReactDOM from 'react-dom'
import {Router, browserHistory} from 'react-router'
import {Provider} from 'react-redux'

import App from './pages/App'
import Home from './pages/Home'
import Register from './pages/Register'
import Login from './pages/Login'
import Provision from './pages/Provision'
import PageNotFound from './pages/PageNotFound'

import store from './store'

// this keeps HMR happy (instead of using JSX)
const routes = {
  path: '/',
  component: App,
  indexRoute: {
    component: Home
  },
  childRoutes: [
    Register,
    Login,
    {
      route: {
        path: 'provision',
        component: Provision
      }
    },
    PageNotFound
  ].map(r => r.route)
}

const Routing = () => (
  <Provider store={store}>
    <Router history={browserHistory} routes={routes}/>
  </Provider>
)

// I dunno if I really need this...
if (module.hot) {
  module.hot.accept()
}

ReactDOM.render(<Routing />, document.getElementById('root'))
