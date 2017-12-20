use futures::{Future, Stream};
use tokio_io::{AsyncRead, AsyncWrite};
use tokio_io::codec::{Framed, Encoder, Decoder};
use tokio_core::net::TcpStream;
use tokio_core::reactor::{Handle, Core};
use tokio_proto::TcpClient;
use tokio_proto::pipeline::{ClientProto, ClientService};
use tokio_service::Service;
use tokio_timer::{Timer, TimerError};
use hyper::Uri;
use std::time::Duration;
use bytes::BytesMut;
use std::{io, str};
use std::string::String;
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use nom::{digit, space, IResult};
use std::sync::atomic::{AtomicUsize, Ordering};
use dns_lookup::lookup_host;

#[derive(Debug)]
pub enum Header {
    ContentLength {
        value: u32
    },
    Generic {
        name: String,
        value: String,
    },
}

#[derive(Debug)]
pub struct Request {
    method: String,
    uri: String,
    version: (u32, u32),
    headers: Vec<Header>,
    body: Vec<u8>,
}

impl Request {
    pub fn options(uri: &str, c_seq: usize) -> Request {
        Request {
            method: "OPTIONS".to_string(),
            uri: uri.to_string(),
            version: (1, 0),
            headers: vec![
                Header::Generic { name: "CSeq".to_string(), value: c_seq.to_string() }
            ],
            body: vec![],
        }
    }

    pub fn describe(uri: &str, c_seq: usize) -> Request {
        Request {
            method: "DESCRIBE".to_string(),
            uri: uri.to_string(),
            version: (1, 0),
            headers: vec![
                Header::Generic { name: "CSeq".to_string(), value: c_seq.to_string() }
            ],
            body: vec![],
        }
    }

    pub fn setup(uri: &str, c_seq: usize, transport: &str) -> Request {
        Request {
            method: "SETUP".to_string(),
            uri: uri.to_string(),
            version: (1, 0),
            headers: vec![
                Header::Generic { name: "CSeq".to_string(), value: c_seq.to_string() },
                Header::Generic { name: "Transport".to_string(), value: transport.to_string() }
            ],
            body: vec![],
        }
    }

    pub fn play(uri: &str, c_seq: usize, session: String) -> Request {
        Request {
            method: "PLAY".to_string(),
            uri: uri.to_string(),
            version: (1, 0),
            headers: vec![
                Header::Generic { name: "CSeq".to_string(), value: c_seq.to_string() },
                Header::Generic { name: "Session".to_string(), value: session },
                Header::Generic { name: "Range".to_string(), value: "npt=0.000-".to_string() }
            ],
            body: vec![],
        }
    }

    pub fn pause(uri: &str, c_seq: usize, session: &str) -> Request {
        Request {
            method: "PAUSE".to_string(),
            uri: uri.to_string(),
            version: (1, 0),
            headers: vec![
                Header::Generic { name: "CSeq".to_string(), value: c_seq.to_string() },
                Header::Generic { name: "Session".to_string(), value: session.to_string() }
            ],
            body: vec![],
        }
    }

    pub fn record(uri: &str, c_seq: usize, session: &str) -> Request {
        Request {
            method: "RECORD".to_string(),
            uri: uri.to_string(),
            version: (1, 0),
            headers: vec![
                Header::Generic { name: "CSeq".to_string(), value: c_seq.to_string() },
                Header::Generic { name: "Session".to_string(), value: session.to_string() }
            ],
            body: vec![],
        }
    }

    pub fn teardown(uri: &str, c_seq: usize, session: &str) -> Request {
        Request {
            method: "TEARDOWN".to_string(),
            uri: uri.to_string(),
            version: (1, 0),
            headers: vec![
                Header::Generic { name: "CSeq".to_string(), value: c_seq.to_string() },
                Header::Generic { name: "Session".to_string(), value: session.to_string() }
            ],
            body: vec![],
        }
    }

    pub fn get_parameter(uri: &str, c_seq: usize, session: String) -> Request {
        Request {
            method: "GET_PARAMETER".to_string(),
            uri: uri.to_string(),
            version: (1, 0),
            headers: vec![
                Header::Generic { name: "CSeq".to_string(), value: c_seq.to_string() },
                Header::Generic { name: "Session".to_string(), value: session }
            ],
            body: vec![],
        }
    }
}

#[derive(Debug)]
pub struct Response {
    version: (u32, u32),
    status_code: u32,
    status: String,
    headers: Vec<Header>,
    body: Vec<u8>,
}

impl Response {
    fn session_raw(&self) -> String {
        for header in &self.headers {
            match header {
                &Header::ContentLength { ref value } => (),
                &Header::Generic { ref name, ref value } => {
                    if name == "Session" {
                        return value.clone();
                    }
                }
            }
        }
        return "".to_string();
    }

    pub fn session(&self) -> String {
        let r = self.session_raw();
        let v: Vec<&str> = r.split(|c| c == ';' || c == ' ').collect();
        return v.get(0).unwrap_or(&"").to_string();
    }

