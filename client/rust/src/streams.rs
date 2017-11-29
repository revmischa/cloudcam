use serde_json;

use config::Config;
use error::Error;

pub fn state_update(_config: &Config, params: &serde_json::Value) -> Result<Option<Vec<u8>>, Error> {
    info!("state_update(): {}", params);
    // todo: setup streaming based on parameters passed from the janus_start_stream lambda via _params
    return Ok(None);
}
