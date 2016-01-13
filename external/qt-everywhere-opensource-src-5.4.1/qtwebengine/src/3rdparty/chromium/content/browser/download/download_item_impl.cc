// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File method ordering: Methods in this file are in the same order as
// in download_item_impl.h, with the following exception: The public
// interface Start is placed in chronological order with the other
// (private) routines that together define a DownloadItem's state
// transitions as the download progresses.  See "Download progression
// cascade" later in this file.

// A regular DownloadItem (created for a download in this session of the
// browser) normally goes through the following states:
//      * Created (when download starts)
//      * Destination filename determined
//      * Entered into the history database.
//      * Made visible in the download shelf.
//      * All the data is saved.  Note that the actual data download occurs
//        in parallel with the above steps, but until those steps are
//        complete, the state of the data save will be ignored.
//      * Download file is renamed to its final name, and possibly
//        auto-opened.

#include "content/browser/download/download_item_impl.h"

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/referrer.h"
#include "net/base/net_util.h"

namespace content {

namespace {

bool DeleteDownloadedFile(const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Make sure we only delete files.
  if (base::DirectoryExists(path))
    return true;
  return base::DeleteFile(path, false);
}

void DeleteDownloadedFileDone(
    base::WeakPtr<DownloadItemImpl> item,
    const base::Callback<void(bool)>& callback,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (success && item.get())
    item->OnDownloadedFileRemoved();
  callback.Run(success);
}

// Wrapper around DownloadFile::Detach and DownloadFile::Cancel that
// takes ownership of the DownloadFile and hence implicitly destroys it
// at the end of the function.
static base::FilePath DownloadFileDetach(
    scoped_ptr<DownloadFile> download_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::FilePath full_path = download_file->FullPath();
  download_file->Detach();
  return full_path;
}

static void DownloadFileCancel(scoped_ptr<DownloadFile> download_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  download_file->Cancel();
}

bool IsDownloadResumptionEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDownloadResumption);
}

}  // namespace

const uint32 DownloadItem::kInvalidId = 0;

const char DownloadItem::kEmptyFileHash[] = "";

// The maximum number of attempts we will make to resume automatically.
const int DownloadItemImpl::kMaxAutoResumeAttempts = 5;

// Constructor for reading from the history service.
DownloadItemImpl::DownloadItemImpl(DownloadItemImplDelegate* delegate,
                                   uint32 download_id,
                                   const base::FilePath& current_path,
                                   const base::FilePath& target_path,
                                   const std::vector<GURL>& url_chain,
                                   const GURL& referrer_url,
                                   const std::string& mime_type,
                                   const std::string& original_mime_type,
                                   const base::Time& start_time,
                                   const base::Time& end_time,
                                   const std::string& etag,
                                   const std::string& last_modified,
                                   int64 received_bytes,
                                   int64 total_bytes,
                                   DownloadItem::DownloadState state,
                                   DownloadDangerType danger_type,
                                   DownloadInterruptReason interrupt_reason,
                                   bool opened,
                                   const net::BoundNetLog& bound_net_log)
    : is_save_package_download_(false),
      download_id_(download_id),
      current_path_(current_path),
      target_path_(target_path),
      target_disposition_(TARGET_DISPOSITION_OVERWRITE),
      url_chain_(url_chain),
      referrer_url_(referrer_url),
      transition_type_(PAGE_TRANSITION_LINK),
      has_user_gesture_(false),
      mime_type_(mime_type),
      original_mime_type_(original_mime_type),
      total_bytes_(total_bytes),
      received_bytes_(received_bytes),
      bytes_per_sec_(0),
      last_modified_time_(last_modified),
      etag_(etag),
      last_reason_(interrupt_reason),
      start_tick_(base::TimeTicks()),
      state_(ExternalToInternalState(state)),
      danger_type_(danger_type),
      start_time_(start_time),
      end_time_(end_time),
      delegate_(delegate),
      is_paused_(false),
      auto_resume_count_(0),
      open_when_complete_(false),
      file_externally_removed_(false),
      auto_opened_(false),
      is_temporary_(false),
      all_data_saved_(state == COMPLETE),
      destination_error_(content::DOWNLOAD_INTERRUPT_REASON_NONE),
      opened_(opened),
      delegate_delayed_complete_(false),
      bound_net_log_(bound_net_log),
      weak_ptr_factory_(this) {
  delegate_->Attach();
  DCHECK_NE(IN_PROGRESS_INTERNAL, state_);
  Init(false /* not actively downloading */, SRC_HISTORY_IMPORT);
}

// Constructing for a regular download:
DownloadItemImpl::DownloadItemImpl(
    DownloadItemImplDelegate* delegate,
    uint32 download_id,
    const DownloadCreateInfo& info,
    const net::BoundNetLog& bound_net_log)
    : is_save_package_download_(false),
      download_id_(download_id),
      target_disposition_(
          (info.save_info->prompt_for_save_location) ?
              TARGET_DISPOSITION_PROMPT : TARGET_DISPOSITION_OVERWRITE),
      url_chain_(info.url_chain),
      referrer_url_(info.referrer_url),
      tab_url_(info.tab_url),
      tab_referrer_url_(info.tab_referrer_url),
      suggested_filename_(base::UTF16ToUTF8(info.save_info->suggested_name)),
      forced_file_path_(info.save_info->file_path),
      transition_type_(info.transition_type),
      has_user_gesture_(info.has_user_gesture),
      content_disposition_(info.content_disposition),
      mime_type_(info.mime_type),
      original_mime_type_(info.original_mime_type),
      remote_address_(info.remote_address),
      total_bytes_(info.total_bytes),
      received_bytes_(0),
      bytes_per_sec_(0),
      last_modified_time_(info.last_modified),
      etag_(info.etag),
      last_reason_(DOWNLOAD_INTERRUPT_REASON_NONE),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS_INTERNAL),
      danger_type_(DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS),
      start_time_(info.start_time),
      delegate_(delegate),
      is_paused_(false),
      auto_resume_count_(0),
      open_when_complete_(false),
      file_externally_removed_(false),
      auto_opened_(false),
      is_temporary_(!info.save_info->file_path.empty()),
      all_data_saved_(false),
      destination_error_(content::DOWNLOAD_INTERRUPT_REASON_NONE),
      opened_(false),
      delegate_delayed_complete_(false),
      bound_net_log_(bound_net_log),
      weak_ptr_factory_(this) {
  delegate_->Attach();
  Init(true /* actively downloading */, SRC_ACTIVE_DOWNLOAD);

  // Link the event sources.
  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_URL_REQUEST,
      info.request_bound_net_log.source().ToEventParametersCallback());

  info.request_bound_net_log.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_STARTED,
      bound_net_log_.source().ToEventParametersCallback());
}

