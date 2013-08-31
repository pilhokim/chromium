// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/profile_resetter.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/management_policy.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"

ProfileResetter::ProfileResetter(Profile* profile)
    : profile_(profile),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile_)),
      pending_reset_flags_(0),
      cookies_remover_(NULL) {
  DCHECK(CalledOnValidThread());
  DCHECK(profile_);
  registrar_.Add(this, chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
                 content::Source<TemplateURLService>(template_url_service_));
}

ProfileResetter::~ProfileResetter() {
  if (cookies_remover_)
    cookies_remover_->RemoveObserver(this);
}

void ProfileResetter::Reset(ProfileResetter::ResettableFlags resettable_flags,
                            const base::Closure& callback) {
  DCHECK(CalledOnValidThread());

  // We should never be called with unknown flags.
  CHECK_EQ(static_cast<ResettableFlags>(0), resettable_flags & ~ALL);

  // We should never be called when a previous reset has not finished.
  CHECK_EQ(static_cast<ResettableFlags>(0), pending_reset_flags_);

  callback_ = callback;

  // These flags are set to false by the individual reset functions.
  pending_reset_flags_ = resettable_flags;

  struct {
    Resettable flag;
    void (ProfileResetter::*method)();
  } flag2Method [] = {
      { DEFAULT_SEARCH_ENGINE, &ProfileResetter::ResetDefaultSearchEngine },
      { HOMEPAGE, &ProfileResetter::ResetHomepage },
      { CONTENT_SETTINGS, &ProfileResetter::ResetContentSettings },
      { COOKIES_AND_SITE_DATA, &ProfileResetter::ResetCookiesAndSiteData },
      { EXTENSIONS, &ProfileResetter::ResetExtensions },
      { STARTUP_PAGES, &ProfileResetter::ResetStartupPages },
      { PINNED_TABS, &ProfileResetter::ResetPinnedTabs },
  };

  ResettableFlags reset_triggered_for_flags = 0;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(flag2Method); ++i) {
    if (resettable_flags & flag2Method[i].flag) {
      reset_triggered_for_flags |= flag2Method[i].flag;
      (this->*flag2Method[i].method)();
    }
  }

  DCHECK_EQ(resettable_flags, reset_triggered_for_flags);
}

bool ProfileResetter::IsActive() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return pending_reset_flags_ != 0;
}

void ProfileResetter::MarkAsDone(Resettable resettable) {
  DCHECK(CalledOnValidThread());

  // Check that we are never called twice or unexpectedly.
  CHECK(pending_reset_flags_ & resettable);

  pending_reset_flags_ &= ~resettable;

  if (!pending_reset_flags_) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     callback_);
    callback_.Reset();
  }
}

void ProfileResetter::ResetDefaultSearchEngine() {
  DCHECK(CalledOnValidThread());
  DCHECK(template_url_service_);

  // If TemplateURLServiceFactory is ready we can clean it right now.
  // Otherwise, load it and continue from ProfileResetter::Observe.
  if (template_url_service_->loaded()) {
    TemplateURLPrepopulateData::ClearPrepopulatedEnginesInPrefs(profile_);
    template_url_service_->ResetNonExtensionURLs();

    // Reset Google search URL.
    PrefService* prefs = profile_->GetPrefs();
    DCHECK(prefs);
    prefs->ClearPref(prefs::kLastPromptedGoogleURL);
    const TemplateURL* default_search_provider =
        template_url_service_->GetDefaultSearchProvider();
    if (default_search_provider &&
        default_search_provider->url_ref().HasGoogleBaseURLs())
      GoogleURLTracker::RequestServerCheck(profile_, true);

    MarkAsDone(DEFAULT_SEARCH_ENGINE);
  } else {
    template_url_service_->Load();
  }
}

void ProfileResetter::ResetHomepage() {
  DCHECK(CalledOnValidThread());
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  prefs->ClearPref(prefs::kHomePageIsNewTabPage);
  prefs->ClearPref(prefs::kHomePage);
  prefs->ClearPref(prefs::kShowHomeButton);
  MarkAsDone(HOMEPAGE);
}

void ProfileResetter::ResetContentSettings() {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  // TODO(battre/vabr): Implement
  MarkAsDone(CONTENT_SETTINGS);
}

void ProfileResetter::ResetCookiesAndSiteData() {
  DCHECK(CalledOnValidThread());
  DCHECK(!cookies_remover_);

  cookies_remover_ = BrowsingDataRemover::CreateForUnboundedRange(profile_);
  cookies_remover_->AddObserver(this);
  int remove_mask = BrowsingDataRemover::REMOVE_SITE_DATA |
                    BrowsingDataRemover::REMOVE_CACHE;
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  // Don't try to clear LSO data if it's not supported.
  if (!prefs->GetBoolean(prefs::kClearPluginLSODataEnabled))
    remove_mask &= ~BrowsingDataRemover::REMOVE_PLUGIN_DATA;
  cookies_remover_->Remove(remove_mask, BrowsingDataHelper::UNPROTECTED_WEB);
}

void ProfileResetter::ResetExtensions() {
  DCHECK(CalledOnValidThread());
  ExtensionService* extension_service = profile_->GetExtensionService();
  DCHECK(extension_service);
  extension_service->DisableUserExtensions();

  MarkAsDone(EXTENSIONS);
}

void ProfileResetter::ResetStartupPages() {
  DCHECK(CalledOnValidThread());
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  prefs->ClearPref(prefs::kRestoreOnStartup);
  prefs->ClearPref(prefs::kURLsToRestoreOnStartup);
  prefs->SetBoolean(prefs::kRestoreOnStartupMigrated, true);
  MarkAsDone(STARTUP_PAGES);
}

void ProfileResetter::ResetPinnedTabs() {
  // Unpin all the tabs.
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if (it->is_type_tabbed() && it->profile() == profile_) {
      TabStripModel* tab_model = it->tab_strip_model();
      // Here we assume that indexof(any mini tab) < indexof(any normal tab).
      // If we unpin the tab, it can be moved to the right. Thus traversing in
      // reverse direction is correct.
      for (int i = tab_model->count() - 1; i >= 0; --i) {
        if (tab_model->IsTabPinned(i) && !tab_model->IsAppTab(i))
          tab_model->SetTabPinned(i, false);
      }
    }
  }
  MarkAsDone(PINNED_TABS);
}

void ProfileResetter::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK(CalledOnValidThread());
  // TemplateURLService has loaded. If we need to clean search engines, it's
  // time to go on.
  if (pending_reset_flags_ & DEFAULT_SEARCH_ENGINE)
    ResetDefaultSearchEngine();
}

void ProfileResetter::OnBrowsingDataRemoverDone() {
  cookies_remover_ = NULL;
  MarkAsDone(COOKIES_AND_SITE_DATA);
}
