// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_FILE_IO_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_FILE_IO_HOST_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/file_proxy.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/shared_impl/file_io_state_manager.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"

namespace ppapi {
struct FileGrowth;
}

namespace content {
class PepperFileSystemBrowserHost;

class PepperFileIOHost : public ppapi::host::ResourceHost,
                         public base::SupportsWeakPtr<PepperFileIOHost> {
 public:
  typedef base::Callback<void(base::File::Error)> NotifyCloseFileCallback;

  PepperFileIOHost(BrowserPpapiHostImpl* host,
                   PP_Instance instance,
                   PP_Resource resource);
  virtual ~PepperFileIOHost();

  // ppapi::host::ResourceHost override.
  virtual int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) OVERRIDE;

  struct UIThreadStuff {
    UIThreadStuff();
    ~UIThreadStuff();
    base::ProcessId resolved_render_process_id;
    scoped_refptr<fileapi::FileSystemContext> file_system_context;
  };

 private:
  int32_t OnHostMsgOpen(ppapi::host::HostMessageContext* context,
                        PP_Resource file_ref_resource,
                        int32_t open_flags);
  int32_t OnHostMsgTouch(ppapi::host::HostMessageContext* context,
                         PP_Time last_access_time,
                         PP_Time last_modified_time);
  int32_t OnHostMsgSetLength(ppapi::host::HostMessageContext* context,
                             int64_t length);
  int32_t OnHostMsgClose(ppapi::host::HostMessageContext* context,
                         const ppapi::FileGrowth& file_growth);
  int32_t OnHostMsgFlush(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgRequestOSFileHandle(
      ppapi::host::HostMessageContext* context);

  void GotPluginAllowedToCallRequestOSFileHandle(
      ppapi::host::ReplyMessageContext reply_context,
      bool plugin_allowed);

  // Callback handlers. These mostly convert the File::Error to the
  // PP_Error code and send back the reply. Note that the argument
  // ReplyMessageContext is copied so that we have a closure containing all
  // necessary information to reply.
  void ExecutePlatformGeneralCallback(
      ppapi::host::ReplyMessageContext reply_context,
      base::File::Error error_code);

  void OnOpenProxyCallback(ppapi::host::ReplyMessageContext reply_context,
                           base::File::Error error_code);

  void GotUIThreadStuffForInternalFileSystems(
      ppapi::host::ReplyMessageContext reply_context,
      int platform_file_flags,
      UIThreadStuff ui_thread_stuff);
  void DidOpenInternalFile(ppapi::host::ReplyMessageContext reply_context,
                           base::File file,
                           const base::Closure& on_close_callback);
  void GotResolvedRenderProcessId(
      ppapi::host::ReplyMessageContext reply_context,
      base::FilePath path,
      int file_flags,
      base::ProcessId resolved_render_process_id);

  void DidOpenQuotaFile(ppapi::host::ReplyMessageContext reply_context,
                        base::File file,
                        int64_t max_written_offset);
  bool CallSetLength(ppapi::host::ReplyMessageContext reply_context,
                     int64_t length);

  void DidCloseFile(base::File::Error error);

  void SendOpenErrorReply(ppapi::host::ReplyMessageContext reply_context);

  // Adds file_ to |reply_context| with the specified |open_flags|.
  bool AddFileToReplyContext(
      int32_t open_flags,
      ppapi::host::ReplyMessageContext* reply_context) const;

  BrowserPpapiHostImpl* browser_ppapi_host_;

  RenderProcessHost* render_process_host_;
  int render_process_id_;
  base::ProcessId resolved_render_process_id_;

  base::FileProxy file_;
  int32_t open_flags_;

  // The file system type specified in the Open() call. This will be
  // PP_FILESYSTEMTYPE_INVALID before open was called. This value does not
  // indicate that the open command actually succeeded.
  PP_FileSystemType file_system_type_;
  base::WeakPtr<PepperFileSystemBrowserHost> file_system_host_;

  // Valid only for PP_FILESYSTEMTYPE_LOCAL{PERSISTENT,TEMPORARY}.
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  fileapi::FileSystemURL file_system_url_;
  base::Closure on_close_callback_;
  int64_t max_written_offset_;
  bool check_quota_;

  ppapi::FileIOStateManager state_manager_;

  DISALLOW_COPY_AND_ASSIGN(PepperFileIOHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_FILE_IO_HOST_H_
