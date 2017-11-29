use hyper;
use hyper_tls;
use hyper::{Method, Request};
use hyper::header::{Authorization, Basic};
use tokio_core::reactor::Core;
use futures::{Future, Stream};

use config::Config;
use error::Error;

// this allows Axis camera control over their HTTP API, VAPIX
// api access requires authentication, currently we rely on cloudcam/cloudcam user being present and given sufficient privileges
// this could be used with both remote camera and running on the camera itself
pub fn get_jpeg_snapshot(config: &Config) -> Result<Vec<u8>, Error> {
    let mut core = Core::new()?;
    let handle = core.handle();
    let client = hyper::Client::configure()
        .connector(hyper_tls::HttpsConnector::new(4, &handle)?)
        .build(&handle);
    let uri = format!("http://{}/axis-cgi/jpg/image.cgi?{}", config.axis.vapix_host.clone().unwrap_or("".to_string()), config.axis.snapshot_media_props).parse()?;
    info!("get_jpeg_snapshot(): {}", uri);
    let mut data = Vec::<u8>::new();
    {
        // todo: set smaller timeouts
        let mut req = Request::new(Method::Get, uri);
        req.headers_mut().set(Authorization(
            Basic {
                username: config.axis.vapix_username.to_owned(),
                password: Some(config.axis.vapix_password.to_owned())
            }
        ));
        let work = client.request(req).and_then(|res| {
            debug!("get_jpeg_snapshot(): response: {}", res.status());
            res.body().for_each(|chunk| {
                data.append(&mut chunk.to_vec());
                Ok(())
            })
        });
        core.run(work)?;
    }
    return Ok(data);
}