    pub fn session_timeout(&self) -> u64 {
        let r = self.session_raw();
        let v: Vec<&str> = r.split(|c| c == ';' || c == ' ').collect();
        return v.get(1).unwrap_or(&"").parse().unwrap_or(60);
    }
}

named!(number<u32>, map_res!(
    map_res!(digit, str::from_utf8),
    str::parse
));

named!(version<(u32, u32)>, do_parse!(tag!("RTSP/") >> major: number >> tag!(".") >> minor: number >> (major, minor)));

named!(crlf, tag!("\r\n"));

named!(content_length_header<Header>, do_parse!(
    tag!("Content-Length:") >> space >> value: number >> crlf >>
    (Header::ContentLength {value: value})));

named!(generic_header<Header>, do_parse!(
    name: map_res!(take_until_either!(" \r\n:"), str::from_utf8) >> tag!(":") >> space >> value: map_res!(take_until_either!("\r\n"), str::from_utf8) >> crlf >>
    (Header::Generic {
        name: name.to_string(),
        value: value.to_string()})));

named!(header<Header>, alt!(content_length_header | generic_header));

//named!(method, alt!(tag!("DESCRIBE") | tag!("GET_PARAMETER") | tag!("OPTIONS") | tag!("PAUSE") | tag!("PLAY") |
//                    tag!("PLAY_NOTIFY") | tag!("REDIRECT") | tag!("SETUP") | tag!("SET_PARAMETER") | tag!("TEARDOWN")));

fn get_content_length(headers: &Vec<Header>) -> u32 {
    for header in headers {
        match header {
            &Header::ContentLength { ref value } => {
                return *value;
            }
            &Header::Generic { ref name, ref value } => ()
        }
    }
    return 0;
}

//named!(request<Request>,
//    do_parse!(
//        method: map_res!(method, str::from_utf8) >> space >> uri: map_res!(take_until_either!("\r\n"), str::from_utf8) >> space >> version: version >> crlf >>
//        headers: terminated!(many0!(header), crlf) >>
//        body: take!(get_content_length(&headers)) >>
//        (Request {
//            method: method.to_string(),
//            uri: uri.to_string(),
//            version: version,
//            headers,
//            body: body.to_vec() })));

named!(response<Response>,
    do_parse!(
        version: version >> space >> status_code: number >> space >> status: map_res!(take_until_either!(" \r\n"), str::from_utf8) >> crlf >>
        headers: terminated!(many0!(header), crlf) >>
        body: take!(get_content_length(&headers)) >>
        (Response {
            version,
            status_code,
            status: status.to_string(),
            headers,
            body: body.to_vec()})));

pub struct Client {
    inner: Validate<ClientService<TcpStream, RtspProto>>,
}

pub struct Validate<T> {
    inner: T,
}

pub struct RtspCodec;

struct RtspProto;

impl Client {
    pub fn connect(addr: &SocketAddr, handle: &Handle) -> Box<Future<Item=Client, Error=io::Error>> {
        let ret = TcpClient::new(RtspProto)
            .connect(addr, handle)
            .map(|client_service| {
                let validate = Validate { inner: client_service };
                Client { inner: validate }
            });

        Box::new(ret)
    }
}

impl Service for Client {
    type Request = Request;
    type Response = Response;
    type Error = io::Error;
    type Future = Box<Future<Item=Response, Error=io::Error>>;

    fn call(&self, req: Request) -> Self::Future {
        self.inner.call(req)
    }
}

impl<T> Service for Validate<T>
    where T: Service<Request=Request, Response=Response, Error=io::Error>,
          T::Future: 'static,
{
    type Request = Request;
    type Response = Response;
    type Error = io::Error;
    type Future = Box<Future<Item=Response, Error=io::Error>>;

    fn call(&self, req: Request) -> Self::Future {
        Box::new(self.inner.call(req)
            .and_then(|resp| {
                Ok(resp)
            }))
    }
}

impl Decoder for RtspCodec {
    type Item = Response;
    type Error = io::Error;

    fn decode(&mut self, buf: &mut BytesMut) -> Result<Option<Response>, io::Error> {
        let input_len = buf.len();
        match response(buf.as_ref()) {
            IResult::Done(remaining_input, result) => {
                Ok(Some((result, input_len - remaining_input.len())))
            }
            IResult::Error(e) => {
                Err(io::Error::new(io::ErrorKind::InvalidInput, e.to_string()))
            }
            IResult::Incomplete(_needed) => {
                Ok(None)
            }
        }.map(|result| {
            match result {
                Some((result, split_idx)) => {
                    buf.split_to(split_idx);
                    Some(result)
                }
                None => None
            }
        })
    }
}

fn encode_headers(headers: Vec<Header>) -> String {
    headers.into_iter()
        .map(|header| -> String {
            match header {
                Header::ContentLength { ref value } => format!("Content-Length: {}\r\n", value),
                Header::Generic { ref name, ref value } => format!("{}: {}\r\n", name, value)
            }
        })
        .collect::<Vec<_>>()
        .concat()
}

