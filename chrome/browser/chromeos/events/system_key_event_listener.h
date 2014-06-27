// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EVENTS_SYSTEM_KEY_EVENT_LISTENER_H_
#define CHROME_BROWSER_CHROMEOS_EVENTS_SYSTEM_KEY_EVENT_LISTENER_H_

#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"

typedef union _XEvent XEvent;

namespace chromeos {

class SystemKeyEventListener : public base::MessageLoopForUI::Observer {
 public:
  static void Initialize();
  static void Shutdown();
  // GetInstance returns NULL if not initialized or if already shutdown.
  static SystemKeyEventListener* GetInstance();

  void Stop();

 private:
  // Defines the delete on exit Singleton traits we like.  Best to have this
  // and const/dest private as recommended for Singletons.
  friend struct DefaultSingletonTraits<SystemKeyEventListener>;
  friend class SystemKeyEventListenerTest;

  SystemKeyEventListener();
  virtual ~SystemKeyEventListener();

  // MessageLoopForUI::Observer overrides.
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE;
  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE;

  // Returns true if the event was processed, false otherwise.
  virtual bool ProcessedXEvent(XEvent* xevent);

  bool stopped_;

  // Base X ID for events from the XKB extension.
  int xkb_event_base_;

  DISALLOW_COPY_AND_ASSIGN(SystemKeyEventListener);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EVENTS_SYSTEM_KEY_EVENT_LISTENER_H_
