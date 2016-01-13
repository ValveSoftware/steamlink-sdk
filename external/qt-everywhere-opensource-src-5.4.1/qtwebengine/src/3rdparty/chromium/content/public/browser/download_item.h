// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Each download is represented by a DownloadItem, and all DownloadItems
// are owned by the DownloadManager which maintains a global list of all
// downloads. DownloadItems are created when a user initiates a download,
// and exist for the duration of the browser life time.
//
// Download observers:
//   DownloadItem::Observer:
//     - allows observers to receive notifications about one download from start
//       to completion
// Use AddObserver() / RemoveObserver() on the appropriate download object to
// receive state updates.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_ITEM_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_ITEM_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/common/page_transition_types.h"

class GURL;

namespace base {
class FilePath;
class Time;
class TimeDelta;
}

namespace content {

class BrowserContext;
class DownloadManager;
class WebContents;

// One DownloadItem per download. This is the model class that stores all the
// state for a download. Multiple views, such as a tab's download shelf and the
// Destination tab's download view, may refer to a given DownloadItem.
//
// This is intended to be used only on the UI thread.
class CONTENT_EXPORT DownloadItem : public base::SupportsUserData {
 public:
  enum DownloadState {
    // Download is actively progressing.
    IN_PROGRESS = 0,

    // Download is completely finished.
    COMPLETE,

    // Download has been cancelled.
    CANCELLED,

    // This state indicates that the download has been interrupted.
    INTERRUPTED,

    // Maximum value.
    MAX_DOWNLOAD_STATE
  };

  // How the final target path should be used.
  enum TargetDisposition {
    TARGET_DISPOSITION_OVERWRITE, // Overwrite if the target already exists.
    TARGET_DISPOSITION_PROMPT     // Prompt the user for the actual
                                  // target. Implies
                                  // TARGET_DISPOSITION_OVERWRITE.
  };

  // Callback used with AcquireFileAndDeleteDownload().
  typedef base::Callback<void(const base::FilePath&)> AcquireFileCallback;

  static const uint32 kInvalidId;

  static const char kEmptyFileHash[];

  // Interface that observers of a particular download must implement in order
  // to receive updates to the download's status.
  class CONTENT_EXPORT Observer {
   public:
    virtual void OnDownloadUpdated(DownloadItem* download) {}
    virtual void OnDownloadOpened(DownloadItem* download) {}
    virtual void OnDownloadRemoved(DownloadItem* download) {}

    // Called when the download is being destroyed. This happens after
    // every OnDownloadRemoved() as well as when the DownloadManager is going
    // down.
    virtual void OnDownloadDestroyed(DownloadItem* download) {}

   protected:
    virtual ~Observer() {}
  };

  virtual ~DownloadItem() {}

  // Observation ---------------------------------------------------------------

  virtual void AddObserver(DownloadItem::Observer* observer) = 0;
  virtual void RemoveObserver(DownloadItem::Observer* observer) = 0;
  virtual void UpdateObservers() = 0;

  // User Actions --------------------------------------------------------------

  // Called when the user has validated the download of a dangerous file.
  virtual void ValidateDangerousDownload() = 0;

  // Called to acquire a dangerous download and remove the DownloadItem from
  // views and history. |callback| will be invoked on the UI thread with the
  // path to the downloaded file. The caller is responsible for cleanup.
  // Note: It is important for |callback| to be valid since the downloaded file
  // will not be cleaned up if the callback fails.
  virtual void StealDangerousDownload(const AcquireFileCallback& callback) = 0;

  // Pause a download.  Will have no effect if the download is already
  // paused.
  virtual void Pause() = 0;

  // Resume a download that has been paused or interrupted. Will have no effect
  // if the download is neither.
  virtual void Resume() = 0;

  // Cancel the download operation. We need to distinguish between cancels at
  // exit (DownloadManager destructor) from user interface initiated cancels
  // because at exit, the history system may not exist, and any updates to it
  // require AddRef'ing the DownloadManager in the destructor which results in
  // a DCHECK failure. Set |user_cancel| to false when canceling from at
  // exit to prevent this crash. This may result in a difference between the
  // downloaded file's size on disk, and what the history system's last record
  // of it is. At worst, we'll end up re-downloading a small portion of the file
  // when resuming a download (assuming the server supports byte ranges).
  virtual void Cancel(bool user_cancel) = 0;

