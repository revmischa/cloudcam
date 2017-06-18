import store from '../store'

var AWS = window.AWS

/**
 * utilities to do sigv4
 * @class SigV4Utils
 */
function SigV4Utils() {
}

SigV4Utils.getSignatureKey = function (key, date, region, service) {
  let kDate = AWS.util.crypto.hmac('AWS4' + key, date, 'buffer');
  let kRegion = AWS.util.crypto.hmac(kDate, region, 'buffer');
  let kService = AWS.util.crypto.hmac(kRegion, service, 'buffer');
  let kCredentials = AWS.util.crypto.hmac(kService, 'aws4_request', 'buffer');
  return kCredentials;
};

SigV4Utils.getSignedUrl = function (host, region, credentials) {
  let datetime = AWS.util.date.iso8601(new Date()).replace(/[:\-]|\.\d{3}/g, '');
  let date = datetime.substr(0, 8);

  let method = 'GET';
  let protocol = 'wss';
  let uri = '/mqtt';
  let service = 'iotdevicegateway';
  let algorithm = 'AWS4-HMAC-SHA256';

  let credentialScope = date + '/' + region + '/' + service + '/' + 'aws4_request';
  let canonicalQuerystring = 'X-Amz-Algorithm=' + algorithm;
  canonicalQuerystring += '&X-Amz-Credential=' + encodeURIComponent(credentials.accessKeyId + '/' + credentialScope);
  canonicalQuerystring += '&X-Amz-Date=' + datetime;
  canonicalQuerystring += '&X-Amz-SignedHeaders=host';

  let canonicalHeaders = 'host:' + host + '\n';
  let payloadHash = AWS.util.crypto.sha256('', 'hex')
  let canonicalRequest = method + '\n' + uri + '\n' + canonicalQuerystring + '\n' + canonicalHeaders + '\nhost\n' + payloadHash;

  let stringToSign = algorithm + '\n' + datetime + '\n' + credentialScope + '\n' + AWS.util.crypto.sha256(canonicalRequest, 'hex');
  let signingKey = SigV4Utils.getSignatureKey(credentials.secretAccessKey, date, region, service);
  let signature = AWS.util.crypto.hmac(signingKey, stringToSign, 'hex');

  canonicalQuerystring += '&X-Amz-Signature=' + signature;
  if (credentials.sessionToken) {
    canonicalQuerystring += '&X-Amz-Security-Token=' + encodeURIComponent(credentials.sessionToken);
  }

  let requestUrl = protocol + '://' + host + uri + '?' + canonicalQuerystring;
  return requestUrl;
};

var mqttClient = null;

export function isMqttConnected() {
  return mqttClient.isConnected();
}

export function mqttConnect() {
  return new Promise((resolve, reject) => {
    let host = process.env.AWS_IOT_ENDPOINT;
    let requestUrl = SigV4Utils.getSignedUrl(host, process.env.AWS_REGION, AWS.config.credentials);
    console.log("MQTT websocket url: " + requestUrl)
    const clientId = "wss-client-" + Math.random().toString(36).substring(7)
    mqttClient = new Paho.MQTT.Client(requestUrl, clientId);
    mqttClient.onConnectionLost = function (responseObject) {
      if (responseObject.errorCode !== 0) {
        console.log("MQTT onConnectionLost:" + responseObject.errorMessage);
        setTimeout(function () {
          mqttConnect(clientId)
        }, 1000);
      }
    };
    mqttClient.onMessageArrived = function (message) {
      console.log("MQTT message to " + message.destinationName);
      let payload = JSON.parse(message.payloadString);
      console.log(payload);
      let thingName = message.destinationName.match(/\$aws\/things\/([^\\]+)\/shadow\/update\/accepted/);
      if (thingName && thingName[1]) {
        shadowUpdateHandler(thingName[1], payload)
      }
    };
    let connectOptions = {
      onSuccess: function () {
        // connect succeeded
        console.log("mqtt connection succeeded");
        resolve();
      },
      useSSL: true,
      timeout: 15,
      mqttVersion: 4,
      onFailure: function (err) {
        console.log("mqtt connection failed: " + JSON.stringify(err))
        reject(err);
      }
    };
    mqttClient.connect(connectOptions);
  });
}

export function requestThumbsProcess() {
  clearTimeout(requestThumbsProcess);
  requestThumbs(Object.keys(store.getState().iot.things));
  setTimeout(requestThumbsProcess, 10000);
}

