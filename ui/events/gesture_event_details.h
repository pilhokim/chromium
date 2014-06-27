// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DETAILS_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DETAILS_H_

#include "base/logging.h"
#include "ui/events/event_constants.h"
#include "ui/events/events_base_export.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"

namespace ui {

struct EVENTS_BASE_EXPORT GestureEventDetails {
 public:
  GestureEventDetails();
  GestureEventDetails(EventType type, float delta_x, float delta_y);
  GestureEventDetails(EventType type,
                      float delta_x, float delta_y,
                      float delta_x_ordinal, float delta_y_ordinal);

  EventType type() const { return type_; }

  int touch_points() const { return touch_points_; }
  void set_touch_points(int touch_points) { touch_points_ = touch_points; }

  // TODO(tdresser): Return RectF. See crbug.com/337824.
  const gfx::Rect bounding_box() const {
    return ToEnclosingRect(bounding_box_);
  }

  const gfx::RectF& bounding_box_f() const {
    return bounding_box_;
  }

  void set_bounding_box(const gfx::RectF& box) { bounding_box_ = box; }

  void SetScrollVelocity(float velocity_x, float velocity_y,
                         float velocity_x_ordinal, float velocity_y_ordinal);

  float scroll_x_hint() const {
    DCHECK_EQ(ui::ET_GESTURE_SCROLL_BEGIN, type_);
    return data.scroll_begin.x_hint;
  }

  float scroll_y_hint() const {
    DCHECK_EQ(ui::ET_GESTURE_SCROLL_BEGIN, type_);
    return data.scroll_begin.y_hint;
  }

  float scroll_x() const {
    DCHECK_EQ(ui::ET_GESTURE_SCROLL_UPDATE, type_);
    return data.scroll_update.x;
  }

  float scroll_y() const {
    DCHECK_EQ(ui::ET_GESTURE_SCROLL_UPDATE, type_);
    return data.scroll_update.y;
  }

  float velocity_x() const {
    DCHECK(type_ == ui::ET_GESTURE_SCROLL_UPDATE ||
          type_ == ui::ET_SCROLL_FLING_START);
    return type_ == ui::ET_SCROLL_FLING_START ? data.fling_velocity.x :
                                                data.scroll_update.velocity_x;
  }

  float velocity_y() const {
    DCHECK(type_ == ui::ET_GESTURE_SCROLL_UPDATE ||
          type_ == ui::ET_SCROLL_FLING_START);
    return type_ == ui::ET_SCROLL_FLING_START ? data.fling_velocity.y :
                                                data.scroll_update.velocity_y;
  }

  // *_ordinal values are unmodified by rail based clamping.
  float scroll_x_ordinal() const {
    DCHECK_EQ(ui::ET_GESTURE_SCROLL_UPDATE, type_);
    return data.scroll_update.x_ordinal;
  }

  float scroll_y_ordinal() const {
    DCHECK_EQ(ui::ET_GESTURE_SCROLL_UPDATE, type_);
    return data.scroll_update.y_ordinal;
  }

  float velocity_x_ordinal() const {
    DCHECK(type_ == ui::ET_GESTURE_SCROLL_UPDATE ||
          type_ == ui::ET_SCROLL_FLING_START);
    return type_ == ui::ET_SCROLL_FLING_START ?
        data.fling_velocity.x_ordinal :
        data.scroll_update.velocity_x_ordinal;
  }

  float velocity_y_ordinal() const {
    DCHECK(type_ == ui::ET_GESTURE_SCROLL_UPDATE ||
          type_ == ui::ET_SCROLL_FLING_START);
    return type_ == ui::ET_SCROLL_FLING_START ?
        data.fling_velocity.y_ordinal :
        data.scroll_update.velocity_y_ordinal;
  }

  int touch_id() const {
    DCHECK_EQ(ui::ET_GESTURE_LONG_PRESS, type_);
    return data.touch_id;
  }

  float first_finger_width() const {
    DCHECK_EQ(ui::ET_GESTURE_TWO_FINGER_TAP, type_);
    return data.first_finger_enclosing_rectangle.width;
  }

  float first_finger_height() const {
    DCHECK_EQ(ui::ET_GESTURE_TWO_FINGER_TAP, type_);
    return data.first_finger_enclosing_rectangle.height;
  }

  float scale() const {
    DCHECK_EQ(ui::ET_GESTURE_PINCH_UPDATE, type_);
    return data.scale;
  }

  bool swipe_left() const {
    DCHECK_EQ(ui::ET_GESTURE_MULTIFINGER_SWIPE, type_);
    return data.swipe.left;
  }

  bool swipe_right() const {
    DCHECK_EQ(ui::ET_GESTURE_MULTIFINGER_SWIPE, type_);
    return data.swipe.right;
  }

  bool swipe_up() const {
    DCHECK_EQ(ui::ET_GESTURE_MULTIFINGER_SWIPE, type_);
    return data.swipe.up;
  }

  bool swipe_down() const {
    DCHECK_EQ(ui::ET_GESTURE_MULTIFINGER_SWIPE, type_);
    return data.swipe.down;
  }

  int tap_count() const {
    DCHECK(type_ == ui::ET_GESTURE_TAP ||
           type_ == ui::ET_GESTURE_TAP_UNCONFIRMED ||
           type_ == ET_GESTURE_DOUBLE_TAP);
    return data.tap_count;
  }

 private:
  ui::EventType type_;
  union Details {
    Details();
    struct {  // SCROLL start details.
      // Distance that caused the scroll to start.  Generally redundant with
      // the x/y values from the first scroll_update.
      float x_hint;
      float y_hint;
    } scroll_begin;

    struct {  // SCROLL delta.
      float x;
      float y;
      float velocity_x;
      float velocity_y;
      float x_ordinal;
      float y_ordinal;
      float velocity_x_ordinal;
      float velocity_y_ordinal;
    } scroll_update;

    float scale;  // PINCH scale.

    struct {  // FLING velocity.
      float x;
      float y;
      float x_ordinal;
      float y_ordinal;
    } fling_velocity;

    int touch_id;  // LONG_PRESS touch-id.

    // Dimensions of the first finger's enclosing rectangle for
    // TWO_FINGER_TAP.
    struct {
      float width;
      float height;
    } first_finger_enclosing_rectangle;

    struct {  // SWIPE direction.
      bool left;
      bool right;
      bool up;
      bool down;
    } swipe;

    // Tap information must be set for ET_GESTURE_TAP,
    // ET_GESTURE_TAP_UNCONFIRMED, and ET_GESTURE_DOUBLE_TAP events.
    int tap_count;  // TAP repeat count.
  } data;

  int touch_points_;  // Number of active touch points in the gesture.

  // Bounding box is an axis-aligned rectangle that contains all the
  // enclosing rectangles of the touch-points in the gesture.
  gfx::RectF bounding_box_;
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DETAILS_H_
