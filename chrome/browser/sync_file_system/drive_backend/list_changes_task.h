// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LIST_CHANGES_TASK_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LIST_CHANGES_TASK_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "google_apis/drive/gdata_errorcode.h"

namespace drive {
class DriveServiceInterface;
}

namespace google_apis {
class ChangeResource;
class ResourceList;
}

namespace sync_file_system {
namespace drive_backend {

class MetadataDatabase;
class SyncEngineContext;

class ListChangesTask : public SyncTask {
 public:
  explicit ListChangesTask(SyncEngineContext* sync_context);
  virtual ~ListChangesTask();

  virtual void Run(scoped_ptr<SyncTaskToken> token) OVERRIDE;

 private:
  void DidListChanges(scoped_ptr<SyncTaskToken> token,
                      google_apis::GDataErrorCode error,
                      scoped_ptr<google_apis::ResourceList> resource_list);

  bool IsContextReady();
  MetadataDatabase* metadata_database();
  drive::DriveServiceInterface* drive_service();

  SyncEngineContext* sync_context_;
  ScopedVector<google_apis::ChangeResource> change_list_;

  base::WeakPtrFactory<ListChangesTask> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ListChangesTask);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LIST_CHANGES_TASK_H_
