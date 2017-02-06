// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_file_system_host.h"

#include "base/bind.h"
#include "base/callback.h"
#include "content/child/child_thread_impl.h"
#include "content/child/fileapi/file_system_dispatcher.h"
#include "content/common/pepper_file_util.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_system_util.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "storage/common/fileapi/file_system_util.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace content {

PepperFileSystemHost::PepperFileSystemHost(RendererPpapiHost* host,
                                           PP_Instance instance,
                                           PP_Resource resource,
                                           PP_FileSystemType type)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      type_(type),
      opened_(false),
      called_open_(false),
      weak_factory_(this) {}

PepperFileSystemHost::PepperFileSystemHost(RendererPpapiHost* host,
                                           PP_Instance instance,
                                           PP_Resource resource,
                                           const GURL& root_url,
                                           PP_FileSystemType type)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      type_(type),
      opened_(true),
      root_url_(root_url),
      called_open_(true),
      weak_factory_(this) {}

PepperFileSystemHost::~PepperFileSystemHost() {}

int32_t PepperFileSystemHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperFileSystemHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileSystem_Open,
                                      OnHostMsgOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_FileSystem_InitIsolatedFileSystem,
        OnHostMsgInitIsolatedFileSystem)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

bool PepperFileSystemHost::IsFileSystemHost() { return true; }

void PepperFileSystemHost::DidOpenFileSystem(
    const std::string& /* name_unused */,
    const GURL& root) {
  opened_ = true;
  root_url_ = root;
  reply_context_.params.set_result(PP_OK);
  host()->SendReply(reply_context_, PpapiPluginMsg_FileSystem_OpenReply());
  reply_context_ = ppapi::host::ReplyMessageContext();
}

void PepperFileSystemHost::DidFailOpenFileSystem(base::File::Error error) {
  int32_t pp_error = ppapi::FileErrorToPepperError(error);
  opened_ = (pp_error == PP_OK);
  reply_context_.params.set_result(pp_error);
  host()->SendReply(reply_context_, PpapiPluginMsg_FileSystem_OpenReply());
  reply_context_ = ppapi::host::ReplyMessageContext();
}

int32_t PepperFileSystemHost::OnHostMsgOpen(
    ppapi::host::HostMessageContext* context,
    int64_t expected_size) {
  // Not allow multiple opens.
  if (called_open_)
    return PP_ERROR_INPROGRESS;
  called_open_ = true;

  storage::FileSystemType file_system_type =
      PepperFileSystemTypeToFileSystemType(type_);
  if (file_system_type == storage::kFileSystemTypeUnknown)
    return PP_ERROR_FAILED;

  GURL document_url = renderer_ppapi_host_->GetDocumentURL(pp_instance());
  if (!document_url.is_valid())
    return PP_ERROR_FAILED;

  FileSystemDispatcher* file_system_dispatcher =
      ChildThreadImpl::current()->file_system_dispatcher();
  reply_context_ = context->MakeReplyMessageContext();
  file_system_dispatcher->OpenFileSystem(
      document_url.GetOrigin(),
      file_system_type,
      base::Bind(&PepperFileSystemHost::DidOpenFileSystem,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&PepperFileSystemHost::DidFailOpenFileSystem,
                 weak_factory_.GetWeakPtr()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileSystemHost::OnHostMsgInitIsolatedFileSystem(
    ppapi::host::HostMessageContext* context,
    const std::string& fsid,
    PP_IsolatedFileSystemType_Private type) {
  // Do not allow multiple opens.
  if (called_open_)
    return PP_ERROR_INPROGRESS;
  called_open_ = true;

  // Do a sanity check.
  if (!storage::ValidateIsolatedFileSystemId(fsid))
    return PP_ERROR_BADARGUMENT;

  RenderView* view =
      renderer_ppapi_host_->GetRenderViewForInstance(pp_instance());
  if (!view)
    return PP_ERROR_FAILED;

  const GURL& url = view->GetWebView()->mainFrame()->document().url();
  const std::string root_name = ppapi::IsolatedFileSystemTypeToRootName(type);
  if (root_name.empty())
    return PP_ERROR_BADARGUMENT;
  root_url_ = GURL(storage::GetIsolatedFileSystemRootURIString(
      url.GetOrigin(), fsid, root_name));
  opened_ = true;
  return PP_OK;
}

}  // namespace content
