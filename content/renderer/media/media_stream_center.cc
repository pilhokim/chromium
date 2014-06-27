// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_center.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_source.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/webrtc_local_audio_source_provider.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrackSourcesRequest.h"
#include "third_party/WebKit/public/platform/WebSourceInfo.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebFrame.h"

using blink::WebFrame;
using blink::WebView;

namespace content {

namespace {

void CreateNativeAudioMediaStreamTrack(
    const blink::WebMediaStreamTrack& track,
    MediaStreamDependencyFactory* factory) {
  DCHECK(!track.extraData());
  blink::WebMediaStreamSource source = track.source();
  DCHECK_EQ(source.type(), blink::WebMediaStreamSource::TypeAudio);
  factory->CreateLocalAudioTrack(track);
}

void CreateNativeVideoMediaStreamTrack(
    const blink::WebMediaStreamTrack& track,
    MediaStreamDependencyFactory* factory) {
  DCHECK(track.extraData() == NULL);
  blink::WebMediaStreamSource source = track.source();
  DCHECK_EQ(source.type(), blink::WebMediaStreamSource::TypeVideo);
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  if (!native_source) {
    // TODO(perkj): Implement support for sources from
    // remote MediaStreams.
    NOTIMPLEMENTED();
    return;
  }
  blink::WebMediaStreamTrack writable_track(track);
  writable_track.setExtraData(
      new MediaStreamVideoTrack(native_source, source.constraints(),
                                MediaStreamVideoSource::ConstraintsCallback(),
                                track.isEnabled(), factory));
}

void CreateNativeMediaStreamTrack(const blink::WebMediaStreamTrack& track,
                                  MediaStreamDependencyFactory* factory) {
  DCHECK(!track.isNull() && !track.extraData());
  DCHECK(!track.source().isNull());

  switch (track.source().type()) {
    case blink::WebMediaStreamSource::TypeAudio:
      CreateNativeAudioMediaStreamTrack(track, factory);
      break;
    case blink::WebMediaStreamSource::TypeVideo:
      CreateNativeVideoMediaStreamTrack(track, factory);
      break;
  }
}

}  // namespace

MediaStreamCenter::MediaStreamCenter(blink::WebMediaStreamCenterClient* client,
                                     MediaStreamDependencyFactory* factory)
    : rtc_factory_(factory), next_request_id_(0) {}

MediaStreamCenter::~MediaStreamCenter() {}

bool MediaStreamCenter::getMediaStreamTrackSources(
    const blink::WebMediaStreamTrackSourcesRequest& request) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableDeviceEnumeration)) {
    int request_id = next_request_id_++;
    requests_.insert(std::make_pair(request_id, request));
    RenderThread::Get()->Send(new MediaStreamHostMsg_GetSources(
        request_id, GURL(request.origin().utf8())));
    return true;
  }
  return false;
}

void MediaStreamCenter::didCreateMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::didCreateMediaStreamTrack";
  CreateNativeMediaStreamTrack(track, rtc_factory_);
}

void MediaStreamCenter::didEnableMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  MediaStreamTrack* native_track =
      MediaStreamTrack::GetTrack(track);
  if (native_track)
    native_track->SetEnabled(true);
}

void MediaStreamCenter::didDisableMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  MediaStreamTrack* native_track =
      MediaStreamTrack::GetTrack(track);
  if (native_track)
    native_track->SetEnabled(false);
}

bool MediaStreamCenter::didStopMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::didStopMediaStreamTrack";
  blink::WebMediaStreamSource source = track.source();
  MediaStreamSource* extra_data =
      static_cast<MediaStreamSource*>(source.extraData());
  if (!extra_data) {
    DVLOG(1) << "didStopMediaStreamTrack called on a remote track.";
    return false;
  }

  extra_data->StopSource();
  return true;
}

blink::WebAudioSourceProvider*
MediaStreamCenter::createWebAudioSourceFromMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::createWebAudioSourceFromMediaStreamTrack";
  MediaStreamTrack* media_stream_track =
      static_cast<MediaStreamTrack*>(track.extraData());
  // Only local audio track is supported now.
  // TODO(xians): Support remote audio track.
  if (!media_stream_track || !media_stream_track->is_local_track ()) {
    NOTIMPLEMENTED();
    return NULL;
  }

  blink::WebMediaStreamSource source = track.source();
  DCHECK_EQ(source.type(), blink::WebMediaStreamSource::TypeAudio);
  WebRtcLocalAudioSourceProvider* source_provider =
      new WebRtcLocalAudioSourceProvider(track);
  return source_provider;
}

