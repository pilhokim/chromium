// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/ime_bridge.h"

#include <map>
#include "base/logging.h"
#include "base/memory/singleton.h"

namespace chromeos {

static IMEBridge* g_ime_bridge = NULL;

// An implementation of IMEBridge.
class IMEBridgeImpl : public IMEBridge {
 public:
  IMEBridgeImpl()
    : input_context_handler_(NULL),
      engine_handler_(NULL),
      candidate_window_handler_(NULL) {
  }

  virtual ~IMEBridgeImpl() {
  }

  // IMEBridge override.
  virtual IMEInputContextHandlerInterface*
      GetInputContextHandler() const OVERRIDE {
    return input_context_handler_;
  }

  // IMEBridge override.
  virtual void SetInputContextHandler(
      IMEInputContextHandlerInterface* handler) OVERRIDE {
    input_context_handler_ = handler;
  }

  // IMEBridge override.
  virtual void SetEngineHandler(
      const std::string& engine_id,
      IMEEngineHandlerInterface* handler) OVERRIDE {
    DCHECK(!engine_id.empty());
    DCHECK(handler);
    engine_handler_map_[engine_id] = handler;
  }

  // IMEBridge override.
  virtual IMEEngineHandlerInterface* GetEngineHandler(
      const std::string& engine_id) OVERRIDE {
    if (engine_id.empty() ||
        engine_handler_map_.find(engine_id) == engine_handler_map_.end()) {
      return NULL;
    }
    return engine_handler_map_[engine_id];
  }

  // IMEBridge override.
  virtual void SetCurrentEngineHandler(
      IMEEngineHandlerInterface* handler) OVERRIDE {
    engine_handler_ = handler;
  }

  // IMEBridge override.
  virtual IMEEngineHandlerInterface* SetCurrentEngineHandlerById(
      const std::string& engine_id) OVERRIDE {
    std::map<std::string, IMEEngineHandlerInterface*>::const_iterator itor =
        engine_handler_map_.find(engine_id);
    // It is normal in that the engine is not found, because sometimes the
    // extension based xkb id may be provided but the xkb component extension
    // is not installed, for example, in browser_tests.
    if (itor == engine_handler_map_.end()) {
      engine_handler_ = NULL;
      return NULL;
    }

    engine_handler_ = engine_handler_map_[engine_id];
    return engine_handler_;
  }

  // IMEBridge override.
  virtual IMEEngineHandlerInterface* GetCurrentEngineHandler() const OVERRIDE {
    return engine_handler_;
  }

  // IMEBridge override.
  virtual IMECandidateWindowHandlerInterface* GetCandidateWindowHandler() const
      OVERRIDE {
    return candidate_window_handler_;
  }

  // IMEBridge override.
  virtual void SetCandidateWindowHandler(
      IMECandidateWindowHandlerInterface* handler) OVERRIDE {
    candidate_window_handler_ = handler;
  }

 private:
  IMEInputContextHandlerInterface* input_context_handler_;
  IMEEngineHandlerInterface* engine_handler_;
  IMECandidateWindowHandlerInterface* candidate_window_handler_;
  std::map<std::string, IMEEngineHandlerInterface*> engine_handler_map_;

  DISALLOW_COPY_AND_ASSIGN(IMEBridgeImpl);
};

///////////////////////////////////////////////////////////////////////////////
// IMEBridge
IMEBridge::IMEBridge() {
}

IMEBridge::~IMEBridge() {
}

// static.
void IMEBridge::Initialize() {
  if (!g_ime_bridge)
    g_ime_bridge = new IMEBridgeImpl();
}

// static.
void IMEBridge::Shutdown() {
  delete g_ime_bridge;
  g_ime_bridge = NULL;
}

// static.
IMEBridge* IMEBridge::Get() {
  return g_ime_bridge;
}

}  // namespace chromeos
