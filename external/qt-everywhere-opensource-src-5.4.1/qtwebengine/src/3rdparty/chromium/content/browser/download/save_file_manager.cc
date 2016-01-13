// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "content/browser/download/save_file_manager.h"

#include "base/bind.h"
#include "base/file_util.h"
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
#include "net/base/filename_util.h"
#include "net/base/io_buffer.h"
#include "url/gurl.h"

namespace content {

SaveFileManager::SaveFileManager()
    : next_id_(0) {
}

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  STLDeleteValues(&save_file_map_);
}

SaveFile* SaveFileManager::LookupSaveFile(int save_id) {
  SaveFileMap::iterator it = save_file_map_.find(save_id);
  return it == save_file_map_.end() ? NULL : it->second;
}

// Called on the IO thread when
// a) The ResourceDispatcherHostImpl has decided that a request is savable.
// b) The resource does not come from the network, but we still need a
// save ID for for managing the status of the saving operation. So we
// file a request from the file thread to the IO thread to generate a
// unique save ID.
int SaveFileManager::GetNextId() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return next_id_++;
}

void SaveFileManager::RegisterStartingRequest(const GURL& save_url,
                                              SavePackage* save_package) {
  // Make sure it runs in the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int contents_id = save_package->contents_id();

  // Register this starting request.
  StartingRequestsMap& starting_requests =
      contents_starting_requests_[contents_id];
  bool never_present = starting_requests.insert(
      StartingRequestsMap::value_type(save_url.spec(), save_package)).second;
  DCHECK(never_present);
}

SavePackage* SaveFileManager::UnregisterStartingRequest(
    const GURL& save_url, int contents_id) {
  // Make sure it runs in UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ContentsToStartingRequestsMap::iterator it =
      contents_starting_requests_.find(contents_id);
  if (it != contents_starting_requests_.end()) {
    StartingRequestsMap& requests = it->second;
    StartingRequestsMap::iterator sit = requests.find(save_url.spec());
    if (sit == requests.end())
      return NULL;

    // Found, erase it from starting list and return SavePackage.
    SavePackage* save_package = sit->second;
    requests.erase(sit);
    // If there is no element in requests, remove it
    if (requests.empty())
      contents_starting_requests_.erase(it);
    return save_package;
  }

  return NULL;
}

// Look up a SavePackage according to a save id.
SavePackage* SaveFileManager::LookupPackage(int save_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SavePackageMap::iterator it = packages_.find(save_id);
  if (it != packages_.end())
    return it->second;
  return NULL;
}

// Call from SavePackage for starting a saving job
void SaveFileManager::SaveURL(
    const GURL& url,
    const Referrer& referrer,
    int render_process_host_id,
    int render_view_id,
    SaveFileCreateInfo::SaveFileSource save_source,
    const base::FilePath& file_full_path,
    ResourceContext* context,
    SavePackage* save_package) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Register a saving job.
  RegisterStartingRequest(url, save_package);
  if (save_source == SaveFileCreateInfo::SAVE_FILE_FROM_NET) {
    DCHECK(url.is_valid());

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SaveFileManager::OnSaveURL, this, url, referrer,
            render_process_host_id, render_view_id, context));
  } else {
    // We manually start the save job.
    SaveFileCreateInfo* info = new SaveFileCreateInfo(file_full_path,
         url,
         save_source,
         -1);
    info->render_process_id = render_process_host_id;
    info->render_view_id = render_view_id;

    // Since the data will come from render process, so we need to start
    // this kind of save job by ourself.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SaveFileManager::OnRequireSaveJobFromOtherSource,
            this, info));
  }
}

// Utility function for look up table maintenance, called on the UI thread.
// A manager may have multiple save page job (SavePackage) in progress,
// so we just look up the save id and remove it from the tracking table.
// If the save id is -1, it means we just send a request to save, but the
// saving action has still not happened, need to call UnregisterStartingRequest
// to remove it from the tracking map.
void SaveFileManager::RemoveSaveFile(int save_id, const GURL& save_url,
                                     SavePackage* package) {
  DCHECK(package);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // A save page job (SavePackage) can only have one manager,
  // so remove it if it exists.
  if (save_id == -1) {
    SavePackage* old_package =
        UnregisterStartingRequest(save_url, package->contents_id());
    DCHECK_EQ(old_package, package);
  } else {
    SavePackageMap::iterator it = packages_.find(save_id);
    if (it != packages_.end())
      packages_.erase(it);
  }
}

// Static
SavePackage* SaveFileManager::GetSavePackageFromRenderIds(
    int render_process_id, int render_view_id) {
  RenderViewHostImpl* render_view_host =
      RenderViewHostImpl::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return NULL;

  WebContentsImpl* contents = static_cast<WebContentsImpl*>(
      render_view_host->GetDelegate()->GetAsWebContents());
  if (!contents)
    return NULL;

  return contents->save_package();
}

