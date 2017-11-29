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
  }
```
to the `config.json`

### Axis cameras

1. Cross-compile a static Rust binary: `docker run --rm -it -v "$(pwd)":/home/rust/src messense/rust-musl-cross:$TAG cargo build --release`
Where $TAG is `arm-musleabi`, `armv7-musleabihf` or `mipsel-musl` depending on your camera's CPU
2. Install appropriate AXIS ACAP SDK (v1.4 or v2.0, latter might require some `package.conf` editing) to $AXIS_SDK_ROOT and run `(cd $AXIS_SDK_ROOT && source init_env)`
3. Create AXIS application packages via `(cd axis && create-package.sh)`
4. Install and run the application via `eap-install.sh install` and `eap-install.sh start` or the camera's web UI 
 
