// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_session_delegate.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/renderer/media/cast_threads.h"
#include "chrome/renderer/media/cast_transport_sender_ipc.h"
#include "content/public/renderer/render_thread.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/encoding_event_subscriber.h"
#include "media/cast/logging/log_serializer.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/stats_event_subscriber.h"
#include "media/cast/logging/stats_util.h"
#include "media/cast/transport/cast_transport_config.h"
#include "media/cast/transport/cast_transport_sender.h"

using media::cast::AudioSenderConfig;
using media::cast::CastEnvironment;
using media::cast::CastSender;
using media::cast::VideoSenderConfig;

static base::LazyInstance<CastThreads> g_cast_threads =
    LAZY_INSTANCE_INITIALIZER;

namespace {

// Allow 9MB for serialized video / audio event logs.
const int kMaxSerializedBytes = 9000000;

// Assume serialized log data for each frame will take up to 150 bytes.
const int kMaxVideoEventEntries = kMaxSerializedBytes / 150;

// Assume serialized log data for each frame will take up to 75 bytes.
const int kMaxAudioEventEntries = kMaxSerializedBytes / 75;

}  // namespace

CastSessionDelegate::CastSessionDelegate()
    : io_message_loop_proxy_(
          content::RenderThread::Get()->GetIOMessageLoopProxy()),
      weak_factory_(this) {
  DCHECK(io_message_loop_proxy_);
}

CastSessionDelegate::~CastSessionDelegate() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (audio_event_subscriber_.get()) {
    cast_environment_->Logging()->RemoveRawEventSubscriber(
        audio_event_subscriber_.get());
  }
  if (video_event_subscriber_.get()) {
    cast_environment_->Logging()->RemoveRawEventSubscriber(
        video_event_subscriber_.get());
  }
}

void CastSessionDelegate::StartAudio(
    const AudioSenderConfig& config,
    const AudioFrameInputAvailableCallback& callback,
    const ErrorCallback& error_callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (!cast_transport_ || !cast_sender_) {
    error_callback.Run("Destination not set.");
    return;
  }

  audio_frame_input_available_callback_ = callback;
  media::cast::transport::CastTransportAudioConfig transport_config;
  transport_config.base.ssrc = config.sender_ssrc;
  transport_config.codec = config.codec;
  transport_config.base.rtp_config = config.rtp_config;
  transport_config.frequency = config.frequency;
  transport_config.channels = config.channels;
  cast_transport_->InitializeAudio(transport_config);
  cast_sender_->InitializeAudio(
      config,
      base::Bind(&CastSessionDelegate::InitializationResultCB,
                 weak_factory_.GetWeakPtr()));
}

void CastSessionDelegate::StartVideo(
    const VideoSenderConfig& config,
    const VideoFrameInputAvailableCallback& callback,
    const ErrorCallback& error_callback,
    const media::cast::CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const media::cast::CreateVideoEncodeMemoryCallback&
        create_video_encode_mem_cb) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (!cast_transport_ || !cast_sender_) {
    error_callback.Run("Destination not set.");
    return;
  }

  video_frame_input_available_callback_ = callback;

  media::cast::transport::CastTransportVideoConfig transport_config;
  transport_config.base.ssrc = config.sender_ssrc;
  transport_config.codec = config.codec;
  transport_config.base.rtp_config = config.rtp_config;
  cast_transport_->InitializeVideo(transport_config);
  cast_sender_->InitializeVideo(
      config,
      base::Bind(&CastSessionDelegate::InitializationResultCB,
                 weak_factory_.GetWeakPtr()),
      create_vea_cb,
      create_video_encode_mem_cb);
}

void CastSessionDelegate::StartUDP(const net::IPEndPoint& remote_endpoint) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  // CastSender uses the renderer's IO thread as the main thread. This reduces
  // thread hopping for incoming video frames and outgoing network packets.
  cast_environment_ = new CastEnvironment(
      scoped_ptr<base::TickClock>(new base::DefaultTickClock()).Pass(),
      base::MessageLoopProxy::current(),
      g_cast_threads.Get().GetAudioEncodeMessageLoopProxy(),
      g_cast_threads.Get().GetVideoEncodeMessageLoopProxy());

  // Rationale for using unretained: The callback cannot be called after the
  // destruction of CastTransportSenderIPC, and they both share the same thread.
  cast_transport_.reset(new CastTransportSenderIPC(
      remote_endpoint,
      base::Bind(&CastSessionDelegate::StatusNotificationCB,
                 base::Unretained(this)),
      base::Bind(&CastSessionDelegate::LogRawEvents, base::Unretained(this))));

  cast_sender_ = CastSender::Create(cast_environment_, cast_transport_.get());
  cast_transport_->SetPacketReceiver(cast_sender_->packet_receiver());
}

