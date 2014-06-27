// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
struct PageTranslatedDetails;
class PrefService;
class TranslateClient;
class TranslateDriver;
struct TranslateErrorDetails;
class TranslateTabHelper;

namespace content {
class WebContents;
}

// The TranslateManager class is responsible for showing an info-bar when a page
// in a language different than the user language is loaded.  It triggers the
// page translation the user requests.
// TranslateManager expects its associated TranslateTabHelper to always have a
// valid WebContents (i.e. the WebContents is never destroyed within the
// lifetime of TranslateManager).

class TranslateManager : public content::NotificationObserver {
 public:
  // TranslateTabHelper is expected to outlive the TranslateManager.
  // |accept_language_pref_name| is the path for the preference for the
  // accept-languages.
  TranslateManager(TranslateTabHelper* helper,
                   const std::string& accept_language_pref_name);
  virtual ~TranslateManager();

  // Returns true if the URL can be translated.
  static bool IsTranslatableURL(const GURL& url);

  // Returns the language to translate to. The language returned is the
  // first language found in the following list that is supported by the
  // translation service:
  //     the UI language
  //     the accept-language list
  // If no language is found then an empty string is returned.
  static std::string GetTargetLanguage(
      const std::vector<std::string>& accept_languages_list);

  // Returns the language to automatically translate to. |original_language| is
  // the webpage's original language.
  static std::string GetAutoTargetLanguage(const std::string& original_language,
                                           PrefService* prefs);

  // Translates the page contents from |source_lang| to |target_lang|.
  // The actual translation might be performed asynchronously if the translate
  // script is not yet available.
  void TranslatePage(const std::string& source_lang,
                     const std::string& target_lang,
                     bool triggered_from_menu);

  // Reverts the contents of the page to its original language.
  void RevertTranslation();

  // Reports to the Google translate server that a page language was incorrectly
  // detected.  This call is initiated by the user selecting the "report" menu
  // under options in the translate infobar.
  void ReportLanguageDetectionError();

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Number of attempts before waiting for a page to be fully reloaded.
  void set_translate_max_reload_attemps(int attempts) {
    max_reload_check_attempts_ = attempts;
  }

  // Callback types for translate errors.
  typedef base::Callback<void(const TranslateErrorDetails&)>
      TranslateErrorCallback;
  typedef base::CallbackList<void(const TranslateErrorDetails&)>
      TranslateErrorCallbackList;

  // Registers a callback for translate errors.
  static scoped_ptr<TranslateErrorCallbackList::Subscription>
      RegisterTranslateErrorCallback(const TranslateErrorCallback& callback);

 private:
  // Starts the translation process for a page in the |page_lang| language.
  void InitiateTranslation(const std::string& page_lang);

  // Initiates translation once the page is finished loading.
  void InitiateTranslationPosted(const std::string& page_lang, int attempt);

  // Sends a translation request to the RenderView.
  void DoTranslatePage(const std::string& translate_script,
                       const std::string& source_lang,
                       const std::string& target_lang);

  // Shows the after translate or error infobar depending on the details.
  void PageTranslated(PageTranslatedDetails* details);

  // Called when the Translate script has been fetched.
  // Initiates the translation.
  void OnTranslateScriptFetchComplete(int page_id,
                                      const std::string& source_lang,
                                      const std::string& target_lang,
                                      bool success,
                                      const std::string& data);

  content::NotificationRegistrar notification_registrar_;

  // Max number of attempts before checking if a page has been reloaded.
  int max_reload_check_attempts_;

  // Preference name for the Accept-Languages HTTP header.
  std::string accept_languages_pref_name_;

  // TODO(droger): Remove all uses of |translate_tab_helper_|, use
  // TranslateClient and TranslateDriver instead.
  TranslateTabHelper* translate_tab_helper_;  // Weak.
  TranslateClient* translate_client_;         // Weak.
  TranslateDriver* translate_driver_;  // Weak.

  base::WeakPtrFactory<TranslateManager> weak_method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TranslateManager);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
