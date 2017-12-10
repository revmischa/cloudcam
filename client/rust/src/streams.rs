use serde_json;
use std::net::SocketAddr;
use std::sync::Mutex;
use dns_lookup::lookup_host;

use config::Config;
use error::Error;
use rtp::RtpDest;

pub fn state_update(_config: &Config, params: &serde_json::Value, rtp_dest: &Mutex<RtpDest>) -> Result<Option<Vec<u8>>, Error> {
    info!("state_update(): {}", params);
    let mut addresses: Option<(SocketAddr, SocketAddr)> = None;
    match params.get("current").unwrap_or(&serde_json::Value::Null).as_str() {
        Some(current) => {
            info!("current stream: {}", current);
            params.get(current).map(|stream| {
                match stream.as_object() {
                    Some(stream) => {
                        let gateway_instance = stream.get("gateway_instance").unwrap_or(&serde_json::Value::Null).as_str().unwrap_or("");
                        let stream_rtp_port = stream.get("stream_rtp_port").unwrap_or(&serde_json::Value::Null).as_u64().unwrap_or(0) as u16;
                        if gateway_instance != "" && stream_rtp_port > 0 {
                            info!("gateway instance: {}:{}", gateway_instance, stream_rtp_port);
                            match lookup_host(gateway_instance) {
                                Ok(ips) => {
                                    match ips.first().cloned() {
                                        Some(ip) => {
                                            addresses = Some((SocketAddr::new(ip, stream_rtp_port), SocketAddr::new(ip, stream_rtp_port + 1)));
                                        }
                                        None => ()
                                    }
                                }
                                Err(e) => {
                                    error!("unable to resolve gateway dns name: {}: {:?}", gateway_instance, e);
                                }
                            }
                        }
                    }
                    None => {
                        error!("invalid stream description");
                    }
                };
            });
        }
        None => {
            info!("no stream");
        }
    };
    info!("rtp dest: {:?}", addresses);
    rtp_dest.lock().unwrap().addresses = addresses;
    return Ok(None);
}
