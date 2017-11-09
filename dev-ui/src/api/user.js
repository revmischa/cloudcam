import store from '../store'
import {mqttConnect, attachThingPolicy, provisionThing, refreshThings, requestThumbsProcess} from '../api/iot'

var AWS = window.AWS

const {
  CognitoUser,
  CognitoUserPool,
  CognitoUserAttribute
} = AWS.CognitoIdentityServiceProvider

export let cognitoUser = null

AWS.config.region = process.env.AWS_REGION

const userPool = new CognitoUserPool({
  UserPoolId: process.env.AWS_USERPOOL.split("/")[1],
  ClientId: process.env.AWS_CLIENTAPP
})

// register a new user
export function register(userData) {
  const attributeList = []
  const {username, password, ...user} = userData
  for (let field in user) {
    attributeList.push(new CognitoUserAttribute({Name: field, Value: user[field]}))
  }
  if (cognitoUser != null) {
    cognitoUser.signOut();
    cognitoUser = null
    store.dispatch({type: 'user/user', user: cognitoUser})
    AWS.config.update({credentials: null, sessionToken: null});
  }
  return new Promise((resolve, reject) => {
    userPool.signUp(username, password, attributeList, null, (err, result) => {
      if (err) return reject(err)
      store.dispatch({type: 'user/user', user: cognitoUser})
      resolve(result.user)
    })
  })
}

// log user out
export function logout() {
  cognitoUser.signOut()
  cognitoUser = null
  store.dispatch({type: 'user/user', user: cognitoUser})
  AWS.config.update({credentials: null, sessionToken: null});
}

// authenticate user, and also ask for MFA or verification code, if needed
export function login(form) {
  var Username = form.username;
  var Password = form.password;
  return new Promise((resolve, reject) => {
    var authenticationData = {
      Username: Username,
      Password: Password,
    };
    var authenticationDetails = new AWS.CognitoIdentityServiceProvider.AuthenticationDetails(authenticationData);
    var userData = {
      Username: Username,
      Pool: userPool
    };
    cognitoUser = new CognitoUser(userData)
    cognitoUser.authenticateUser(authenticationDetails, {
      onSuccess: function (result) {
        var params = {
          IdentityPoolId: process.env.AWS_IDENTITYPOOL,
          Logins: {}
        };
        params.Logins[process.env.AWS_USERPOOL] = result.getIdToken().getJwtToken()
        console.log('AWS CognitoIdentityCredentials params:' + JSON.stringify(params));
        var credentials = new AWS.CognitoIdentityCredentials(params);
        credentials.clearCachedId();
        credentials = new AWS.CognitoIdentityCredentials(params);
        AWS.config.update({credentials: credentials});
        AWS.config.credentials.get(function (err) {
          console.log('AWS credentials:');
          if (err) {
            console.log(err);
            reject(err);
          }
          else {
            store.dispatch({type: 'user/user', user: cognitoUser.username})
            mqttConnect()
              .then(() => {
                refreshThings()
                  .then(() => {
                    resolve();
                  })
                  .catch(e => {
                    console.log(e);
                    resolve();
                  })
              })
              .catch(e => {
                console.log(e);
                resolve();
              });
          }
        });
      },
      onFailure: reject
    })
  })
}

// allow user to reset password
export function reset() {
  console.log('Not implemented.')
}

export function reducer(state = {user: false}, action) {
  let newState
  switch (action.type) {
    // trigger when user is changed
    case 'user/user':
      newState = Object.assign({}, state)
      newState.user = action.user
      return newState
    default:
      return state
  }
}
