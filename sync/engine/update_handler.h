// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_UPDATE_HANDLER_H_
#define SYNC_ENGINE_UPDATE_HANDLER_H_

#include <vector>

#include "sync/base/sync_export.h"

namespace sync_pb {
class DataTypeProgressMarker;
class SyncEntity;
}

typedef std::vector<const sync_pb::SyncEntity*> SyncEntityList;

namespace syncer {

namespace sessions {
class StatusController;
}

class ModelSafeWorker;

// This class represents an entity that can request, receive, and apply updates
// from the sync server.
class SYNC_EXPORT_PRIVATE UpdateHandler {
 public:
  UpdateHandler();
  virtual ~UpdateHandler() = 0;

  // Fills the given parameter with the stored progress marker for this type.
  virtual void GetDownloadProgress(
      sync_pb::DataTypeProgressMarker* progress_marker) const = 0;

  // Processes the contents of a GetUpdates response message.
  //
  // Should be invoked with the progress marker and set of SyncEntities from a
  // single GetUpdates response message.  The progress marker's type must match
  // this update handler's type, and the set of SyncEntities must include all
  // entities of this type found in the response message.
  //
  // In this context, "applicable_updates" means the set of updates belonging to
  // this type.
  virtual void ProcessGetUpdatesResponse(
      const sync_pb::DataTypeProgressMarker& progress_marker,
      const SyncEntityList& applicable_updates,
      sessions::StatusController* status) = 0;

  // Called at the end of a non-configure GetUpdates loop to apply any unapplied
  // updates.
  virtual void ApplyUpdates(sessions::StatusController* status) = 0;

  // Called at the end of a configure GetUpdates loop to perform any required
  // post-initial-download update application.
  virtual void PassiveApplyUpdates(sessions::StatusController* status) = 0;
};

}  // namespace syncer

#endif  // SYNC_ENGINE_UPDATE_HANDLER_H_