  // Removes the download from the views and history. If the download was
  // in-progress or interrupted, then the intermediate file will also be
  // deleted.
  virtual void Remove() = 0;

  // Open the file associated with this download.  If the download is
  // still in progress, marks the download to be opened when it is complete.
  virtual void OpenDownload() = 0;

  // Show the download via the OS shell.
  virtual void ShowDownloadInShell() = 0;

  // State accessors -----------------------------------------------------------

  virtual uint32 GetId() const = 0;
  virtual DownloadState GetState() const = 0;

  // Returns the most recent interrupt reason for this download. Returns
  // DOWNLOAD_INTERRUPT_REASON_NONE if there is no previous interrupt reason.
  // Cancelled downloads return DOWNLOAD_INTERRUPT_REASON_USER_CANCELLED. If
  // the download was resumed, then the return value is the interrupt reason
  // prior to resumption.
  virtual DownloadInterruptReason GetLastReason() const = 0;

  virtual bool IsPaused() const = 0;
  virtual bool IsTemporary() const = 0;

  // Returns true if the download can be resumed. A download can be resumed if
  // an in-progress download was paused or if an interrupted download requires
  // user-interaction to resume.
  virtual bool CanResume() const = 0;

  // Returns true if the download is in a terminal state. This includes
  // completed downloads, cancelled downloads, and interrupted downloads that
  // can't be resumed.
  virtual bool IsDone() const = 0;

  //    Origin State accessors -------------------------------------------------

  virtual const GURL& GetURL() const = 0;
  virtual const std::vector<GURL>& GetUrlChain() const = 0;
  virtual const GURL& GetOriginalUrl() const = 0;
  virtual const GURL& GetReferrerUrl() const = 0;
  virtual const GURL& GetTabUrl() const = 0;
  virtual const GURL& GetTabReferrerUrl() const = 0;
  virtual std::string GetSuggestedFilename() const = 0;
  virtual std::string GetContentDisposition() const = 0;
  virtual std::string GetMimeType() const = 0;
  virtual std::string GetOriginalMimeType() const = 0;
  virtual std::string GetRemoteAddress() const = 0;
  virtual bool HasUserGesture() const = 0;
  virtual PageTransition GetTransitionType() const = 0;
  virtual const std::string& GetLastModifiedTime() const = 0;
  virtual const std::string& GetETag() const = 0;
  virtual bool IsSavePackageDownload() const = 0;

  //    Destination State accessors --------------------------------------------

  // Full path to the downloaded or downloading file. This is the path to the
  // physical file, if one exists. It should be considered a hint; changes to
  // this value and renames of the file on disk are not atomic with each other.
  // May be empty if the in-progress path hasn't been determined yet or if the
  // download was interrupted.
  //
  // DO NOT USE THIS METHOD to access the target path of the DownloadItem. Use
  // GetTargetFilePath() instead. While the download is in progress, the
  // intermediate file named by GetFullPath() may be renamed or disappear
  // completely on the FILE thread. The path may also be reset to empty when the
  // download is interrupted.
  virtual const base::FilePath& GetFullPath() const = 0;

  // Target path of an in-progress download. We may be downloading to a
  // temporary or intermediate file (specified by GetFullPath()); this is the
  // name we will use once the download completes.
  // May be empty if the target path hasn't yet been determined.
  virtual const base::FilePath& GetTargetFilePath() const = 0;

  // If the download forced a path rather than requesting name determination,
  // return the path requested.
  virtual const base::FilePath& GetForcedFilePath() const = 0;

  // Returns the file-name that should be reported to the user. If a display
  // name has been explicitly set using SetDisplayName(), this function returns
  // that display name. Otherwise returns the final target filename.
  virtual base::FilePath GetFileNameToReportUser() const = 0;

  virtual TargetDisposition GetTargetDisposition() const = 0;

  // Final hash of completely downloaded file; not valid if
  // GetState() != COMPLETED.
  virtual const std::string& GetHash() const = 0;

