// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_file_system_browser_host.h"

#include "base/bind.h"
#include "base/callback.h"
#include "content/browser/renderer_host/pepper/pepper_file_io_host.h"
#include "content/browser/renderer_host/pepper/quota_reservation.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/pepper_plugin_info.h"
#include "net/base/mime_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_system_util.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/quota/quota_manager_proxy.h"
#include "webkit/common/fileapi/file_system_util.h"
#include "webkit/common/quota/quota_types.h"

namespace content {

namespace {

// This is the minimum amount of quota we reserve per file system.
const int64_t kMinimumQuotaReservationSize = 1024 * 1024;  // 1 MB

scoped_refptr<fileapi::FileSystemContext> GetFileSystemContextFromRenderId(
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  if (!host)
    return NULL;
  StoragePartition* storage_partition = host->GetStoragePartition();
  if (!storage_partition)
    return NULL;
  return storage_partition->GetFileSystemContext();
}

}  // namespace

PepperFileSystemBrowserHost::PepperFileSystemBrowserHost(BrowserPpapiHost* host,
                                                         PP_Instance instance,
                                                         PP_Resource resource,
                                                         PP_FileSystemType type)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      browser_ppapi_host_(host),
      type_(type),
      called_open_(false),
      opened_(false),
      file_system_context_(NULL),
      reserved_quota_(0),
      reserving_quota_(false),
      weak_factory_(this) {}

PepperFileSystemBrowserHost::~PepperFileSystemBrowserHost() {
  // If |files_| is not empty, the plugin failed to close some files. It must
  // have crashed.
  if (!files_.empty()) {
    file_system_context_->default_file_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&QuotaReservation::OnClientCrash, quota_reservation_));
  }

  // All FileRefs and FileIOs that reference us must have been destroyed. Cancel
  // all pending file system operations.
  if (file_system_operation_runner_)
    file_system_operation_runner_->Shutdown();
}

void PepperFileSystemBrowserHost::OpenExisting(const GURL& root_url,
                                               const base::Closure& callback) {
  root_url_ = root_url;
  int render_process_id = 0;
  int unused;
  if (!browser_ppapi_host_->GetRenderFrameIDsForInstance(
          pp_instance(), &render_process_id, &unused)) {
    NOTREACHED();
  }
  called_open_ = true;
  // Get the file system context asynchronously, and then complete the Open
  // operation by calling |callback|.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GetFileSystemContextFromRenderId, render_process_id),
      base::Bind(&PepperFileSystemBrowserHost::OpenExistingFileSystem,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

int32_t PepperFileSystemBrowserHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperFileSystemBrowserHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileSystem_Open,
                                      OnHostMsgOpen)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_FileSystem_InitIsolatedFileSystem,
        OnHostMsgInitIsolatedFileSystem)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_FileSystem_ReserveQuota,
                                      OnHostMsgReserveQuota)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

bool PepperFileSystemBrowserHost::IsFileSystemHost() { return true; }

void PepperFileSystemBrowserHost::OpenQuotaFile(
    PepperFileIOHost* file_io_host,
    const fileapi::FileSystemURL& url,
    const OpenQuotaFileCallback& callback) {
  int32_t id = file_io_host->pp_resource();
  std::pair<FileMap::iterator, bool> insert_result =
      files_.insert(std::make_pair(id, file_io_host));
  if (insert_result.second) {
    base::PostTaskAndReplyWithResult(
        file_system_context_->default_file_task_runner(),
        FROM_HERE,
        base::Bind(&QuotaReservation::OpenFile, quota_reservation_, id, url),
        callback);
  } else {
    NOTREACHED();
  }
}

void PepperFileSystemBrowserHost::CloseQuotaFile(
    PepperFileIOHost* file_io_host,
    const ppapi::FileGrowth& file_growth) {
  int32_t id = file_io_host->pp_resource();
  FileMap::iterator it = files_.find(id);
  if (it != files_.end()) {
    files_.erase(it);
  } else {
    NOTREACHED();
    return;
  }

  file_system_context_->default_file_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          &QuotaReservation::CloseFile, quota_reservation_, id, file_growth));
}

