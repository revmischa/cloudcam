# Rust Cloudcam client

## Building

A recent stable Rust toolchain is recommended (install via [https://www.rustup.rs](https://www.rustup.rs))

For running the client, a valid `config.json` provisioned via the cloud UI must be present in the current directory

### Host

`source ~/.cargo/env`

`cargo build --release` or `RUST_LOG=cloudcam=info,rumqtt=info cargo run` 

To connect to remote AXIS camera via VAPIX HTTP API, add this: 
```
  "axis": {
    "vapix_host": "<camera ip>",
    "vapix_username": "<camera user name>",
    "vapix_password": "<camera user password>"
  },
  "rtsp": {
    "uri": "rtsp://<camera ip>/axis-media/media.amp"
  }  
```
to the `config.json`

### Axis cameras

1. Install appropriate AXIS ACAP SDKs (v1.4 or v2.0 or both) to `axis/emb-app-sdk_<version>` (or somewhere else, adjusting paths in `client/rust/axis/build-packages.sh`)
2. Build Rust binary and AXIS application packages for a selected platform (`mipsisa32r2el` or `armv6`) via `(cd client/rust/axis && build-packages.sh [ mipsisa32r2el | armv6 ])`
3. Install and run the application via `eap-install.sh install` and `eap-install.sh start` (`eap-install.sh` is a part of ACAP SDK) or the camera's web UI 
 
