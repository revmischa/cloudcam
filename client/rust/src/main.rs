extern crate rumqtt;
#[macro_use]
extern crate log;
extern crate env_logger;
extern crate serde;
extern crate serde_json;
#[macro_use]
extern crate serde_derive;
extern crate native_tls;
extern crate hyper;
extern crate hyper_tls;
extern crate futures;
extern crate tokio_io;
extern crate tokio_core;
extern crate tokio_proto;
extern crate tokio_service;
extern crate tokio_timer;
#[macro_use]
extern crate nom;
extern crate bytes;
extern crate dns_lookup;

use rumqtt::{MqttOptions, MqttClient, QoS, MqttCallback, Message};
use std::env;
use std::thread;
use std::time::Duration;
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::option::Option;
use std::str::from_utf8;
use std::net::{IpAddr, Ipv4Addr};

mod error;
mod config;
mod tools;
mod axis_vapix;
mod axis_native;
mod upload_thumb;
mod streams;
mod rtsp;
mod rtp;

use config::Config;
use rtp::RtpDest;

fn handle_command(config: &Config, mqtt_client: &mut MqttClient, payload: &serde_json::Value) {
    payload.get("command").map(|command| {
        match command.as_str() {
            Some("upload_thumb") => {
                upload_thumb::exec(config, payload).unwrap_or_else(|e| {
                    error!("upload_thumb::exec() error: {:?}", e);
                    None
                })
            }
            Some(command) => {
                error!("unknown command: {:?}", command);
                None
            }
            None => {
                error!("no command specified");
                None
            }
        }.map(|payload| {
            // if the handler above returned a shadow update document, publish it to the appropriate topic
            info!("command {} state update: {:?}", command, from_utf8(payload.as_ref()));
            mqtt_client.publish(config.mqtt_topics.shadow_update.as_str(), QoS::Level1, payload).unwrap_or_else(|e| {
                error!("error publishing command state update: {}", e);
            });
        });
    });
}

fn handle_shadow_update_delta(config: &Config, mqtt_client: &mut MqttClient, rtp_dest: &Mutex<RtpDest>, payload: &serde_json::Value) {
    payload.get("state").map(|shadow_state| {
        shadow_state.as_object().map(|shadow_state| {
            for (key, value) in shadow_state {
                match key.as_str() {
                    "streams" => {
                        streams::state_update(config, value, rtp_dest).unwrap_or_else(|e| {
                            error!("streams::state_update() error: {:?}", e);
                            None
                        })
                    }
                    otherwise => {
                        error!("unrecognized shadow update key: {}", otherwise);
                        None
                    }
                }.map(|payload| {
                    // if the handler above returned a shadow update document, publish it to the appropriate topic
                    info!("shadow update handler {} state update: {:?}", key, from_utf8(payload.as_ref()));
                    mqtt_client.publish(config.mqtt_topics.shadow_update.as_str(), QoS::Level1, payload).unwrap_or_else(|e| {
                        error!("error publishing shadpw update handler state update: {}", e);
                    });
                });
            }
        });
    });
}

fn main() {
    env_logger::init().unwrap();
    // global state which could be updated by commands/shadow delta handlers
    let rtp_dest = Arc::new(Mutex::<RtpDest>::new(RtpDest { addresses: None }));
    // read config.json
    let exe_path = env::current_exe().unwrap();
    let config_dir = exe_path.parent().unwrap();
    let config = Config::read(&config_dir.join(Path::new("config.json"))).expect("no or malformed config.json");
    info!("iot endpoint: {endpoint}, thing name: {thing_name}, client id: {client_id}",
          endpoint = config.mqtt_endpoint, thing_name = config.thing_name, client_id = config.mqtt_client_id);
    // write tls certs/keys to temporary files for the mqtt client
    let ca_path = Path::new("./cloudcam-ca.pem");
    let cert_path = Path::new("./cloudcam.crt");
    let key_path = Path::new("./cloudcam.key");
    tools::spit(ca_path, &config.ca_pem);
    tools::spit(cert_path, &config.cert_pem);
    tools::spit(key_path, &config.cert_private_key);
    // configure mqtt client
    let client_options = MqttOptions::new()
        .set_client_id(config.mqtt_client_id.clone())
        .set_clean_session(true) // AWS IoT broker only supports clean sessions
        .set_keep_alive(5)
        .set_should_verify_ca(!config.mqtt_endpoint.starts_with("127.0.0.1")) // don't verify certs when talking to localhost mqtt broker (e.g. testing)
        .set_reconnect(3) // seconds between reconnect attempts
        .set_ca(ca_path)
        .set_client_cert(cert_path, key_path)
        .set_broker(config.mqtt_endpoint.as_str());
    // reference to optionally present mqtt client so we can pass it to callback before creating the client itself
    let mqtt_client_ref = Arc::new(Mutex::<Option<MqttClient>>::new(None));
    let msg_callback = {
        let mqtt_client_ref = mqtt_client_ref.clone();
        let config = config.clone();
        let rtp_dest = rtp_dest.clone();
        // a callback to be called upon receiving any mqtt message
        MqttCallback::new().on_message(move |message: Message| {
            match *mqtt_client_ref.lock().unwrap() {
                Some(ref mut mqtt_client) => {
                    serde_json::from_slice(message.payload.as_ref()).map(|json_value: serde_json::Value| {
                        let topic = message.topic.as_str();
                        if topic == config.mqtt_topics.commands {
                            handle_command(&config, mqtt_client, &json_value);
                        } else if topic == config.mqtt_topics.shadow_delta {
                            handle_shadow_update_delta(&config, mqtt_client, &rtp_dest, &json_value);
                        } else {
                            error!("unrecognized message: topic: {:?}, payload: {:?}", message.topic, json_value.to_string());
                        };
                    }).unwrap_or_else(|e| {
                        error!("mqtt message handling error: {}", e);
                    });
                }
                None => panic!("no mqtt client")
            };
        })
    };
    // start the mqtt client, put it into the mutex-protected reference (see above) and subscribe to the topics we want
    {
        let mut mqtt_client_guard = mqtt_client_ref.lock().unwrap();
        *mqtt_client_guard = Some(MqttClient::start(client_options, Some(msg_callback)).expect("coudn't start mqtt client"));
        match *mqtt_client_guard {
            Some(ref mut mqtt_client) => {
                let topics = vec![(config.mqtt_topics.commands.as_str(), QoS::Level1),
                                  (config.mqtt_topics.shadow_delta.as_str(), QoS::Level1)];
                info!("subscribing to {:?}", topics);
                mqtt_client.subscribe(topics).expect("subcription failure");
            }
            None => panic!("no mqtt client")
        };
    }

    // if rtsp is configured, run rtsp client and rtp reflector
    match config.rtsp.uri {
        Some(rtsp_uri) => {
            // run rtp reflector
            let rtp_recv_ports = rtp::find_port_pair((40000..50000)).unwrap();
            std::thread::spawn(move || {
                // todo: listen on 127.0.0.1 instead of 0.0.0.0 if RTSP session is local-only (e.g. running on camera)
                rtp::run_reflector(IpAddr::V4(Ipv4Addr::new(0, 0, 0, 0)), rtp_recv_ports, &rtp_dest);
            });
            // this blocks forever running RTSP keepalives
            match rtsp::run_client(rtsp_uri.as_str(), rtp_recv_ports) {
                Ok(()) => (),
                Err(e) => error!("rtsp::run_client error: {}", e)
            }
        }
        None => ()
    }

    // now await the heat death of the universe
    loop {
        // todo: implement a watchdog timer
        thread::sleep(Duration::new(1, 0));
    }
}
