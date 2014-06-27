// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/fling_animator_impl_android.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebFloatSize.h"
#include "third_party/WebKit/public/platform/WebGestureCurveTarget.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/vector2d.h"

namespace content {

namespace {

// Value taken directly from Android's ViewConfiguration. As the value has not
// changed in 4+ years, and does not depend on any device-specific configuration
// parameters, copy it directly to avoid potential JNI interop issues in the
// render process (see crbug.com/362614).
const float kDefaultAndroidPlatformScrollFriction = 0.015f;

gfx::Scroller::Config GetScrollerConfig() {
  gfx::Scroller::Config config;
  config.flywheel_enabled = false;
  config.fling_friction = kDefaultAndroidPlatformScrollFriction;
  return config;
}

}  // namespace

FlingAnimatorImpl::FlingAnimatorImpl()
    : is_active_(false),
      scroller_(GetScrollerConfig()) {}

FlingAnimatorImpl::~FlingAnimatorImpl() {}

void FlingAnimatorImpl::StartFling(const gfx::PointF& velocity) {
  // No bounds on the fling. See http://webkit.org/b/96403
  // Instead, use the largest possible bounds for minX/maxX/minY/maxY. The
  // compositor will ignore any attempt to scroll beyond the end of the page.

  DCHECK(velocity.x() || velocity.y());
  if (is_active_)
    CancelFling();

  is_active_ = true;
  scroller_.Fling(0,
                  0,
                  velocity.x(),
                  velocity.y(),
                  INT_MIN,
                  INT_MAX,
                  INT_MIN,
                  INT_MAX,
                  gfx::FrameTime::Now());
  // TODO(jdduke): Initialize the fling at time 0 and use the monotonic
  // time in |apply()| for updates, crbug.com/345459.
}

void FlingAnimatorImpl::CancelFling() {
  if (!is_active_)
    return;

  is_active_ = false;
  scroller_.AbortAnimation();
}

bool FlingAnimatorImpl::apply(double /* time */,
                              blink::WebGestureCurveTarget* target) {
  // Historically, Android's Scroller used |currentAnimationTimeMillis()|,
  // which is equivalent to gfx::FrameTime::Now().  In practice, this produces
  // smoother results than using |time|, so continue using FrameTime::Now().
  // TODO(jdduke): Use |time| upon resolution of crbug.com/345459.
  if (!scroller_.ComputeScrollOffset(gfx::FrameTime::Now())) {
    is_active_ = false;
    return false;
  }

  target->notifyCurrentFlingVelocity(blink::WebFloatSize(
      scroller_.GetCurrVelocityX(), scroller_.GetCurrVelocityY()));

  gfx::PointF current_position(scroller_.GetCurrX(), scroller_.GetCurrY());
  gfx::Vector2dF scroll_amount(current_position - last_position_);
  last_position_ = current_position;

  // scrollBy() could delete this curve if the animation is over, so don't touch
  // any member variables after making that call.
  target->scrollBy(blink::WebFloatSize(scroll_amount));
  return true;
}

FlingAnimatorImpl* FlingAnimatorImpl::CreateAndroidGestureCurve(
    const blink::WebFloatPoint& velocity,
    const blink::WebSize&) {
  FlingAnimatorImpl* gesture_curve = new FlingAnimatorImpl();
  gesture_curve->StartFling(velocity);
  return gesture_curve;
}

}  // namespace content
