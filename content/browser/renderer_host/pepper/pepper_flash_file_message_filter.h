// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FLASH_FILE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FLASH_FILE_MESSAGE_FILTER_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/process.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/host/resource_message_filter.h"

namespace content {
class BrowserPpapiHost;
}

namespace ppapi {
class PepperFilePath;
}

namespace ppapi {
namespace host {
struct HostMessageContext;
}
}

namespace content {

class BrowserPpapiHost;

// All file messages are handled by BrowserThread's blocking pool.
class PepperFlashFileMessageFilter : public ppapi::host::ResourceMessageFilter {
 public:
  PepperFlashFileMessageFilter(PP_Instance instance,
                               BrowserPpapiHost* host);

  static base::FilePath GetDataDirName(const base::FilePath& profile_path);

 private:
  virtual ~PepperFlashFileMessageFilter();

  // ppapi::host::ResourceMessageFilter overrides.
  virtual scoped_refptr<base::TaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& msg) OVERRIDE;
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  int32_t OnOpenFile(ppapi::host::HostMessageContext* context,
                     const ppapi::PepperFilePath& path,
                     int flags);
  int32_t OnRenameFile(ppapi::host::HostMessageContext* context,
                       const ppapi::PepperFilePath& from_path,
                       const ppapi::PepperFilePath& to_path);
  int32_t OnDeleteFileOrDir(ppapi::host::HostMessageContext* context,
                            const ppapi::PepperFilePath& path,
                            bool recursive);
  int32_t OnCreateDir(ppapi::host::HostMessageContext* context,
                      const ppapi::PepperFilePath& path);
  int32_t OnQueryFile(ppapi::host::HostMessageContext* context,
                      const ppapi::PepperFilePath& path);
  int32_t OnGetDirContents(ppapi::host::HostMessageContext* context,
                           const ppapi::PepperFilePath& path);
  int32_t OnCreateTemporaryFile(ppapi::host::HostMessageContext* context);

  base::FilePath ValidateAndConvertPepperFilePath(
      const ppapi::PepperFilePath& pepper_path,
      int flags);

  base::FilePath plugin_data_directory_;
  int render_process_id_;
  base::ProcessHandle plugin_process_handle_;

  DISALLOW_COPY_AND_ASSIGN(PepperFlashFileMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FLASH_FILE_MESSAGE_FILTER_H_