// Constructing for the "Save Page As..." feature:
DownloadItemImpl::DownloadItemImpl(
    DownloadItemImplDelegate* delegate,
    uint32 download_id,
    const base::FilePath& path,
    const GURL& url,
    const std::string& mime_type,
    scoped_ptr<DownloadRequestHandleInterface> request_handle,
    const net::BoundNetLog& bound_net_log)
    : is_save_package_download_(true),
      request_handle_(request_handle.Pass()),
      download_id_(download_id),
      current_path_(path),
      target_path_(path),
      target_disposition_(TARGET_DISPOSITION_OVERWRITE),
      url_chain_(1, url),
      referrer_url_(GURL()),
      transition_type_(PAGE_TRANSITION_LINK),
      has_user_gesture_(false),
      mime_type_(mime_type),
      original_mime_type_(mime_type),
      total_bytes_(0),
      received_bytes_(0),
      bytes_per_sec_(0),
      last_reason_(DOWNLOAD_INTERRUPT_REASON_NONE),
      start_tick_(base::TimeTicks::Now()),
      state_(IN_PROGRESS_INTERNAL),
      danger_type_(DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS),
      start_time_(base::Time::Now()),
      delegate_(delegate),
      is_paused_(false),
      auto_resume_count_(0),
      open_when_complete_(false),
      file_externally_removed_(false),
      auto_opened_(false),
      is_temporary_(false),
      all_data_saved_(false),
      destination_error_(content::DOWNLOAD_INTERRUPT_REASON_NONE),
      opened_(false),
      delegate_delayed_complete_(false),
      bound_net_log_(bound_net_log),
      weak_ptr_factory_(this) {
  delegate_->Attach();
  Init(true /* actively downloading */, SRC_SAVE_PAGE_AS);
}

DownloadItemImpl::~DownloadItemImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Should always have been nuked before now, at worst in
  // DownloadManager shutdown.
  DCHECK(!download_file_.get());

  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadDestroyed(this));
  delegate_->AssertStateConsistent(this);
  delegate_->Detach();
}

void DownloadItemImpl::AddObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  observers_.AddObserver(observer);
}

void DownloadItemImpl::RemoveObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  observers_.RemoveObserver(observer);
}

void DownloadItemImpl::UpdateObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadUpdated(this));
}

void DownloadItemImpl::ValidateDangerousDownload() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!IsDone());
  DCHECK(IsDangerous());

  VLOG(20) << __FUNCTION__ << " download=" << DebugString(true);

  if (IsDone() || !IsDangerous())
    return;

  RecordDangerousDownloadAccept(GetDangerType(),
                                GetTargetFilePath());

  danger_type_ = DOWNLOAD_DANGER_TYPE_USER_VALIDATED;

  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_ITEM_SAFETY_STATE_UPDATED,
      base::Bind(&ItemCheckedNetLogCallback, GetDangerType()));

  UpdateObservers();

  MaybeCompleteDownload();
}

void DownloadItemImpl::StealDangerousDownload(
    const AcquireFileCallback& callback) {
  VLOG(20) << __FUNCTION__ << "() download = " << DebugString(true);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(IsDangerous());
  if (download_file_) {
    BrowserThread::PostTaskAndReplyWithResult(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&DownloadFileDetach, base::Passed(&download_file_)),
        callback);
  } else {
    callback.Run(current_path_);
  }
  current_path_.clear();
  Remove();
  // We have now been deleted.
}

void DownloadItemImpl::Pause() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Ignore irrelevant states.
  if (state_ != IN_PROGRESS_INTERNAL || is_paused_)
    return;

  request_handle_->PauseRequest();
  is_paused_ = true;
  UpdateObservers();
}

void DownloadItemImpl::Resume() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (state_) {
    case IN_PROGRESS_INTERNAL:
      if (!is_paused_)
        return;
      request_handle_->ResumeRequest();
      is_paused_ = false;
      UpdateObservers();
      return;

    case COMPLETING_INTERNAL:
    case COMPLETE_INTERNAL:
    case CANCELLED_INTERNAL:
    case RESUMING_INTERNAL:
      return;

    case INTERRUPTED_INTERNAL:
      auto_resume_count_ = 0;  // User input resets the counter.
      ResumeInterruptedDownload();
      return;

    case MAX_DOWNLOAD_INTERNAL_STATE:
      NOTREACHED();
  }
}

void DownloadItemImpl::Cancel(bool user_cancel) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(20) << __FUNCTION__ << "() download = " << DebugString(true);
  if (state_ != IN_PROGRESS_INTERNAL &&
      state_ != INTERRUPTED_INTERNAL &&
      state_ != RESUMING_INTERNAL) {
    // Small downloads might be complete before this method has a chance to run.
    return;
  }

  if (IsDangerous()) {
    RecordDangerousDownloadDiscard(
        user_cancel ? DOWNLOAD_DISCARD_DUE_TO_USER_ACTION
                    : DOWNLOAD_DISCARD_DUE_TO_SHUTDOWN,
        GetDangerType(),
        GetTargetFilePath());
  }

  last_reason_ = user_cancel ? DOWNLOAD_INTERRUPT_REASON_USER_CANCELED
                             : DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN;

  RecordDownloadCount(CANCELLED_COUNT);

  // TODO(rdsmith/benjhayden): Remove condition as part of
  // |SavePackage| integration.
  // |download_file_| can be NULL if Interrupt() is called after the
  // download file has been released.
  if (!is_save_package_download_ && download_file_)
    ReleaseDownloadFile(true);

  if (state_ == IN_PROGRESS_INTERNAL) {
    // Cancel the originating URL request unless it's already been cancelled
    // by interrupt.
    request_handle_->CancelRequest();
  }

  // Remove the intermediate file if we are cancelling an interrupted download.
  // Continuable interruptions leave the intermediate file around.
  if ((state_ == INTERRUPTED_INTERNAL || state_ == RESUMING_INTERNAL) &&
      !current_path_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(base::IgnoreResult(&DeleteDownloadedFile), current_path_));
    current_path_.clear();
  }

  TransitionTo(CANCELLED_INTERNAL, UPDATE_OBSERVERS);
}

void DownloadItemImpl::Remove() {
  VLOG(20) << __FUNCTION__ << "() download = " << DebugString(true);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  delegate_->AssertStateConsistent(this);
  Cancel(true);
  delegate_->AssertStateConsistent(this);

  NotifyRemoved();
  delegate_->DownloadRemoved(this);
  // We have now been deleted.
}

void DownloadItemImpl::OpenDownload() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!IsDone()) {
    // We don't honor the open_when_complete_ flag for temporary
    // downloads. Don't set it because it shows up in the UI.
    if (!IsTemporary())
      open_when_complete_ = !open_when_complete_;
    return;
  }

  if (state_ != COMPLETE_INTERNAL || file_externally_removed_)
    return;

  // Ideally, we want to detect errors in opening and report them, but we
  // don't generally have the proper interface for that to the external
  // program that opens the file.  So instead we spawn a check to update
  // the UI if the file has been deleted in parallel with the open.
  delegate_->CheckForFileRemoval(this);
  RecordOpen(GetEndTime(), !GetOpened());
  opened_ = true;
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadOpened(this));
  delegate_->OpenDownload(this);
}

void DownloadItemImpl::ShowDownloadInShell() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  delegate_->ShowDownloadInShell(this);
}

uint32 DownloadItemImpl::GetId() const {
  return download_id_;
}

DownloadItem::DownloadState DownloadItemImpl::GetState() const {
  return InternalToExternalState(state_);
}

DownloadInterruptReason DownloadItemImpl::GetLastReason() const {
  return last_reason_;
}

bool DownloadItemImpl::IsPaused() const {
  return is_paused_;
}

bool DownloadItemImpl::IsTemporary() const {
  return is_temporary_;
}