void CastSessionDelegate::ToggleLogging(bool is_audio, bool enable) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  if (enable) {
    if (is_audio) {
      if (!audio_event_subscriber_.get()) {
        audio_event_subscriber_.reset(new media::cast::EncodingEventSubscriber(
            media::cast::AUDIO_EVENT, kMaxAudioEventEntries));
        cast_environment_->Logging()->AddRawEventSubscriber(
            audio_event_subscriber_.get());
      }
      if (!audio_stats_subscriber_.get()) {
        audio_stats_subscriber_.reset(
            new media::cast::StatsEventSubscriber(media::cast::AUDIO_EVENT));
        cast_environment_->Logging()->AddRawEventSubscriber(
            audio_stats_subscriber_.get());
      }
    } else {
      if (!video_event_subscriber_.get()) {
        video_event_subscriber_.reset(new media::cast::EncodingEventSubscriber(
            media::cast::VIDEO_EVENT, kMaxVideoEventEntries));
        cast_environment_->Logging()->AddRawEventSubscriber(
            video_event_subscriber_.get());
      }
      if (!video_stats_subscriber_.get()) {
        video_stats_subscriber_.reset(
            new media::cast::StatsEventSubscriber(media::cast::VIDEO_EVENT));
        cast_environment_->Logging()->AddRawEventSubscriber(
            video_stats_subscriber_.get());
      }
    }
  } else {
    if (is_audio) {
      if (audio_event_subscriber_.get()) {
        cast_environment_->Logging()->RemoveRawEventSubscriber(
            audio_event_subscriber_.get());
        audio_event_subscriber_.reset();
      }
      if (audio_stats_subscriber_.get()) {
        cast_environment_->Logging()->RemoveRawEventSubscriber(
            audio_stats_subscriber_.get());
        audio_stats_subscriber_.reset();
      }
    } else {
      if (video_event_subscriber_.get()) {
        cast_environment_->Logging()->RemoveRawEventSubscriber(
            video_event_subscriber_.get());
        video_event_subscriber_.reset();
      }
      if (video_stats_subscriber_.get()) {
        cast_environment_->Logging()->RemoveRawEventSubscriber(
            video_stats_subscriber_.get());
        video_stats_subscriber_.reset();
      }
    }
  }
}

void CastSessionDelegate::GetEventLogsAndReset(
    bool is_audio,
    const EventLogsCallback& callback) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  media::cast::EncodingEventSubscriber* subscriber =
      is_audio ? audio_event_subscriber_.get() : video_event_subscriber_.get();
  if (!subscriber) {
    callback.Run(make_scoped_ptr(new base::BinaryValue).Pass());
    return;
  }

  media::cast::proto::LogMetadata metadata;
  media::cast::FrameEventMap frame_events;
  media::cast::PacketEventMap packet_events;

  subscriber->GetEventsAndReset(&metadata, &frame_events, &packet_events);

  scoped_ptr<char[]> serialized_log(new char[kMaxSerializedBytes]);
  int output_bytes;
  bool success = media::cast::SerializeEvents(metadata,
                                              frame_events,
                                              packet_events,
                                              true,
                                              kMaxSerializedBytes,
                                              serialized_log.get(),
                                              &output_bytes);

  if (!success) {
    VLOG(2) << "Failed to serialize event log.";
    callback.Run(make_scoped_ptr(new base::BinaryValue).Pass());
    return;
  }

  DVLOG(2) << "Serialized log length: " << output_bytes;

  scoped_ptr<base::BinaryValue> blob(
      new base::BinaryValue(serialized_log.Pass(), output_bytes));
  callback.Run(blob.Pass());
}

void CastSessionDelegate::GetStatsAndReset(bool is_audio,
                                           const StatsCallback& callback) {
  media::cast::StatsEventSubscriber* subscriber =
      is_audio ? audio_stats_subscriber_.get() : video_stats_subscriber_.get();
  if (!subscriber) {
    callback.Run(make_scoped_ptr(new base::DictionaryValue).Pass());
    return;
  }

  media::cast::FrameStatsMap frame_stats;
  subscriber->GetFrameStats(&frame_stats);
  media::cast::PacketStatsMap packet_stats;
  subscriber->GetPacketStats(&packet_stats);
  subscriber->Reset();

  scoped_ptr<base::DictionaryValue> stats = media::cast::ConvertStats(
      frame_stats, packet_stats);

  callback.Run(stats.Pass());
}

void CastSessionDelegate::StatusNotificationCB(
    media::cast::transport::CastTransportStatus unused_status) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());
  // TODO(hubbe): Call javascript UDPTransport error function.
}

void CastSessionDelegate::InitializationResultCB(
    media::cast::CastInitializationStatus result) const {
  DCHECK(cast_sender_);

  // TODO(pwestin): handle the error codes.
  if (result == media::cast::STATUS_AUDIO_INITIALIZED) {
    audio_frame_input_available_callback_.Run(
        cast_sender_->audio_frame_input());
  } else if (result == media::cast::STATUS_VIDEO_INITIALIZED) {
    video_frame_input_available_callback_.Run(
        cast_sender_->video_frame_input());
  }
}

void CastSessionDelegate::LogRawEvents(
    const std::vector<media::cast::PacketEvent>& packet_events) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  for (std::vector<media::cast::PacketEvent>::const_iterator it =
           packet_events.begin();
       it != packet_events.end();
       ++it) {
    cast_environment_->Logging()->InsertPacketEvent(it->timestamp,
                                                    it->type,
                                                    it->rtp_timestamp,
                                                    it->frame_id,
                                                    it->packet_id,
                                                    it->max_packet_id,
                                                    it->size);
  }
}
