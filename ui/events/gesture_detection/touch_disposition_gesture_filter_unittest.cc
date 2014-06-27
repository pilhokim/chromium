// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/gesture_detection/mock_motion_event.h"
#include "ui/events/gesture_detection/touch_disposition_gesture_filter.h"

namespace ui {

class TouchDispositionGestureFilterTest
    : public testing::Test,
      public TouchDispositionGestureFilterClient {
 public:
  TouchDispositionGestureFilterTest() : sent_gesture_count_(0) {}
  virtual ~TouchDispositionGestureFilterTest() {}

  // testing::Test
  virtual void SetUp() OVERRIDE {
    queue_.reset(new TouchDispositionGestureFilter(this));
  }

  virtual void TearDown() OVERRIDE {
    queue_.reset();
  }

  // TouchDispositionGestureFilterClient
  virtual void ForwardGestureEvent(const GestureEventData& event) OVERRIDE {
    ++sent_gesture_count_;
    sent_gestures_.push_back(event.type);
  }

 protected:
  typedef std::vector<EventType> GestureList;

  ::testing::AssertionResult GesturesMatch(const GestureList& expected,
                                           const GestureList& actual) {
    if (expected.size() != actual.size()) {
      return ::testing::AssertionFailure()
          << "actual.size(" << actual.size()
          << ") != expected.size(" << expected.size() << ")";
    }

    for (size_t i = 0; i < expected.size(); ++i) {
      if (expected[i] != actual[i]) {
        return ::testing::AssertionFailure()
            << "actual[" << i << "] ("
            << actual[i]
            << ") != expected[" << i << "] ("
            << expected[i] << ")";
      }
    }

    return ::testing::AssertionSuccess();
  }

  GestureList Gestures(EventType type) {
    return GestureList(1, type);
  }

  GestureList Gestures(EventType type0, EventType type1) {
    GestureList gestures(2);
    gestures[0] = type0;
    gestures[1] = type1;
    return gestures;
  }

  GestureList Gestures(EventType type0,
                       EventType type1,
                       EventType type2) {
    GestureList gestures(3);
    gestures[0] = type0;
    gestures[1] = type1;
    gestures[2] = type2;
    return gestures;
  }

  GestureList Gestures(EventType type0,
                       EventType type1,
                       EventType type2,
                       EventType type3) {
    GestureList gestures(4);
    gestures[0] = type0;
    gestures[1] = type1;
    gestures[2] = type2;
    gestures[3] = type3;
    return gestures;
  }

  void SendTouchGestures() {
    GestureEventDataPacket gesture_packet;
    std::swap(gesture_packet, pending_gesture_packet_);
    EXPECT_EQ(TouchDispositionGestureFilter::SUCCESS,
              SendTouchGestures(touch_event_, gesture_packet));
  }

  TouchDispositionGestureFilter::PacketResult
  SendTouchGestures(const MotionEvent& touch,
                    const GestureEventDataPacket& packet) {
    GestureEventDataPacket touch_packet =
        GestureEventDataPacket::FromTouch(touch);
    for (size_t i = 0; i < packet.gesture_count(); ++i)
      touch_packet.Push(packet.gesture(i));
    return queue_->OnGesturePacket(touch_packet);
  }

  TouchDispositionGestureFilter::PacketResult
  SendTimeoutGesture(EventType type) {
    return queue_->OnGesturePacket(
        GestureEventDataPacket::FromTouchTimeout(CreateGesture(type)));
  }

  TouchDispositionGestureFilter::PacketResult
  SendGesturePacket(const GestureEventDataPacket& packet) {
    return queue_->OnGesturePacket(packet);
  }

  void SendTouchEventAck(bool event_consumed) {
    queue_->OnTouchEventAck(event_consumed);
  }

  void SendTouchConsumedAck() {
    SendTouchEventAck(true);
  }

  void SendTouchNotConsumedAck() {
    SendTouchEventAck(false);
  }

  void PushGesture(EventType type) {
    pending_gesture_packet_.Push(CreateGesture(type));
  }

  void PressTouchPoint(int x, int y) {
    touch_event_.PressPoint(x, y);
    SendTouchGestures();
  }

  void MoveTouchPoint(size_t index, int x, int y) {
    touch_event_.MovePoint(index, x, y);
    SendTouchGestures();
  }

  void ReleaseTouchPoint() {
    touch_event_.ReleasePoint();
    SendTouchGestures();
  }

  void CancelTouchPoint() {
    touch_event_.CancelPoint();
    SendTouchGestures();
  }

  bool GesturesSent() const {
    return !sent_gestures_.empty();
  }

  bool IsEmpty() const {
    return queue_->IsEmpty();
  }

  GestureList GetAndResetSentGestures() {
    GestureList sent_gestures;
    sent_gestures.swap(sent_gestures_);
    return sent_gestures;
  }

  static GestureEventData CreateGesture(EventType type) {
    return GestureEventData(type, base::TimeTicks(), 0, 0);
  }

 private:
  scoped_ptr<TouchDispositionGestureFilter> queue_;
  MockMotionEvent touch_event_;
  GestureEventDataPacket pending_gesture_packet_;
  size_t sent_gesture_count_;
  GestureList sent_gestures_;
};

TEST_F(TouchDispositionGestureFilterTest, BasicNoGestures) {
  PressTouchPoint(1, 1);
  EXPECT_FALSE(GesturesSent());

  MoveTouchPoint(0, 2, 2);
  EXPECT_FALSE(GesturesSent());

  // No gestures should be dispatched by the ack, as the queued packets
  // contained no gestures.
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // Release the touch gesture.
  ReleaseTouchPoint();
  SendTouchConsumedAck();
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, BasicGestures) {
  // An unconsumed touch's gesture should be sent.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  EXPECT_FALSE(GesturesSent());
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  // Multiple gestures can be queued for a single event.
  PushGesture(ET_SCROLL_FLING_START);
  PushGesture(ET_SCROLL_FLING_CANCEL);
  ReleaseTouchPoint();
  EXPECT_FALSE(GesturesSent());
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_SCROLL_FLING_START,
                                     ET_SCROLL_FLING_CANCEL),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, BasicGesturesConsumed) {
  // A consumed touch's gesture should not be sent.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  PushGesture(ET_SCROLL_FLING_START);
  PushGesture(ET_SCROLL_FLING_CANCEL);
  ReleaseTouchPoint();
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, ConsumedThenNotConsumed) {
  // A consumed touch's gesture should not be sent.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // Even if the subsequent touch is not consumed, continue dropping gestures.
  PushGesture(ET_GESTURE_SCROLL_UPDATE);
  MoveTouchPoint(0, 2, 2);
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // Even if the subsequent touch had no consumer, continue dropping gestures.
  PushGesture(ET_SCROLL_FLING_START);
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, NotConsumedThenConsumed) {
  // A not consumed touch's gesture should be sent.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  // A newly consumed gesture should not be sent.
  PushGesture(ET_GESTURE_PINCH_BEGIN);
  PressTouchPoint(10, 10);
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // And subsequent non-consumed pinch updates should not be sent.
  PushGesture(ET_GESTURE_SCROLL_UPDATE);
  PushGesture(ET_GESTURE_PINCH_UPDATE);
  MoveTouchPoint(0, 2, 2);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_UPDATE),
                            GetAndResetSentGestures()));

  // End events dispatched only when their start events were.
  PushGesture(ET_GESTURE_PINCH_END);
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  PushGesture(ET_GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, ScrollAlternatelyConsumed) {
  // A consumed touch's gesture should not be sent.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  for (size_t i = 0; i < 3; ++i) {
    PushGesture(ET_GESTURE_SCROLL_UPDATE);
    MoveTouchPoint(0, 2, 2);
    SendTouchConsumedAck();
    EXPECT_FALSE(GesturesSent());

    PushGesture(ET_GESTURE_SCROLL_UPDATE);
    MoveTouchPoint(0, 3, 3);
    SendTouchNotConsumedAck();
    EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_UPDATE),
                              GetAndResetSentGestures()));
  }

  PushGesture(ET_GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, NotConsumedThenNoConsumer) {
  // An unconsumed touch's gesture should be sent.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  // If the subsequent touch has no consumer (e.g., a secondary pointer is
  // pressed but not on a touch handling rect), send the gesture.
  PushGesture(ET_GESTURE_PINCH_BEGIN);
  PressTouchPoint(2, 2);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_BEGIN),
                            GetAndResetSentGestures()));

  // End events should be dispatched when their start events were, independent
  // of the ack state.
  PushGesture(ET_GESTURE_PINCH_END);
  ReleaseTouchPoint();
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_END),
                            GetAndResetSentGestures()));

  PushGesture(ET_GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, EndingEventsSent) {
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  PushGesture(ET_GESTURE_PINCH_BEGIN);
  PressTouchPoint(2, 2);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_BEGIN),
                            GetAndResetSentGestures()));

  // Consuming the touchend event can't suppress the match end gesture.
  PushGesture(ET_GESTURE_PINCH_END);
  ReleaseTouchPoint();
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_END),
                            GetAndResetSentGestures()));

  // But other events in the same packet are still suppressed.
  PushGesture(ET_GESTURE_SCROLL_UPDATE);
  PushGesture(ET_GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));

  // ET_GESTURE_SCROLL_END and ET_SCROLL_FLING_START behave the same in this
  // regard.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  PushGesture(ET_SCROLL_FLING_START);
  ReleaseTouchPoint();
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_SCROLL_FLING_START),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, EndingEventsNotSent) {
  // Consuming a begin event ensures no end events are sent.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  PushGesture(ET_GESTURE_PINCH_BEGIN);
  PressTouchPoint(2, 2);
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  PushGesture(ET_GESTURE_PINCH_END);
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  PushGesture(ET_GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, UpdateEventsSuppressedPerEvent) {
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));

  // Consuming a single scroll or pinch update should suppress only that event.
  PushGesture(ET_GESTURE_SCROLL_UPDATE);
  MoveTouchPoint(0, 2, 2);
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  PushGesture(ET_GESTURE_PINCH_BEGIN);
  PressTouchPoint(2, 2);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_BEGIN),
                            GetAndResetSentGestures()));

  PushGesture(ET_GESTURE_PINCH_UPDATE);
  MoveTouchPoint(1, 2, 3);
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // Subsequent updates should not be affected.
  PushGesture(ET_GESTURE_SCROLL_UPDATE);
  MoveTouchPoint(0, 4, 4);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_UPDATE),
                            GetAndResetSentGestures()));

  PushGesture(ET_GESTURE_PINCH_UPDATE);
  MoveTouchPoint(0, 4, 5);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_UPDATE),
                            GetAndResetSentGestures()));

  PushGesture(ET_GESTURE_PINCH_END);
  ReleaseTouchPoint();
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_PINCH_END),
                            GetAndResetSentGestures()));

  PushGesture(ET_GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, UpdateEventsDependOnBeginEvents) {
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // Scroll and pinch gestures depend on the scroll begin gesture being
  // dispatched.
  PushGesture(ET_GESTURE_SCROLL_UPDATE);
  MoveTouchPoint(0, 2, 2);
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  PushGesture(ET_GESTURE_PINCH_BEGIN);
  PressTouchPoint(2, 2);
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  PushGesture(ET_GESTURE_PINCH_UPDATE);
  MoveTouchPoint(1, 2, 3);
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  PushGesture(ET_GESTURE_PINCH_END);
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  PushGesture(ET_GESTURE_SCROLL_END);
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, MultipleTouchSequences) {
  // Queue two touch-to-gestures sequences.
  PushGesture(ET_GESTURE_TAP_DOWN);
  PressTouchPoint(1, 1);
  PushGesture(ET_GESTURE_TAP);
  ReleaseTouchPoint();
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  PushGesture(ET_GESTURE_SCROLL_END);
  ReleaseTouchPoint();

  // The first gesture sequence should not be allowed.
  SendTouchConsumedAck();
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  // The subsequent sequence should "reset" allowance.
  SendTouchNotConsumedAck();
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN,
                                     ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, FlingCancelledOnNewTouchSequence) {
  // Simulate a fling.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));
  PushGesture(ET_SCROLL_FLING_START);
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_SCROLL_FLING_START),
                            GetAndResetSentGestures()));

  // A new touch seqeuence should cancel the outstanding fling.
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_SCROLL_FLING_CANCEL),
                            GetAndResetSentGestures()));
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, ScrollEndedOnNewTouchSequence) {
  // Simulate a scroll.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();

  // A new touch seqeuence should end the outstanding scroll.
  PressTouchPoint(1, 1);
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, FlingCancelledOnScrollBegin) {
  // Simulate a fling sequence.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PushGesture(ET_SCROLL_FLING_START);
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN,
                                     ET_SCROLL_FLING_START),
                            GetAndResetSentGestures()));

  // The new fling should cancel the preceding one.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PushGesture(ET_SCROLL_FLING_START);
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_SCROLL_FLING_CANCEL,
                                     ET_GESTURE_SCROLL_BEGIN,
                                     ET_SCROLL_FLING_START),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, FlingNotCancelledIfGFCEventReceived) {
  // Simulate a fling that is started then cancelled.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  PushGesture(ET_SCROLL_FLING_START);
  MoveTouchPoint(0, 1, 1);
  SendTouchNotConsumedAck();
  PushGesture(ET_SCROLL_FLING_CANCEL);
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN,
                                     ET_SCROLL_FLING_START,
                                     ET_SCROLL_FLING_CANCEL),
                            GetAndResetSentGestures()));

  // A new touch sequence will not inject a ET_SCROLL_FLING_CANCEL, as the fling
  // has already been cancelled.
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledWhenScrollBegins) {
  PushGesture(ET_GESTURE_TAP_DOWN);
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  // If the subsequent touch turns into a scroll, the tap should be cancelled.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  MoveTouchPoint(0, 2, 2);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_CANCEL,
                                     ET_GESTURE_SCROLL_BEGIN),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TapCancelledWhenTouchConsumed) {
  PushGesture(ET_GESTURE_TAP_DOWN);
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  // If the subsequent touch is consumed, the tap should be cancelled.
  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  MoveTouchPoint(0, 2, 2);
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_CANCEL),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest,
       TapNotCancelledIfTapEndingEventReceived) {
  PushGesture(ET_GESTURE_TAP_DOWN);
  PressTouchPoint(1, 1);
  PressTouchPoint(2, 2);
  SendTouchNotConsumedAck();
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  PushGesture(ET_GESTURE_TAP);
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP),
                            GetAndResetSentGestures()));

  // The tap should not be cancelled as it was terminated by a |ET_GESTURE_TAP|.
  ReleaseTouchPoint();
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, TimeoutGestures) {
  // If the sequence is allowed, and there are no preceding gestures, the
  // timeout gestures should be forwarded immediately.
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendTimeoutGesture(ET_GESTURE_SHOW_PRESS);
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SHOW_PRESS),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(ET_GESTURE_LONG_PRESS);
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_LONG_PRESS),
                            GetAndResetSentGestures()));

  ReleaseTouchPoint();
  SendTouchNotConsumedAck();

  // If the sequence is disallowed, and there are no preceding gestures, the
  // timeout gestures should be dropped immediately.
  PressTouchPoint(1, 1);
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  SendTimeoutGesture(ET_GESTURE_SHOW_PRESS);
  EXPECT_FALSE(GesturesSent());
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();

  // If the sequence has a pending ack, the timeout gestures should
  // remain queued until the ack is received.
  PressTouchPoint(1, 1);
  EXPECT_FALSE(GesturesSent());

  SendTimeoutGesture(ET_GESTURE_LONG_PRESS);
  EXPECT_FALSE(GesturesSent());

  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_LONG_PRESS),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, SpuriousAcksIgnored) {
  // Acks received when the queue is empty will be safely ignored.
  ASSERT_TRUE(IsEmpty());
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());

  PushGesture(ET_GESTURE_SCROLL_BEGIN);
  PressTouchPoint(1, 1);
  PushGesture(ET_GESTURE_SCROLL_UPDATE);
  MoveTouchPoint(0, 3,3);
  SendTouchNotConsumedAck();
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_SCROLL_BEGIN,
                                     ET_GESTURE_SCROLL_UPDATE),
                            GetAndResetSentGestures()));

  // Even if all packets have been dispatched, the filter may not be empty as
  // there could be follow-up timeout events.  Spurious acks in such cases
  // should also be safely ignored.
  ASSERT_FALSE(IsEmpty());
  SendTouchConsumedAck();
  EXPECT_FALSE(GesturesSent());
}

