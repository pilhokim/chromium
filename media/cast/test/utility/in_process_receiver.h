// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_IN_PROCESS_RECEIVER_H_
#define MEDIA_CAST_TEST_IN_PROCESS_RECEIVER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/transport/cast_transport_config.h"

namespace base {
class TimeTicks;
}  // namespace base

namespace net {
class IPEndPoint;
}  // namespace net

namespace media {

class VideoFrame;

namespace cast {

class CastEnvironment;
class CastReceiver;

namespace transport {
class UdpTransport;
}  // namespace transport

// Common base functionality for an in-process Cast receiver.  This is meant to
// be subclassed with the OnAudioFrame() and OnVideoFrame() methods implemented,
// so that the implementor can focus on what is to be done with the frames,
// rather than on the boilerplate "glue" code.
class InProcessReceiver {
 public:
  // Construct a receiver with the given configuration.  |remote_end_point| can
  // be left empty, if the transport should automatically mate with the first
  // remote sender it encounters.
  InProcessReceiver(const scoped_refptr<CastEnvironment>& cast_environment,
                    const net::IPEndPoint& local_end_point,
                    const net::IPEndPoint& remote_end_point,
                    const AudioReceiverConfig& audio_config,
                    const VideoReceiverConfig& video_config);

  // Must be destroyed on the cast MAIN thread.  See DestroySoon().
  virtual ~InProcessReceiver();

  // Convenience accessor to CastEnvironment.
  scoped_refptr<CastEnvironment> cast_env() const { return cast_environment_; }

  // Begin delivering any received audio/video frames to the OnXXXFrame()
  // methods.
  void Start();

  // Schedules destruction on the cast MAIN thread.  Any external references to
  // the InProcessReceiver instance become invalid.
  void DestroySoon();

 protected:
  // To be implemented by subclasses.  These are called on the Cast MAIN thread
  // as each frame is received.
  virtual void OnAudioFrame(scoped_ptr<PcmAudioFrame> audio_frame,
                            const base::TimeTicks& playout_time) = 0;
  virtual void OnVideoFrame(const scoped_refptr<VideoFrame>& video_frame,
                            const base::TimeTicks& render_time) = 0;

  // Helper method that creates |transport_| and |cast_receiver_|, starts
  // |transport_| receiving, and requests the first audio/video frame.
  // Subclasses may override to provide additional start-up functionality.
  virtual void StartOnMainThread();

  // Callback for the transport to notify of status changes.  A default
  // implementation is provided here that simply logs socket errors.
  virtual void UpdateCastTransportStatus(transport::CastTransportStatus status);

 private:
  friend class base::RefCountedThreadSafe<InProcessReceiver>;

  // CastReceiver callbacks that receive a frame and then request another.
  void GotAudioFrame(scoped_ptr<PcmAudioFrame> audio_frame,
                     const base::TimeTicks& playout_time);
  void GotVideoFrame(const scoped_refptr<VideoFrame>& video_frame,
                     const base::TimeTicks& render_time);
  void PullNextAudioFrame();
  void PullNextVideoFrame();

  // Invoked just before the destruction of |receiver| on the cast MAIN thread.
  static void WillDestroyReceiver(InProcessReceiver* receiver);

  const scoped_refptr<CastEnvironment> cast_environment_;
  const net::IPEndPoint local_end_point_;
  const net::IPEndPoint remote_end_point_;
  const AudioReceiverConfig audio_config_;
  const VideoReceiverConfig video_config_;

  scoped_ptr<transport::UdpTransport> transport_;
  scoped_ptr<CastReceiver> cast_receiver_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<InProcessReceiver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InProcessReceiver);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_IN_PROCESS_RECEIVER_H_
