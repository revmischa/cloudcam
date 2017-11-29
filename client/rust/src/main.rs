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
extern crate tokio_core;
extern crate futures;

use std::time::Duration;
use std::thread;
use rumqtt::{MqttOptions, MqttClient, QoS, MqttCallback, Message};
use std::path::Path;
use std::sync::{Arc, Mutex};
use std::option::Option;
use std::str::from_utf8;

mod error;
mod config;
mod tools;
mod axis_vapix;
mod axis_native;
mod upload_thumb;
mod streams;

use config::Config;

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
            info!("command {} state update: {:?}", command, from_utf8(&*payload));
            mqtt_client.publish(config.mqtt_topics.shadow_update.as_str(), QoS::Level1, payload).unwrap_or_else(|e| {
                error!("error publishing command state update: {}", e);
            });
        });
    });
}

fn handle_shadow_update_delta(config: &Config, mqtt_client: &mut MqttClient, payload: &serde_json::Value) {
    payload.get("state").map(|state| {
        state.as_object().map(|state| {
            for (key, value) in state {
                match key.as_str() {
                    "streams" => {
                        streams::state_update(config, value).unwrap_or_else(|e| {
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
                    info!("shadow update handler {} state update: {:?}", key, from_utf8(&*payload));
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
    // read config.json
    let config = Config::read(Path::new("./config.json")).expect("no or malformed config.json");
    // append the default mqtt port to the endpoint if isn't there
    let iot_endpoint = if config.endpoint.find(':') == None {
        format!("{}:8883", config.endpoint)
    } else {
        config.endpoint.clone()
    };
    info!("iot endpoint: {iot_endpoint}, thing name: {thing_name}, client id: {client_id}",
          iot_endpoint = iot_endpoint, thing_name = config.thing_name, client_id = config.client_id);
    // write tls certs/keys to temporary files for the mqtt client
    let ca_path = Path::new("./cloudcam-ca.pem");
    let cert_path = Path::new("./cloudcam.crt");
    let key_path = Path::new("./cloudcam.key");
    tools::spit(ca_path, &config.ca_pem);
    tools::spit(cert_path, &config.cert_pem);
    tools::spit(key_path, &config.cert_private_key);
    // configure mqtt client
    let client_options = MqttOptions::new()
        .set_keep_alive(5)
        .set_reconnect(3)
        .set_client_id(config.client_id.clone())
        .set_ca(ca_path)
        .set_client_cert(cert_path, key_path)
        .set_broker(iot_endpoint.as_str());
    // setup a mutex-protected reference to optionally present mqtt client so we can pass it to callback before creating the client itself
    let mqtt_client_ref = Arc::new(Mutex::<Option<Box<MqttClient>>>::new(None));
    let msg_callback = {
        let mqtt_client_ref = mqtt_client_ref.clone();
        let config = config.clone();
        // a callback to be called upon receiving any mqtt message
        MqttCallback::new().on_message(move |message: Message| {
            match *mqtt_client_ref.lock().unwrap() {
                Some(ref mut mqtt_client) => {
                    serde_json::from_slice(&*message.payload).map(|json_value: serde_json::Value| {
                        let topic = message.topic.as_str();
                        if topic == config.mqtt_topics.commands {
                            handle_command(&config, mqtt_client, &json_value);
                        } else if topic == config.mqtt_topics.shadow_delta {
                            handle_shadow_update_delta(&config, mqtt_client, &json_value);
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
        *mqtt_client_guard = Some(Box::new(MqttClient::start(client_options, Some(msg_callback)).expect("coudn't start mqtt client")));
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
    // now await the heat death of the universe
    loop {
        thread::sleep(Duration::new(1, 0));
    }
}
