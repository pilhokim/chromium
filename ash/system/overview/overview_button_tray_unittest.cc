// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/overview/overview_button_tray.h"

#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

OverviewButtonTray* GetTray() {
  return Shell::GetPrimaryRootWindowController()->shelf()->
      status_area_widget()->overview_button_tray();
}

OverviewButtonTray* GetSecondaryTray() {
  internal::RootWindowController* primary_controller =
      Shell::GetPrimaryRootWindowController();
  Shell::RootWindowControllerList controllers =
      Shell::GetAllRootWindowControllers();
  for (size_t i = 0; i < controllers.size(); ++i) {
    if (controllers[i] != primary_controller) {
      return controllers[i]->shelf()->
          status_area_widget()->overview_button_tray();
    }
  }

  return NULL;
}

}  // namespace

class OverviewButtonTrayTest : public test::AshTestBase {
 public:
  OverviewButtonTrayTest() {}
  virtual ~OverviewButtonTrayTest() {}

 protected:
  views::ImageView* GetImageView(OverviewButtonTray* tray) {
    return tray->icon_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverviewButtonTrayTest);
};

// Ensures that creation doesn't cause any crashes and adds the image icon.
TEST_F(OverviewButtonTrayTest, BasicConstruction) {
  EXPECT_TRUE(GetImageView(GetTray()) != NULL);
}

// Test that maximize mode toggle changes visibility.
// OverviewButtonTray should only be visible when MaximizeMode is enabled.
// By default the system should not have MaximizeMode enabled.
TEST_F(OverviewButtonTrayTest, MaximizeModeObserverOnMaximizeModeToggled) {
  ASSERT_FALSE(GetTray()->visible());
  Shell::GetInstance()->EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(GetTray()->visible());

  Shell::GetInstance()->EnableMaximizeModeWindowManager(false);
  EXPECT_FALSE(GetTray()->visible());
}

// Tests that activating this control brings up window selection mode.
TEST_F(OverviewButtonTrayTest, PerformAction) {
  ASSERT_FALSE(Shell::GetInstance()->window_selector_controller()->
      IsSelecting());

  // Overview Mode only works when there is a window
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  ui::GestureEvent tap(ui::ET_GESTURE_TAP, 0, 0, 0, base::TimeDelta(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP, 0.0f, 0.0f), 0);
  GetTray()->PerformAction(tap);
  EXPECT_TRUE(Shell::GetInstance()->window_selector_controller()->
      IsSelecting());
}

// Tests that a second OverviewButtonTray has been created, and only shows
// when MaximizeMode has been enabled,  when we are using multiple displays.
// By default the DisplayManger is in extended mode.
TEST_F(OverviewButtonTrayTest, DisplaysOnBothDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,200x200");
  EXPECT_FALSE(GetTray()->visible());
  EXPECT_FALSE(GetSecondaryTray()->visible());
  Shell::GetInstance()->EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(GetTray()->visible());
  EXPECT_TRUE(GetSecondaryTray()->visible());
  Shell::GetInstance()->EnableMaximizeModeWindowManager(false);
}

// Tests if Maximize Mode is enabled before a secondary display is attached
// that the second OverviewButtonTray should be created in a visible state.
TEST_F(OverviewButtonTrayTest, SecondaryTrayCreatedVisible) {
  if (!SupportsMultipleDisplays())
    return;

  Shell::GetInstance()->EnableMaximizeModeWindowManager(true);
  UpdateDisplay("400x400,200x200");
  EXPECT_TRUE(GetSecondaryTray()->visible());
  Shell::GetInstance()->EnableMaximizeModeWindowManager(false);
}

}  // namespace ash