  // Intermediate hash state, for persisting partial downloads.
  virtual const std::string& GetHashState() const = 0;

  // True if the file associated with the download has been removed by
  // external action.
  virtual bool GetFileExternallyRemoved() const = 0;

  // If the file is successfully deleted, then GetFileExternallyRemoved() will
  // become true, GetFullPath() will become empty, and
  // DownloadItem::OnDownloadUpdated() will be called. Does nothing if
  // GetState() == COMPLETE or GetFileExternallyRemoved() is already true or
  // GetFullPath() is already empty. The callback is always run, and it is
  // always run asynchronously. It will be passed true if the file is
  // successfully deleted or if GetFilePath() was already empty or if
  // GetFileExternallyRemoved() was already true. The callback will be passed
  // false if the DownloadItem was not yet complete or if the file could not be
  // deleted for any reason.
  virtual void DeleteFile(const base::Callback<void(bool)>& callback) = 0;

  // True if the file that will be written by the download is dangerous
  // and we will require a call to ValidateDangerousDownload() to complete.
  // False if the download is safe or that function has been called.
  virtual bool IsDangerous() const = 0;

  // Why |safety_state_| is not SAFE.
  virtual DownloadDangerType GetDangerType() const = 0;

  //    Progress State accessors -----------------------------------------------

  // Simple calculation of the amount of time remaining to completion. Fills
  // |*remaining| with the amount of time remaining if successful. Fails and
  // returns false if we do not have the number of bytes or the speed so can
  // not estimate.
  virtual bool TimeRemaining(base::TimeDelta* remaining) const = 0;

  // Simple speed estimate in bytes/s
  virtual int64 CurrentSpeed() const = 0;

  // Rough percent complete, -1 means we don't know (== we didn't receive a
  // total size).
  virtual int PercentComplete() const = 0;

  // Returns true if this download has saved all of its data.
  virtual bool AllDataSaved() const = 0;

  virtual int64 GetTotalBytes() const = 0;
  virtual int64 GetReceivedBytes() const = 0;
  virtual base::Time GetStartTime() const = 0;
  virtual base::Time GetEndTime() const = 0;

  //    Open/Show State accessors ----------------------------------------------

  // Returns true if it is OK to open a folder which this file is inside.
  virtual bool CanShowInFolder() = 0;

  // Returns true if it is OK to open the download.
  virtual bool CanOpenDownload() = 0;

  // Tests if a file type should be opened automatically.
  virtual bool ShouldOpenFileBasedOnExtension() = 0;

  // Returns true if the download will be auto-opened when complete.
  virtual bool GetOpenWhenComplete() const = 0;

  // Returns true if the download has been auto-opened by the system.
  virtual bool GetAutoOpened() = 0;

  // Returns true if the download has been opened.
  virtual bool GetOpened() const = 0;

  //    Misc State accessors ---------------------------------------------------

  virtual BrowserContext* GetBrowserContext() const = 0;
  virtual WebContents* GetWebContents() const = 0;

  // External state transitions/setters ----------------------------------------
  // TODO(rdsmith): These should all be removed; the download item should
  // control its own state transitions.

  // Called if a check of the download contents was performed and the results of
  // the test are available. This should only be called after AllDataSaved() is
  // true.
  virtual void OnContentCheckCompleted(DownloadDangerType danger_type) = 0;

  // Mark the download to be auto-opened when completed.
  virtual void SetOpenWhenComplete(bool open) = 0;

  // Mark the download as temporary (not visible in persisted store or
  // SearchDownloads(), removed from main UI upon completion).
  virtual void SetIsTemporary(bool temporary) = 0;

  // Mark the download as having been opened (without actually opening it).
  virtual void SetOpened(bool opened) = 0;

  // Set a display name for the download that will be independent of the target
  // filename. If |name| is not empty, then GetFileNameToReportUser() will
  // return |name|. Has no effect on the final target filename.
  virtual void SetDisplayName(const base::FilePath& name) = 0;

  // Debug/testing -------------------------------------------------------------
  virtual std::string DebugString(bool verbose) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_ITEM_H_