void SaveFileManager::DeleteDirectoryOrFile(const base::FilePath& full_path,
                                            bool is_dir) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::OnDeleteDirectoryOrFile,
          this, full_path, is_dir));
}

void SaveFileManager::SendCancelRequest(int save_id) {
  // Cancel the request which has specific save id.
  DCHECK_GT(save_id, -1);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::CancelSave, this, save_id));
}

// Notifications sent from the IO thread and run on the file thread:

// The IO thread created |info|, but the file thread (this method) uses it
// to create a SaveFile which will hold and finally destroy |info|. It will
// then passes |info| to the UI thread for reporting saving status.
void SaveFileManager::StartSave(SaveFileCreateInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(info);
  // No need to calculate hash.
  SaveFile* save_file = new SaveFile(info, false);

  // TODO(phajdan.jr): We should check the return value and handle errors here.
  save_file->Initialize();

  DCHECK(!LookupSaveFile(info->save_id));
  save_file_map_[info->save_id] = save_file;
  info->path = save_file->FullPath();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SaveFileManager::OnStartSave, this, info));
}

// We do forward an update to the UI thread here, since we do not use timer to
// update the UI. If the user has canceled the saving action (in the UI
// thread). We may receive a few more updates before the IO thread gets the
// cancel message. We just delete the data since the SaveFile has been deleted.
void SaveFileManager::UpdateSaveProgress(int save_id,
                                         net::IOBuffer* data,
                                         int data_len) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  SaveFile* save_file = LookupSaveFile(save_id);
  if (save_file) {
    DCHECK(save_file->InProgress());

    DownloadInterruptReason reason =
        save_file->AppendDataToFile(data->data(), data_len);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SaveFileManager::OnUpdateSaveProgress,
                   this,
                   save_file->save_id(),
                   save_file->BytesSoFar(),
                   reason == DOWNLOAD_INTERRUPT_REASON_NONE));
  }
}

// The IO thread will call this when saving is completed or it got error when
// fetching data. In the former case, we forward the message to OnSaveFinished
// in UI thread. In the latter case, the save ID will be -1, which means the
// saving action did not even start, so we need to call OnErrorFinished in UI
// thread, which will use the save URL to find corresponding request record and
// delete it.
void SaveFileManager::SaveFinished(int save_id,
                                   const GURL& save_url,
                                   int render_process_id,
                                   bool is_success) {
  VLOG(20) << " " << __FUNCTION__ << "()"
           << " save_id = " << save_id
           << " save_url = \"" << save_url.spec() << "\""
           << " is_success = " << is_success;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  SaveFileMap::iterator it = save_file_map_.find(save_id);
  if (it != save_file_map_.end()) {
    SaveFile* save_file = it->second;
    // This routine may be called twice for the same from from
    // SaveePackage::OnReceivedSerializedHtmlData, once for the file
    // itself, and once when all frames have been serialized.
    // So we can't assert that the file is InProgress() here.
    // TODO(rdsmith): Fix this logic and put the DCHECK below back in.
    // DCHECK(save_file->InProgress());

    VLOG(20) << " " << __FUNCTION__ << "()"
             << " save_file = " << save_file->DebugString();
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SaveFileManager::OnSaveFinished, this, save_id,
            save_file->BytesSoFar(), is_success));

    save_file->Finish();
    save_file->Detach();
  } else if (save_id == -1) {
    // Before saving started, we got error. We still call finish process.
    DCHECK(!save_url.is_empty());
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SaveFileManager::OnErrorFinished, this, save_url,
            render_process_id));
  }
}

// Notifications sent from the file thread and run on the UI thread.

void SaveFileManager::OnStartSave(const SaveFileCreateInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SavePackage* save_package =
      GetSavePackageFromRenderIds(info->render_process_id,
                                  info->render_view_id);
  if (!save_package) {
    // Cancel this request.
    SendCancelRequest(info->save_id);
    return;
  }

  // Insert started saving job to tracking list.
  SavePackageMap::iterator sit = packages_.find(info->save_id);
  if (sit == packages_.end()) {
    // Find the registered request. If we can not find, it means we have
    // canceled the job before.
    SavePackage* old_save_package = UnregisterStartingRequest(info->url,
        info->render_process_id);
    if (!old_save_package) {
      // Cancel this request.
      SendCancelRequest(info->save_id);
      return;
    }
    DCHECK_EQ(old_save_package, save_package);
    packages_[info->save_id] = save_package;
  } else {
    NOTREACHED();
  }

  // Forward this message to SavePackage.
  save_package->StartSave(info);
}