bool DownloadItemImpl::CanResume() const {
  if ((GetState() == IN_PROGRESS) && IsPaused())
    return true;

  if (state_ != INTERRUPTED_INTERNAL)
    return false;

  // Downloads that don't have a WebContents should still be resumable, but this
  // isn't currently the case. See ResumeInterruptedDownload().
  if (!GetWebContents())
    return false;

  ResumeMode resume_mode = GetResumeMode();
  return IsDownloadResumptionEnabled() &&
      (resume_mode == RESUME_MODE_USER_RESTART ||
       resume_mode == RESUME_MODE_USER_CONTINUE);
}

bool DownloadItemImpl::IsDone() const {
  switch (state_) {
    case IN_PROGRESS_INTERNAL:
    case COMPLETING_INTERNAL:
      return false;

    case COMPLETE_INTERNAL:
    case CANCELLED_INTERNAL:
      return true;

    case INTERRUPTED_INTERNAL:
      return !CanResume();

    case RESUMING_INTERNAL:
      return false;

    case MAX_DOWNLOAD_INTERNAL_STATE:
      break;
  }
  NOTREACHED();
  return true;
}

const GURL& DownloadItemImpl::GetURL() const {
  return url_chain_.empty() ? GURL::EmptyGURL() : url_chain_.back();
}

const std::vector<GURL>& DownloadItemImpl::GetUrlChain() const {
  return url_chain_;
}

const GURL& DownloadItemImpl::GetOriginalUrl() const {
  // Be careful about taking the front() of possibly-empty vectors!
  // http://crbug.com/190096
  return url_chain_.empty() ? GURL::EmptyGURL() : url_chain_.front();
}

const GURL& DownloadItemImpl::GetReferrerUrl() const {
  return referrer_url_;
}

const GURL& DownloadItemImpl::GetTabUrl() const {
  return tab_url_;
}

const GURL& DownloadItemImpl::GetTabReferrerUrl() const {
  return tab_referrer_url_;
}

std::string DownloadItemImpl::GetSuggestedFilename() const {
  return suggested_filename_;
}

std::string DownloadItemImpl::GetContentDisposition() const {
  return content_disposition_;
}

std::string DownloadItemImpl::GetMimeType() const {
  return mime_type_;
}

std::string DownloadItemImpl::GetOriginalMimeType() const {
  return original_mime_type_;
}

std::string DownloadItemImpl::GetRemoteAddress() const {
  return remote_address_;
}

bool DownloadItemImpl::HasUserGesture() const {
  return has_user_gesture_;
};

PageTransition DownloadItemImpl::GetTransitionType() const {
  return transition_type_;
};

const std::string& DownloadItemImpl::GetLastModifiedTime() const {
  return last_modified_time_;
}

const std::string& DownloadItemImpl::GetETag() const {
  return etag_;
}

bool DownloadItemImpl::IsSavePackageDownload() const {
  return is_save_package_download_;
}

const base::FilePath& DownloadItemImpl::GetFullPath() const {
  return current_path_;
}

const base::FilePath& DownloadItemImpl::GetTargetFilePath() const {
  return target_path_;
}

const base::FilePath& DownloadItemImpl::GetForcedFilePath() const {
  // TODO(asanka): Get rid of GetForcedFilePath(). We should instead just
  // require that clients respect GetTargetFilePath() if it is already set.
  return forced_file_path_;
}

base::FilePath DownloadItemImpl::GetFileNameToReportUser() const {
  if (!display_name_.empty())
    return display_name_;
  return target_path_.BaseName();
}

DownloadItem::TargetDisposition DownloadItemImpl::GetTargetDisposition() const {
  return target_disposition_;
}

const std::string& DownloadItemImpl::GetHash() const {
  return hash_;
}

const std::string& DownloadItemImpl::GetHashState() const {
  return hash_state_;
}

bool DownloadItemImpl::GetFileExternallyRemoved() const {
  return file_externally_removed_;
}

