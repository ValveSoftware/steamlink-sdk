// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "content/browser/download/save_file_manager.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "content/browser/download/save_file.h"
#include "content/browser/download/save_package.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/io_buffer.h"
#include "url/gurl.h"

namespace content {

SaveFileManager::SaveFileManager() {}

SaveFileManager::~SaveFileManager() {
  // Check for clean shutdown.
  DCHECK(save_file_map_.empty());
}

// Called during the browser shutdown process to clean up any state (open files,
// timers) that live on the saving thread (file thread).
void SaveFileManager::Shutdown() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::OnShutdown, this));
}

// Stop file thread operations.
void SaveFileManager::OnShutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  STLDeleteValues(&save_file_map_);
}

SaveFile* SaveFileManager::LookupSaveFile(SaveItemId save_item_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  SaveFileMap::iterator it = save_file_map_.find(save_item_id);
  return it == save_file_map_.end() ? nullptr : it->second;
}

// Look up a SavePackage according to a save id.
SavePackage* SaveFileManager::LookupPackage(SaveItemId save_item_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SavePackageMap::iterator it = packages_.find(save_item_id);
  if (it != packages_.end())
    return it->second;
  return nullptr;
}

// Call from SavePackage for starting a saving job
void SaveFileManager::SaveURL(SaveItemId save_item_id,
                              const GURL& url,
                              const Referrer& referrer,
                              int render_process_host_id,
                              int render_view_routing_id,
                              int render_frame_routing_id,
                              SaveFileCreateInfo::SaveFileSource save_source,
                              const base::FilePath& file_full_path,
                              ResourceContext* context,
                              SavePackage* save_package) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Insert started saving job to tracking list.
  DCHECK(packages_.find(save_item_id) == packages_.end());
  packages_[save_item_id] = save_package;

  // Register a saving job.
  if (save_source == SaveFileCreateInfo::SAVE_FILE_FROM_NET) {
    DCHECK(url.is_valid());

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SaveFileManager::OnSaveURL, this, url, referrer,
                   save_item_id, save_package->id(), render_process_host_id,
                   render_view_routing_id, render_frame_routing_id, context));
  } else {
    // We manually start the save job.
    SaveFileCreateInfo* info = new SaveFileCreateInfo(
        file_full_path, url, save_item_id, save_package->id(),
        render_process_host_id, render_frame_routing_id, save_source);

    // Since the data will come from render process, so we need to start
    // this kind of save job by ourself.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&SaveFileManager::StartSave, this, info));
  }
}

// Utility function for look up table maintenance, called on the UI thread.
// A manager may have multiple save page job (SavePackage) in progress,
// so we just look up the save id and remove it from the tracking table.
void SaveFileManager::RemoveSaveFile(SaveItemId save_item_id,
                                     SavePackage* save_package) {
  DCHECK(save_package);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // A save page job (SavePackage) can only have one manager,
  // so remove it if it exists.
  SavePackageMap::iterator it = packages_.find(save_item_id);
  if (it != packages_.end())
    packages_.erase(it);
}

// Static
SavePackage* SaveFileManager::GetSavePackageFromRenderIds(
    int render_process_id,
    int render_frame_routing_id) {
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_routing_id);
  if (!render_frame_host)
    return nullptr;

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(
          render_frame_host));
  if (!web_contents)
    return nullptr;

  return web_contents->save_package();
}

void SaveFileManager::DeleteDirectoryOrFile(const base::FilePath& full_path,
                                            bool is_dir) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::OnDeleteDirectoryOrFile,
          this, full_path, is_dir));
}

void SaveFileManager::SendCancelRequest(SaveItemId save_item_id) {
  // Cancel the request which has specific save id.
  DCHECK(!save_item_id.is_null());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::CancelSave, this, save_item_id));
}

// Notifications sent from the IO thread and run on the file thread:

// The IO thread created |info|, but the file thread (this method) uses it
// to create a SaveFile which will hold and finally destroy |info|. It will
// then passes |info| to the UI thread for reporting saving status.
void SaveFileManager::StartSave(SaveFileCreateInfo* info) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(info);
  // No need to calculate hash.
  SaveFile* save_file = new SaveFile(info, false);

  // TODO(phajdan.jr): We should check the return value and handle errors here.
  save_file->Initialize();

  DCHECK(!LookupSaveFile(info->save_item_id));
  save_file_map_[info->save_item_id] = save_file;
  info->path = save_file->FullPath();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SaveFileManager::OnStartSave, this, *info));
}

// We do forward an update to the UI thread here, since we do not use timer to
// update the UI. If the user has canceled the saving action (in the UI
// thread). We may receive a few more updates before the IO thread gets the
// cancel message. We just delete the data since the SaveFile has been deleted.
void SaveFileManager::UpdateSaveProgress(SaveItemId save_item_id,
                                         net::IOBuffer* data,
                                         int data_len) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  SaveFile* save_file = LookupSaveFile(save_item_id);
  if (save_file) {
    DCHECK(save_file->InProgress());

    DownloadInterruptReason reason =
        save_file->AppendDataToFile(data->data(), data_len);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SaveFileManager::OnUpdateSaveProgress, this,
                   save_file->save_item_id(), save_file->BytesSoFar(),
                   reason == DOWNLOAD_INTERRUPT_REASON_NONE));
  }
}

// The IO thread will call this when saving is completed or it got error when
// fetching data. We forward the message to OnSaveFinished in UI thread.
void SaveFileManager::SaveFinished(SaveItemId save_item_id,
                                   SavePackageId save_package_id,
                                   bool is_success) {
  DVLOG(20) << " " << __FUNCTION__ << "()"
            << " save_item_id = " << save_item_id
            << " save_package_id = " << save_package_id
            << " is_success = " << is_success;
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  int64_t bytes_so_far = 0;
  SaveFile* save_file = LookupSaveFile(save_item_id);
  if (save_file != nullptr) {
    DCHECK(save_file->InProgress());
    DVLOG(20) << " " << __FUNCTION__ << "()"
              << " save_file = " << save_file->DebugString();
    bytes_so_far = save_file->BytesSoFar();
    save_file->Finish();
    save_file->Detach();
  } else {
    // We got called before StartSave - this should only happen if
    // ResourceHandler failed before it got a chance to parse headers
    // and metadata.
    DCHECK(!is_success);
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SaveFileManager::OnSaveFinished, this, save_item_id,
                 bytes_so_far, is_success));
}

// Notifications sent from the file thread and run on the UI thread.

void SaveFileManager::OnStartSave(const SaveFileCreateInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SavePackage* save_package = GetSavePackageFromRenderIds(
      info.render_process_id, info.render_frame_routing_id);
  if (!save_package) {
    // Cancel this request.
    SendCancelRequest(info.save_item_id);
    return;
  }

  // Forward this message to SavePackage.
  save_package->StartSave(&info);
}

void SaveFileManager::OnUpdateSaveProgress(SaveItemId save_item_id,
                                           int64_t bytes_so_far,
                                           bool write_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SavePackage* package = LookupPackage(save_item_id);
  if (package)
    package->UpdateSaveProgress(save_item_id, bytes_so_far, write_success);
  else
    SendCancelRequest(save_item_id);
}

void SaveFileManager::OnSaveFinished(SaveItemId save_item_id,
                                     int64_t bytes_so_far,
                                     bool is_success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SavePackage* package = LookupPackage(save_item_id);
  if (package)
    package->SaveFinished(save_item_id, bytes_so_far, is_success);
}

// Notifications sent from the UI thread and run on the IO thread.

