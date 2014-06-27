// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_WIDGET_TEST_H_
#define UI_VIEWS_TEST_WIDGET_TEST_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/native_widget_aura.h"

namespace views {

class NativeWidget;
class Widget;

namespace internal {

class RootView;

}  // namespace internal

namespace test {

// A widget that assumes mouse capture always works. It won't on Aura in
// testing, so we mock it.
class NativeWidgetCapture : public NativeWidgetAura {
 public:
  explicit NativeWidgetCapture(internal::NativeWidgetDelegate* delegate);
  virtual ~NativeWidgetCapture();

  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual bool HasCapture() const OVERRIDE;

 private:
  bool mouse_capture_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetCapture);
};

class WidgetTest : public ViewsTestBase {
 public:
  WidgetTest();
  virtual ~WidgetTest();

  NativeWidget* CreatePlatformNativeWidget(
      internal::NativeWidgetDelegate* delegate);

  Widget* CreateTopLevelPlatformWidget();

  Widget* CreateTopLevelFramelessPlatformWidget();

  Widget* CreateChildPlatformWidget(gfx::NativeView parent_native_view);

  Widget* CreateTopLevelNativeWidget();

  Widget* CreateChildNativeWidgetWithParent(Widget* parent);

  Widget* CreateChildNativeWidget();

  View* GetMousePressedHandler(internal::RootView* root_view);

  View* GetMouseMoveHandler(internal::RootView* root_view);

  View* GetGestureHandler(internal::RootView* root_view);

 private:
  DISALLOW_COPY_AND_ASSIGN(WidgetTest);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_WIDGET_TEST_H_