int32_t PepperFileSystemBrowserHost::OnHostMsgOpen(
    ppapi::host::HostMessageContext* context,
    int64_t /* unused */) {
  // TODO(raymes): The file system size is now unused by FileSystemDispatcher.
  // Figure out why. Why is the file system size signed?

  // Not allow multiple opens.
  if (called_open_)
    return PP_ERROR_INPROGRESS;
  called_open_ = true;

  fileapi::FileSystemType file_system_type =
      ppapi::PepperFileSystemTypeToFileSystemType(type_);
  if (file_system_type == fileapi::kFileSystemTypeUnknown)
    return PP_ERROR_FAILED;

  int render_process_id = 0;
  int unused;
  if (!browser_ppapi_host_->GetRenderFrameIDsForInstance(
          pp_instance(), &render_process_id, &unused)) {
    return PP_ERROR_FAILED;
  }

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GetFileSystemContextFromRenderId, render_process_id),
      base::Bind(&PepperFileSystemBrowserHost::OpenFileSystem,
                 weak_factory_.GetWeakPtr(),
                 context->MakeReplyMessageContext(),
                 file_system_type));
  return PP_OK_COMPLETIONPENDING;
}

void PepperFileSystemBrowserHost::OpenExistingFileSystem(
    const base::Closure& callback,
    scoped_refptr<fileapi::FileSystemContext> file_system_context) {
  if (file_system_context.get()) {
    opened_ = true;
  } else {
    // If there is no file system context, we log a warning and continue with an
    // invalid resource (which will produce errors when used), since we have no
    // way to communicate the error to the caller.
    LOG(WARNING) << "Could not retrieve file system context.";
  }
  SetFileSystemContext(file_system_context);

  if (ShouldCreateQuotaReservation())
    CreateQuotaReservation(callback);
  else
    callback.Run();
}

void PepperFileSystemBrowserHost::OpenFileSystem(
    ppapi::host::ReplyMessageContext reply_context,
    fileapi::FileSystemType file_system_type,
    scoped_refptr<fileapi::FileSystemContext> file_system_context) {
  if (!file_system_context.get()) {
    OpenFileSystemComplete(
        reply_context, GURL(), std::string(), base::File::FILE_ERROR_FAILED);
    return;
  }

  SetFileSystemContext(file_system_context);

  GURL origin =
      browser_ppapi_host_->GetDocumentURLForInstance(pp_instance()).GetOrigin();
  file_system_context_->OpenFileSystem(
      origin,
      file_system_type,
      fileapi::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::Bind(&PepperFileSystemBrowserHost::OpenFileSystemComplete,
                 weak_factory_.GetWeakPtr(),
                 reply_context));
}

void PepperFileSystemBrowserHost::OpenFileSystemComplete(
    ppapi::host::ReplyMessageContext reply_context,
    const GURL& root,
    const std::string& /* unused */,
    base::File::Error error) {
  int32 pp_error = ppapi::FileErrorToPepperError(error);
  if (pp_error == PP_OK) {
    opened_ = true;
    root_url_ = root;

    if (ShouldCreateQuotaReservation()) {
      CreateQuotaReservation(
          base::Bind(&PepperFileSystemBrowserHost::SendReplyForFileSystem,
                     weak_factory_.GetWeakPtr(),
                     reply_context,
                     static_cast<int32_t>(PP_OK)));
      return;
    }
  }
  SendReplyForFileSystem(reply_context, pp_error);
}

void PepperFileSystemBrowserHost::OpenIsolatedFileSystem(
    ppapi::host::ReplyMessageContext reply_context,
    const std::string& fsid,
    PP_IsolatedFileSystemType_Private type,
    scoped_refptr<fileapi::FileSystemContext> file_system_context) {
  if (!file_system_context.get()) {
    SendReplyForIsolatedFileSystem(reply_context, fsid, PP_ERROR_FAILED);
    return;
  }
  SetFileSystemContext(file_system_context);

  root_url_ = GURL(fileapi::GetIsolatedFileSystemRootURIString(
      browser_ppapi_host_->GetDocumentURLForInstance(pp_instance()).GetOrigin(),
      fsid,
      ppapi::IsolatedFileSystemTypeToRootName(type)));
  if (!root_url_.is_valid()) {
    SendReplyForIsolatedFileSystem(reply_context, fsid, PP_ERROR_FAILED);
    return;
  }

  switch (type) {
    case PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_CRX:
      opened_ = true;
      SendReplyForIsolatedFileSystem(reply_context, fsid, PP_OK);
      return;
    case PP_ISOLATEDFILESYSTEMTYPE_PRIVATE_PLUGINPRIVATE:
      OpenPluginPrivateFileSystem(reply_context, fsid, file_system_context_);
      return;
    default:
      NOTREACHED();
      SendReplyForIsolatedFileSystem(reply_context, fsid, PP_ERROR_BADARGUMENT);
      return;
  }
}

