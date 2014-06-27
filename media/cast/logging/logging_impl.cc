// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/big_endian.h"
#include "base/debug/trace_event.h"
#include "media/cast/logging/logging_impl.h"

namespace media {
namespace cast {

// TODO(imcheng): Collapse LoggingRaw onto LoggingImpl.
LoggingImpl::LoggingImpl() {
  // LoggingImpl can be constructed on any thread, but its methods should all be
  // called on the same thread.
  thread_checker_.DetachFromThread();
}

LoggingImpl::~LoggingImpl() {}

void LoggingImpl::InsertFrameEvent(const base::TimeTicks& time_of_event,
                                   CastLoggingEvent event, uint32 rtp_timestamp,
                                   uint32 frame_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  raw_.InsertFrameEvent(time_of_event, event, rtp_timestamp, frame_id);
}

void LoggingImpl::InsertFrameEventWithSize(const base::TimeTicks& time_of_event,
                                           CastLoggingEvent event,
                                           uint32 rtp_timestamp,
                                           uint32 frame_id, int frame_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  raw_.InsertFrameEventWithSize(time_of_event, event, rtp_timestamp, frame_id,
                                frame_size);
}

void LoggingImpl::InsertFrameEventWithDelay(
    const base::TimeTicks& time_of_event, CastLoggingEvent event,
    uint32 rtp_timestamp, uint32 frame_id, base::TimeDelta delay) {
  DCHECK(thread_checker_.CalledOnValidThread());
  raw_.InsertFrameEventWithDelay(time_of_event, event, rtp_timestamp,
                                 frame_id, delay);
}

void LoggingImpl::InsertSinglePacketEvent(const base::TimeTicks& time_of_event,
                                          CastLoggingEvent event,
                                          const Packet& packet) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Parse basic properties.
  uint32 rtp_timestamp;
  uint16 packet_id, max_packet_id;
  const uint8* packet_data = &packet[0];
  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(packet_data + 4), 4);
  big_endian_reader.ReadU32(&rtp_timestamp);
  base::BigEndianReader cast_big_endian_reader(
      reinterpret_cast<const char*>(packet_data + 12 + 2), 4);
  cast_big_endian_reader.ReadU16(&packet_id);
  cast_big_endian_reader.ReadU16(&max_packet_id);

  // rtp_timestamp is enough - no need for frame_id as well.
  InsertPacketEvent(time_of_event,
                    event,
                    rtp_timestamp,
                    kFrameIdUnknown,
                    packet_id,
                    max_packet_id,
                    packet.size());
}

void LoggingImpl::InsertPacketListEvent(const base::TimeTicks& time_of_event,
                                        CastLoggingEvent event,
                                        const PacketList& packets) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (PacketList::const_iterator it = packets.begin(); it != packets.end();
       ++it) {
    InsertSinglePacketEvent(time_of_event, event, *it);
  }
}

void LoggingImpl::InsertPacketEvent(const base::TimeTicks& time_of_event,
                                    CastLoggingEvent event,
                                    uint32 rtp_timestamp, uint32 frame_id,
                                    uint16 packet_id, uint16 max_packet_id,
                                    size_t size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  raw_.InsertPacketEvent(time_of_event, event, rtp_timestamp, frame_id,
                         packet_id, max_packet_id, size);
}

void LoggingImpl::InsertGenericEvent(const base::TimeTicks& time_of_event,
                                     CastLoggingEvent event, int value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  raw_.InsertGenericEvent(time_of_event, event, value);
}

void LoggingImpl::AddRawEventSubscriber(RawEventSubscriber* subscriber) {
  DCHECK(thread_checker_.CalledOnValidThread());
  raw_.AddSubscriber(subscriber);
}

void LoggingImpl::RemoveRawEventSubscriber(RawEventSubscriber* subscriber) {
  DCHECK(thread_checker_.CalledOnValidThread());
  raw_.RemoveSubscriber(subscriber);
}

}  // namespace cast
}  // namespace media
