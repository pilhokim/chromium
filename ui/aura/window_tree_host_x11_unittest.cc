// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host_x11.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_target.h"
#include "ui/events/event_target_iterator.h"
#include "ui/events/test/events_test_utils_x11.h"

namespace {

class RootWindowEventHandler : public ui::EventHandler {
 public:
  RootWindowEventHandler() : last_touch_type_(ui::ET_UNKNOWN),
                             last_touch_id_(-1),
                             last_touch_location_(0, 0) {
  }
  virtual ~RootWindowEventHandler () {}

  // ui::EventHandler:
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE {
    last_touch_id_ = event->touch_id();
    last_touch_type_ = event->type();
    last_touch_location_ = event->location();
  }

  ui::EventType last_touch_type() {
    return last_touch_type_;
  }

  int last_touch_id() {
    return last_touch_id_;
  }

  gfx::Point last_touch_location() {
    return last_touch_location_;
  }

 private:
  ui::EventType last_touch_type_;
  int last_touch_id_;
  gfx::Point last_touch_location_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowEventHandler);
};

}  // namespace

namespace aura {

typedef test::AuraTestBase WindowTreeHostX11Test;

// Send X touch events to one WindowTreeHost. The WindowTreeHost's
// delegate will get corresponding ui::TouchEvent if the touch events
// are winthin the bound of the WindowTreeHost.
TEST_F(WindowTreeHostX11Test, DispatchTouchEventToOneRootWindow) {
#if defined(OS_CHROMEOS)
  // Fake a ChromeOS running env.
  const char* kLsbRelease = "CHROMEOS_RELEASE_NAME=Chromium OS\n";
  base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());
#endif  // defined(OS_CHROMEOS)

  scoped_ptr<WindowTreeHostX11> window_tree_host(
      new WindowTreeHostX11(gfx::Rect(0, 0, 2560, 1700)));
  window_tree_host->InitHost();
  scoped_ptr<RootWindowEventHandler> handler(new RootWindowEventHandler());
  window_tree_host->window()->AddPreTargetHandler(handler.get());

  std::vector<unsigned int> devices;
  devices.push_back(0);
  ui::SetUpTouchDevicesForTest(devices);
  std::vector<ui::Valuator> valuators;

  EXPECT_EQ(ui::ET_UNKNOWN, handler->last_touch_type());
  EXPECT_EQ(-1, handler->last_touch_id());

  ui::ScopedXI2Event scoped_xevent;
#if defined(OS_CHROMEOS)
  // This touch is out of bounds.
  scoped_xevent.InitTouchEvent(
      0, XI_TouchBegin, 5, gfx::Point(1500, 2500), valuators);
  window_tree_host->Dispatch(scoped_xevent);
  EXPECT_EQ(ui::ET_UNKNOWN, handler->last_touch_type());
  EXPECT_EQ(-1, handler->last_touch_id());
  EXPECT_EQ(gfx::Point(0, 0), handler->last_touch_location());
#endif  // defined(OS_CHROMEOS)

  // Following touchs are within bounds and are passed to delegate.
  scoped_xevent.InitTouchEvent(
      0, XI_TouchBegin, 5, gfx::Point(1500, 1500), valuators);
  window_tree_host->Dispatch(scoped_xevent);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, handler->last_touch_type());
  EXPECT_EQ(0, handler->last_touch_id());
  EXPECT_EQ(gfx::Point(1500, 1500), handler->last_touch_location());

  scoped_xevent.InitTouchEvent(
      0, XI_TouchUpdate, 5, gfx::Point(1500, 1600), valuators);
  window_tree_host->Dispatch(scoped_xevent);
  EXPECT_EQ(ui::ET_TOUCH_MOVED, handler->last_touch_type());
  EXPECT_EQ(0, handler->last_touch_id());
  EXPECT_EQ(gfx::Point(1500, 1600), handler->last_touch_location());

  scoped_xevent.InitTouchEvent(
      0, XI_TouchEnd, 5, gfx::Point(1500, 1600), valuators);
  window_tree_host->Dispatch(scoped_xevent);
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, handler->last_touch_type());
  EXPECT_EQ(0, handler->last_touch_id());
  EXPECT_EQ(gfx::Point(1500, 1600), handler->last_touch_location());

  // Revert the CrOS testing env otherwise the following non-CrOS aura
  // tests will fail.
#if defined(OS_CHROMEOS)
  // Fake a ChromeOS running env.
  kLsbRelease = "";
  base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());
#endif  // defined(OS_CHROMEOS)
}