void DownloadItemImpl::DeleteFile(const base::Callback<void(bool)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (GetState() != DownloadItem::COMPLETE) {
    // Pass a null WeakPtr so it doesn't call OnDownloadedFileRemoved.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DeleteDownloadedFileDone,
                   base::WeakPtr<DownloadItemImpl>(), callback, false));
    return;
  }
  if (current_path_.empty() || file_externally_removed_) {
    // Pass a null WeakPtr so it doesn't call OnDownloadedFileRemoved.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DeleteDownloadedFileDone,
                   base::WeakPtr<DownloadItemImpl>(), callback, true));
    return;
  }
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DeleteDownloadedFile, current_path_),
      base::Bind(&DeleteDownloadedFileDone,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

bool DownloadItemImpl::IsDangerous() const {
#if defined(OS_WIN)
  // TODO(noelutz): At this point only the windows views UI supports
  // warnings based on dangerous content.
  return (danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED);
#else
  return (danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
          danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_URL);
#endif
}

DownloadDangerType DownloadItemImpl::GetDangerType() const {
  return danger_type_;
}

bool DownloadItemImpl::TimeRemaining(base::TimeDelta* remaining) const {
  if (total_bytes_ <= 0)
    return false;  // We never received the content_length for this download.

  int64 speed = CurrentSpeed();
  if (speed == 0)
    return false;

  *remaining = base::TimeDelta::FromSeconds(
      (total_bytes_ - received_bytes_) / speed);
  return true;
}

int64 DownloadItemImpl::CurrentSpeed() const {
  if (is_paused_)
    return 0;
  return bytes_per_sec_;
}

int DownloadItemImpl::PercentComplete() const {
  // If the delegate is delaying completion of the download, then we have no
  // idea how long it will take.
  if (delegate_delayed_complete_ || total_bytes_ <= 0)
    return -1;

  return static_cast<int>(received_bytes_ * 100.0 / total_bytes_);
}

bool DownloadItemImpl::AllDataSaved() const {
  return all_data_saved_;
}

int64 DownloadItemImpl::GetTotalBytes() const {
  return total_bytes_;
}

int64 DownloadItemImpl::GetReceivedBytes() const {
  return received_bytes_;
}

base::Time DownloadItemImpl::GetStartTime() const {
  return start_time_;
}

base::Time DownloadItemImpl::GetEndTime() const {
  return end_time_;
}

bool DownloadItemImpl::CanShowInFolder() {
  // A download can be shown in the folder if the downloaded file is in a known
  // location.
  return CanOpenDownload() && !GetFullPath().empty();
}

bool DownloadItemImpl::CanOpenDownload() {
  // We can open the file or mark it for opening on completion if the download
  // is expected to complete successfully. Exclude temporary downloads, since
  // they aren't owned by the download system.
  const bool is_complete = GetState() == DownloadItem::COMPLETE;
  return (!IsDone() || is_complete) && !IsTemporary() &&
         !file_externally_removed_;
}

bool DownloadItemImpl::ShouldOpenFileBasedOnExtension() {
  return delegate_->ShouldOpenFileBasedOnExtension(GetTargetFilePath());
}

bool DownloadItemImpl::GetOpenWhenComplete() const {
  return open_when_complete_;
}

bool DownloadItemImpl::GetAutoOpened() {
  return auto_opened_;
}

bool DownloadItemImpl::GetOpened() const {
  return opened_;
}

BrowserContext* DownloadItemImpl::GetBrowserContext() const {
  return delegate_->GetBrowserContext();
}

WebContents* DownloadItemImpl::GetWebContents() const {
  // TODO(rdsmith): Remove null check after removing GetWebContents() from
  // paths that might be used by DownloadItems created from history import.
  // Currently such items have null request_handle_s, where other items
  // (regular and SavePackage downloads) have actual objects off the pointer.
  if (request_handle_)
    return request_handle_->GetWebContents();
  return NULL;
}

void DownloadItemImpl::OnContentCheckCompleted(DownloadDangerType danger_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(AllDataSaved());
  VLOG(20) << __FUNCTION__ << " danger_type=" << danger_type
           << " download=" << DebugString(true);
  SetDangerType(danger_type);
  UpdateObservers();
}

void DownloadItemImpl::SetOpenWhenComplete(bool open) {
  open_when_complete_ = open;
}

void DownloadItemImpl::SetIsTemporary(bool temporary) {
  is_temporary_ = temporary;
}

void DownloadItemImpl::SetOpened(bool opened) {
  opened_ = opened;
}

void DownloadItemImpl::SetDisplayName(const base::FilePath& name) {
  display_name_ = name;
}

std::string DownloadItemImpl::DebugString(bool verbose) const {
  std::string description =
      base::StringPrintf("{ id = %d"
                         " state = %s",
                         download_id_,
                         DebugDownloadStateString(state_));

  // Construct a string of the URL chain.
  std::string url_list("<none>");
  if (!url_chain_.empty()) {
    std::vector<GURL>::const_iterator iter = url_chain_.begin();
    std::vector<GURL>::const_iterator last = url_chain_.end();
    url_list = (*iter).is_valid() ? (*iter).spec() : "<invalid>";
    ++iter;
    for ( ; verbose && (iter != last); ++iter) {
      url_list += " ->\n\t";
      const GURL& next_url = *iter;
      url_list += next_url.is_valid() ? next_url.spec() : "<invalid>";
    }
  }

  if (verbose) {
    description += base::StringPrintf(
        " total = %" PRId64
        " received = %" PRId64
        " reason = %s"
        " paused = %c"
        " resume_mode = %s"
        " auto_resume_count = %d"
        " danger = %d"
        " all_data_saved = %c"
        " last_modified = '%s'"
        " etag = '%s'"
        " has_download_file = %s"
        " url_chain = \n\t\"%s\"\n\t"
        " full_path = \"%" PRFilePath "\"\n\t"
        " target_path = \"%" PRFilePath "\"",
        GetTotalBytes(),
        GetReceivedBytes(),
        DownloadInterruptReasonToString(last_reason_).c_str(),
        IsPaused() ? 'T' : 'F',
        DebugResumeModeString(GetResumeMode()),
        auto_resume_count_,
        GetDangerType(),
        AllDataSaved() ? 'T' : 'F',
        GetLastModifiedTime().c_str(),
        GetETag().c_str(),
        download_file_.get() ? "true" : "false",
        url_list.c_str(),
        GetFullPath().value().c_str(),
        GetTargetFilePath().value().c_str());
  } else {
    description += base::StringPrintf(" url = \"%s\"", url_list.c_str());
  }

  description += " }";

  return description;
}

DownloadItemImpl::ResumeMode DownloadItemImpl::GetResumeMode() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // We can't continue without a handle on the intermediate file.
  // We also can't continue if we don't have some verifier to make sure
  // we're getting the same file.
  const bool force_restart =
      (current_path_.empty() || (etag_.empty() && last_modified_time_.empty()));

  // We won't auto-restart if we've used up our attempts or the
  // download has been paused by user action.
  const bool force_user =
      (auto_resume_count_ >= kMaxAutoResumeAttempts || is_paused_);

  ResumeMode mode = RESUME_MODE_INVALID;

  switch(last_reason_) {
    case DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR:
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT:
      if (force_restart && force_user)
        mode = RESUME_MODE_USER_RESTART;
      else if (force_restart)
        mode = RESUME_MODE_IMMEDIATE_RESTART;
      else if (force_user)
        mode = RESUME_MODE_USER_CONTINUE;
      else
        mode = RESUME_MODE_IMMEDIATE_CONTINUE;
      break;

    case DOWNLOAD_INTERRUPT_REASON_SERVER_PRECONDITION:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE:
    case DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT:
      if (force_user)
        mode = RESUME_MODE_USER_RESTART;
      else
        mode = RESUME_MODE_IMMEDIATE_RESTART;
      break;

    case DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED:
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED:
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN:
    case DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED:
    case DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN:
    case DOWNLOAD_INTERRUPT_REASON_CRASH:
      if (force_restart)
        mode = RESUME_MODE_USER_RESTART;
      else
        mode = RESUME_MODE_USER_CONTINUE;
      break;

    case DOWNLOAD_INTERRUPT_REASON_FILE_FAILED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE:
    case DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG:
    case DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE:
      mode = RESUME_MODE_USER_RESTART;
      break;

    case DOWNLOAD_INTERRUPT_REASON_NONE:
    case DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED:
    case DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT:
    case DOWNLOAD_INTERRUPT_REASON_USER_CANCELED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED:
    case DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED:
      mode = RESUME_MODE_INVALID;
      break;
  }

  return mode;
}

void DownloadItemImpl::MergeOriginInfoOnResume(
    const DownloadCreateInfo& new_create_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(RESUMING_INTERNAL, state_);
  DCHECK(!new_create_info.url_chain.empty());

  // We are going to tack on any new redirects to our list of redirects.
  // When a download is resumed, the URL used for the resumption request is the
  // one at the end of the previous redirect chain. Tacking additional redirects
  // to the end of this chain ensures that:
  // - If the download needs to be resumed again, the ETag/Last-Modified headers
  //   will be used with the last server that sent them to us.
  // - The redirect chain contains all the servers that were involved in this
  //   download since the initial request, in order.
  std::vector<GURL>::const_iterator chain_iter =
      new_create_info.url_chain.begin();
  if (*chain_iter == url_chain_.back())
    ++chain_iter;

  // Record some stats. If the precondition failed (the server returned
  // HTTP_PRECONDITION_FAILED), then the download will automatically retried as
  // a full request rather than a partial. Full restarts clobber validators.
  int origin_state = 0;
  if (chain_iter != new_create_info.url_chain.end())
    origin_state |= ORIGIN_STATE_ON_RESUMPTION_ADDITIONAL_REDIRECTS;
  if (etag_ != new_create_info.etag ||
      last_modified_time_ != new_create_info.last_modified)
    origin_state |= ORIGIN_STATE_ON_RESUMPTION_VALIDATORS_CHANGED;
  if (content_disposition_ != new_create_info.content_disposition)
    origin_state |= ORIGIN_STATE_ON_RESUMPTION_CONTENT_DISPOSITION_CHANGED;
  RecordOriginStateOnResumption(new_create_info.save_info->offset != 0,
                                origin_state);

  url_chain_.insert(
      url_chain_.end(), chain_iter, new_create_info.url_chain.end());
  etag_ = new_create_info.etag;
  last_modified_time_ = new_create_info.last_modified;
  content_disposition_ = new_create_info.content_disposition;

  // Don't update observers. This method is expected to be called just before a
  // DownloadFile is created and Start() is called. The observers will be
  // notified when the download transitions to the IN_PROGRESS state.
}

