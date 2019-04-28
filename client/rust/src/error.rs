use std::io;
use hyper;
use rumqtt;
use native_tls;

#[derive(Debug)]
pub enum Error {
    Io(io::Error),
    Tls(native_tls::Error),
    Hyper(hyper::Error),
    HyperUri(hyper::error::UriError),
    Mqtt(rumqtt::Error),
}

impl From<io::Error> for Error {
    fn from(error: io::Error) -> Self {
        Error::Io(error)
    }
}

impl From<native_tls::Error> for Error {
    fn from(error: native_tls::Error) -> Self {
        Error::Tls(error)
    }
}

impl From<hyper::Error> for Error {
    fn from(error: hyper::Error) -> Self {
        Error::Hyper(error)
    }
}

impl From<hyper::error::UriError> for Error {
    fn from(error: hyper::error::UriError) -> Self {
        Error::HyperUri(error)
    }
}

impl From<rumqtt::Error> for Error {
    fn from(error: rumqtt::Error) -> Self {
        Error::Mqtt(error)
    }
}