TEST_F(TouchDispositionGestureFilterTest, PacketWithInvalidTypeIgnored) {
  GestureEventDataPacket packet;
  EXPECT_EQ(TouchDispositionGestureFilter::INVALID_PACKET_TYPE,
            SendGesturePacket(packet));
  EXPECT_TRUE(IsEmpty());
}

TEST_F(TouchDispositionGestureFilterTest, PacketsWithInvalidOrderIgnored) {
  EXPECT_EQ(TouchDispositionGestureFilter::INVALID_PACKET_ORDER,
            SendTimeoutGesture(ET_GESTURE_SHOW_PRESS));
  EXPECT_TRUE(IsEmpty());
}

TEST_F(TouchDispositionGestureFilterTest, ConsumedTouchCancel) {
  // An unconsumed touch's gesture should be sent.
  PushGesture(ET_GESTURE_TAP_DOWN);
  PressTouchPoint(1, 1);
  EXPECT_FALSE(GesturesSent());
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_DOWN),
                            GetAndResetSentGestures()));

  PushGesture(ET_GESTURE_TAP_CANCEL);
  PushGesture(ET_GESTURE_SCROLL_END);
  CancelTouchPoint();
  EXPECT_FALSE(GesturesSent());
  SendTouchConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_CANCEL,
                                     ET_GESTURE_SCROLL_END),
                            GetAndResetSentGestures()));
}

TEST_F(TouchDispositionGestureFilterTest, TimeoutEventAfterRelease) {
  PressTouchPoint(1, 1);
  SendTouchNotConsumedAck();
  EXPECT_FALSE(GesturesSent());
  PushGesture(ET_GESTURE_TAP_UNCONFIRMED);
  ReleaseTouchPoint();
  SendTouchNotConsumedAck();
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP_UNCONFIRMED),
                            GetAndResetSentGestures()));

  SendTimeoutGesture(ET_GESTURE_TAP);
  EXPECT_TRUE(GesturesMatch(Gestures(ET_GESTURE_TAP),
                            GetAndResetSentGestures()));
}

}  // namespace ui