void DownloadItemImpl::NotifyRemoved() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadRemoved(this));
}

void DownloadItemImpl::OnDownloadedFileRemoved() {
  file_externally_removed_ = true;
  VLOG(20) << __FUNCTION__ << " download=" << DebugString(true);
  UpdateObservers();
}

base::WeakPtr<DownloadDestinationObserver>
DownloadItemImpl::DestinationObserverAsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

const net::BoundNetLog& DownloadItemImpl::GetBoundNetLog() const {
  return bound_net_log_;
}

void DownloadItemImpl::SetTotalBytes(int64 total_bytes) {
  total_bytes_ = total_bytes;
}

void DownloadItemImpl::OnAllDataSaved(const std::string& final_hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK_EQ(IN_PROGRESS_INTERNAL, state_);
  DCHECK(!all_data_saved_);
  all_data_saved_ = true;
  VLOG(20) << __FUNCTION__ << " download=" << DebugString(true);

  // Store final hash and null out intermediate serialized hash state.
  hash_ = final_hash;
  hash_state_ = "";

  UpdateObservers();
}

void DownloadItemImpl::MarkAsComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(all_data_saved_);
  end_time_ = base::Time::Now();
  TransitionTo(COMPLETE_INTERNAL, UPDATE_OBSERVERS);
}

void DownloadItemImpl::DestinationUpdate(int64 bytes_so_far,
                                         int64 bytes_per_sec,
                                         const std::string& hash_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(20) << __FUNCTION__ << " so_far=" << bytes_so_far
           << " per_sec=" << bytes_per_sec << " download=" << DebugString(true);

  if (GetState() != IN_PROGRESS) {
    // Ignore if we're no longer in-progress.  This can happen if we race a
    // Cancel on the UI thread with an update on the FILE thread.
    //
    // TODO(rdsmith): Arguably we should let this go through, as this means
    // the download really did get further than we know before it was
    // cancelled.  But the gain isn't very large, and the code is more
    // fragile if it has to support in progress updates in a non-in-progress
    // state.  This issue should be readdressed when we revamp performance
    // reporting.
    return;
  }
  bytes_per_sec_ = bytes_per_sec;
  hash_state_ = hash_state;
  received_bytes_ = bytes_so_far;

  // If we've received more data than we were expecting (bad server info?),
  // revert to 'unknown size mode'.
  if (received_bytes_ > total_bytes_)
    total_bytes_ = 0;

  if (bound_net_log_.IsLogging()) {
    bound_net_log_.AddEvent(
        net::NetLog::TYPE_DOWNLOAD_ITEM_UPDATED,
        net::NetLog::Int64Callback("bytes_so_far", received_bytes_));
  }

  UpdateObservers();
}

void DownloadItemImpl::DestinationError(DownloadInterruptReason reason) {
  // Postpone recognition of this error until after file name determination
  // has completed and the intermediate file has been renamed to simplify
  // resumption conditions.
  if (current_path_.empty() || target_path_.empty())
    destination_error_ = reason;
  else
    Interrupt(reason);
}

void DownloadItemImpl::DestinationCompleted(const std::string& final_hash) {
  VLOG(20) << __FUNCTION__ << " download=" << DebugString(true);
  if (GetState() != IN_PROGRESS)
    return;
  OnAllDataSaved(final_hash);
  MaybeCompleteDownload();
}

// **** Download progression cascade

void DownloadItemImpl::Init(bool active,
                            DownloadType download_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (active)
    RecordDownloadCount(START_COUNT);

  std::string file_name;
  if (download_type == SRC_HISTORY_IMPORT) {
    // target_path_ works for History and Save As versions.
    file_name = target_path_.AsUTF8Unsafe();
  } else {
    // See if it's set programmatically.
    file_name = forced_file_path_.AsUTF8Unsafe();
    // Possibly has a 'download' attribute for the anchor.
    if (file_name.empty())
      file_name = suggested_filename_;
    // From the URL file name.
    if (file_name.empty())
      file_name = GetURL().ExtractFileName();
  }

  base::Callback<base::Value*(net::NetLog::LogLevel)> active_data = base::Bind(
      &ItemActivatedNetLogCallback, this, download_type, &file_name);
  if (active) {
    bound_net_log_.BeginEvent(
        net::NetLog::TYPE_DOWNLOAD_ITEM_ACTIVE, active_data);
  } else {
    bound_net_log_.AddEvent(
        net::NetLog::TYPE_DOWNLOAD_ITEM_ACTIVE, active_data);
  }

  VLOG(20) << __FUNCTION__ << "() " << DebugString(true);
}

// We're starting the download.
void DownloadItemImpl::Start(
    scoped_ptr<DownloadFile> file,
    scoped_ptr<DownloadRequestHandleInterface> req_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download_file_.get());
  DCHECK(file.get());
  DCHECK(req_handle.get());

  download_file_ = file.Pass();
  request_handle_ = req_handle.Pass();

  if (GetState() == CANCELLED) {
    // The download was in the process of resuming when it was cancelled. Don't
    // proceed.
    ReleaseDownloadFile(true);
    request_handle_->CancelRequest();
    return;
  }

  TransitionTo(IN_PROGRESS_INTERNAL, UPDATE_OBSERVERS);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFile::Initialize,
                 // Safe because we control download file lifetime.
                 base::Unretained(download_file_.get()),
                 base::Bind(&DownloadItemImpl::OnDownloadFileInitialized,
                            weak_ptr_factory_.GetWeakPtr())));
}

void DownloadItemImpl::OnDownloadFileInitialized(
    DownloadInterruptReason result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (result != DOWNLOAD_INTERRUPT_REASON_NONE) {
    Interrupt(result);
    // TODO(rdsmith/asanka): Arguably we should show this in the UI, but
    // it's not at all clear what to show--we haven't done filename
    // determination, so we don't know what name to display.  OTOH,
    // the failure mode of not showing the DI if the file initialization
    // fails isn't a good one.  Can we hack up a name based on the
    // URLRequest?  We'll need to make sure that initialization happens
    // properly.  Possibly the right thing is to have the UI handle
    // this case specially.
    return;
  }

  delegate_->DetermineDownloadTarget(
      this, base::Bind(&DownloadItemImpl::OnDownloadTargetDetermined,
                       weak_ptr_factory_.GetWeakPtr()));
}