export function subscribeToShadowUpdates(thingName) {
  return new Promise((resolve, reject) => {
    mqttClient.subscribe("$aws/things/" + thingName + "/shadow/update/accepted", {
      onSuccess: function () {
        console.log("mqtt subscription to " + thingName + " shadow updates succeeded");
        resolve();
      },
      onFailure: function (err) {
        console.log("mqtt subscription to " + thingName + " shadow updates failed: " + JSON.stringify(err))
        reject(err);
      }
    });
  });
}

function shadowUpdateHandler(thingName, doc) {
  if (doc.state.reported &&
    doc.state.reported.thumb_upload &&
    doc.state.reported.thumb_upload.status === "success") {
    console.log("new " + thingName + " thumb available at " + doc.state.reported.thumb_upload.download_url);
    store.dispatch({
      type: 'iot/thumb', thingName: thingName, thumbUrl: doc.state.reported.thumb_upload.download_url
    })
  }
}

// doc format:
// {
//   "state": {
//     "desired": {
//       "key": <value>
//     }
//   }
// }

function updateShadow(thingName, doc) {
  let message = new Paho.MQTT.Message(JSON.stringify(doc));
  message.destinationName = "$aws/things/" + thingName + "/shadow/update";
  mqttClient.send(message);
}

export function attachThingPolicy(thingName) {
  return new Promise((resolve, reject) => {
    console.log('invoking iot_attach_thing_policy:');
    var lambda = new window.AWS.Lambda();
    lambda.invoke({
      FunctionName: "iot_attach_thing_policy",
      Payload: JSON.stringify({
        thingName: thingName
      })
    }, function (err, data) {
      console.log('iot_attach_thing_policy result:');
      if (err) {
        console.log(err);
        reject(err);
      }
      else {
        console.log(data);
        resolve(data);
      }
    });
  });
}

export function provisionThing(thingName) {
  return new Promise((resolve, reject) => {
    console.log('invoking iot_provision_thing:');
    var lambda = new window.AWS.Lambda();
    lambda.invoke({
      FunctionName: "iot_provision_thing",
      Payload: JSON.stringify({
        thingName: thingName
      })
    }, function (err, data) {
      console.log('iot_provision_thing result:');
      if (err) {
        console.log(err);
        reject(err);
      }
      else {
        console.log(data);
        refreshThings();
        resolve(JSON.parse(data.Payload));
      }
    });
  });
}

export function refreshThings() {
  return new Promise((resolve, reject) => {
    console.log('invoking iot_list_things:');
    var lambda = new window.AWS.Lambda();
    lambda.invoke({
      FunctionName: "iot_list_things",
      Payload: ""
    }, function (err, data) {
      if (err) {
        console.log(err);
        reject(err);
      }
      else {
        console.log(data);
        let result = JSON.parse(data.Payload);
        store.dispatch({
          type: 'iot/things', thingNames: result.thingNames
        });
        requestThumbsProcess();
        var subscribeFn = () => {
          result.thingNames.map(function (name) {
            subscribeToShadowUpdates(name)
              .catch(e => {
                console.log(e);
              });
          });
        };
        if (!isMqttConnected()) {
          mqttConnect().then(() => {
            subscribeFn();
          });
        }
        else {
          subscribeFn()
        }
        resolve(data);
      }
    });
  });
}

export function requestThumbs(thingNames) {
  return new Promise((resolve, reject) => {
    console.log('invoking request_thumb: ' + thingNames);
    let params = {
      FunctionName: "iot_request_thumb",
      Payload: JSON.stringify({
        thingNames: thingNames
      })
    };
    let lambda = new window.AWS.Lambda();
    lambda.invoke(params, function (err, data) {
      if (err) {
        console.log(err);
        reject(err);
      }
      else {
        console.log(data);
        resolve(data);
      }
    })
  })
}

export function reducer(state = {things: {}}, action) {
  let newState;
  switch (action.type) {
    case 'iot/things':
      newState = JSON.parse(JSON.stringify(state));
      newState.things = action.thingNames.reduce((o, name) => {
        o[name] = Object.assign({name: name}, state.things[name]);
        return o
      }, {});
      console.log("iot/things new state: " + JSON.stringify(newState));
      return newState;
    case 'iot/thumb':
      newState = JSON.parse(JSON.stringify(state));
      newState.things[action.thingName] = Object.assign({}, newState.things[action.thingName], {thumbUrl: action.thumbUrl});
      console.log("iot/thumb new state: " + JSON.stringify(newState));
      return newState;
    case 'iot/provision':
      newState = JSON.parse(JSON.stringify(state));
      newState.thingConfig = action.thingConfig;
      console.log("iot/provision new state: " + JSON.stringify(newState));
      return newState;
    default:
      return state
  }
}
