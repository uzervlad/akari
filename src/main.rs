use std::{env, fmt::Debug, io::Cursor, net::UdpSocket};

use anyhow::Result;
use binrw::{BinRead, BinWrite};
use clap::{Parser, Subcommand};
use easy_color::{Hex, IntoHSV};

#[derive(Debug, BinWrite)]
#[bw(little)]
enum AkariMessage {
  #[bw(magic = 0u8)]
  Ping,
  #[bw(magic = 1u8)]
  SetHue(f32),
  #[bw(magic = 2u8)]
  SetBaseValue(f32),
  #[bw(magic = 3u8)]
  Pulse,
  #[bw(magic = 255u8)]
  Listen,
}

impl AkariMessage {
  fn message(self) -> Vec<u8> {
    let mut buf = Cursor::new(vec![]);
    self.write(&mut buf).unwrap();
    buf.into_inner()
  }
}

#[derive(Debug, BinRead)]
#[br(little)]
#[allow(unused)]
enum AkariResponse {
  #[br(magic = 0u8)]
  Pong,
  #[br(magic = 1u8)]
  Result(u8),
  #[br(magic = 255u8)]
  Color([u8; 3]),
}

impl AkariResponse {
  fn wait_for(socket: &UdpSocket) -> Result<AkariResponse> {
    let mut buf = [0u8; 256];
    let (len, _) = socket.recv_from(&mut buf)?;
    Ok(AkariResponse::read(&mut Cursor::new(&buf[..len]))?)
  }
}

#[derive(Debug, Parser)]
struct AkariCli {
  #[command(subcommand)]
  command: AkariCommand
}

#[derive(Debug, Subcommand)]
enum AkariCommand {
  Ping,
  #[command(name = "hue")]
  SetHue {
    hue: f32,
  },
  #[command(name = "color")]
  SetColor {
    color: String,
  },
  #[command(name = "value")]
  SetBaseValue {
    value: f32,
  },
  Pulse,
  Listen,
}

fn main() -> Result<()> {
  let _ = dotenvy::dotenv();

  let socket = UdpSocket::bind("0.0.0.0:0")?;
  let address = env::var("MCU_ADDRESS")?;

  let cli = AkariCli::parse();

  match cli.command {
    AkariCommand::Ping => {
      let msg = AkariMessage::Ping.message();
      socket.send_to(&msg, address)?;
      println!("Received {:?}", AkariResponse::wait_for(&socket)?);
    },
    AkariCommand::SetHue { hue } => {
      let msg = AkariMessage::SetHue(hue).message();
      socket.send_to(&msg, address)?;
      println!("Received {:?}", AkariResponse::wait_for(&socket)?);
    }
    AkariCommand::SetColor { color } => {
      let hex = Hex::try_from(color.as_str()).unwrap();
      let hue = hex.to_hsv().hue() as f32 / 360.;

      let msg = AkariMessage::SetHue(hue).message();
      socket.send_to(&msg, address)?;
      println!("Received {:?}", AkariResponse::wait_for(&socket)?);
    },
    AkariCommand::SetBaseValue { value } => {
      let msg = AkariMessage::SetBaseValue(value).message();
      socket.send_to(&msg, address)?;
      println!("Received {:?}", AkariResponse::wait_for(&socket)?);
    }
    AkariCommand::Pulse => {
      let msg = AkariMessage::Pulse.message();
      socket.send_to(&msg, address)?;
      println!("Received {:?}", AkariResponse::wait_for(&socket)?);
    }
    AkariCommand::Listen => {
      let msg = AkariMessage::Listen.message();
      socket.send_to(&msg, address)?;

      println!("Listening...");

      loop {
        let res = AkariResponse::wait_for(&socket).unwrap();
        println!("Received {:?}", res);
      }
    }
  }

  Ok(())
}