impl Encoder for RtspCodec {
    type Item = Request;
    type Error = io::Error;

    fn encode(&mut self, request: Request, buf: &mut BytesMut) -> io::Result<()> {
        buf.clear();
        let formatted = format!("{} {} RTSP/{}.{}\r\n{}\r\n", request.method, request.uri, request.version.0, request.version.1, encode_headers(request.headers)).into_bytes();
        buf.reserve(formatted.len() + request.body.len());
        buf.extend_from_slice(formatted.as_ref());
        buf.extend_from_slice(request.body.as_ref());
        println!("encode(): {:?}", str::from_utf8(buf.as_ref()));
        Ok(())
    }
}

impl<T: AsyncRead + AsyncWrite + 'static> ClientProto<T> for RtspProto {
    type Request = Request;
    type Response = Response;

    /// `Framed<T, RtspCodec>` is the return value of `io.framed(RtspCodec)`
    type Transport = Framed<T, RtspCodec>;
    type BindTransport = Result<Self::Transport, io::Error>;

    fn bind_transport(&self, io: T) -> Self::BindTransport {
        Ok(io.framed(RtspCodec))
    }
}

fn fetch_add_1(c_seq: &AtomicUsize) -> usize {
    c_seq.fetch_add(1, Ordering::SeqCst)
}

pub fn run_client(uri: &str, rtp_ports: (u16, u16)) -> Result<(), io::Error> {
    let parsed_uri: Uri = uri.parse().map_err(|_| io::Error::new(io::ErrorKind::Other, "uri parse error"))?;
    if parsed_uri.scheme().unwrap_or("") != "rtsp" || parsed_uri.host().is_none() {
        return Err(io::Error::new(io::ErrorKind::Other, "invalid rtsp uri"));
    }
    let host = parsed_uri.host().unwrap();
    let port = parsed_uri.port().unwrap_or(554);
    let ip_addr = lookup_host(host)
        .map(|ips| ips.first().cloned())
        .map_err(|_| io::Error::new(io::ErrorKind::Other, "ip address lookup failed"))?;
    let sock_addr = SocketAddr::new(ip_addr.unwrap_or(IpAddr::V4(Ipv4Addr::new(127, 0, 0, 1))), port);
    info!("connecting to {}:{} [{}]", host, port, sock_addr);
    let mut core = Core::new().unwrap();
    let handle = core.handle();
    let c_seq = AtomicUsize::new(1);
    core.run(
        Client::connect(&sock_addr, &handle)
            .and_then(|client| {
                client
                    .call(Request::options(uri, fetch_add_1(&c_seq)))
                    .and_then(move |response| {
                        info!("response: {:?}\n{}", response, str::from_utf8(response.body.as_ref()).unwrap());
                        // todo: check Public header for a list of supported methods and if GET_PARAMETER is not there, use OPTIONS as keep-alive method
                        client
                            .call(Request::describe(uri, fetch_add_1(&c_seq)))
                            .and_then(move |response| {
                                // todo: extract video track parameters from the response so janus endpoint could be setup correctly
                                // a=rtpmap:96 H264/90000
                                // a=fmtp:96 packetization-mode=1; profile-level-id=4D4029; sprop-parameter-sets=Z01AKZpmAoAy2AtQEBAQXpw=,aO48gA==
                                info!("response: {:?}\n{}", response, str::from_utf8(response.body.as_ref()).unwrap());
                                // todo: extract all a:control endpoints and setup all of them
                                client
                                    // note: some clients might want to receive RTP/AVP/UDP;unicast instead of RTP/AVP;unicast
                                    .call(Request::setup(uri, fetch_add_1(&c_seq),
                                                         format!("RTP/AVP;unicast;client_port={}-{}", rtp_ports.0, rtp_ports.1).as_ref()))
                                    .and_then(move |response| {
                                        info!("response: {:?}\n{}", response, str::from_utf8(response.body.as_ref()).unwrap());
                                        client
                                            .call(Request::play(uri, fetch_add_1(&c_seq), response.session()))
                                            .and_then(move |response| {
                                                info!("response: {:?}\n{}", response, str::from_utf8(response.body.as_ref()).unwrap());
                                                let session = response.session();
                                                let session_timeout = response.session_timeout();
                                                // create a interval timer to send RTSP GET_PARAMETER keepalives
                                                let timer = Timer::default();
                                                let keepalives = timer
                                                    .interval(Duration::from_secs(session_timeout * 3 / 4))
                                                    .for_each(move |_| {
                                                        client
                                                            .call(Request::get_parameter(uri,
                                                                                         fetch_add_1(&c_seq),
                                                                                         session.clone()))
                                                            .and_then(move |response| {
                                                                info!("keepalive response: {:?}\n{}", response, str::from_utf8(response.body.as_ref()).unwrap());
                                                                Ok(())
                                                            })
                                                            .map_err(|_| TimerError::NoCapacity)
                                                    });
                                                keepalives.map_err(|_| io::Error::new(io::ErrorKind::Other, "timer error"))
                                            })
                                    })
                            })
                    })
            }))
}