void SaveFileManager::OnUpdateSaveProgress(int save_id, int64 bytes_so_far,
                                           bool write_success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SavePackage* package = LookupPackage(save_id);
  if (package)
    package->UpdateSaveProgress(save_id, bytes_so_far, write_success);
  else
    SendCancelRequest(save_id);
}

void SaveFileManager::OnSaveFinished(int save_id,
                                     int64 bytes_so_far,
                                     bool is_success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SavePackage* package = LookupPackage(save_id);
  if (package)
    package->SaveFinished(save_id, bytes_so_far, is_success);
}

void SaveFileManager::OnErrorFinished(const GURL& save_url, int contents_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SavePackage* save_package = UnregisterStartingRequest(save_url, contents_id);
  if (save_package)
    save_package->SaveFailed(save_url);
}

// Notifications sent from the UI thread and run on the IO thread.

void SaveFileManager::OnSaveURL(
    const GURL& url,
    const Referrer& referrer,
    int render_process_host_id,
    int render_view_id,
    ResourceContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ResourceDispatcherHostImpl::Get()->BeginSaveFile(url,
                                                   referrer,
                                                   render_process_host_id,
                                                   render_view_id,
                                                   context);
}

void SaveFileManager::OnRequireSaveJobFromOtherSource(
    SaveFileCreateInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(info->save_id, -1);
  // Generate a unique save id.
  info->save_id = GetNextId();
  // Start real saving action.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SaveFileManager::StartSave, this, info));
}

void SaveFileManager::ExecuteCancelSaveRequest(int render_process_id,
                                               int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ResourceDispatcherHostImpl::Get()->CancelRequest(
      render_process_id, request_id);
}

// Notifications sent from the UI thread and run on the file thread.

// This method will be sent via a user action, or shutdown on the UI thread,
// and run on the file thread. We don't post a message back for cancels,
// but we do forward the cancel to the IO thread. Since this message has been
// sent from the UI thread, the saving job may have already completed and
// won't exist in our map.
void SaveFileManager::CancelSave(int save_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  SaveFileMap::iterator it = save_file_map_.find(save_id);
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

// It is possible that SaveItem which has specified save_id has been canceled
// before this function runs. So if we can not find corresponding SaveFile by
// using specified save_id, just return.
void SaveFileManager::SaveLocalFile(const GURL& original_file_url,
                                    int save_id,
                                    int render_process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  SaveFile* save_file = LookupSaveFile(save_id);
  if (!save_file)
    return;
  // If it has finished, just return.
  if (!save_file->InProgress())
    return;

  // Close the save file before the copy operation.
  save_file->Finish();
  save_file->Detach();

  DCHECK(original_file_url.SchemeIsFile());
  base::FilePath file_path;
  net::FileURLToFilePath(original_file_url, &file_path);
  // If we can not get valid file path from original URL, treat it as
  // disk error.
  if (file_path.empty())
    SaveFinished(save_id, original_file_url, render_process_id, false);

  // Copy the local file to the temporary file. It will be renamed to its
  // final name later.
  bool success = base::CopyFile(file_path, save_file->FullPath());
  if (!success)
    base::DeleteFile(save_file->FullPath(), false);
  SaveFinished(save_id, original_file_url, render_process_id, success);
}

void SaveFileManager::OnDeleteDirectoryOrFile(const base::FilePath& full_path,
                                              bool is_dir) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!full_path.empty());

  base::DeleteFile(full_path, is_dir);
}

void SaveFileManager::RenameAllFiles(
    const FinalNameList& final_names,
    const base::FilePath& resource_dir,
    int render_process_id,
    int render_view_id,
    int save_package_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!resource_dir.empty() && !base::PathExists(resource_dir))
    base::CreateDirectory(resource_dir);

  for (FinalNameList::const_iterator i = final_names.begin();
      i != final_names.end(); ++i) {
    SaveFileMap::iterator it = save_file_map_.find(i->first);
    if (it != save_file_map_.end()) {
      SaveFile* save_file = it->second;
      DCHECK(!save_file->InProgress());
      save_file->Rename(i->second);
      delete save_file;
      save_file_map_.erase(it);
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SaveFileManager::OnFinishSavePageJob, this,
          render_process_id, render_view_id, save_package_id));
}

void SaveFileManager::OnFinishSavePageJob(int render_process_id,
                                          int render_view_id,
                                          int save_package_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SavePackage* save_package =
      GetSavePackageFromRenderIds(render_process_id, render_view_id);

  if (save_package && save_package->id() == save_package_id)
    save_package->Finish();
}

void SaveFileManager::RemoveSavedFileFromFileMap(
    const SaveIDList& save_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  for (SaveIDList::const_iterator i = save_ids.begin();
      i != save_ids.end(); ++i) {
    SaveFileMap::iterator it = save_file_map_.find(*i);
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
