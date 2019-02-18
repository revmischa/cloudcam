use std::fs::File;
use std::io::Read;
use hyper;
use hyper_tls;
use hyper::{Method, Request, Body};
use hyper::header::ContentLength;
use tokio_core::reactor::Core;
use serde_json;

use config::Config;
use error::Error;
use axis_vapix;
use axis_native;

pub fn exec(config: &Config, payload: &serde_json::Value) -> Result<Option<Vec<u8>>, Error> {
    let mut core = Core::new()?;
    let handle = core.handle();
    let client = hyper::Client::configure()
        .connector(hyper_tls::HttpsConnector::new(4, &handle)?)
        .build(&handle);
    // fixme: running on camera leads to s3 tls cert being untrusted, so until this is sorted out via
    // adding needed trust roots via TlsConnector, upload over http instead
    let uri = payload["upload_url"].as_str().unwrap().replace("https", "http").parse()?;
    let mut req: Request<Body> = Request::new(Method::Put, uri);
    // get snapshot via native lib, vapix api or from a local file
    let buf = axis_native::get_jpeg_snapshot(config).unwrap_or(
        axis_vapix::get_jpeg_snapshot(config).unwrap_or_else(|_| {
            let mut buf: Vec<u8> = Vec::new();
            File::open("sample/snowdino.jpg").and_then(|mut f| f.read_to_end(&mut buf)).is_ok();
            buf
    }));
    req.headers_mut().set(ContentLength(buf.len() as u64));
    info!("uploading {} bytes to {:?}", buf.len(), payload["upload_url"].as_str());
    req.set_body(buf);
    let put = client.request(req);
    let response: hyper::Response = core.run(put)?;
    return Ok(Some(format!("{{\"state\":{{\"reported\":{{\"last_uploaded_thumb\":{{\"upload_url\":\"{}\",\"download_url\":\"{}\",\"status\":\"{}\"}}}}}}}}",
                           payload["upload_url"].as_str().unwrap_or(""),
                           payload["download_url"].as_str().unwrap_or(""),
                           if response.status() == hyper::Ok { "success" } else { "failure" }).into_bytes()));
}
