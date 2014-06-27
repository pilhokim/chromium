// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_WINDOW_STATE_H_
#define ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_WINDOW_STATE_H_

#include "ash/wm/window_state.h"

namespace ash {
namespace internal {
class MaximizeModeWindowManager;

// The MaximizeModeWindowState implementation which reduces all possible window
// states to minimized and maximized. If a window cannot be maximized it will be
// set to normal. If a window cannot fill the entire workspace it will be
// centered within the workspace.
class MaximizeModeWindowState : public wm::WindowState::State {
 public:
  // Called when the window position might need to be updated.
  static void UpdateWindowPosition(wm::WindowState* window_state,
                                   bool animated);

  // The |window|'s state object will be modified to use this new window mode
  // state handler. Upon destruction it will restore the previous state handler
  // and call |creator::WindowStateDestroyed()| to inform that the window mode
  // was reverted to the old window manager.
  MaximizeModeWindowState(aura::Window* window,
                          MaximizeModeWindowManager* creator);
  virtual ~MaximizeModeWindowState();

  // Leaves the maximize mode by reverting to previous state object.
  void LeaveMaximizeMode(wm::WindowState* window_state);

  // WindowState::State overrides:
  virtual void OnWMEvent(wm::WindowState* window_state,
                         const wm::WMEvent* event) OVERRIDE;

  virtual wm::WindowStateType GetType() const OVERRIDE;
  virtual void AttachState(wm::WindowState* window_state,
                           wm::WindowState::State* previous_state) OVERRIDE;
  virtual void DetachState(wm::WindowState* window_state) OVERRIDE;

 private:
  // Centers the window on top of the workspace or maximizes it. If |animate| is
  // set to true, a bounds change will be animated - otherwise immediate.
  void MaximizeOrCenterWindow(wm::WindowState* window_state, bool animate);

  // The original state object of the window.
  scoped_ptr<wm::WindowState::State> old_state_;

  // The state object for this object which owns this instance.
  aura::Window* window_;

  // The creator which needs to be informed when this state goes away.
  MaximizeModeWindowManager* creator_;

  // The current state type. Due to the nature of this state, this can only be
  // WM_STATE_TYPE{NORMAL, MINIMIZED, MAXIMIZED}.
  wm::WindowStateType current_state_type_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeModeWindowState);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_WINDOW_STATE_H_