// Called by delegate_ when the download target path has been
// determined.
void DownloadItemImpl::OnDownloadTargetDetermined(
    const base::FilePath& target_path,
    TargetDisposition disposition,
    DownloadDangerType danger_type,
    const base::FilePath& intermediate_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the |target_path| is empty, then we consider this download to be
  // canceled.
  if (target_path.empty()) {
    Cancel(true);
    return;
  }

  // TODO(rdsmith,asanka): We are ignoring the possibility that the download
  // has been interrupted at this point until we finish the intermediate
  // rename and set the full path.  That's dangerous, because we might race
  // with resumption, either manual (because the interrupt is visible to the
  // UI) or automatic.  If we keep the "ignore an error on download until file
  // name determination complete" semantics, we need to make sure that the
  // error is kept completely invisible until that point.

  VLOG(20) << __FUNCTION__ << " " << target_path.value() << " " << disposition
           << " " << danger_type << " " << DebugString(true);

  target_path_ = target_path;
  target_disposition_ = disposition;
  SetDangerType(danger_type);

  // We want the intermediate and target paths to refer to the same directory so
  // that they are both on the same device and subject to same
  // space/permission/availability constraints.
  DCHECK(intermediate_path.DirName() == target_path.DirName());

  // During resumption, we may choose to proceed with the same intermediate
  // file. No rename is necessary if our intermediate file already has the
  // correct name.
  //
  // The intermediate name may change from its original value during filename
  // determination on resumption, for example if the reason for the interruption
  // was the download target running out space, resulting in a user prompt.
  if (intermediate_path == current_path_) {
    OnDownloadRenamedToIntermediateName(DOWNLOAD_INTERRUPT_REASON_NONE,
                                        intermediate_path);
    return;
  }

  // Rename to intermediate name.
  // TODO(asanka): Skip this rename if AllDataSaved() is true. This avoids a
  //               spurious rename when we can just rename to the final
  //               filename. Unnecessary renames may cause bugs like
  //               http://crbug.com/74187.
  DCHECK(!is_save_package_download_);
  DCHECK(download_file_.get());
  DownloadFile::RenameCompletionCallback callback =
      base::Bind(&DownloadItemImpl::OnDownloadRenamedToIntermediateName,
                 weak_ptr_factory_.GetWeakPtr());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFile::RenameAndUniquify,
                 // Safe because we control download file lifetime.
                 base::Unretained(download_file_.get()),
                 intermediate_path, callback));
}

void DownloadItemImpl::OnDownloadRenamedToIntermediateName(
    DownloadInterruptReason reason,
    const base::FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(20) << __FUNCTION__ << " download=" << DebugString(true);

  if (DOWNLOAD_INTERRUPT_REASON_NONE != destination_error_) {
    // Process destination error.  If both |reason| and |destination_error_|
    // refer to actual errors, we want to use the |destination_error_| as the
    // argument to the Interrupt() routine, as it happened first.
    if (reason == DOWNLOAD_INTERRUPT_REASON_NONE)
      SetFullPath(full_path);
    Interrupt(destination_error_);
    destination_error_ = DOWNLOAD_INTERRUPT_REASON_NONE;
  } else if (DOWNLOAD_INTERRUPT_REASON_NONE != reason) {
    Interrupt(reason);
    // All file errors result in file deletion above; no need to cleanup.  The
    // current_path_ should be empty. Resuming this download will force a
    // restart and a re-doing of filename determination.
    DCHECK(current_path_.empty());
  } else {
    SetFullPath(full_path);
    UpdateObservers();
    MaybeCompleteDownload();
  }
}

// When SavePackage downloads MHTML to GData (see
// SavePackageFilePickerChromeOS), GData calls MaybeCompleteDownload() like it
// does for non-SavePackage downloads, but SavePackage downloads never satisfy
// IsDownloadReadyForCompletion(). GDataDownloadObserver manually calls
// DownloadItem::UpdateObservers() when the upload completes so that SavePackage
// notices that the upload has completed and runs its normal Finish() pathway.
// MaybeCompleteDownload() is never the mechanism by which SavePackage completes
// downloads. SavePackage always uses its own Finish() to mark downloads
// complete.
void DownloadItemImpl::MaybeCompleteDownload() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_save_package_download_);

  if (!IsDownloadReadyForCompletion(
          base::Bind(&DownloadItemImpl::MaybeCompleteDownload,
                     weak_ptr_factory_.GetWeakPtr())))
    return;

  // TODO(rdsmith): DCHECK that we only pass through this point
  // once per download.  The natural way to do this is by a state
  // transition on the DownloadItem.

  // Confirm we're in the proper set of states to be here;
  // have all data, have a history handle, (validated or safe).
  DCHECK_EQ(IN_PROGRESS_INTERNAL, state_);
  DCHECK(!IsDangerous());
  DCHECK(all_data_saved_);

  OnDownloadCompleting();
}

// Called by MaybeCompleteDownload() when it has determined that the download
// is ready for completion.
void DownloadItemImpl::OnDownloadCompleting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (state_ != IN_PROGRESS_INTERNAL)
    return;

  VLOG(20) << __FUNCTION__ << "()"
           << " " << DebugString(true);
  DCHECK(!GetTargetFilePath().empty());
  DCHECK(!IsDangerous());

  // TODO(rdsmith/benjhayden): Remove as part of SavePackage integration.
  if (is_save_package_download_) {
    // Avoid doing anything on the file thread; there's nothing we control
    // there.
    // Strictly speaking, this skips giving the embedder a chance to open
    // the download.  But on a save package download, there's no real
    // concept of opening.
    Completed();
    return;
  }

  DCHECK(download_file_.get());
  // Unilaterally rename; even if it already has the right name,
  // we need theannotation.
  DownloadFile::RenameCompletionCallback callback =
      base::Bind(&DownloadItemImpl::OnDownloadRenamedToFinalName,
                 weak_ptr_factory_.GetWeakPtr());
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DownloadFile::RenameAndAnnotate,
                 base::Unretained(download_file_.get()),
                 GetTargetFilePath(), callback));
}

void DownloadItemImpl::OnDownloadRenamedToFinalName(
    DownloadInterruptReason reason,
    const base::FilePath& full_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_save_package_download_);

  // If a cancel or interrupt hit, we'll cancel the DownloadFile, which
  // will result in deleting the file on the file thread.  So we don't
  // care about the name having been changed.
  if (state_ != IN_PROGRESS_INTERNAL)
    return;

  VLOG(20) << __FUNCTION__ << "()"
           << " full_path = \"" << full_path.value() << "\""
           << " " << DebugString(false);

  if (DOWNLOAD_INTERRUPT_REASON_NONE != reason) {
    Interrupt(reason);

    // All file errors should have resulted in in file deletion above. On
    // resumption we will need to re-do filename determination.
    DCHECK(current_path_.empty());
    return;
  }

  DCHECK(target_path_ == full_path);

  if (full_path != current_path_) {
    // full_path is now the current and target file path.
    DCHECK(!full_path.empty());
    SetFullPath(full_path);
  }

  // Complete the download and release the DownloadFile.
  DCHECK(download_file_.get());
  ReleaseDownloadFile(false);

  // We're not completely done with the download item yet, but at this
  // point we're committed to complete the download.  Cancels (or Interrupts,
  // though it's not clear how they could happen) after this point will be
  // ignored.
  TransitionTo(COMPLETING_INTERNAL, DONT_UPDATE_OBSERVERS);

  if (delegate_->ShouldOpenDownload(
          this, base::Bind(&DownloadItemImpl::DelayedDownloadOpened,
                           weak_ptr_factory_.GetWeakPtr()))) {
    Completed();
  } else {
    delegate_delayed_complete_ = true;
    UpdateObservers();
  }
}

void DownloadItemImpl::DelayedDownloadOpened(bool auto_opened) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  auto_opened_ = auto_opened;
  Completed();
}