void PepperFileSystemBrowserHost::OpenPluginPrivateFileSystem(
    ppapi::host::ReplyMessageContext reply_context,
    const std::string& fsid,
    scoped_refptr<fileapi::FileSystemContext> file_system_context) {
  GURL origin =
      browser_ppapi_host_->GetDocumentURLForInstance(pp_instance()).GetOrigin();
  if (!origin.is_valid()) {
    SendReplyForIsolatedFileSystem(reply_context, fsid, PP_ERROR_FAILED);
    return;
  }

  const std::string& plugin_id = GeneratePluginId(GetPluginMimeType());
  if (plugin_id.empty()) {
    SendReplyForIsolatedFileSystem(reply_context, fsid, PP_ERROR_BADARGUMENT);
    return;
  }

  file_system_context->OpenPluginPrivateFileSystem(
      origin,
      fileapi::kFileSystemTypePluginPrivate,
      fsid,
      plugin_id,
      fileapi::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::Bind(
          &PepperFileSystemBrowserHost::OpenPluginPrivateFileSystemComplete,
          weak_factory_.GetWeakPtr(),
          reply_context,
          fsid));
}

void PepperFileSystemBrowserHost::OpenPluginPrivateFileSystemComplete(
    ppapi::host::ReplyMessageContext reply_context,
    const std::string& fsid,
    base::File::Error error) {
  int32 pp_error = ppapi::FileErrorToPepperError(error);
  if (pp_error == PP_OK)
    opened_ = true;
  SendReplyForIsolatedFileSystem(reply_context, fsid, pp_error);
}

int32_t PepperFileSystemBrowserHost::OnHostMsgInitIsolatedFileSystem(
    ppapi::host::HostMessageContext* context,
    const std::string& fsid,
    PP_IsolatedFileSystemType_Private type) {
  // Do not allow multiple opens.
  if (called_open_)
    return PP_ERROR_INPROGRESS;
  called_open_ = true;

  // Do a sanity check.
  if (!fileapi::ValidateIsolatedFileSystemId(fsid))
    return PP_ERROR_BADARGUMENT;

  int render_process_id = 0;
  int unused;
  if (!browser_ppapi_host_->GetRenderFrameIDsForInstance(
          pp_instance(), &render_process_id, &unused)) {
    fileapi::IsolatedContext::GetInstance()->RevokeFileSystem(fsid);
    return PP_ERROR_FAILED;
  }

  root_url_ = GURL(fileapi::GetIsolatedFileSystemRootURIString(
      browser_ppapi_host_->GetDocumentURLForInstance(pp_instance()).GetOrigin(),
      fsid,
      ppapi::IsolatedFileSystemTypeToRootName(type)));

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GetFileSystemContextFromRenderId, render_process_id),
      base::Bind(&PepperFileSystemBrowserHost::OpenIsolatedFileSystem,
                 weak_factory_.GetWeakPtr(),
                 context->MakeReplyMessageContext(),
                 fsid,
                 type));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperFileSystemBrowserHost::OnHostMsgReserveQuota(
    ppapi::host::HostMessageContext* context,
    int64_t amount,
    const ppapi::FileGrowthMap& file_growths) {
  DCHECK(ChecksQuota());
  DCHECK_GT(amount, 0);

  if (reserving_quota_)
    return PP_ERROR_INPROGRESS;
  reserving_quota_ = true;

  int64_t reservation_amount =
      std::max<int64_t>(kMinimumQuotaReservationSize, amount);
  file_system_context_->default_file_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&QuotaReservation::ReserveQuota,
                 quota_reservation_,
                 reservation_amount,
                 file_growths,
                 base::Bind(&PepperFileSystemBrowserHost::GotReservedQuota,
                            weak_factory_.GetWeakPtr(),
                            context->MakeReplyMessageContext())));

  return PP_OK_COMPLETIONPENDING;
}

void PepperFileSystemBrowserHost::SendReplyForFileSystem(
    ppapi::host::ReplyMessageContext reply_context,
    int32_t pp_error) {
  reply_context.params.set_result(pp_error);
  host()->SendReply(reply_context, PpapiPluginMsg_FileSystem_OpenReply());
}

