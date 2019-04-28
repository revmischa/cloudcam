use std::fs::File;
use std::io::Write;
use std::path::Path;

pub fn spit(path: &Path, contents: &String) {
    let mut f = File::create(&path).expect("could not create file");
    f.write_all(contents.as_bytes()).expect("could not write to file");
}