void MediaStreamCenter::didStopLocalMediaStream(
    const blink::WebMediaStream& stream) {
  DVLOG(1) << "MediaStreamCenter::didStopLocalMediaStream";
  MediaStream* native_stream = MediaStream::GetMediaStream(stream);
  if (!native_stream) {
    NOTREACHED();
    return;
  }

  // TODO(perkj): MediaStream::Stop is being deprecated. But for the moment we
  // need to support the old behavior and the new. Since we only create one
  // source object per actual device- we need to fake stopping a
  // MediaStreamTrack by disabling it if the same device is used as source by
  // multiple tracks. Note that disabling a track here, don't affect the
  // enabled property in JS.
  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  stream.audioTracks(audio_tracks);
  for (size_t i = 0; i < audio_tracks.size(); ++i)
    didDisableMediaStreamTrack(audio_tracks[i]);

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  stream.videoTracks(video_tracks);
  for (size_t i = 0; i < video_tracks.size(); ++i)
    didDisableMediaStreamTrack(video_tracks[i]);

  native_stream->OnStreamStopped();
}

void MediaStreamCenter::didCreateMediaStream(blink::WebMediaStream& stream) {
  DVLOG(1) << "MediaStreamCenter::didCreateMediaStream";
  blink::WebMediaStream writable_stream(stream);
  MediaStream* native_stream(
      new MediaStream(rtc_factory_,
                      MediaStream::StreamStopCallback(),
                      stream));
  writable_stream.setExtraData(native_stream);

  // TODO(perkj): Remove track creation once crbug/294145 is fixed. A track
  // should already have been created before reaching here.
  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  stream.audioTracks(audio_tracks);
  for (size_t i = 0; i < audio_tracks.size(); ++i) {
    if (!MediaStreamTrack::GetTrack(audio_tracks[i]))
      CreateNativeMediaStreamTrack(audio_tracks[i], rtc_factory_);
  }

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  stream.videoTracks(video_tracks);
  for (size_t i = 0; i < video_tracks.size(); ++i) {
    if (!MediaStreamTrack::GetTrack(video_tracks[i]))
      CreateNativeMediaStreamTrack(video_tracks[i], rtc_factory_);
  }

}

bool MediaStreamCenter::didAddMediaStreamTrack(
    const blink::WebMediaStream& stream,
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::didAddMediaStreamTrack";
  // TODO(perkj): Remove track creation once crbug/294145 is fixed. A track
  // should already have been created before reaching here.
  if (!MediaStreamTrack::GetTrack(track))
    CreateNativeMediaStreamTrack(track, rtc_factory_);
  MediaStream* native_stream = MediaStream::GetMediaStream(stream);
  return native_stream->AddTrack(stream, track);
}

bool MediaStreamCenter::didRemoveMediaStreamTrack(
    const blink::WebMediaStream& stream,
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::didRemoveMediaStreamTrack";
  MediaStream* native_stream = MediaStream::GetMediaStream(stream);
  return native_stream->RemoveTrack(stream, track);
}

bool MediaStreamCenter::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaStreamCenter, message)
    IPC_MESSAGE_HANDLER(MediaStreamMsg_GetSourcesACK,
                        OnGetSourcesComplete)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaStreamCenter::OnGetSourcesComplete(
    int request_id,
    const content::StreamDeviceInfoArray& devices) {
  RequestMap::iterator request_it = requests_.find(request_id);
  DCHECK(request_it != requests_.end());

  blink::WebVector<blink::WebSourceInfo> sourceInfos(devices.size());
  for (size_t i = 0; i < devices.size(); ++i) {
    const MediaStreamDevice& device = devices[i].device;
    DCHECK(device.type == MEDIA_DEVICE_AUDIO_CAPTURE ||
           device.type == MEDIA_DEVICE_VIDEO_CAPTURE);
    blink::WebSourceInfo::VideoFacingMode video_facing;
    switch (device.video_facing) {
      case MEDIA_VIDEO_FACING_USER:
        video_facing = blink::WebSourceInfo::VideoFacingModeUser;
        break;
      case MEDIA_VIDEO_FACING_ENVIRONMENT:
        video_facing = blink::WebSourceInfo::VideoFacingModeEnvironment;
        break;
      default:
        video_facing = blink::WebSourceInfo::VideoFacingModeNone;
    }

    sourceInfos[i]
        .initialize(blink::WebString::fromUTF8(device.id),
                    device.type == MEDIA_DEVICE_AUDIO_CAPTURE
                        ? blink::WebSourceInfo::SourceKindAudio
                        : blink::WebSourceInfo::SourceKindVideo,
                    blink::WebString::fromUTF8(device.name),
                    video_facing);
  }
  request_it->second.requestSucceeded(sourceInfos);
}

}  // namespace content
