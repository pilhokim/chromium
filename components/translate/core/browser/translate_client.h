// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_CLIENT_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "components/translate/core/browser/translate_prefs.h"

class PrefService;
class TranslateAcceptLanguages;
class TranslateDriver;

// A client interface that needs to be supplied to TranslateManager by the
// embedder.
//
// Each client instance is associated with a given context within which a
// TranslateManager is used (e.g. a single tab).
class TranslateClient {
 public:
  // Gets the TranslateDriver associated with the client.
  virtual TranslateDriver* GetTranslateDriver() = 0;

  // Returns the associated PrefService.
  virtual PrefService* GetPrefs() = 0;

  // Returns the associated TranslatePrefs.
  virtual scoped_ptr<TranslatePrefs> GetTranslatePrefs() = 0;

  // Returns the associated TranslateAcceptLanguages.
  virtual TranslateAcceptLanguages* GetTranslateAcceptLanguages() = 0;
};

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_CLIENT_H_
