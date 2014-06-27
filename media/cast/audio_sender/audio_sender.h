// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_AUDIO_SENDER_H_
#define MEDIA_CAST_AUDIO_SENDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_config.h"
#include "media/cast/rtcp/rtcp.h"
#include "media/cast/transport/rtp_sender/rtp_sender.h"

namespace media {
namespace cast {

class AudioEncoder;
class LocalRtcpAudioSenderFeedback;

// This class is not thread safe.
// It's only called from the main cast thread.
class AudioSender : public base::NonThreadSafe,
                    public base::SupportsWeakPtr<AudioSender> {
 public:
  AudioSender(scoped_refptr<CastEnvironment> cast_environment,
              const AudioSenderConfig& audio_config,
              transport::CastTransportSender* const transport_sender);

  virtual ~AudioSender();

  CastInitializationStatus InitializationResult() const {
    return cast_initialization_cb_;
  }

  void InsertAudio(scoped_ptr<AudioBus> audio_bus,
                   const base::TimeTicks& recorded_time);

  // Only called from the main cast thread.
  void IncomingRtcpPacket(scoped_ptr<Packet> packet);

 protected:
  void SendEncodedAudioFrame(
      scoped_ptr<transport::EncodedAudioFrame> audio_frame,
      const base::TimeTicks& recorded_time);

 private:
  friend class LocalRtcpAudioSenderFeedback;

  void ResendPackets(
      const MissingFramesAndPacketsMap& missing_frames_and_packets);

  void StoreStatistics(const transport::RtcpSenderInfo& sender_info,
                       base::TimeTicks time_sent,
                       uint32 rtp_timestamp);

  void ScheduleNextRtcpReport();
  void SendRtcpReport();

  void InitializeTimers();

  scoped_refptr<CastEnvironment> cast_environment_;
  transport::CastTransportSender* const transport_sender_;
  scoped_ptr<AudioEncoder> audio_encoder_;
  RtpSenderStatistics rtp_stats_;
  scoped_ptr<LocalRtcpAudioSenderFeedback> rtcp_feedback_;
  Rtcp rtcp_;
  bool timers_initialized_;
  CastInitializationStatus cast_initialization_cb_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<AudioSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioSender);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_AUDIO_SENDER_H_