void DownloadItemImpl::Completed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(20) << __FUNCTION__ << "() " << DebugString(false);

  DCHECK(all_data_saved_);
  end_time_ = base::Time::Now();
  TransitionTo(COMPLETE_INTERNAL, UPDATE_OBSERVERS);
  RecordDownloadCompleted(start_tick_, received_bytes_);

  if (auto_opened_) {
    // If it was already handled by the delegate, do nothing.
  } else if (GetOpenWhenComplete() ||
             ShouldOpenFileBasedOnExtension() ||
             IsTemporary()) {
    // If the download is temporary, like in drag-and-drop, do not open it but
    // we still need to set it auto-opened so that it can be removed from the
    // download shelf.
    if (!IsTemporary())
      OpenDownload();

    auto_opened_ = true;
    UpdateObservers();
  }
}

void DownloadItemImpl::OnResumeRequestStarted(
    DownloadItem* item,
    DownloadInterruptReason interrupt_reason) {
  // If |item| is not NULL, then Start() has been called already, and nothing
  // more needs to be done here.
  if (item) {
    DCHECK_EQ(DOWNLOAD_INTERRUPT_REASON_NONE, interrupt_reason);
    DCHECK_EQ(static_cast<DownloadItem*>(this), item);
    return;
  }
  // Otherwise, the request failed without passing through
  // DownloadResourceHandler::OnResponseStarted.
  DCHECK_NE(DOWNLOAD_INTERRUPT_REASON_NONE, interrupt_reason);
  Interrupt(interrupt_reason);
}

// **** End of Download progression cascade

// An error occurred somewhere.
void DownloadItemImpl::Interrupt(DownloadInterruptReason reason) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_NE(DOWNLOAD_INTERRUPT_REASON_NONE, reason);

  // Somewhat counter-intuitively, it is possible for us to receive an
  // interrupt after we've already been interrupted.  The generation of
  // interrupts from the file thread Renames and the generation of
  // interrupts from disk writes go through two different mechanisms (driven
  // by rename requests from UI thread and by write requests from IO thread,
  // respectively), and since we choose not to keep state on the File thread,
  // this is the place where the races collide.  It's also possible for
  // interrupts to race with cancels.

  // Whatever happens, the first one to hit the UI thread wins.
  if (state_ != IN_PROGRESS_INTERNAL && state_ != RESUMING_INTERNAL)
    return;

  last_reason_ = reason;

  ResumeMode resume_mode = GetResumeMode();

  if (state_ == IN_PROGRESS_INTERNAL) {
    // Cancel (delete file) if:
    // 1) we're going to restart.
    // 2) Resumption isn't possible (download was cancelled or blocked due to
    //    security restrictions).
    // 3) Resumption isn't enabled.
    // No point in leaving data around we aren't going to use.
    ReleaseDownloadFile(resume_mode == RESUME_MODE_IMMEDIATE_RESTART ||
                        resume_mode == RESUME_MODE_USER_RESTART ||
                        resume_mode == RESUME_MODE_INVALID ||
                        !IsDownloadResumptionEnabled());

    // Cancel the originating URL request.
    request_handle_->CancelRequest();
  } else {
    DCHECK(!download_file_.get());
  }

  // Reset all data saved, as even if we did save all the data we're going
  // to go through another round of downloading when we resume.
  // There's a potential problem here in the abstract, as if we did download
  // all the data and then run into a continuable error, on resumption we
  // won't download any more data.  However, a) there are currently no
  // continuable errors that can occur after we download all the data, and
  // b) if there were, that would probably simply result in a null range
  // request, which would generate a DestinationCompleted() notification
  // from the DownloadFile, which would behave properly with setting
  // all_data_saved_ to false here.
  all_data_saved_ = false;

  TransitionTo(INTERRUPTED_INTERNAL, DONT_UPDATE_OBSERVERS);
  RecordDownloadInterrupted(reason, received_bytes_, total_bytes_);
  if (!GetWebContents())
    RecordDownloadCount(INTERRUPTED_WITHOUT_WEBCONTENTS);

  AutoResumeIfValid();
  UpdateObservers();
}

void DownloadItemImpl::ReleaseDownloadFile(bool destroy_file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (destroy_file) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        // Will be deleted at end of task execution.
        base::Bind(&DownloadFileCancel, base::Passed(&download_file_)));
    // Avoid attempting to reuse the intermediate file by clearing out
    // current_path_.
    current_path_.clear();
  } else {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(base::IgnoreResult(&DownloadFileDetach),
                   // Will be deleted at end of task execution.
                   base::Passed(&download_file_)));
  }
  // Don't accept any more messages from the DownloadFile, and null
  // out any previous "all data received".  This also breaks links to
  // other entities we've given out weak pointers to.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool DownloadItemImpl::IsDownloadReadyForCompletion(
    const base::Closure& state_change_notification) {
  // If we don't have all the data, the download is not ready for
  // completion.
  if (!AllDataSaved())
    return false;

  // If the download is dangerous, but not yet validated, it's not ready for
  // completion.
  if (IsDangerous())
    return false;

  // If the download isn't active (e.g. has been cancelled) it's not
  // ready for completion.
  if (state_ != IN_PROGRESS_INTERNAL)
    return false;

  // If the target filename hasn't been determined, then it's not ready for
  // completion. This is checked in ReadyForDownloadCompletionDone().
  if (GetTargetFilePath().empty())
    return false;

  // This is checked in NeedsRename(). Without this conditional,
  // browser_tests:DownloadTest.DownloadMimeType fails the DCHECK.
  if (target_path_.DirName() != current_path_.DirName())
    return false;

  // Give the delegate a chance to hold up a stop sign.  It'll call
  // use back through the passed callback if it does and that state changes.
  if (!delegate_->ShouldCompleteDownload(this, state_change_notification))
    return false;

  return true;
}

void DownloadItemImpl::TransitionTo(DownloadInternalState new_state,
                                    ShouldUpdateObservers notify_action) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (state_ == new_state)
    return;

  DownloadInternalState old_state = state_;
  state_ = new_state;

  switch (state_) {
    case COMPLETING_INTERNAL:
      bound_net_log_.AddEvent(
          net::NetLog::TYPE_DOWNLOAD_ITEM_COMPLETING,
          base::Bind(&ItemCompletingNetLogCallback, received_bytes_, &hash_));
      break;
    case COMPLETE_INTERNAL:
      bound_net_log_.AddEvent(
          net::NetLog::TYPE_DOWNLOAD_ITEM_FINISHED,
          base::Bind(&ItemFinishedNetLogCallback, auto_opened_));
      break;
    case INTERRUPTED_INTERNAL:
      bound_net_log_.AddEvent(
          net::NetLog::TYPE_DOWNLOAD_ITEM_INTERRUPTED,
          base::Bind(&ItemInterruptedNetLogCallback, last_reason_,
                     received_bytes_, &hash_state_));
      break;
    case IN_PROGRESS_INTERNAL:
      if (old_state == INTERRUPTED_INTERNAL) {
        bound_net_log_.AddEvent(
            net::NetLog::TYPE_DOWNLOAD_ITEM_RESUMED,
            base::Bind(&ItemResumingNetLogCallback,
                       false, last_reason_, received_bytes_, &hash_state_));
      }
      break;
    case CANCELLED_INTERNAL:
      bound_net_log_.AddEvent(
          net::NetLog::TYPE_DOWNLOAD_ITEM_CANCELED,
          base::Bind(&ItemCanceledNetLogCallback, received_bytes_,
                     &hash_state_));
      break;
    default:
      break;
  }

  VLOG(20) << " " << __FUNCTION__ << "()" << " this = " << DebugString(true)
    << " " << InternalToExternalState(old_state)
    << " " << InternalToExternalState(state_);

  bool is_done = (state_ != IN_PROGRESS_INTERNAL &&
                  state_ != COMPLETING_INTERNAL);
  bool was_done = (old_state != IN_PROGRESS_INTERNAL &&
                   old_state != COMPLETING_INTERNAL);
  // Termination
  if (is_done && !was_done)
    bound_net_log_.EndEvent(net::NetLog::TYPE_DOWNLOAD_ITEM_ACTIVE);

  // Resumption
  if (was_done && !is_done) {
    std::string file_name(target_path_.BaseName().AsUTF8Unsafe());
    bound_net_log_.BeginEvent(net::NetLog::TYPE_DOWNLOAD_ITEM_ACTIVE,
                              base::Bind(&ItemActivatedNetLogCallback,
                                         this, SRC_ACTIVE_DOWNLOAD,
                                         &file_name));
  }

  if (notify_action == UPDATE_OBSERVERS)
    UpdateObservers();
}