// Send X touch events to two WindowTreeHost. The WindowTreeHost which is
// the event target of the X touch events should generate the corresponding
// ui::TouchEvent for its delegate.
#if defined(OS_CHROMEOS)
TEST_F(WindowTreeHostX11Test, DispatchTouchEventToTwoRootWindow) {
  // Fake a ChromeOS running env.
  const char* kLsbRelease = "CHROMEOS_RELEASE_NAME=Chromium OS\n";
  base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());

  scoped_ptr<WindowTreeHostX11> window_tree_host1(
      new WindowTreeHostX11(gfx::Rect(0, 0, 2560, 1700)));
  window_tree_host1->InitHost();
  scoped_ptr<RootWindowEventHandler> handler1(new RootWindowEventHandler());
  window_tree_host1->window()->AddPreTargetHandler(handler1.get());

  int host2_y_offset = 1700;
  scoped_ptr<WindowTreeHostX11> window_tree_host2(
      new WindowTreeHostX11(gfx::Rect(0, host2_y_offset, 1920, 1080)));
  window_tree_host2->InitHost();
  scoped_ptr<RootWindowEventHandler> handler2(new RootWindowEventHandler());
  window_tree_host2->window()->AddPreTargetHandler(handler2.get());

  std::vector<unsigned int> devices;
  devices.push_back(0);
  ui::SetUpTouchDevicesForTest(devices);
  std::vector<ui::Valuator> valuators;

  EXPECT_EQ(ui::ET_UNKNOWN, handler1->last_touch_type());
  EXPECT_EQ(-1, handler1->last_touch_id());
  EXPECT_EQ(ui::ET_UNKNOWN, handler2->last_touch_type());
  EXPECT_EQ(-1, handler2->last_touch_id());

  // 2 Touch events are targeted at the second WindowTreeHost.
  ui::ScopedXI2Event scoped_xevent;
  scoped_xevent.InitTouchEvent(
      0, XI_TouchBegin, 5, gfx::Point(1500, 2500), valuators);
  window_tree_host1->Dispatch(scoped_xevent);
  window_tree_host2->Dispatch(scoped_xevent);
  EXPECT_EQ(ui::ET_UNKNOWN, handler1->last_touch_type());
  EXPECT_EQ(-1, handler1->last_touch_id());
  EXPECT_EQ(gfx::Point(0, 0), handler1->last_touch_location());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, handler2->last_touch_type());
  EXPECT_EQ(0, handler2->last_touch_id());
  EXPECT_EQ(gfx::Point(1500, 2500 - host2_y_offset),
            handler2->last_touch_location());

  scoped_xevent.InitTouchEvent(
      0, XI_TouchBegin, 6, gfx::Point(1600, 2600), valuators);
  window_tree_host1->Dispatch(scoped_xevent);
  window_tree_host2->Dispatch(scoped_xevent);
  EXPECT_EQ(ui::ET_UNKNOWN, handler1->last_touch_type());
  EXPECT_EQ(-1, handler1->last_touch_id());
  EXPECT_EQ(gfx::Point(0, 0), handler1->last_touch_location());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, handler2->last_touch_type());
  EXPECT_EQ(1, handler2->last_touch_id());
  EXPECT_EQ(gfx::Point(1600, 2600 - host2_y_offset),
            handler2->last_touch_location());

  scoped_xevent.InitTouchEvent(
      0, XI_TouchUpdate, 5, gfx::Point(1500, 2550), valuators);
  window_tree_host1->Dispatch(scoped_xevent);
  window_tree_host2->Dispatch(scoped_xevent);
  EXPECT_EQ(ui::ET_UNKNOWN, handler1->last_touch_type());
  EXPECT_EQ(-1, handler1->last_touch_id());
  EXPECT_EQ(gfx::Point(0, 0), handler1->last_touch_location());
  EXPECT_EQ(ui::ET_TOUCH_MOVED, handler2->last_touch_type());
  EXPECT_EQ(0, handler2->last_touch_id());
  EXPECT_EQ(gfx::Point(1500, 2550 - host2_y_offset),
            handler2->last_touch_location());

  scoped_xevent.InitTouchEvent(
      0, XI_TouchUpdate, 6, gfx::Point(1600, 2650), valuators);
  window_tree_host1->Dispatch(scoped_xevent);
  window_tree_host2->Dispatch(scoped_xevent);
  EXPECT_EQ(ui::ET_UNKNOWN, handler1->last_touch_type());
  EXPECT_EQ(-1, handler1->last_touch_id());
  EXPECT_EQ(gfx::Point(0, 0), handler1->last_touch_location());
  EXPECT_EQ(ui::ET_TOUCH_MOVED, handler2->last_touch_type());
  EXPECT_EQ(1, handler2->last_touch_id());
  EXPECT_EQ(gfx::Point(1600, 2650 - host2_y_offset),
            handler2->last_touch_location());

  scoped_xevent.InitTouchEvent(
      0, XI_TouchEnd, 5, gfx::Point(1500, 2550), valuators);
  window_tree_host1->Dispatch(scoped_xevent);
  window_tree_host2->Dispatch(scoped_xevent);
  EXPECT_EQ(ui::ET_UNKNOWN, handler1->last_touch_type());
  EXPECT_EQ(-1, handler1->last_touch_id());
  EXPECT_EQ(gfx::Point(0, 0), handler1->last_touch_location());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, handler2->last_touch_type());
  EXPECT_EQ(0, handler2->last_touch_id());
  EXPECT_EQ(gfx::Point(1500, 2550 - host2_y_offset),
            handler2->last_touch_location());

  scoped_xevent.InitTouchEvent(
      0, XI_TouchEnd, 6, gfx::Point(1600, 2650), valuators);
  window_tree_host1->Dispatch(scoped_xevent);
  window_tree_host2->Dispatch(scoped_xevent);
  EXPECT_EQ(ui::ET_UNKNOWN, handler1->last_touch_type());
  EXPECT_EQ(-1, handler1->last_touch_id());
  EXPECT_EQ(gfx::Point(0, 0), handler1->last_touch_location());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, handler2->last_touch_type());
  EXPECT_EQ(1, handler2->last_touch_id());
  EXPECT_EQ(gfx::Point(1600, 2650 - host2_y_offset),
            handler2->last_touch_location());

  // Revert the CrOS testing env otherwise the following non-CrOS aura
  // tests will fail.
  // Fake a ChromeOS running env.
  kLsbRelease = "";
  base::SysInfo::SetChromeOSVersionInfoForTest(kLsbRelease, base::Time());
}
#endif  // defined(OS_CHROMEOS)

}  // namespace aura
