// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_file_chooser_host.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/pepper_file_ref_renderer_host.h"
#include "content/renderer/render_view_impl.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebFileChooserCompletion.h"
#include "third_party/WebKit/public/web/WebFileChooserParams.h"

namespace content {

class PepperFileChooserHost::CompletionHandler
    : public blink::WebFileChooserCompletion {
 public:
  explicit CompletionHandler(const base::WeakPtr<PepperFileChooserHost>& host)
      : host_(host) {}

  virtual ~CompletionHandler() {}

  virtual void didChooseFile(
      const blink::WebVector<blink::WebString>& file_names) {
    if (host_.get()) {
      std::vector<PepperFileChooserHost::ChosenFileInfo> files;
      for (size_t i = 0; i < file_names.size(); i++) {
        files.push_back(PepperFileChooserHost::ChosenFileInfo(
            file_names[i].utf8(), std::string()));
      }
      host_->StoreChosenFiles(files);
    }

    // It is the responsibility of this method to delete the instance.
    delete this;
  }
  virtual void didChooseFile(
      const blink::WebVector<SelectedFileInfo>& file_names) {
    if (host_.get()) {
      std::vector<PepperFileChooserHost::ChosenFileInfo> files;
      for (size_t i = 0; i < file_names.size(); i++) {
        files.push_back(PepperFileChooserHost::ChosenFileInfo(
            file_names[i].path.utf8(), file_names[i].displayName.utf8()));
      }
      host_->StoreChosenFiles(files);
    }

    // It is the responsibility of this method to delete the instance.
    delete this;
  }

 private:
  base::WeakPtr<PepperFileChooserHost> host_;

  DISALLOW_COPY_AND_ASSIGN(CompletionHandler);
};

PepperFileChooserHost::ChosenFileInfo::ChosenFileInfo(
    const std::string& path,
    const std::string& display_name)
    : path(path), display_name(display_name) {}

PepperFileChooserHost::PepperFileChooserHost(RendererPpapiHost* host,
                                             PP_Instance instance,
                                             PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      handler_(NULL),
      weak_factory_(this) {}

PepperFileChooserHost::~PepperFileChooserHost() {}

int32_t PepperFileChooserHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperFileChooserHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileChooser_Show, OnShow)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

void PepperFileChooserHost::StoreChosenFiles(
    const std::vector<ChosenFileInfo>& files) {
  std::vector<IPC::Message> create_msgs;
  std::vector<base::FilePath> file_paths;
  std::vector<std::string> display_names;
  for (size_t i = 0; i < files.size(); i++) {
#if defined(OS_WIN)
    base::FilePath file_path(base::UTF8ToWide(files[i].path));
#else
    base::FilePath file_path(files[i].path);
#endif
    file_paths.push_back(file_path);
    create_msgs.push_back(PpapiHostMsg_FileRef_CreateForRawFS(file_path));
    display_names.push_back(files[i].display_name);
  }

  if (!files.empty()) {
    renderer_ppapi_host_->CreateBrowserResourceHosts(
        pp_instance(),
        create_msgs,
        base::Bind(&PepperFileChooserHost::DidCreateResourceHosts,
                   weak_factory_.GetWeakPtr(),
                   file_paths,
                   display_names));
  } else {
    reply_context_.params.set_result(PP_ERROR_USERCANCEL);
    std::vector<ppapi::FileRefCreateInfo> chosen_files;
    host()->SendReply(reply_context_,
                      PpapiPluginMsg_FileChooser_ShowReply(chosen_files));
    reply_context_ = ppapi::host::ReplyMessageContext();
    handler_ = NULL;  // Handler deletes itself.
  }
}

int32_t PepperFileChooserHost::OnShow(
    ppapi::host::HostMessageContext* context,
    bool save_as,
    bool open_multiple,
    const std::string& suggested_file_name,
    const std::vector<std::string>& accept_mime_types) {
  if (handler_)
    return PP_ERROR_INPROGRESS;  // Already pending.

  if (!host()->permissions().HasPermission(
          ppapi::PERMISSION_BYPASS_USER_GESTURE) &&
      !renderer_ppapi_host_->HasUserGesture(pp_instance())) {
    return PP_ERROR_NO_USER_GESTURE;
  }

  blink::WebFileChooserParams params;
  if (save_as) {
    params.saveAs = true;
    params.initialValue = blink::WebString::fromUTF8(
        suggested_file_name.data(), suggested_file_name.size());
  } else {
    params.multiSelect = open_multiple;
  }
  std::vector<blink::WebString> mine_types(accept_mime_types.size());
  for (size_t i = 0; i < accept_mime_types.size(); i++) {
    mine_types[i] = blink::WebString::fromUTF8(accept_mime_types[i].data(),
                                               accept_mime_types[i].size());
  }
  params.acceptTypes = mine_types;
  params.directory = false;

  handler_ = new CompletionHandler(AsWeakPtr());
  RenderViewImpl* render_view = static_cast<RenderViewImpl*>(
      renderer_ppapi_host_->GetRenderViewForInstance(pp_instance()));
  if (!render_view || !render_view->runFileChooser(params, handler_)) {
    delete handler_;
    handler_ = NULL;
    return PP_ERROR_NOACCESS;
  }

  reply_context_ = context->MakeReplyMessageContext();
  return PP_OK_COMPLETIONPENDING;
}

void PepperFileChooserHost::DidCreateResourceHosts(
    const std::vector<base::FilePath>& file_paths,
    const std::vector<std::string>& display_names,
    const std::vector<int>& browser_ids) {
  DCHECK(file_paths.size() == display_names.size());
  DCHECK(file_paths.size() == browser_ids.size());

  std::vector<ppapi::FileRefCreateInfo> chosen_files;
  for (size_t i = 0; i < browser_ids.size(); ++i) {
    PepperFileRefRendererHost* renderer_host = new PepperFileRefRendererHost(
        renderer_ppapi_host_, pp_instance(), 0, file_paths[i]);
    int renderer_id =
        renderer_ppapi_host_->GetPpapiHost()->AddPendingResourceHost(
            scoped_ptr<ppapi::host::ResourceHost>(renderer_host));
    ppapi::FileRefCreateInfo info = ppapi::MakeExternalFileRefCreateInfo(
        file_paths[i], display_names[i], browser_ids[i], renderer_id);
    chosen_files.push_back(info);
  }

  reply_context_.params.set_result(PP_OK);
  host()->SendReply(reply_context_,
                    PpapiPluginMsg_FileChooser_ShowReply(chosen_files));
  reply_context_ = ppapi::host::ReplyMessageContext();
  handler_ = NULL;  // Handler deletes itself.
}

}  // namespace content