void PepperFileSystemBrowserHost::SendReplyForIsolatedFileSystem(
    ppapi::host::ReplyMessageContext reply_context,
    const std::string& fsid,
    int32_t error) {
  if (error != PP_OK)
    fileapi::IsolatedContext::GetInstance()->RevokeFileSystem(fsid);
  reply_context.params.set_result(error);
  host()->SendReply(reply_context,
                    PpapiPluginMsg_FileSystem_InitIsolatedFileSystemReply());
}

void PepperFileSystemBrowserHost::SetFileSystemContext(
    scoped_refptr<fileapi::FileSystemContext> file_system_context) {
  file_system_context_ = file_system_context;
  if (type_ != PP_FILESYSTEMTYPE_EXTERNAL || root_url_.is_valid()) {
    file_system_operation_runner_ =
        file_system_context_->CreateFileSystemOperationRunner();
  }
}

bool PepperFileSystemBrowserHost::ShouldCreateQuotaReservation() const {
  // Some file system types don't have quota.
  if (!ppapi::FileSystemTypeHasQuota(type_))
    return false;

  // For file system types with quota, some origins have unlimited storage.
  quota::QuotaManagerProxy* quota_manager_proxy =
      file_system_context_->quota_manager_proxy();
  CHECK(quota_manager_proxy);
  CHECK(quota_manager_proxy->quota_manager());
  fileapi::FileSystemType file_system_type =
      ppapi::PepperFileSystemTypeToFileSystemType(type_);
  return !quota_manager_proxy->quota_manager()->IsStorageUnlimited(
      root_url_.GetOrigin(),
      fileapi::FileSystemTypeToQuotaStorageType(file_system_type));
}

void PepperFileSystemBrowserHost::CreateQuotaReservation(
    const base::Closure& callback) {
  DCHECK(root_url_.is_valid());
  base::PostTaskAndReplyWithResult(
      file_system_context_->default_file_task_runner(),
      FROM_HERE,
      base::Bind(&QuotaReservation::Create,
                 file_system_context_,
                 root_url_.GetOrigin(),
                 ppapi::PepperFileSystemTypeToFileSystemType(type_)),
      base::Bind(&PepperFileSystemBrowserHost::GotQuotaReservation,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void PepperFileSystemBrowserHost::GotQuotaReservation(
    const base::Closure& callback,
    scoped_refptr<QuotaReservation> quota_reservation) {
  quota_reservation_ = quota_reservation;
  callback.Run();
}

void PepperFileSystemBrowserHost::GotReservedQuota(
    ppapi::host::ReplyMessageContext reply_context,
    int64_t amount,
    const ppapi::FileSizeMap& file_sizes) {
  DCHECK(reserving_quota_);
  reserving_quota_ = false;
  reserved_quota_ = amount;

  reply_context.params.set_result(PP_OK);
  host()->SendReply(
      reply_context,
      PpapiPluginMsg_FileSystem_ReserveQuotaReply(amount, file_sizes));
}

std::string PepperFileSystemBrowserHost::GetPluginMimeType() const {
  base::FilePath plugin_path = browser_ppapi_host_->GetPluginPath();
  PepperPluginInfo* info =
      PluginService::GetInstance()->GetRegisteredPpapiPluginInfo(plugin_path);
  if (!info || info->mime_types.empty())
    return std::string();
  // Use the first element in |info->mime_types| even if several elements exist.
  return info->mime_types[0].mime_type;
}

std::string PepperFileSystemBrowserHost::GeneratePluginId(
    const std::string& mime_type) const {
  // TODO(nhiroki): This function is very specialized for specific plugins (MIME
  // types).  If we bring this API to stable, we might have to make it more
  // general.

  std::string top_level_type;
  std::string subtype;
  if (!net::ParseMimeTypeWithoutParameter(
          mime_type, &top_level_type, &subtype) ||
      !net::IsValidTopLevelMimeType(top_level_type))
    return std::string();

  // Replace a slash used for type/subtype separator with an underscore.
  std::string output = top_level_type + "_" + subtype;

  // Verify |output| contains only alphabets, digits, or "._-".
  for (std::string::const_iterator it = output.begin(); it != output.end();
       ++it) {
    if (!IsAsciiAlpha(*it) && !IsAsciiDigit(*it) && *it != '.' && *it != '_' &&
        *it != '-') {
      LOG(WARNING) << "Failed to generate a plugin id.";
      return std::string();
    }
  }
  return output;
}

}  // namespace content
