// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/touch_disposition_gesture_filter.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "ui/events/gesture_event_details.h"

namespace ui {
namespace {

// A BitSet32 is used for tracking dropped gesture types.
COMPILE_ASSERT(ET_GESTURE_TYPE_END - ET_GESTURE_TYPE_START < 32,
               gesture_type_count_too_large);

GestureEventData CreateGesture(EventType type) {
  return GestureEventData(
      type, base::TimeTicks(), 0, 0, GestureEventDetails(type, 0, 0));
}

enum RequiredTouches {
  RT_NONE = 0,
  RT_START = 1 << 0,
  RT_CURRENT = 1 << 1,
};

struct DispositionHandlingInfo {
  // A bitwise-OR of |RequiredTouches|.
  int required_touches;
  EventType antecedent_event_type;

  DispositionHandlingInfo(int required_touches)
      : required_touches(required_touches) {}

  DispositionHandlingInfo(int required_touches,
                          EventType antecedent_event_type)
      : required_touches(required_touches),
        antecedent_event_type(antecedent_event_type) {}
};

DispositionHandlingInfo Info(int required_touches) {
  return DispositionHandlingInfo(required_touches);
}

DispositionHandlingInfo Info(int required_touches,
                             EventType antecedent_event_type) {
  return DispositionHandlingInfo(required_touches, antecedent_event_type);
}

// This approach to disposition handling is described at http://goo.gl/5G8PWJ.
DispositionHandlingInfo GetDispositionHandlingInfo(EventType type) {
  switch (type) {
    case ET_GESTURE_TAP_DOWN:
      return Info(RT_START);
    case ET_GESTURE_TAP_CANCEL:
      return Info(RT_START);
    case ET_GESTURE_SHOW_PRESS:
      return Info(RT_START);
    case ET_GESTURE_LONG_PRESS:
      return Info(RT_START);
    case ET_GESTURE_LONG_TAP:
      return Info(RT_START | RT_CURRENT);
    case ET_GESTURE_TAP:
      return Info(RT_START | RT_CURRENT, ET_GESTURE_TAP_UNCONFIRMED);
    case ET_GESTURE_TAP_UNCONFIRMED:
      return Info(RT_START | RT_CURRENT);
    case ET_GESTURE_DOUBLE_TAP:
      return Info(RT_START | RT_CURRENT, ET_GESTURE_TAP_UNCONFIRMED);
    case ET_GESTURE_SCROLL_BEGIN:
      return Info(RT_START | RT_CURRENT);
    case ET_GESTURE_SCROLL_UPDATE:
      return Info(RT_CURRENT, ET_GESTURE_SCROLL_BEGIN);
    case ET_GESTURE_SCROLL_END:
      return Info(RT_NONE, ET_GESTURE_SCROLL_BEGIN);
    case ET_SCROLL_FLING_START:
      return Info(RT_NONE, ET_GESTURE_SCROLL_BEGIN);
    case ET_SCROLL_FLING_CANCEL:
      return Info(RT_NONE, ET_SCROLL_FLING_START);
    case ET_GESTURE_PINCH_BEGIN:
      return Info(RT_START, ET_GESTURE_SCROLL_BEGIN);
    case ET_GESTURE_PINCH_UPDATE:
      return Info(RT_CURRENT, ET_GESTURE_PINCH_BEGIN);
    case ET_GESTURE_PINCH_END:
      return Info(RT_NONE, ET_GESTURE_PINCH_BEGIN);
    default:
      break;
  }
  NOTREACHED();
  return Info(RT_NONE);
}

int GetGestureTypeIndex(EventType type) {
  return type - ET_GESTURE_TYPE_START;
}

bool IsTouchStartEvent(GestureEventDataPacket::GestureSource gesture_source) {
  return gesture_source == GestureEventDataPacket::TOUCH_SEQUENCE_START ||
         gesture_source == GestureEventDataPacket::TOUCH_START;
}

}  // namespace

// TouchDispositionGestureFilter

TouchDispositionGestureFilter::TouchDispositionGestureFilter(
    TouchDispositionGestureFilterClient* client)
    : client_(client),
      needs_tap_ending_event_(false),
      needs_fling_ending_event_(false),
      needs_scroll_ending_event_(false) {
  DCHECK(client_);
}

TouchDispositionGestureFilter::~TouchDispositionGestureFilter() {}

TouchDispositionGestureFilter::PacketResult
TouchDispositionGestureFilter::OnGesturePacket(
    const GestureEventDataPacket& packet) {
  if (packet.gesture_source() == GestureEventDataPacket::UNDEFINED ||
      packet.gesture_source() == GestureEventDataPacket::INVALID)
    return INVALID_PACKET_TYPE;

  if (packet.gesture_source() == GestureEventDataPacket::TOUCH_SEQUENCE_START)
    sequences_.push(GestureSequence());

  if (IsEmpty())
    return INVALID_PACKET_ORDER;

  if (packet.gesture_source() == GestureEventDataPacket::TOUCH_TIMEOUT &&
      Tail().empty()) {
    // Handle the timeout packet immediately if the packet preceding the timeout
    // has already been dispatched.
    FilterAndSendPacket(packet);
    return SUCCESS;
  }

  Tail().push(packet);
  return SUCCESS;
}

void TouchDispositionGestureFilter::OnTouchEventAck(bool event_consumed) {
  // Spurious touch acks from the renderer should not trigger a crash.
  if (IsEmpty() || (Head().empty() && sequences_.size() == 1))
    return;

  if (Head().empty()) {
    CancelTapIfNecessary();
    CancelFlingIfNecessary();
    EndScrollIfNecessary();
    state_ = GestureHandlingState();
    sequences_.pop();
  }

  GestureSequence& sequence = Head();

  // Dispatch the packet corresponding to the ack'ed touch, as well as any
  // additional timeout-based packets queued before the ack was received.
  bool touch_packet_for_current_ack_handled = false;
  while (!sequence.empty()) {
    const GestureEventDataPacket& packet = sequence.front();
    DCHECK_NE(packet.gesture_source(), GestureEventDataPacket::UNDEFINED);
    DCHECK_NE(packet.gesture_source(), GestureEventDataPacket::INVALID);

    if (packet.gesture_source() != GestureEventDataPacket::TOUCH_TIMEOUT) {
      // We should handle at most one non-timeout based packet.
      if (touch_packet_for_current_ack_handled)
        break;
      state_.OnTouchEventAck(event_consumed,
                             IsTouchStartEvent(packet.gesture_source()));
      touch_packet_for_current_ack_handled = true;
    }
    FilterAndSendPacket(packet);
    sequence.pop();
  }
  DCHECK(touch_packet_for_current_ack_handled);
}

bool TouchDispositionGestureFilter::IsEmpty() const {
  return sequences_.empty();
}

void TouchDispositionGestureFilter::FilterAndSendPacket(
    const GestureEventDataPacket& packet) {
  for (size_t i = 0; i < packet.gesture_count(); ++i) {
    const GestureEventData& gesture = packet.gesture(i);
    DCHECK(ET_GESTURE_TYPE_START <= gesture.type &&
           gesture.type <= ET_GESTURE_TYPE_END);
    if (state_.Filter(gesture.type)) {
      CancelTapIfNecessary();
      continue;
    }
    SendGesture(gesture);
  }
}

void TouchDispositionGestureFilter::SendGesture(const GestureEventData& event) {
  // TODO(jdduke): Factor out gesture stream reparation code into a standalone
  // utility class.
  switch (event.type) {
    case ET_GESTURE_LONG_TAP:
      CancelTapIfNecessary();
      CancelFlingIfNecessary();
      break;
    case ET_GESTURE_TAP_DOWN:
      DCHECK(!needs_tap_ending_event_);
      needs_tap_ending_event_ = true;
      break;
    case ET_GESTURE_TAP:
    case ET_GESTURE_TAP_CANCEL:
    case ET_GESTURE_TAP_UNCONFIRMED:
    case ET_GESTURE_DOUBLE_TAP:
      needs_tap_ending_event_ = false;
      break;
    case ET_GESTURE_SCROLL_BEGIN:
      CancelTapIfNecessary();
      CancelFlingIfNecessary();
      EndScrollIfNecessary();
      needs_scroll_ending_event_ = true;
      break;
    case ET_GESTURE_SCROLL_END:
      needs_scroll_ending_event_ = false;
      break;
    case ET_SCROLL_FLING_START:
      CancelFlingIfNecessary();
      needs_fling_ending_event_ = true;
      needs_scroll_ending_event_ = false;
      break;
    case ET_SCROLL_FLING_CANCEL:
      needs_fling_ending_event_ = false;
      break;
    default:
      break;
  }
  client_->ForwardGestureEvent(event);
}

void TouchDispositionGestureFilter::CancelTapIfNecessary() {
  if (!needs_tap_ending_event_)
    return;

  SendGesture(CreateGesture(ET_GESTURE_TAP_CANCEL));
  DCHECK(!needs_tap_ending_event_);
}

void TouchDispositionGestureFilter::CancelFlingIfNecessary() {
  if (!needs_fling_ending_event_)
    return;

  SendGesture(CreateGesture(ET_SCROLL_FLING_CANCEL));
  DCHECK(!needs_fling_ending_event_);
}

void TouchDispositionGestureFilter::EndScrollIfNecessary() {
  if (!needs_scroll_ending_event_)
    return;

  SendGesture(CreateGesture(ET_GESTURE_SCROLL_END));
  DCHECK(!needs_scroll_ending_event_);
}

TouchDispositionGestureFilter::GestureSequence&
TouchDispositionGestureFilter::Head() {
  DCHECK(!sequences_.empty());
  return sequences_.front();
}

TouchDispositionGestureFilter::GestureSequence&
TouchDispositionGestureFilter::Tail() {
  DCHECK(!sequences_.empty());
  return sequences_.back();
}

// TouchDispositionGestureFilter::GestureHandlingState

TouchDispositionGestureFilter::GestureHandlingState::GestureHandlingState()
    : start_touch_consumed_(false),
      current_touch_consumed_(false) {}

void TouchDispositionGestureFilter::GestureHandlingState::OnTouchEventAck(
    bool event_consumed,
    bool is_touch_start_event) {
  current_touch_consumed_ = event_consumed;
  if (event_consumed && is_touch_start_event)
    start_touch_consumed_ = true;
}

bool TouchDispositionGestureFilter::GestureHandlingState::Filter(
    EventType gesture_type) {
  DispositionHandlingInfo disposition_handling_info =
      GetDispositionHandlingInfo(gesture_type);

  int required_touches = disposition_handling_info.required_touches;
  if ((required_touches & RT_START && start_touch_consumed_) ||
      (required_touches & RT_CURRENT && current_touch_consumed_) ||
      (last_gesture_of_type_dropped_.has_bit(GetGestureTypeIndex(
          disposition_handling_info.antecedent_event_type)))) {
    last_gesture_of_type_dropped_.mark_bit(GetGestureTypeIndex(gesture_type));
    return true;
  }
  last_gesture_of_type_dropped_.clear_bit(GetGestureTypeIndex(gesture_type));
  return false;
}

}  // namespace content
