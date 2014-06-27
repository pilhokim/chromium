// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/transport/udp_transport.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"

namespace media {
namespace cast {
namespace transport {

namespace {
const int kMaxPacketSize = 1500;

bool IsEmpty(const net::IPEndPoint& addr) {
  net::IPAddressNumber empty_addr(addr.address().size());
  return std::equal(
             empty_addr.begin(), empty_addr.end(), addr.address().begin()) &&
         !addr.port();
}

bool IsEqual(const net::IPEndPoint& addr1, const net::IPEndPoint& addr2) {
  return addr1.port() == addr2.port() && std::equal(addr1.address().begin(),
                                                    addr1.address().end(),
                                                    addr2.address().begin());
}
}  // namespace

UdpTransport::UdpTransport(
    net::NetLog* net_log,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_proxy,
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point,
    const CastTransportStatusCallback& status_callback)
    : io_thread_proxy_(io_thread_proxy),
      local_addr_(local_end_point),
      remote_addr_(remote_end_point),
      udp_socket_(new net::UDPSocket(net::DatagramSocket::DEFAULT_BIND,
                                     net::RandIntCallback(),
                                     net_log,
                                     net::NetLog::Source())),
      send_pending_(false),
      client_connected_(false),
      status_callback_(status_callback),
      weak_factory_(this) {
  DCHECK(!IsEmpty(local_end_point) || !IsEmpty(remote_end_point));
}

UdpTransport::~UdpTransport() {}

void UdpTransport::StartReceiving(
    const PacketReceiverCallback& packet_receiver) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  packet_receiver_ = packet_receiver;
  udp_socket_->AllowAddressReuse();
  udp_socket_->SetMulticastLoopbackMode(true);
  if (!IsEmpty(local_addr_)) {
    if (udp_socket_->Bind(local_addr_) < 0) {
      status_callback_.Run(TRANSPORT_SOCKET_ERROR);
      LOG(ERROR) << "Failed to bind local address.";
      return;
    }
  } else if (!IsEmpty(remote_addr_)) {
    if (udp_socket_->Connect(remote_addr_) < 0) {
      status_callback_.Run(TRANSPORT_SOCKET_ERROR);
      LOG(ERROR) << "Failed to connect to remote address.";
      return;
    }
    client_connected_ = true;
  } else {
    NOTREACHED() << "Either local or remote address has to be defined.";
  }

  ReceiveNextPacket(net::ERR_IO_PENDING);
}

void UdpTransport::ReceiveNextPacket(int length_or_status) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  // Loop while UdpSocket is delivering data synchronously.  When it responds
  // with a "pending" status, break and expect this method to be called back in
  // the future when a packet is ready.
  while (true) {
    if (length_or_status == net::ERR_IO_PENDING) {
      next_packet_.reset(new Packet(kMaxPacketSize));
      recv_buf_ = new net::WrappedIOBuffer(
          reinterpret_cast<char*>(&next_packet_->front()));
      length_or_status = udp_socket_->RecvFrom(
          recv_buf_,
          kMaxPacketSize,
          &recv_addr_,
          base::Bind(&UdpTransport::ReceiveNextPacket,
                     weak_factory_.GetWeakPtr()));
      if (length_or_status == net::ERR_IO_PENDING)
        return;
    }

    // Note: At this point, either a packet is ready or an error has occurred.

    if (length_or_status < 0) {
      VLOG(1) << "Failed to receive packet: Status code is "
              << length_or_status << ".  Stop receiving packets.";
      status_callback_.Run(TRANSPORT_SOCKET_ERROR);
      return;
    }

    // Confirm the packet has come from the expected remote address; otherwise,
    // ignore it.  If this is the first packet being received and no remote
    // address has been set, set the remote address and expect all future
    // packets to come from the same one.
    // TODO(hubbe): We should only do this if the caller used a valid ssrc.
    if (IsEmpty(remote_addr_)) {
      remote_addr_ = recv_addr_;
      VLOG(1) << "Setting remote address from first received packet: "
              << remote_addr_.ToString();
    } else if (!IsEqual(remote_addr_, recv_addr_)) {
      VLOG(1) << "Ignoring packet received from an unrecognized address: "
              << recv_addr_.ToString() << ".";
      length_or_status = net::ERR_IO_PENDING;
      continue;
    }

    next_packet_->resize(length_or_status);
    packet_receiver_.Run(next_packet_.Pass());
    length_or_status = net::ERR_IO_PENDING;
  }
}

bool UdpTransport::SendPacket(const Packet& packet) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  if (send_pending_) {
    VLOG(1) << "Cannot send because of pending IO.";
    return false;
  }

  // TODO(hclam): This interface should take a net::IOBuffer to minimize
  // memcpy.
  scoped_refptr<net::IOBuffer> buf =
      new net::IOBuffer(static_cast<int>(packet.size()));
  memcpy(buf->data(), &packet[0], packet.size());

  int ret;
  if (client_connected_) {
    // If we called Connect() before we must call Write() instead of
    // SendTo(). Otherwise on some platforms we might get
    // ERR_SOCKET_IS_CONNECTED.
    ret = udp_socket_->Write(
      buf,
      static_cast<int>(packet.size()),
      base::Bind(&UdpTransport::OnSent, weak_factory_.GetWeakPtr(), buf));
  } else if (!IsEmpty(remote_addr_)) {
    ret = udp_socket_->SendTo(
      buf,
      static_cast<int>(packet.size()),
      remote_addr_,
      base::Bind(&UdpTransport::OnSent, weak_factory_.GetWeakPtr(), buf));
  } else {
    return false;
  }
  if (ret == net::ERR_IO_PENDING)
    send_pending_ = true;
  // When ok, will return a positive value equal the number of bytes sent.
  return ret >= net::OK;
}

void UdpTransport::OnSent(const scoped_refptr<net::IOBuffer>& buf, int result) {
  DCHECK(io_thread_proxy_->RunsTasksOnCurrentThread());

  send_pending_ = false;
  if (result < 0) {
    LOG(ERROR) << "Failed to send packet: " << result << ".";
    status_callback_.Run(TRANSPORT_SOCKET_ERROR);
  }
}

}  // namespace transport
}  // namespace cast
}  // namespace media
