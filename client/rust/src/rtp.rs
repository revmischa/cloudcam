use futures::{Future, Stream};
use tokio_io::AsyncRead;
use tokio_core::net::{UdpSocket, UdpCodec};
use tokio_core::reactor::Core;
use std::io;
use std::ops::Range;
use std::net::{IpAddr, Ipv4Addr, SocketAddr, TcpListener};
use std::sync::Mutex;

pub struct RtpCodec;

impl UdpCodec for RtpCodec {
    type In = (SocketAddr, Vec<u8>);
    type Out = (SocketAddr, Vec<u8>);

    fn decode(&mut self, addr: &SocketAddr, buf: &[u8]) -> io::Result<Self::In> {
        Ok((*addr, buf.to_vec()))
    }

    fn encode(&mut self, (addr, buf): Self::Out, into: &mut Vec<u8>) -> SocketAddr {
        into.extend(buf);
        addr
    }
}

fn is_port_available(port: u16) -> bool {
    match TcpListener::bind((Ipv4Addr::new(127, 0, 0, 1), port)) {
        Ok(_) => true,
        Err(_) => false,
    }
}

pub fn find_port_pair(range: Range<u16>) -> Option<(u16, u16)> {
    for port in range {
        if is_port_available(port) && is_port_available(port + 1) {
            return Some((port, port + 1));
        }
    }
    None
}

#[derive(Debug)]
pub struct RtpDest {
    pub addresses: Option<(SocketAddr, SocketAddr)>
}

pub fn run_reflector(listen_ip: IpAddr, listen_ports: (u16, u16), rtp_dest: &Mutex<RtpDest>) {
    // todo: we probably want to send RTCP NACK PLI to the source at the start of the stream so a keyframe is forced
    let mut core = Core::new().unwrap();
    let handle = core.handle();
    loop {
        // rtp sink
        let null_addr = SocketAddr::new(IpAddr::V4(Ipv4Addr::new(0, 0, 0, 0)), 0);
        let rtp_listen_addr = SocketAddr::new(listen_ip, listen_ports.0);
        let rtp_sock = UdpSocket::bind(&rtp_listen_addr, &handle).unwrap();
        let outbound_sock = UdpSocket::bind(&null_addr, &handle).unwrap();
        let rtp_future = rtp_sock.framed(RtpCodec {}).for_each(|(_addr, data)| {
//            info!("rtp packet received from {}: {}", addr, data.len());
            match rtp_dest.lock().unwrap().addresses {
                Some(addresses) => {
                    outbound_sock.send_to(data.as_ref(), &addresses.0).expect("rtp send failed");
                    ()
                }
                None => ()
            }
            Ok(())
        });
        // rtcp sink
        let rtcp_listen_addr = SocketAddr::new(listen_ip, listen_ports.1);
        let rtcp_sock = UdpSocket::bind(&rtcp_listen_addr, &handle).unwrap();
        let rtcp_future = rtcp_sock.framed(RtpCodec {}).for_each(|(addr, data)| {
            info!("rtcp packet received from {}: {}", addr, data.len());
            match rtp_dest.lock().unwrap().addresses {
                Some(addresses) => {
                    outbound_sock.send_to(data.as_ref(), &addresses.1).expect("rtcp send failed");
                    ()
                }
                None => ()
            }
            Ok(())
        });
        match core.run(rtp_future.join(rtcp_future)) {
            Ok(_) => (),
            Err(e) => error!("rtp reflector error: {:?}", e)
        }
    }
}