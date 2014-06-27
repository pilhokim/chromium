// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_

#include <string>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace content {
class RenderProcessHost;
class RenderViewHost;
class SiteInstance;
struct CustomContextMenuContext;

// The interface provides a communication conduit with a frame in the renderer.
class CONTENT_EXPORT RenderFrameHost : public IPC::Listener,
                                       public IPC::Sender {
 public:
  // Returns the RenderFrameHost given its ID and the ID of its render process.
  // Returns NULL if the IDs do not correspond to a live RenderFrameHost.
  static RenderFrameHost* FromID(int render_process_id, int render_frame_id);

  virtual ~RenderFrameHost() {}

  // Returns the route id for this frame.
  virtual int GetRoutingID() = 0;

  // Returns the SiteInstance grouping all RenderFrameHosts that have script
  // access to this RenderFrameHost, and must therefore live in the same
  // process.
  virtual SiteInstance* GetSiteInstance() = 0;

  // Returns the process for this frame.
  virtual RenderProcessHost* GetProcess() = 0;

  // Returns the current RenderFrameHost of the parent frame, or NULL if there
  // is no parent. The result may be in a different process than the current
  // RenderFrameHost.
  virtual RenderFrameHost* GetParent() = 0;

  // Returns the assigned name of the frame, the name of the iframe tag
  // declaring it. For example, <iframe name="framename">[...]</iframe>. It is
  // quite possible for a frame to have no name, in which case GetFrameName will
  // return an empty string.
  virtual const std::string& GetFrameName() = 0;

  // Returns true if the frame is out of process.
  virtual bool IsCrossProcessSubframe() = 0;

  // Returns the last committed URL of the frame.
  virtual GURL GetLastCommittedURL() = 0;

  // Returns the associated widget's native view.
  virtual gfx::NativeView GetNativeView() = 0;

  // Runs the beforeunload handler for this frame.  The result will be returned
  // via FrameMsg_BeforeUnload_ACK.  Currently only used for the main frame.
  // See also ClosePage and SwapOut in RenderViewHost, which run the unload
  // handler.
  //
  // |for_cross_site_transition| indicates whether this call is for the current
  // frame during a cross-process navigation.  False means we're closing the
  // entire tab.
  //
  // TODO(creis): We should run the beforeunload handler for every frame that
  // has one.
  // TODO(creis): This should be public on the Navigator interface instead of
  // RenderFrameHost, since callers outside content shouldn't need to call this
  // directly on subframes.
  virtual void DispatchBeforeUnload(bool for_cross_site_transition) = 0;

  // Let the renderer know that the menu has been closed.
  virtual void NotifyContextMenuClosed(
      const CustomContextMenuContext& context) = 0;

  // Executes custom context menu action that was provided from Blink.
  virtual void ExecuteCustomContextMenuCommand(
      int action, const CustomContextMenuContext& context) = 0;

  // Edit operations.
  virtual void Undo() = 0;
  virtual void Redo() = 0;
  virtual void Cut() = 0;
  virtual void Copy() = 0;
  virtual void CopyToFindPboard() = 0;
  virtual void Paste() = 0;
  virtual void PasteAndMatchStyle() = 0;
  virtual void Delete() = 0;
  virtual void SelectAll() = 0;
  virtual void Unselect() = 0;

  // Requests the renderer to insert CSS into the frame's document.
  virtual void InsertCSS(const std::string& css) = 0;

  // Runs some JavaScript in this frame's context. If a callback is provided, it
  // will be used to return the result, when the result is available.
  typedef base::Callback<void(const base::Value*)> JavaScriptResultCallback;
  virtual void ExecuteJavaScript(const base::string16& javascript) = 0;
  virtual void ExecuteJavaScript(const base::string16& javascript,
                                 const JavaScriptResultCallback& callback) = 0;

  // Temporary until we get rid of RenderViewHost.
  virtual RenderViewHost* GetRenderViewHost() = 0;

 private:
  // This interface should only be implemented inside content.
  friend class RenderFrameHostImpl;
  RenderFrameHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