void SaveFileManager::OnSaveURL(const GURL& url,
                                const Referrer& referrer,
                                SaveItemId save_item_id,
                                SavePackageId save_package_id,
                                int render_process_host_id,
                                int render_view_routing_id,
                                int render_frame_routing_id,
                                ResourceContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ResourceDispatcherHostImpl::Get()->BeginSaveFile(
      url, referrer, save_item_id, save_package_id, render_process_host_id,
      render_view_routing_id, render_frame_routing_id, context);
}

void SaveFileManager::ExecuteCancelSaveRequest(int render_process_id,
                                               int request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ResourceDispatcherHostImpl::Get()->CancelRequest(
      render_process_id, request_id);
}

// Notifications sent from the UI thread and run on the file thread.

// This method will be sent via a user action, or shutdown on the UI thread,
// and run on the file thread. We don't post a message back for cancels,
// but we do forward the cancel to the IO thread. Since this message has been
// sent from the UI thread, the saving job may have already completed and
// won't exist in our map.
void SaveFileManager::CancelSave(SaveItemId save_item_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  SaveFileMap::iterator it = save_file_map_.find(save_item_id);
  if (it != save_file_map_.end()) {
    SaveFile* save_file = it->second;

    if (!save_file->InProgress()) {
      // We've won a race with the UI thread--we finished the file before
      // the UI thread cancelled it on us.  Unfortunately, in this situation
      // the cancel wins, so we need to delete the now detached file.
      base::DeleteFile(save_file->FullPath(), false);
    } else if (save_file->save_source() ==
               SaveFileCreateInfo::SAVE_FILE_FROM_NET) {
      // If the data comes from the net IO thread and hasn't completed
      // yet, then forward the cancel message to IO thread & cancel the
      // save locally.  If the data doesn't come from the IO thread,
      // we can ignore the message.
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&SaveFileManager::ExecuteCancelSaveRequest, this,
              save_file->render_process_id(), save_file->request_id()));
    }

    // Whatever the save file is complete or not, just delete it.  This
    // will delete the underlying file if InProgress() is true.
    save_file_map_.erase(it);
    delete save_file;
  }
}

void SaveFileManager::OnDeleteDirectoryOrFile(const base::FilePath& full_path,
                                              bool is_dir) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(!full_path.empty());

  base::DeleteFile(full_path, is_dir);
}

void SaveFileManager::RenameAllFiles(const FinalNamesMap& final_names,
                                     const base::FilePath& resource_dir,
                                     int render_process_id,
                                     int render_frame_routing_id,
                                     SavePackageId save_package_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  if (!resource_dir.empty() && !base::PathExists(resource_dir))
    base::CreateDirectory(resource_dir);

  for (const auto& i : final_names) {
    SaveItemId save_item_id = i.first;
    const base::FilePath& final_name = i.second;

    SaveFileMap::iterator it = save_file_map_.find(save_item_id);
    if (it != save_file_map_.end()) {
      SaveFile* save_file = it->second;
      DCHECK(!save_file->InProgress());
      save_file->Rename(final_name);
      delete save_file;
      save_file_map_.erase(it);
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SaveFileManager::OnFinishSavePageJob, this, render_process_id,
                 render_frame_routing_id, save_package_id));
}

void SaveFileManager::OnFinishSavePageJob(int render_process_id,
                                          int render_frame_routing_id,
                                          SavePackageId save_package_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SavePackage* save_package =
      GetSavePackageFromRenderIds(render_process_id, render_frame_routing_id);

  if (save_package && save_package->id() == save_package_id)
    save_package->Finish();
}

void SaveFileManager::RemoveSavedFileFromFileMap(
    const std::vector<SaveItemId>& save_item_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  for (const SaveItemId save_item_id : save_item_ids) {
    SaveFileMap::iterator it = save_file_map_.find(save_item_id);
    if (it != save_file_map_.end()) {
      SaveFile* save_file = it->second;
      DCHECK(!save_file->InProgress());
      base::DeleteFile(save_file->FullPath(), false);
      delete save_file;
      save_file_map_.erase(it);
    }
  }
}

}  // namespace content