void DownloadItemImpl::SetDangerType(DownloadDangerType danger_type) {
  if (danger_type != danger_type_) {
    bound_net_log_.AddEvent(
        net::NetLog::TYPE_DOWNLOAD_ITEM_SAFETY_STATE_UPDATED,
        base::Bind(&ItemCheckedNetLogCallback, danger_type));
  }
  // Only record the Malicious UMA stat if it's going from {not malicious} ->
  // {malicious}.
  if ((danger_type_ == DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS ||
       danger_type_ == DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
       danger_type_ == DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT ||
       danger_type_ == DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT) &&
      (danger_type == DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST ||
       danger_type == DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
       danger_type == DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT ||
       danger_type == DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED)) {
    RecordMaliciousDownloadClassified(danger_type);
  }
  danger_type_ = danger_type;
}

void DownloadItemImpl::SetFullPath(const base::FilePath& new_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(20) << __FUNCTION__ << "()"
           << " new_path = \"" << new_path.value() << "\""
           << " " << DebugString(true);
  DCHECK(!new_path.empty());

  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_ITEM_RENAMED,
      base::Bind(&ItemRenamedNetLogCallback, &current_path_, &new_path));

  current_path_ = new_path;
}

void DownloadItemImpl::AutoResumeIfValid() {
  DVLOG(20) << __FUNCTION__ << "() " << DebugString(true);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ResumeMode mode = GetResumeMode();

  if (mode != RESUME_MODE_IMMEDIATE_RESTART &&
      mode != RESUME_MODE_IMMEDIATE_CONTINUE) {
    return;
  }

  auto_resume_count_++;

  ResumeInterruptedDownload();
}

void DownloadItemImpl::ResumeInterruptedDownload() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the flag for downloads resumption isn't enabled, ignore
  // this request.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kEnableDownloadResumption))
    return;

  // If we're not interrupted, ignore the request; our caller is drunk.
  if (state_ != INTERRUPTED_INTERNAL)
    return;

  // If we can't get a web contents, we can't resume the download.
  // TODO(rdsmith): Find some alternative web contents to use--this
  // means we can't restart a download if it's a download imported
  // from the history.
  if (!GetWebContents())
    return;

  // Reset the appropriate state if restarting.
  ResumeMode mode = GetResumeMode();
  if (mode == RESUME_MODE_IMMEDIATE_RESTART ||
      mode == RESUME_MODE_USER_RESTART) {
    received_bytes_ = 0;
    hash_state_ = "";
    last_modified_time_ = "";
    etag_ = "";
  }

  scoped_ptr<DownloadUrlParameters> download_params(
      DownloadUrlParameters::FromWebContents(GetWebContents(),
                                             GetOriginalUrl()));

  download_params->set_file_path(GetFullPath());
  download_params->set_offset(GetReceivedBytes());
  download_params->set_hash_state(GetHashState());
  download_params->set_last_modified(GetLastModifiedTime());
  download_params->set_etag(GetETag());
  download_params->set_callback(
      base::Bind(&DownloadItemImpl::OnResumeRequestStarted,
                 weak_ptr_factory_.GetWeakPtr()));

  delegate_->ResumeInterruptedDownload(download_params.Pass(), GetId());
  // Just in case we were interrupted while paused.
  is_paused_ = false;

  TransitionTo(RESUMING_INTERNAL, DONT_UPDATE_OBSERVERS);
}

// static
DownloadItem::DownloadState DownloadItemImpl::InternalToExternalState(
    DownloadInternalState internal_state) {
  switch (internal_state) {
    case IN_PROGRESS_INTERNAL:
      return IN_PROGRESS;
    case COMPLETING_INTERNAL:
      return IN_PROGRESS;
    case COMPLETE_INTERNAL:
      return COMPLETE;
    case CANCELLED_INTERNAL:
      return CANCELLED;
    case INTERRUPTED_INTERNAL:
      return INTERRUPTED;
    case RESUMING_INTERNAL:
      return INTERRUPTED;
    case MAX_DOWNLOAD_INTERNAL_STATE:
      break;
  }
  NOTREACHED();
  return MAX_DOWNLOAD_STATE;
}

// static
DownloadItemImpl::DownloadInternalState
DownloadItemImpl::ExternalToInternalState(
    DownloadState external_state) {
  switch (external_state) {
    case IN_PROGRESS:
      return IN_PROGRESS_INTERNAL;
    case COMPLETE:
      return COMPLETE_INTERNAL;
    case CANCELLED:
      return CANCELLED_INTERNAL;
    case INTERRUPTED:
      return INTERRUPTED_INTERNAL;
    default:
      NOTREACHED();
  }
  return MAX_DOWNLOAD_INTERNAL_STATE;
}

const char* DownloadItemImpl::DebugDownloadStateString(
    DownloadInternalState state) {
  switch (state) {
    case IN_PROGRESS_INTERNAL:
      return "IN_PROGRESS";
    case COMPLETING_INTERNAL:
      return "COMPLETING";
    case COMPLETE_INTERNAL:
      return "COMPLETE";
    case CANCELLED_INTERNAL:
      return "CANCELLED";
    case INTERRUPTED_INTERNAL:
      return "INTERRUPTED";
    case RESUMING_INTERNAL:
      return "RESUMING";
    case MAX_DOWNLOAD_INTERNAL_STATE:
      break;
  };
  NOTREACHED() << "Unknown download state " << state;
  return "unknown";
}

const char* DownloadItemImpl::DebugResumeModeString(ResumeMode mode) {
  switch (mode) {
    case RESUME_MODE_INVALID:
      return "INVALID";
    case RESUME_MODE_IMMEDIATE_CONTINUE:
      return "IMMEDIATE_CONTINUE";
    case RESUME_MODE_IMMEDIATE_RESTART:
      return "IMMEDIATE_RESTART";
    case RESUME_MODE_USER_CONTINUE:
      return "USER_CONTINUE";
    case RESUME_MODE_USER_RESTART:
      return "USER_RESTART";
  }
  NOTREACHED() << "Unknown resume mode " << mode;
  return "unknown";
}

}  // namespace content
