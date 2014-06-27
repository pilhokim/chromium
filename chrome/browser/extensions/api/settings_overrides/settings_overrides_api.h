// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

class TemplateURL;

namespace extensions {

class SettingsOverridesAPI : public BrowserContextKeyedAPI,
                             public content::NotificationObserver {
 public:
  explicit SettingsOverridesAPI(content::BrowserContext* context);
  virtual ~SettingsOverridesAPI();

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SettingsOverridesAPI>*
      GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<SettingsOverridesAPI>;

  typedef std::set<scoped_refptr<const Extension> > PendingExtensions;

  // Wrappers around PreferenceAPI.
  void SetPref(const std::string& extension_id,
               const std::string& pref_key,
               base::Value* value);
  void UnsetPref(const std::string& extension_id,
                 const std::string& pref_key);
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  void OnTemplateURLsLoaded();

  void RegisterSearchProvider(const Extension* extension) const;
  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "SettingsOverridesAPI"; }

  Profile* profile_;
  TemplateURLService* url_service_;

  // List of extensions waiting for the TemplateURLService to Load to
  // have search provider registered.
  PendingExtensions pending_extensions_;

  content::NotificationRegistrar registrar_;
  scoped_ptr<TemplateURLService::Subscription> template_url_sub_;

  DISALLOW_COPY_AND_ASSIGN(SettingsOverridesAPI);
};

template <>
void BrowserContextKeyedAPIFactory<
    SettingsOverridesAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_OVERRIDES_SETTINGS_OVERRIDES_API_H_
