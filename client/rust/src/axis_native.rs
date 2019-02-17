use std::process::{Command, Stdio};

use config::Config;
use error::Error;

// uses capture-jpeg-snapshot c helper to capture the jpeg snapshot from the camera and pipe it to stdout
// this is the easiest way when using static rust executables since linking or dynamic loading of libcapture.so doesn't work in this case
pub fn get_jpeg_snapshot(config: &Config) -> Result<Vec<u8>, Error> {
    let media_props = &config.axis.snapshot_media_props;
    let child = Command::new("./capture-jpeg-snapshot")
        .arg(media_props)
        .stdout(Stdio::piped())
        .spawn()?;
    let output = child
        .wait_with_output()?;
    info!("get_jpeg_snapshot(): {}, {} bytes", media_props, output.stdout.len());
    Ok(output.stdout)
}
