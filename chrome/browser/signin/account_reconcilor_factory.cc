// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_reconcilor_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

AccountReconcilorFactory::AccountReconcilorFactory()
    : BrowserContextKeyedServiceFactory(
        "AccountReconcilor",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
}

AccountReconcilorFactory::~AccountReconcilorFactory() {}

// static
AccountReconcilor* AccountReconcilorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AccountReconcilor*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AccountReconcilorFactory* AccountReconcilorFactory::GetInstance() {
  return Singleton<AccountReconcilorFactory>::get();
}

KeyedService* AccountReconcilorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  AccountReconcilor* reconcilor =
      new AccountReconcilor(static_cast<Profile*>(context));
  reconcilor->Initialize(true /* start_reconcile_if_tokens_available */);
  return reconcilor;
}

void AccountReconcilorFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
}
