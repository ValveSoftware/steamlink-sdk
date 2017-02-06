// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "content/browser/download/download_destination_observer.h"
#include "content/browser/download/download_net_log_parameters.h"
#include "content/browser/download/download_request_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "net/log/net_log.h"
#include "url/gurl.h"

namespace content {
class DownloadFile;
class DownloadItemImplDelegate;

// See download_item.h for usage.
class CONTENT_EXPORT DownloadItemImpl
    : public DownloadItem,
      public DownloadDestinationObserver {
 public:
  enum ResumeMode {
    RESUME_MODE_INVALID = 0,
    RESUME_MODE_IMMEDIATE_CONTINUE,
    RESUME_MODE_IMMEDIATE_RESTART,
    RESUME_MODE_USER_CONTINUE,
    RESUME_MODE_USER_RESTART
  };

  // The maximum number of attempts we will make to resume automatically.
  static const int kMaxAutoResumeAttempts;

  // Note that it is the responsibility of the caller to ensure that a
  // DownloadItemImplDelegate passed to a DownloadItemImpl constructor
  // outlives the DownloadItemImpl.

  // Constructing from persistent store:
  // |bound_net_log| is constructed externally for our use.
  DownloadItemImpl(DownloadItemImplDelegate* delegate,
                   const std::string& guid,
                   uint32_t id,
                   const base::FilePath& current_path,
                   const base::FilePath& target_path,
                   const std::vector<GURL>& url_chain,
                   const GURL& referrer_url,
                   const GURL& site_url,
                   const GURL& tab_url,
                   const GURL& tab_referrer_url,
                   const std::string& mime_type,
                   const std::string& original_mime_type,
                   const base::Time& start_time,
                   const base::Time& end_time,
                   const std::string& etag,
                   const std::string& last_modified,
                   int64_t received_bytes,
                   int64_t total_bytes,
                   const std::string& hash,
                   DownloadItem::DownloadState state,
                   DownloadDangerType danger_type,
                   DownloadInterruptReason interrupt_reason,
                   bool opened,
                   const net::BoundNetLog& bound_net_log);

  // Constructing for a regular download.
  // |bound_net_log| is constructed externally for our use.
  DownloadItemImpl(DownloadItemImplDelegate* delegate,
                   uint32_t id,
                   const DownloadCreateInfo& info,
                   const net::BoundNetLog& bound_net_log);

  // Constructing for the "Save Page As..." feature:
  // |bound_net_log| is constructed externally for our use.
  DownloadItemImpl(
      DownloadItemImplDelegate* delegate,
      uint32_t id,
      const base::FilePath& path,
      const GURL& url,
      const std::string& mime_type,
      std::unique_ptr<DownloadRequestHandleInterface> request_handle,
      const net::BoundNetLog& bound_net_log);

  ~DownloadItemImpl() override;

  // DownloadItem
  void AddObserver(DownloadItem::Observer* observer) override;
  void RemoveObserver(DownloadItem::Observer* observer) override;
  void UpdateObservers() override;
  void ValidateDangerousDownload() override;
  void StealDangerousDownload(const AcquireFileCallback& callback) override;
  void Pause() override;
  void Resume() override;
  void Cancel(bool user_cancel) override;
  void Remove() override;
  void OpenDownload() override;
  void ShowDownloadInShell() override;
  uint32_t GetId() const override;
  const std::string& GetGuid() const override;
  DownloadState GetState() const override;
  DownloadInterruptReason GetLastReason() const override;
  bool IsPaused() const override;
  bool IsTemporary() const override;
  bool CanResume() const override;
  bool IsDone() const override;
  const GURL& GetURL() const override;
  const std::vector<GURL>& GetUrlChain() const override;
  const GURL& GetOriginalUrl() const override;
  const GURL& GetReferrerUrl() const override;
  const GURL& GetSiteUrl() const override;
  const GURL& GetTabUrl() const override;
  const GURL& GetTabReferrerUrl() const override;
  std::string GetSuggestedFilename() const override;
  std::string GetContentDisposition() const override;
  std::string GetMimeType() const override;
  std::string GetOriginalMimeType() const override;
  std::string GetRemoteAddress() const override;
  bool HasUserGesture() const override;
  ui::PageTransition GetTransitionType() const override;
  const std::string& GetLastModifiedTime() const override;
  const std::string& GetETag() const override;
  bool IsSavePackageDownload() const override;
  const base::FilePath& GetFullPath() const override;
  const base::FilePath& GetTargetFilePath() const override;
  const base::FilePath& GetForcedFilePath() const override;
  base::FilePath GetFileNameToReportUser() const override;
  TargetDisposition GetTargetDisposition() const override;
  const std::string& GetHash() const override;
  bool GetFileExternallyRemoved() const override;
  void DeleteFile(const base::Callback<void(bool)>& callback) override;
  bool IsDangerous() const override;
  DownloadDangerType GetDangerType() const override;
  bool TimeRemaining(base::TimeDelta* remaining) const override;
  int64_t CurrentSpeed() const override;
  int PercentComplete() const override;
  bool AllDataSaved() const override;
  int64_t GetTotalBytes() const override;
  int64_t GetReceivedBytes() const override;
  base::Time GetStartTime() const override;
  base::Time GetEndTime() const override;
  bool CanShowInFolder() override;
  bool CanOpenDownload() override;
  bool ShouldOpenFileBasedOnExtension() override;
  bool GetOpenWhenComplete() const override;
  bool GetAutoOpened() override;
  bool GetOpened() const override;
  BrowserContext* GetBrowserContext() const override;
  WebContents* GetWebContents() const override;
  void OnContentCheckCompleted(DownloadDangerType danger_type) override;
  void SetOpenWhenComplete(bool open) override;
  void SetIsTemporary(bool temporary) override;
  void SetOpened(bool opened) override;
  void SetDisplayName(const base::FilePath& name) override;
  std::string DebugString(bool verbose) const override;

  // All remaining public interfaces virtual to allow for DownloadItemImpl
  // mocks.

  // Determines the resume mode for an interrupted download. Requires
  // last_reason_ to be set, but doesn't require the download to be in
  // INTERRUPTED state.
  virtual ResumeMode GetResumeMode() const;

  // State transition operations on regular downloads --------------------------

  // Start the download.
  // |download_file| is the associated file on the storage medium.
  // |req_handle| is the new request handle associated with the download.
  // |new_create_info| is a DownloadCreateInfo containing the new response
  // parameters. It may be different from the DownloadCreateInfo used to create
  // the DownloadItem if Start() is being called in response for a download
  // resumption request.
  virtual void Start(std::unique_ptr<DownloadFile> download_file,
                     std::unique_ptr<DownloadRequestHandleInterface> req_handle,
                     const DownloadCreateInfo& new_create_info);

  // Needed because of intertwining with DownloadManagerImpl -------------------

  // TODO(rdsmith): Unwind DownloadManagerImpl and DownloadItemImpl,
  // removing these from the public interface.

  // Notify observers that this item is being removed by the user.
  virtual void NotifyRemoved();

  virtual void OnDownloadedFileRemoved();

  // Provide a weak pointer reference to a DownloadDestinationObserver
  // for use by download destinations.
  virtual base::WeakPtr<DownloadDestinationObserver>
      DestinationObserverAsWeakPtr();

  // Get the download's BoundNetLog.
  virtual const net::BoundNetLog& GetBoundNetLog() const;

  // DownloadItemImpl routines only needed by SavePackage ----------------------

  // Called by SavePackage to set the total number of bytes on the item.
  virtual void SetTotalBytes(int64_t total_bytes);

  virtual void OnAllDataSaved(int64_t total_bytes,
                              std::unique_ptr<crypto::SecureHash> hash_state);

  // Called by SavePackage to display progress when the DownloadItem
  // should be considered complete.
  virtual void MarkAsComplete();

  // DownloadDestinationObserver
  void DestinationUpdate(int64_t bytes_so_far, int64_t bytes_per_sec) override;
  void DestinationError(
      DownloadInterruptReason reason,
      int64_t bytes_so_far,
      std::unique_ptr<crypto::SecureHash> hash_state) override;
  void DestinationCompleted(
      int64_t total_bytes,
      std::unique_ptr<crypto::SecureHash> hash_state) override;

 private:
  // Fine grained states of a download.
  //
  // New downloads can be created in the following states:
  //
  //     INITIAL_INTERNAL:        All active new downloads.
  //
  //     COMPLETE_INTERNAL:       Downloads restored from persisted state.
  //     CANCELLED_INTERNAL:      - do -
  //     INTERRUPTED_INTERNAL:    - do -
  //
  //     IN_PROGRESS_INTERNAL:    SavePackage downloads.
  //
  // On debug builds, state transitions can be verified via
  // IsValidStateTransition() and IsValidSavePackageStateTransition(). Allowed
  // state transitions are described below, both for normal downloads and
  // SavePackage downloads.
  enum DownloadInternalState {
    // Initial state. Regular downloads are created in this state until the
    // Start() call is received.
    //
    // Transitions to (regular):
    //   TARGET_PENDING_INTERNAL: After a successful Start() call.
    //   INTERRUPTED_TARGET_PENDING_INTERNAL: After a failed Start() call.
    //
    // Transitions to (SavePackage):
    //   <n/a>                    SavePackage downloads never reach this state.
    INITIAL_INTERNAL,

    // Embedder is in the process of determining the target of the download.
    // Since the embedder is sensitive to state transitions during this time,
    // any DestinationError/DestinationCompleted events are deferred until
    // TARGET_RESOLVED_INTERNAL.
    //
    // Transitions to (regular):
    //   TARGET_RESOLVED_INTERNAL: Once the embedder invokes the callback.
    //   INTERRUPTED_TARGET_PENDING_INTERNAL: An error occurred prior to target
    //                            determination.
    //   CANCELLED_INTERNAL:      Cancelled.
    //
    // Transitions to (SavePackage):
    //   <n/a>                    SavePackage downloads never reach this state.
    TARGET_PENDING_INTERNAL,

    // Embedder is in the process of determining the target of the download, and
    // the download is in an interrupted state. The interrupted state is not
    // exposed to the emedder until target determination is complete.
    //
    // Transitions to (regular):
    //   INTERRUPTED_INTERNAL:    Once the target is determined, the download
    //                            is marked as interrupted.
    //   CANCELLED_INTERNAL:      Cancelled.
    //
    // Transitions to (SavePackage):
    //   <n/a>                    SavePackage downloads never reach this state.
    INTERRUPTED_TARGET_PENDING_INTERNAL,

    // Embedder has completed target determination. It is now safe to resolve
    // the download target as well as process deferred DestinationError events.
    // This state is differs from TARGET_PENDING_INTERNAL due to it being
    // allowed to transition to INTERRUPTED_INTERNAL, and it's different from
    // IN_PROGRESS_INTERNAL in that entering this state doesn't require having
    // a valid target. This state is transient (i.e. DownloadItemImpl will
    // transition out of it before yielding execution). It's only purpose in
    // life is to ensure the integrity of state transitions.
    //
    // Transitions to (regular):
    //   IN_PROGRESS_INTERNAL:    Target successfully determined. The incoming
    //                            data stream can now be written to the target.
    //   INTERRUPTED_INTERNAL:    Either the target determination or one of the
    //                            deferred signals indicated that the download
    //                            should be interrupted.
    //   CANCELLED_INTERNAL:      User cancelled the download or there was a
    //                            deferred Cancel() call.
    //
    // Transitions to (SavePackage):
    //   <n/a>                    SavePackage downloads never reach this state.
    TARGET_RESOLVED_INTERNAL,

    // Download target is known and the data can be transferred from our source
    // to our sink.
    //
    // Transitions to (regular):
    //   COMPLETING_INTERNAL:     On final rename completion.
    //   CANCELLED_INTERNAL:      On cancel.
    //   INTERRUPTED_INTERNAL:    On interrupt.
    //
    // Transitions to (SavePackage):
    //   COMPLETE_INTERNAL:       On completion.
    //   CANCELLED_INTERNAL:      On cancel.
    IN_PROGRESS_INTERNAL,

    // Between commit point (dispatch of download file release) and completed.
    // Embedder may be opening the file in this state.
    //
    // Transitions to (regular):
    //   COMPLETE_INTERNAL:       On successful completion.
    //
    // Transitions to (SavePackage):
    //   <n/a>                    SavePackage downloads never reach this state.
    COMPLETING_INTERNAL,

    // After embedder has had a chance to auto-open.  User may now open
    // or auto-open based on extension.
    //
    // Transitions to (regular):
    //   <none>                   Terminal state.
    //
    // Transitions to (SavePackage):
    //   <none>                   Terminal state.
    COMPLETE_INTERNAL,

    // An error has interrupted the download.
    //
    // Transitions to (regular):
    //   RESUMING_INTERNAL:       On resumption.
    //   CANCELLED_INTERNAL:      On cancel.
    //
    // Transitions to (SavePackage):
    //   <n/a>                    SavePackage downloads never reach this state.
    INTERRUPTED_INTERNAL,

    // A request to resume this interrupted download is in progress.
    //
    // Transitions to (regular):
    //   TARGET_PENDING_INTERNAL: Once a server response is received from a
    //                            resumption.
    //   INTERRUPTED_TARGET_PENDING_INTERNAL: A server response was received,
    //                            but it indicated an error, and the download
    //                            needs to go through target determination.
    //   TARGET_RESOLVED_INTERNAL: A resumption attempt received an error
    //                            but it was not necessary to go through target
    //                            determination.
    //   CANCELLED_INTERNAL:      On cancel.
    //
    // Transitions to (SavePackage):
    //   <n/a>                    SavePackage downloads never reach this state.
    RESUMING_INTERNAL,

    // User has cancelled the download.
    // TODO(asanka): Merge interrupted and cancelled states.
    //
    // Transitions to (regular):
    //   <none>                   Terminal state.
    //
    // Transitions to (SavePackage):
    //   <none>                   Terminal state.
    CANCELLED_INTERNAL,

    MAX_DOWNLOAD_INTERNAL_STATE,
  };

  // Normal progression of a download ------------------------------------------

  // These are listed in approximately chronological order.  There are also
  // public methods involved in normal download progression; see
  // the implementation ordering in download_item_impl.cc.

  // Construction common to all constructors. |active| should be true for new
  // downloads and false for downloads from the history.
  // |download_type| indicates to the net log system what kind of download
  // this is.
  void Init(bool active, DownloadType download_type);

  // Callback from file thread when we initialize the DownloadFile.
  void OnDownloadFileInitialized(DownloadInterruptReason result);

  // Called to determine the target path. Will cause OnDownloadTargetDetermined
  // to be called when the target information is available.
  void DetermineDownloadTarget();

  // Called when the target path has been determined. |target_path| is the
  // suggested target path. |disposition| indicates how the target path should
  // be used (see TargetDisposition). |danger_type| is the danger level of
  // |target_path| as determined by the caller. |intermediate_path| is the path
  // to use to store the download until OnDownloadCompleting() is called.
  virtual void OnDownloadTargetDetermined(
      const base::FilePath& target_path,
      TargetDisposition disposition,
      DownloadDangerType danger_type,
      const base::FilePath& intermediate_path);

  void OnDownloadRenamedToIntermediateName(
      DownloadInterruptReason reason, const base::FilePath& full_path);

  // If all pre-requisites have been met, complete download processing, i.e. do
  // internal cleanup, file rename, and potentially auto-open.  (Dangerous
  // downloads still may block on user acceptance after this point.)
  void MaybeCompleteDownload();

  // Called when the download is ready to complete.
  // This may perform final rename if necessary and will eventually call
  // DownloadItem::Completed().
  void OnDownloadCompleting();

  void OnDownloadRenamedToFinalName(DownloadInterruptReason reason,
                                    const base::FilePath& full_path);

  // Called if the embedder took over opening a download, to indicate that
  // the download has been opened.
  void DelayedDownloadOpened(bool auto_opened);

  // Called when the entire download operation (including renaming etc.)
  // is completed.
  void Completed();

  // Helper routines -----------------------------------------------------------

  // Indicate that an error has occurred on the download. Discards partial
  // state. The interrupted download will not be considered continuable, but may
  // be restarted.
  void InterruptAndDiscardPartialState(DownloadInterruptReason reason);

  // Indiates that an error has occurred on the download. The |bytes_so_far| and
  // |hash_state| should correspond to the state of the DownloadFile. If the
  // interrupt reason allows, this partial state may be allowed to continue the
  // interrupted download upon resumption.
  void InterruptWithPartialState(int64_t bytes_so_far,
                                 std::unique_ptr<crypto::SecureHash> hash_state,
                                 DownloadInterruptReason reason);

  void UpdateProgress(int64_t bytes_so_far, int64_t bytes_per_sec);

  // Set |hash_| and |hash_state_| based on |hash_state|.
  void SetHashState(std::unique_ptr<crypto::SecureHash> hash_state);

  // Destroy the DownloadFile object.  If |destroy_file| is true, the file is
  // destroyed with it.  Otherwise, DownloadFile::Detach() is called before
  // object destruction to prevent file destruction. Destroying the file also
  // resets |current_path_|.
  void ReleaseDownloadFile(bool destroy_file);

  // Check if a download is ready for completion.  The callback provided
  // may be called at some point in the future if an external entity
  // state has change s.t. this routine should be checked again.
  bool IsDownloadReadyForCompletion(const base::Closure& state_change_notify);

  // Call to transition state; all state transitions should go through this.
  // |notify_action| specifies whether or not to call UpdateObservers() after
  // the state transition.
  void TransitionTo(DownloadInternalState new_state);

  // Set the |danger_type_| and invoke observers if necessary.
  void SetDangerType(DownloadDangerType danger_type);

  void SetFullPath(const base::FilePath& new_path);

  void AutoResumeIfValid();

  enum class ResumptionRequestSource { AUTOMATIC, USER };
  void ResumeInterruptedDownload(ResumptionRequestSource source);

  // Update origin information based on the response to a download resumption
  // request. Should only be called if the resumption request was successful.
  virtual void UpdateValidatorsOnResumption(
      const DownloadCreateInfo& new_create_info);

  static DownloadState InternalToExternalState(
      DownloadInternalState internal_state);
  static DownloadInternalState ExternalToInternalState(
      DownloadState external_state);

  // Debugging routines --------------------------------------------------------
  static const char* DebugDownloadStateString(DownloadInternalState state);
  static const char* DebugResumeModeString(ResumeMode mode);
  static bool IsValidSavePackageStateTransition(DownloadInternalState from,
                                                DownloadInternalState to);
  static bool IsValidStateTransition(DownloadInternalState from,
                                     DownloadInternalState to);

  // Will be false for save package downloads retrieved from the history.
  // TODO(rdsmith): Replace with a generalized enum for "download source".
  const bool is_save_package_download_ = false;

  // The handle to the request information.  Used for operations outside the
  // download system.
  std::unique_ptr<DownloadRequestHandleInterface> request_handle_;

  std::string guid_;

  uint32_t download_id_ = kInvalidId;

  // Display name for the download. If this is empty, then the display name is
  // considered to be |target_path_.BaseName()|.
  base::FilePath display_name_;

  // Target path of an in-progress download. We may be downloading to a
  // temporary or intermediate file (specified by |current_path_|.  Once the
  // download completes, we will rename the file to |target_path_|.
  base::FilePath target_path_;

  // Whether the target should be overwritten, uniquified or prompted for.
  TargetDisposition target_disposition_ = TARGET_DISPOSITION_OVERWRITE;

  // The chain of redirects that leading up to and including the final URL.
  std::vector<GURL> url_chain_;

  // The URL of the page that initiated the download.
  GURL referrer_url_;

  // Site URL for the site instance that initiated this download.
  GURL site_url_;

  // The URL of the tab that initiated the download.
  GURL tab_url_;

  // The URL of the referrer of the tab that initiated the download.
  GURL tab_referrer_url_;

  // Filename suggestion from DownloadSaveInfo. It could, among others, be the
  // suggested filename in 'download' attribute of an anchor. Details:
  // http://www.whatwg.org/specs/web-apps/current-work/#downloading-hyperlinks
  std::string suggested_filename_;

  // If non-empty, contains an externally supplied path that should be used as
  // the target path.
  base::FilePath forced_file_path_;

  // Page transition that triggerred the download.
  ui::PageTransition transition_type_ = ui::PAGE_TRANSITION_LINK;

  // Whether the download was triggered with a user gesture.
  bool has_user_gesture_ = false;

  // Information from the request.
  // Content-disposition field from the header.
  std::string content_disposition_;

  // Mime-type from the header.  Subject to change.
  std::string mime_type_;

  // The value of the content type header sent with the downloaded item.  It
  // may be different from |mime_type_|, which may be set based on heuristics
  // which may look at the file extension and first few bytes of the file.
  std::string original_mime_type_;

  // The remote IP address where the download was fetched from.  Copied from
  // DownloadCreateInfo::remote_address.
  std::string remote_address_;

  // Total bytes expected.
  int64_t total_bytes_ = 0;

  // Last reason.
  DownloadInterruptReason last_reason_ = DOWNLOAD_INTERRUPT_REASON_NONE;

  // Start time for recording statistics.
  base::TimeTicks start_tick_;

  // The current state of this download.
  DownloadInternalState state_ = INITIAL_INTERNAL;

  // Current danger type for the download.
  DownloadDangerType danger_type_ = DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;

  // The views of this item in the download shelf and download contents.
  base::ObserverList<Observer> observers_;

  // Time the download was started.
  base::Time start_time_;

  // Time the download completed.
  base::Time end_time_;

  // Our delegate.
  DownloadItemImplDelegate* delegate_ = nullptr;

  // In progress downloads may be paused by the user, we note it here.
  bool is_paused_ = false;

  // A flag for indicating if the download should be opened at completion.
  bool open_when_complete_ = false;

  // A flag for indicating if the downloaded file is externally removed.
  bool file_externally_removed_ = false;

  // True if the download was auto-opened. We set this rather than using
  // an observer as it's frequently possible for the download to be auto opened
  // before the observer is added.
  bool auto_opened_ = false;

  // True if the item was downloaded temporarily.
  bool is_temporary_ = false;

  // Did the user open the item either directly or indirectly (such as by
  // setting always open files of this type)? The shelf also sets this field
  // when the user closes the shelf before the item has been opened but should
  // be treated as though the user opened it.
  bool opened_ = false;

  // Did the delegate delay calling Complete on this download?
  bool delegate_delayed_complete_ = false;

  // Error return from DestinationError.  Stored separately from
  // last_reason_ so that we can avoid handling destination errors until
  // after file name determination has occurred.
  DownloadInterruptReason destination_error_ = DOWNLOAD_INTERRUPT_REASON_NONE;

  // The following fields describe the current state of the download file.

  // DownloadFile associated with this download.  Note that this
  // pointer may only be used or destroyed on the FILE thread.
  // This pointer will be non-null only while the DownloadItem is in
  // the IN_PROGRESS state.
  std::unique_ptr<DownloadFile> download_file_;

  // Full path to the downloaded or downloading file. This is the path to the
  // physical file, if one exists. The final target path is specified by
  // |target_path_|. |current_path_| can be empty if the in-progress path hasn't
  // been determined.
  base::FilePath current_path_;

  // Current received bytes.
  int64_t received_bytes_ = 0;

  // Current speed. Calculated by the DownloadFile.
  int64_t bytes_per_sec_ = 0;

  // True if we've saved all the data for the download. If true, then the file
  // at |current_path_| contains |received_bytes_|, which constitute the
  // entirety of what we expect to save there. A digest of its contents can be
  // found at |hash_|.
  bool all_data_saved_ = false;

  // The number of times this download has been resumed automatically. Will be
  // reset to 0 if a resumption is performed in response to a Resume() call.
  int auto_resume_count_ = 0;

  // SHA256 hash of the possibly partial content. The hash is updated each time
  // the download is interrupted, and when the all the data has been
  // transferred. |hash_| contains the raw binary hash and is not hex encoded.
  //
  // While the download is in progress, and while resuming, |hash_| will be
  // empty.
  std::string hash_;

  // In the event of an interruption, the DownloadDestinationObserver interface
  // exposes the partial hash state. This state can be held by the download item
  // in case it's needed for resumption.
  std::unique_ptr<crypto::SecureHash> hash_state_;

  // Contents of the Last-Modified header for the most recent server response.
  std::string last_modified_time_;

  // Server's ETAG for the file.
  std::string etag_;

  // Net log to use for this download.
  const net::BoundNetLog bound_net_log_;

  base::WeakPtrFactory<DownloadItemImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_IMPL_H_
