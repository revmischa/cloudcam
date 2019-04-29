import React from 'react'
import ReactDOM from 'react-dom'
import App from './App'
import * as serviceWorker from './serviceWorker'
import * as dotenv from 'dotenv'
import AWS from 'aws-sdk'
import * as CCStack from './aws-stack.json'
import Amplify from 'aws-amplify'
import './index.css'

dotenv.config()

AWS.config.region = CCStack.Region
Amplify.configure({
  Auth: {
    region: CCStack.Region,
    userPoolId: CCStack.UserPoolId,
    userPoolWebClientId: CCStack.UserPoolClientName,
    identityPoolId: CCStack.IdentityPoolId,
  },
})

ReactDOM.render(<App />, document.getElementById('root'))

// If you want your app to work offline and load faster, you can change
// unregister() to register() below. Note this comes with some pitfalls.
// Learn more about service workers: http://bit.ly/CRA-PWA
serviceWorker.unregister()
