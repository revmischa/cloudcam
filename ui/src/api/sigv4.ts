import moment from 'moment'
import CryptoJS from 'crypto-js'

// AWS URL signing utils
function SigV4Utils() {}

SigV4Utils.sign = function(key, msg) {
  var hash = CryptoJS.HmacSHA256(msg, key)
  return hash.toString(CryptoJS.enc.Hex)
}

SigV4Utils.sha256 = function(msg) {
  var hash = CryptoJS.SHA256(msg)
  return hash.toString(CryptoJS.enc.Hex)
}

SigV4Utils.getSignatureKey = function(key, date, region, service) {
  var kDate = CryptoJS.HmacSHA256(date, 'AWS4' + key)
  var kRegion = CryptoJS.HmacSHA256(region, kDate)
  var kService = CryptoJS.HmacSHA256(service, kRegion)
  var kSigning = CryptoJS.HmacSHA256('aws4_request', kService)
  return kSigning
}

SigV4Utils.getSignedUrl = function(host, region, credentials) {
  var time = moment.utc()
  var dateStamp = time.format('YYYYMMDD')
  var amzdate = dateStamp + 'T' + time.format('HHmmss') + 'Z'

  let method = 'GET'
  let protocol = 'wss'
  let uri = '/mqtt'
  let service = 'iotdevicegateway'
  let algorithm = 'AWS4-HMAC-SHA256'

  let credentialScope = dateStamp + '/' + region + '/' + service + '/aws4_request'
  let canonicalQuerystring = 'X-Amz-Algorithm=' + algorithm
  canonicalQuerystring += '&X-Amz-Credential=' + encodeURIComponent(credentials.accessKeyId + '/' + credentialScope)
  canonicalQuerystring += '&X-Amz-Date=' + amzdate
  canonicalQuerystring += '&X-Amz-SignedHeaders=host'

  let canonicalHeaders = 'host:' + host + '\n'
  let payloadHash = SigV4Utils.sha256('')
  let canonicalRequest =
    method + '\n' + uri + '\n' + canonicalQuerystring + '\n' + canonicalHeaders + '\nhost\n' + payloadHash

  let stringToSign =
    algorithm + '\n' + amzdate + '\n' + credentialScope + '\n' + CryptoJS.SHA256(canonicalRequest, 'hex')
  let signingKey = SigV4Utils.getSignatureKey(credentials.secretAccessKey, dateStamp, region, service)
  let signature = SigV4Utils.sign(signingKey, stringToSign)

  canonicalQuerystring += '&X-Amz-Signature=' + signature
  if (credentials.sessionToken) {
    canonicalQuerystring += '&X-Amz-Security-Token=' + encodeURIComponent(credentials.sessionToken)
  }

  return protocol + '://' + host + uri + '?' + canonicalQuerystring
}

export default SigV4Utils
