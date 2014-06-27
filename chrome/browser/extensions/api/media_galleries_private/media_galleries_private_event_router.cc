// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaGalleriesPrivateEventRouter implementation.

#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_event_router.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/media_galleries_private.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

namespace media_galleries_private = extensions::api::media_galleries_private;

namespace extensions {

using media_galleries_private::GalleryChangeDetails;

MediaGalleriesPrivateEventRouter::MediaGalleriesPrivateEventRouter(
    Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

MediaGalleriesPrivateEventRouter::~MediaGalleriesPrivateEventRouter() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void MediaGalleriesPrivateEventRouter::OnGalleryChanged(
    MediaGalleryPrefId gallery_id,
    const std::set<std::string>& extension_ids) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  EventRouter* router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  if (!router->HasEventListener(
          media_galleries_private::OnGalleryChanged::kEventName))
    return;

  for (std::set<std::string>::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    GalleryChangeDetails details;
    details.gallery_id = gallery_id;
    scoped_ptr<base::ListValue> args(new base::ListValue());
    args->Append(details.ToValue().release());
    scoped_ptr<extensions::Event> event(new extensions::Event(
        media_galleries_private::OnGalleryChanged::kEventName,
        args.Pass()));
    // Use DispatchEventToExtension() instead of BroadcastEvent().
    // BroadcastEvent() sends the gallery changed events to all the extensions
    // who have added a listener to the onGalleryChanged event. There is a
    // chance that an extension might have added an onGalleryChanged() listener
    // without calling addGalleryWatch(). Therefore, use
    // DispatchEventToExtension() to dispatch the gallery changed event only to
    // the watching extensions.
    router->DispatchEventToExtension(*it, event.Pass());
  }
}

}  // namespace extensions
