// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/rect.h"

class PasswordGenerationManager;
class PasswordManager;
class Profile;

namespace autofill {
class PasswordGenerationPopupObserver;
class PasswordGenerationPopupControllerImpl;
}

namespace content {
class WebContents;
}

// ChromePasswordManagerClient implements the PasswordManagerClient interface.
class ChromePasswordManagerClient
    : public PasswordManagerClient,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ChromePasswordManagerClient> {
 public:
  virtual ~ChromePasswordManagerClient();

  // PasswordManagerClient implementation.
  virtual void PromptUserToSavePassword(PasswordFormManager* form_to_save)
      OVERRIDE;
  virtual void PasswordWasAutofilled(
      const autofill::PasswordFormMap& best_matches) const OVERRIDE;
  virtual void AuthenticateAutofillAndFillForm(
      scoped_ptr<autofill::PasswordFormFillData> fill_data) OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual PasswordStore* GetPasswordStore() OVERRIDE;
  virtual PasswordManagerDriver* GetDriver() OVERRIDE;
  virtual base::FieldTrial::Probability GetProbabilityForExperiment(
      const std::string& experiment_name) OVERRIDE;
  virtual bool IsPasswordSyncEnabled() OVERRIDE;
  virtual void SetLogger(PasswordManagerLogger* logger) OVERRIDE;
  virtual void LogSavePasswordProgress(const std::string& text) OVERRIDE;

  // Hides any visible generation UI.
  void HidePasswordGenerationPopup();

  // Convenience method to allow //chrome code easy access to a PasswordManager
  // from a WebContents instance.
  static PasswordManager* GetManagerFromWebContents(
      content::WebContents* contents);

  // Convenience method to allow //chrome code easy access to a
  // PasswordGenerationManager from a WebContents instance.
  static PasswordGenerationManager* GetGenerationManagerFromWebContents(
      content::WebContents* contents);

  // Observer for PasswordGenerationPopup events. Used for testing.
  void SetTestObserver(autofill::PasswordGenerationPopupObserver* observer);

 private:
  explicit ChromePasswordManagerClient(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ChromePasswordManagerClient>;

  // content::WebContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Callback method to be triggered when authentication is successful for a
  // given password authentication request.  If authentication is disabled or
  // not supported, this will be triggered directly.
  void CommitFillPasswordForm(autofill::PasswordFormFillData* fill_data);

  // Given |bounds| in the renderers coordinate system, return the same bounds
  // in the screens coordinate system.
  gfx::RectF GetBoundsInScreenSpace(const gfx::RectF& bounds);

  // Causes the password generation UI to be shown for the specified form.
  // The popup will be anchored at |element_bounds|. The generated password
  // will be no longer than |max_length|.
  void ShowPasswordGenerationPopup(const gfx::RectF& bounds,
                                   int max_length,
                                   const autofill::PasswordForm& form);

  // Causes the password editing UI to be shown anchored at |element_bounds|.
  void ShowPasswordEditingPopup(
      const gfx::RectF& bounds, const autofill::PasswordForm& form);

  Profile* GetProfile();

  ContentPasswordManagerDriver driver_;

  // Observer for password generation popup.
  autofill::PasswordGenerationPopupObserver* observer_;

  // Controls the popup
  base::WeakPtr<
    autofill::PasswordGenerationPopupControllerImpl> popup_controller_;

  // Allows authentication callbacks to be destroyed when this client is gone.
  base::WeakPtrFactory<ChromePasswordManagerClient> weak_factory_;

  // Points to an active logger instance to use for, e.g., reporting progress on
  // saving passwords. If there is no active logger (most of the time), the
  // pointer will be NULL.
  PasswordManagerLogger* logger_;

  DISALLOW_COPY_AND_ASSIGN(ChromePasswordManagerClient);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_PASSWORD_MANAGER_CLIENT_H_
