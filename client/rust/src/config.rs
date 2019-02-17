use std::fs::File;
use std::path::Path;
use serde_json;
use serde_json::Error;

fn default_vapix_username() -> String { "cloudcam".to_string() }

fn default_vapix_password() -> String { "cloudcam".to_string() }

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct AxisConfig {
    #[serde(default)] pub vapix_host: Option<String>,
    #[serde(default = "default_vapix_username")] pub vapix_username: String,
    #[serde(default = "default_vapix_password")] pub vapix_password: String,
    #[serde(default)] pub snapshot_media_props: String
}

impl Default for AxisConfig {
    fn default() -> AxisConfig {
        AxisConfig {
            vapix_host: None,
            vapix_username: default_vapix_username(),
            vapix_password: default_vapix_password(),
            snapshot_media_props: "".to_string()
        }
    }
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct RtspConfig {
    #[serde(default)] pub uri: Option<String>,
}

impl Default for RtspConfig {
    fn default() -> RtspConfig {
        RtspConfig {
            uri: None
        }
    }
}

#[derive(Debug, Clone)]
pub struct MqttTopicsConfig {
    pub commands: String,
    pub shadow_delta: String,
    pub shadow_update: String
}

impl Default for MqttTopicsConfig {
    fn default() -> MqttTopicsConfig {
        MqttTopicsConfig {
            commands: "".to_string(),
            shadow_delta: "".to_string(),
            shadow_update: "".to_string()
        }
    }
}

impl MqttTopicsConfig {
    pub fn new(thing_name: &String) -> MqttTopicsConfig {
        MqttTopicsConfig {
            commands: format!("cloudcam/{}/commands", thing_name),
            shadow_delta: format!("$aws/things/{}/shadow/update/delta", thing_name),
            shadow_update: format!("$aws/things/{}/shadow/update", thing_name)
        }
    }
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Config {
    #[serde(rename = "thingName")] pub thing_name: String,
    #[serde(rename = "thingTypeName")] pub thing_type_name: String,
    #[serde(rename = "clientId")] pub mqtt_client_id: String,
    #[serde(rename = "endpoint")] pub mqtt_endpoint: String,
    #[serde(rename = "caPem")] pub ca_pem: String,
    #[serde(rename = "certificatePem")] pub cert_pem: String,
    #[serde(rename = "certificatePrivateKey")] pub cert_private_key: String,

    #[serde(default)] pub axis: AxisConfig,
    #[serde(default)] pub rtsp: RtspConfig,

    #[serde(skip)] pub mqtt_topics: MqttTopicsConfig
}

impl Config {
    pub fn read(path: &Path) -> Result<Config, Error> {
        let f = File::open(path).expect("file not found");
        let mut config: Config = serde_json::from_reader(f).unwrap();
        // assign mqtt topic names based on supplied thing name
        config.mqtt_topics = MqttTopicsConfig::new(&config.thing_name);
        // append the default mqtt port to the endpoint if isn't there
        if config.mqtt_endpoint.find(':') == None {
            config.mqtt_endpoint = format!("{}:8883", config.mqtt_endpoint)
        };
        return Ok(config);
    }
}

