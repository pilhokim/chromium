// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_CAPTURER_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_CAPTURER_ADAPTER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "media/base/video_frame.h"
#include "media/video/capture/video_capture_types.h"
#include "third_party/libjingle/source/talk/media/base/videocapturer.h"

namespace content {

// WebRtcVideoCapturerAdapter implements a simple cricket::VideoCapturer that is
// used for VideoCapturing in libJingle and especially in PeerConnections.
// The class is created and destroyed on the main render thread.
// PeerConnection access cricket::VideoCapturer from a libJingle worker thread.
class CONTENT_EXPORT WebRtcVideoCapturerAdapter
    : NON_EXPORTED_BASE(public cricket::VideoCapturer) {
 public:
  explicit WebRtcVideoCapturerAdapter(bool is_screencast);
  virtual ~WebRtcVideoCapturerAdapter();

  // Sets the requested format. cricket::VideoCapturer may try to scale or
  // crop to this format if the frame delivered in OnFrameCaptured is not in
  // this format.
  // This method is virtual for testing purposes.
  virtual void SetRequestedFormat(const media::VideoCaptureFormat& format);

  // This method is virtual for testing purposes.
  virtual void OnFrameCaptured(const scoped_refptr<media::VideoFrame>& frame);

 private:
  // cricket::VideoCapturer implementation.
  // These methods are accessed from a libJingle worker thread.
  virtual cricket::CaptureState Start(
      const cricket::VideoFormat& capture_format) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual bool IsRunning() OVERRIDE;
  virtual bool GetPreferredFourccs(std::vector<uint32>* fourccs) OVERRIDE;
  virtual bool GetBestCaptureFormat(const cricket::VideoFormat& desired,
                                    cricket::VideoFormat* best_format) OVERRIDE;
  virtual bool IsScreencast() const OVERRIDE;

  void UpdateI420Buffer(const scoped_refptr<media::VideoFrame>& src);

 private:
  const bool is_screencast_;
  bool running_;
  base::TimeDelta first_frame_timestamp_;
  // |buffer_| used if cropping is needed. It is created only if needed and
  // owned by WebRtcVideoCapturerAdapter. If its created, it exists until
  // WebRtcVideoCapturerAdapter is destroyed.
  uint8* buffer_;
  size_t buffer_size_;
  scoped_ptr<cricket::CapturedFrame> captured_frame_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcVideoCapturerAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_CAPTURER_ADAPTER_H_
