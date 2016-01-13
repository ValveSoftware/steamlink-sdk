// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/download/download_net_log_parameters.h"
#include "content/browser/download/download_request_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_destination_observer.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "net/base/net_log.h"
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
                   uint32 id,
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
                   const net::BoundNetLog& bound_net_log);

  // Constructing for a regular download.
  // |bound_net_log| is constructed externally for our use.
  DownloadItemImpl(DownloadItemImplDelegate* delegate,
                   uint32 id,
                   const DownloadCreateInfo& info,
                   const net::BoundNetLog& bound_net_log);

  // Constructing for the "Save Page As..." feature:
  // |bound_net_log| is constructed externally for our use.
  DownloadItemImpl(DownloadItemImplDelegate* delegate,
                   uint32 id,
                   const base::FilePath& path,
                   const GURL& url,
                   const std::string& mime_type,
                   scoped_ptr<DownloadRequestHandleInterface> request_handle,
                   const net::BoundNetLog& bound_net_log);

  virtual ~DownloadItemImpl();

  // DownloadItem
  virtual void AddObserver(DownloadItem::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(DownloadItem::Observer* observer) OVERRIDE;
  virtual void UpdateObservers() OVERRIDE;
  virtual void ValidateDangerousDownload() OVERRIDE;
  virtual void StealDangerousDownload(const AcquireFileCallback& callback)
      OVERRIDE;
  virtual void Pause() OVERRIDE;
  virtual void Resume() OVERRIDE;
  virtual void Cancel(bool user_cancel) OVERRIDE;
  virtual void Remove() OVERRIDE;
  virtual void OpenDownload() OVERRIDE;
  virtual void ShowDownloadInShell() OVERRIDE;
  virtual uint32 GetId() const OVERRIDE;
  virtual DownloadState GetState() const OVERRIDE;
  virtual DownloadInterruptReason GetLastReason() const OVERRIDE;
  virtual bool IsPaused() const OVERRIDE;
  virtual bool IsTemporary() const OVERRIDE;
  virtual bool CanResume() const OVERRIDE;
  virtual bool IsDone() const OVERRIDE;
  virtual const GURL& GetURL() const OVERRIDE;
  virtual const std::vector<GURL>& GetUrlChain() const OVERRIDE;
  virtual const GURL& GetOriginalUrl() const OVERRIDE;
  virtual const GURL& GetReferrerUrl() const OVERRIDE;
  virtual const GURL& GetTabUrl() const OVERRIDE;
  virtual const GURL& GetTabReferrerUrl() const OVERRIDE;
  virtual std::string GetSuggestedFilename() const OVERRIDE;
  virtual std::string GetContentDisposition() const OVERRIDE;
  virtual std::string GetMimeType() const OVERRIDE;
  virtual std::string GetOriginalMimeType() const OVERRIDE;
  virtual std::string GetRemoteAddress() const OVERRIDE;
  virtual bool HasUserGesture() const OVERRIDE;
  virtual PageTransition GetTransitionType() const OVERRIDE;
  virtual const std::string& GetLastModifiedTime() const OVERRIDE;
  virtual const std::string& GetETag() const OVERRIDE;
  virtual bool IsSavePackageDownload() const OVERRIDE;
  virtual const base::FilePath& GetFullPath() const OVERRIDE;
  virtual const base::FilePath& GetTargetFilePath() const OVERRIDE;
  virtual const base::FilePath& GetForcedFilePath() const OVERRIDE;
  virtual base::FilePath GetFileNameToReportUser() const OVERRIDE;
  virtual TargetDisposition GetTargetDisposition() const OVERRIDE;
  virtual const std::string& GetHash() const OVERRIDE;
  virtual const std::string& GetHashState() const OVERRIDE;
  virtual bool GetFileExternallyRemoved() const OVERRIDE;
  virtual void DeleteFile(const base::Callback<void(bool)>& callback) OVERRIDE;
  virtual bool IsDangerous() const OVERRIDE;
  virtual DownloadDangerType GetDangerType() const OVERRIDE;
  virtual bool TimeRemaining(base::TimeDelta* remaining) const OVERRIDE;
  virtual int64 CurrentSpeed() const OVERRIDE;
  virtual int PercentComplete() const OVERRIDE;
  virtual bool AllDataSaved() const OVERRIDE;
  virtual int64 GetTotalBytes() const OVERRIDE;
  virtual int64 GetReceivedBytes() const OVERRIDE;
  virtual base::Time GetStartTime() const OVERRIDE;
  virtual base::Time GetEndTime() const OVERRIDE;
  virtual bool CanShowInFolder() OVERRIDE;
  virtual bool CanOpenDownload() OVERRIDE;
  virtual bool ShouldOpenFileBasedOnExtension() OVERRIDE;
  virtual bool GetOpenWhenComplete() const OVERRIDE;
  virtual bool GetAutoOpened() OVERRIDE;
  virtual bool GetOpened() const OVERRIDE;
  virtual BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual WebContents* GetWebContents() const OVERRIDE;
  virtual void OnContentCheckCompleted(DownloadDangerType danger_type) OVERRIDE;
  virtual void SetOpenWhenComplete(bool open) OVERRIDE;
  virtual void SetIsTemporary(bool temporary) OVERRIDE;
  virtual void SetOpened(bool opened) OVERRIDE;
  virtual void SetDisplayName(const base::FilePath& name) OVERRIDE;
  virtual std::string DebugString(bool verbose) const OVERRIDE;

  // All remaining public interfaces virtual to allow for DownloadItemImpl
  // mocks.

  // Determines the resume mode for an interrupted download. Requires
  // last_reason_ to be set, but doesn't require the download to be in
  // INTERRUPTED state.
  virtual ResumeMode GetResumeMode() const;

  // Notify the download item that new origin information is available due to a
  // resumption request receiving a response.
  virtual void MergeOriginInfoOnResume(
      const DownloadCreateInfo& new_create_info);

  // State transition operations on regular downloads --------------------------

  // Start the download.
  // |download_file| is the associated file on the storage medium.
  // |req_handle| is the new request handle associated with the download.
  virtual void Start(scoped_ptr<DownloadFile> download_file,
                     scoped_ptr<DownloadRequestHandleInterface> req_handle);

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
  virtual void SetTotalBytes(int64 total_bytes);

  virtual void OnAllDataSaved(const std::string& final_hash);

  // Called by SavePackage to display progress when the DownloadItem
  // should be considered complete.
  virtual void MarkAsComplete();

  // DownloadDestinationObserver
  virtual void DestinationUpdate(int64 bytes_so_far,
                                 int64 bytes_per_sec,
                                 const std::string& hash_state) OVERRIDE;
  virtual void DestinationError(DownloadInterruptReason reason) OVERRIDE;
  virtual void DestinationCompleted(const std::string& final_hash) OVERRIDE;

 private:
  // Fine grained states of a download. Note that active downloads are created
  // in IN_PROGRESS_INTERNAL state. However, downloads creates via history can
  // be created in COMPLETE_INTERNAL, CANCELLED_INTERNAL and
  // INTERRUPTED_INTERNAL.

  enum DownloadInternalState {
    // Includes both before and after file name determination, and paused
    // downloads.
    // TODO(rdsmith): Put in state variable for file name determination.
    // Transitions from:
    //   <Initial creation>    Active downloads are created in this state.
    //   RESUMING_INTERNAL
    // Transitions to:
    //   COMPLETING_INTERNAL   On final rename completion.
    //   CANCELLED_INTERNAL    On cancel.
    //   INTERRUPTED_INTERNAL  On interrupt.
    //   COMPLETE_INTERNAL     On SavePackage download completion.
    IN_PROGRESS_INTERNAL,

    // Between commit point (dispatch of download file release) and completed.
    // Embedder may be opening the file in this state.
    // Transitions from:
    //   IN_PROGRESS_INTERNAL
    // Transitions to:
    //   COMPLETE_INTERNAL     On successful completion.
    COMPLETING_INTERNAL,

    // After embedder has had a chance to auto-open.  User may now open
    // or auto-open based on extension.
    // Transitions from:
    //   COMPLETING_INTERNAL
    //   IN_PROGRESS_INTERNAL  SavePackage only.
    //   <Initial creation>    Completed persisted downloads.
    // Transitions to:
    //   <none>                Terminal state.
    COMPLETE_INTERNAL,

    // User has cancelled the download.
    // Transitions from:
    //   IN_PROGRESS_INTERNAL
    //   INTERRUPTED_INTERNAL
    //   RESUMING_INTERNAL
    //   <Initial creation>    Canceleld persisted downloads.
    // Transitions to:
    //   <none>                Terminal state.
    CANCELLED_INTERNAL,

    // An error has interrupted the download.
    // Transitions from:
    //   IN_PROGRESS_INTERNAL
    //   RESUMING_INTERNAL
    //   <Initial creation>    Interrupted persisted downloads.
    // Transitions to:
    //   RESUMING_INTERNAL     On resumption.
    INTERRUPTED_INTERNAL,

    // A request to resume this interrupted download is in progress.
    // Transitions from:
    //   INTERRUPTED_INTERNAL
    // Transitions to:
    //   IN_PROGRESS_INTERNAL  Once a server response is received from a
    //                         resumption.
    //   INTERRUPTED_INTERNAL  If the resumption request fails.
    //   CANCELLED_INTERNAL    On cancel.
    RESUMING_INTERNAL,

    MAX_DOWNLOAD_INTERNAL_STATE,
  };

  // Used with TransitionTo() to indicate whether or not to call
  // UpdateObservers() after the state transition.
  enum ShouldUpdateObservers {
    UPDATE_OBSERVERS,
    DONT_UPDATE_OBSERVERS
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

  // Callback from file thread when we initialize the DownloadFile.
  void OnDownloadFileInitialized(DownloadInterruptReason result);

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

  // Called when the entire download operation (including renaming etc)
  // is completed.
  void Completed();

  // Callback invoked when the URLRequest for a download resumption has started.
  void OnResumeRequestStarted(DownloadItem* item,
                              DownloadInterruptReason interrupt_reason);

  // Helper routines -----------------------------------------------------------

  // Indicate that an error has occurred on the download.
  void Interrupt(DownloadInterruptReason reason);

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
  void TransitionTo(DownloadInternalState new_state,
                    ShouldUpdateObservers notify_action);

  // Set the |danger_type_| and invoke obserers if necessary.
  void SetDangerType(DownloadDangerType danger_type);

  void SetFullPath(const base::FilePath& new_path);

  void AutoResumeIfValid();

  void ResumeInterruptedDownload();

  static DownloadState InternalToExternalState(
      DownloadInternalState internal_state);
  static DownloadInternalState ExternalToInternalState(
      DownloadState external_state);

  // Debugging routines --------------------------------------------------------
  static const char* DebugDownloadStateString(DownloadInternalState state);
  static const char* DebugResumeModeString(ResumeMode mode);

  // Will be false for save package downloads retrieved from the history.
  // TODO(rdsmith): Replace with a generalized enum for "download source".
  const bool is_save_package_download_;

  // The handle to the request information.  Used for operations outside the
  // download system.
  scoped_ptr<DownloadRequestHandleInterface> request_handle_;

  uint32 download_id_;

  // Display name for the download. If this is empty, then the display name is
  // considered to be |target_path_.BaseName()|.
  base::FilePath display_name_;

  // Full path to the downloaded or downloading file. This is the path to the
  // physical file, if one exists. The final target path is specified by
  // |target_path_|. |current_path_| can be empty if the in-progress path hasn't
  // been determined.
  base::FilePath current_path_;

  // Target path of an in-progress download. We may be downloading to a
  // temporary or intermediate file (specified by |current_path_|.  Once the
  // download completes, we will rename the file to |target_path_|.
  base::FilePath target_path_;

  // Whether the target should be overwritten, uniquified or prompted for.
  TargetDisposition target_disposition_;

  // The chain of redirects that leading up to and including the final URL.
  std::vector<GURL> url_chain_;

  // The URL of the page that initiated the download.
  GURL referrer_url_;

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
  PageTransition transition_type_;

  // Whether the download was triggered with a user gesture.
  bool has_user_gesture_;

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
  int64 total_bytes_;

  // Current received bytes.
  int64 received_bytes_;

  // Current speed. Calculated by the DownloadFile.
  int64 bytes_per_sec_;

  // Sha256 hash of the content.  This might be empty either because
  // the download isn't done yet or because the hash isn't needed
  // (ChromeDownloadManagerDelegate::GenerateFileHash() returned false).
  std::string hash_;

  // A blob containing the state of the hash algorithm.  Only valid while the
  // download is in progress.
  std::string hash_state_;

  // Server's time stamp for the file.
  std::string last_modified_time_;

  // Server's ETAG for the file.
  std::string etag_;

  // Last reason.
  DownloadInterruptReason last_reason_;

  // Start time for recording statistics.
  base::TimeTicks start_tick_;

  // The current state of this download.
  DownloadInternalState state_;

  // Current danger type for the download.
  DownloadDangerType danger_type_;

  // The views of this item in the download shelf and download contents.
  ObserverList<Observer> observers_;

  // Time the download was started.
  base::Time start_time_;

  // Time the download completed.
  base::Time end_time_;

  // Our delegate.
  DownloadItemImplDelegate* delegate_;

  // In progress downloads may be paused by the user, we note it here.
  bool is_paused_;

  // The number of times this download has been resumed automatically.
  int auto_resume_count_;

  // A flag for indicating if the download should be opened at completion.
  bool open_when_complete_;

  // A flag for indicating if the downloaded file is externally removed.
  bool file_externally_removed_;

  // True if the download was auto-opened. We set this rather than using
  // an observer as it's frequently possible for the download to be auto opened
  // before the observer is added.
  bool auto_opened_;

  // True if the item was downloaded temporarily.
  bool is_temporary_;

  // True if we've saved all the data for the download.
  bool all_data_saved_;

  // Error return from DestinationError.  Stored separately from
  // last_reason_ so that we can avoid handling destination errors until
  // after file name determination has occurred.
  DownloadInterruptReason destination_error_;

  // Did the user open the item either directly or indirectly (such as by
  // setting always open files of this type)? The shelf also sets this field
  // when the user closes the shelf before the item has been opened but should
  // be treated as though the user opened it.
  bool opened_;

  // Did the delegate delay calling Complete on this download?
  bool delegate_delayed_complete_;

  // DownloadFile associated with this download.  Note that this
  // pointer may only be used or destroyed on the FILE thread.
  // This pointer will be non-null only while the DownloadItem is in
  // the IN_PROGRESS state.
  scoped_ptr<DownloadFile> download_file_;

  // Net log to use for this download.
  const net::BoundNetLog bound_net_log_;

  base::WeakPtrFactory<DownloadItemImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadItemImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_IMPL_H_
