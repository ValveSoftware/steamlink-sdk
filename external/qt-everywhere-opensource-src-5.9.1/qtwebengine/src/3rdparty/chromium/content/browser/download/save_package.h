// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/download/save_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

class GURL;

namespace content {
class DownloadItemImpl;
class DownloadManagerImpl;
class FrameTreeNode;
class RenderFrameHostImpl;
struct SavableSubframe;
class SaveFileManager;
class SaveItem;
class SavePackage;
class WebContents;

// The SavePackage object manages the process of saving a page as only-html or
// complete-html or MHTML and providing the information for displaying saving
// status.  Saving page as only-html means means that we save web page to a
// single HTML file regardless internal sub resources and sub frames.  Saving
// page as complete-html page means we save not only the main html file the user
// told it to save but also a directory for the auxiliary files such as all
// sub-frame html files, image files, css files and js files.  Saving page as
// MHTML means the same thing as complete-html, but it uses the MHTML format to
// contain the html and all auxiliary files in a single text file.
//
// Each page saving job may include one or multiple files which need to be
// saved. Each file is represented by a SaveItem, and all SaveItems are owned
// by the SavePackage. SaveItems are created when a user initiates a page
// saving job, and exist for the duration of one contents's life time.
class CONTENT_EXPORT SavePackage
    : public base::RefCountedThreadSafe<SavePackage>,
      public WebContentsObserver,
      public DownloadItem::Observer,
      public base::SupportsWeakPtr<SavePackage> {
 public:
  enum WaitState {
    // State when created but not initialized.
    INITIALIZE = 0,
    // State when after initializing, but not yet saving.
    START_PROCESS,
    // Waiting on a list of savable resources from the backend.
    RESOURCES_LIST,
    // Waiting for data sent from net IO or from file system.
    NET_FILES,
    // Waiting for html DOM data sent from render process.
    HTML_DATA,
    // Saving page finished successfully.
    SUCCESSFUL,
    // Failed to save page.
    FAILED
  };

  static const base::FilePath::CharType kDefaultHtmlExtension[];

  // Constructor for user initiated page saving. This constructor results in a
  // SavePackage that will generate and sanitize a suggested name for the user
  // in the "Save As" dialog box.
  explicit SavePackage(WebContents* web_contents);

  // Initialize the SavePackage. Returns true if it initializes properly.  Need
  // to make sure that this method must be called in the UI thread because using
  // g_browser_process on a non-UI thread can cause crashes during shutdown.
  // |cb| will be called when the DownloadItem is created, before data is
  // written to disk.
  bool Init(const SavePackageDownloadCreatedCallback& cb);

  // Cancel all in progress request, might be called by user or internal error.
  void Cancel(bool user_action);

  void Finish();

  // Notifications sent from the FILE thread to the UI thread.
  void StartSave(const SaveFileCreateInfo* info);
  bool UpdateSaveProgress(SaveItemId save_item_id,
                          int64_t size,
                          bool write_success);
  // Called for updating end state.
  void SaveFinished(SaveItemId save_item_id, int64_t size, bool is_success);
  void SaveCanceled(const SaveItem* save_item);

  // Calculate the percentage of whole save page job.
  // Rough percent complete, -1 means we don't know (since we didn't receive a
  // total size).
  int PercentComplete();

  bool canceled() const { return user_canceled_ || disk_error_occurred_; }
  bool finished() const { return finished_; }
  SavePageType save_type() const { return save_type_; }

  SavePackageId id() const { return unique_id_; }

  void GetSaveInfo();

 private:
  friend class base::RefCountedThreadSafe<SavePackage>;

  // Friends for testing. Needed for accessing the test-only constructor below.
  friend class SavePackageTest;
  friend class WebContentsImpl;
  FRIEND_TEST_ALL_PREFIXES(SavePackageTest, TestSuggestedSaveNames);
  FRIEND_TEST_ALL_PREFIXES(SavePackageTest, TestLongSafePureFilename);
  FRIEND_TEST_ALL_PREFIXES(SavePackageBrowserTest, ImplicitCancel);
  FRIEND_TEST_ALL_PREFIXES(SavePackageBrowserTest, ExplicitCancel);

  // Map from SaveItem::id() (aka save_item_id) into a SaveItem.
  using SaveItemIdMap = std::
      unordered_map<SaveItemId, std::unique_ptr<SaveItem>, SaveItemId::Hasher>;

  using FileNameSet = std::set<base::FilePath::StringType,
                               bool (*)(base::FilePath::StringPieceType,
                                        base::FilePath::StringPieceType)>;

  using FileNameCountMap =
      std::unordered_map<base::FilePath::StringType, uint32_t>;

  // Used only for testing. Bypasses the file and directory name generation /
  // sanitization by providing well known paths better suited for tests.
  SavePackage(WebContents* web_contents,
              SavePageType save_type,
              const base::FilePath& file_full_path,
              const base::FilePath& directory_full_path);

  void InitWithDownloadItem(
      const SavePackageDownloadCreatedCallback& download_created_callback,
      DownloadItemImpl* item);

  // Callback for WebContents::GenerateMHTML().
  void OnMHTMLGenerated(int64_t size);

  ~SavePackage() override;

  // Notes from Init() above applies here as well.
  void InternalInit();

  void Stop();
  void CheckFinish();

  // Initiate a saving job of a specific URL. We send the request to
  // SaveFileManager, which will dispatch it to different approach according to
  // the save source. |process_all_remaining_items| indicates whether we need to
  // save all remaining items.
  void SaveNextFile(bool process_all_remainder_items);

  // Continue processing the save page job after one SaveItem has been finished.
  void DoSavingProcess();

  // WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;

  // DownloadItem::Observer implementation.
  void OnDownloadDestroyed(DownloadItem* download) override;

  // Update the download history of this item upon completion.
  void FinalizeDownloadEntry();

  // Detach from DownloadManager.
  void RemoveObservers();

  // Return max length of a path for a specific base directory.
  // This is needed on POSIX, which restrict the length of file names in
  // addition to the restriction on the length of path names.
  // |base_dir| is assumed to be a directory name with no trailing slash.
  static uint32_t GetMaxPathLengthForDirectory(const base::FilePath& base_dir);

  // Truncates a filename to fit length constraints.
  //
  // |directory|    : Directory containing target file.
  // |extension|    : Extension.
  // |max_path_len| : Maximum size allowed for |len(directory + base_name +
  //                  extension|.
  // |base_name|    : Variable portion. The length of this component will be
  //                  adjusted to fit the length constraints described at
  //                  |max_path_len| above.
  //
  // Returns true if |base_name| could be successfully adjusted to fit the
  // aforementioned constraints, or false otherwise.
  // TODO(asanka): This function is wrong. |base_name| cannot be truncated
  //   without knowing its encoding and truncation has to be performed on
  //   character boundaries. Also the implementation doesn't look up the actual
  //   path constraints and instead uses hard coded constants. crbug.com/618737
  static bool TruncateBaseNameToFitPathConstraints(
      const base::FilePath& directory,
      const base::FilePath::StringType& extension,
      uint32_t max_path_len,
      base::FilePath::StringType* base_name);

  // Create a file name based on the response from the server.
  bool GenerateFileName(const std::string& disposition,
                        const GURL& url,
                        bool need_html_ext,
                        base::FilePath::StringType* generated_name);

  // Main routine that initiates asking all frames for their savable resources.
  //
  // Responses are received asynchronously by OnSavableResourceLinks... methods
  // and pending responses are counted/tracked by
  // CompleteSavableResourceLinksResponse.
  //
  // OnSavableResourceLinksResponse creates SaveItems for each savable resource
  // and each subframe - these SaveItems get enqueued into |waiting_item_queue_|
  // with the help of CreatePendingSaveItem, EnqueueSavableResource,
  // EnqueueFrame.
  void GetSavableResourceLinks();

  // Response from |sender| frame to GetSavableResourceLinks request.
  void OnSavableResourceLinksResponse(
      RenderFrameHostImpl* sender,
      const std::vector<GURL>& resources_list,
      const Referrer& referrer,
      const std::vector<SavableSubframe>& subframes);

  // Helper for finding or creating a SaveItem with the given parameters.
  SaveItem* CreatePendingSaveItem(
      int container_frame_tree_node_id,
      int save_item_frame_tree_node_id,
      const GURL& url,
      const Referrer& referrer,
      SaveFileCreateInfo::SaveFileSource save_source);

  // Helper for finding a SaveItem with the given url, or falling back to
  // creating a SaveItem with the given parameters.
  SaveItem* CreatePendingSaveItemDeduplicatingByUrl(
      int container_frame_tree_node_id,
      int save_item_frame_tree_node_id,
      const GURL& url,
      const Referrer& referrer,
      SaveFileCreateInfo::SaveFileSource save_source);

  // Helper to enqueue a savable resource reported by GetSavableResourceLinks.
  void EnqueueSavableResource(int container_frame_tree_node_id,
                              const GURL& url,
                              const Referrer& referrer);
  // Helper to enqueue a subframe reported by GetSavableResourceLinks.
  void EnqueueFrame(int container_frame_tree_node_id,
                    int frame_tree_node_id,
                    const GURL& frame_original_url);

  // Response to GetSavableResourceLinks that indicates an error when processing
  // the frame associated with |sender|.
  void OnSavableResourceLinksError(RenderFrameHostImpl* sender);

  // Helper tracking how many |number_of_frames_pending_response_| we have
  // left and kicking off the next phase after we got all the
  // OnSavableResourceLinksResponse messages we were waiting for.
  void CompleteSavableResourceLinksResponse();

  // For each frame in the current page, ask the renderer process associated
  // with that frame to serialize that frame into html.
  void GetSerializedHtmlWithLocalLinks();

  // Ask renderer process to serialize |target_tree_node| into html data
  // with resource links replaced with a link to a locally saved copy.
  void GetSerializedHtmlWithLocalLinksForFrame(FrameTreeNode* target_tree_node);

  // Routes html data (sent by renderer process in response to
  // GetSerializedHtmlWithLocalLinksForFrame above) to the associated local file
  // (and also keeps track of when all frames have been completed).
  void OnSerializedHtmlWithLocalLinksResponse(RenderFrameHostImpl* sender,
                                              const std::string& data,
                                              bool end_of_data);

  // Look up SaveItem by save item id from in progress map.
  SaveItem* LookupInProgressSaveItem(SaveItemId save_item_id);

  // Remove SaveItem from in progress map and put it to saved map.
  void PutInProgressItemToSavedMap(SaveItem* save_item);

  // Retrieves the URL to be saved from the WebContents.
  static GURL GetUrlToBeSaved(WebContents* web_contents);

  static base::FilePath CreateDirectoryOnFileThread(
      const base::string16& title,
      const GURL& page_url,
      bool can_save_as_complete,
      const std::string& mime_type,
      const base::FilePath& website_save_dir,
      const base::FilePath& download_save_dir,
      bool skip_dir_check);
  void ContinueGetSaveInfo(bool can_save_as_complete,
                           const base::FilePath& suggested_path);
  void OnPathPicked(
      const base::FilePath& final_name,
      SavePageType type,
      const SavePackageDownloadCreatedCallback& cb);

  // The number of in process SaveItems.
  int in_process_count() const {
    return static_cast<int>(in_progress_items_.size());
  }

  // The number of all SaveItems which have completed, including success items
  // and failed items.
  int completed_count() const {
    return static_cast<int>(saved_success_items_.size() +
                            saved_failed_items_.size());
  }

  // The current speed in files per second. This is used to update the
  // DownloadItem associated to this SavePackage. The files per second is
  // presented by the DownloadItem to the UI as bytes per second, which is
  // not correct but matches the way the total and received number of files is
  // presented as the total and received bytes.
  int64_t CurrentSpeed() const;

  // Helper function for preparing suggested name for the SaveAs Dialog. The
  // suggested name is determined by the web document's title.
  static base::FilePath GetSuggestedNameForSaveAs(
      const base::string16& title,
      const GURL& page_url,
      bool can_save_as_complete,
      const std::string& contents_mime_type);

  // Ensures that the file name has a proper extension for HTML by adding ".htm"
  // if necessary.
  static base::FilePath EnsureHtmlExtension(const base::FilePath& name);

  // Ensures that the file name has a proper extension for supported formats
  // if necessary.
  static base::FilePath EnsureMimeExtension(const base::FilePath& name,
      const std::string& contents_mime_type);

  // Returns extension for supported MIME types (for example, for "text/plain"
  // it returns "txt").
  static const base::FilePath::CharType* ExtensionForMimeType(
      const std::string& contents_mime_type);

  // A queue for items we are about to start saving.
  std::deque<std::unique_ptr<SaveItem>> waiting_item_queue_;

  // Map of all saving job in in-progress state.
  SaveItemIdMap in_progress_items_;

  // Map of all saving job which are failed.
  SaveItemIdMap saved_failed_items_;

  // Used to de-dupe urls that are being gathered into |waiting_item_queue_|
  // and also to find SaveItems to associate with a containing frame.
  // Note that |url_to_save_item_| does NOT own SaveItems - they
  // remain owned by waiting_item_queue_, in_progress_items_, etc.
  std::map<GURL, SaveItem*> url_to_save_item_;

  // Map used to route responses from a given a subframe (i.e.
  // OnSerializedHtmlWithLocalLinksResponse) to the right SaveItem.
  // Note that |frame_tree_node_id_to_save_item_| does NOT own SaveItems - they
  // remain owned by waiting_item_queue_, in_progress_items_, etc.
  std::unordered_map<int, SaveItem*> frame_tree_node_id_to_save_item_;

  // Used to limit which local paths get exposed to which frames
  // (i.e. to prevent information disclosure to oop frames).
  // Note that |frame_tree_node_id_to_contained_save_items_| does NOT own
  // SaveItems - they remain owned by waiting_item_queue_, in_progress_items_,
  // etc.
  std::unordered_map<int, std::vector<SaveItem*>>
      frame_tree_node_id_to_contained_save_items_;

  // Number of frames that we still need to get a response from.
  int number_of_frames_pending_response_ = 0;

  // Map of all saving job which are successfully saved.
  SaveItemIdMap saved_success_items_;

  // Non-owning pointer for handling file writing on the FILE thread.
  SaveFileManager* file_manager_ = nullptr;

  // DownloadManager owns the DownloadItem and handles history and UI.
  DownloadManagerImpl* download_manager_ = nullptr;
  DownloadItemImpl* download_ = nullptr;

  // The URL of the page the user wants to save.
  GURL page_url_;
  base::FilePath saved_main_file_path_;
  base::FilePath saved_main_directory_path_;

  // The title of the page the user wants to save.
  base::string16 title_;

  // Used to calculate package download speed (in files per second).
  const base::TimeTicks start_tick_;

  // Indicates whether the actual saving job is finishing or not.
  bool finished_ = false;

  // Indicates whether user canceled the saving job.
  bool user_canceled_ = false;

  // Indicates whether user get disk error.
  bool disk_error_occurred_ = false;

  // Variables to record errors that happened so we can record them via
  // UMA statistics.
  bool wrote_to_completed_file_ = false;
  bool wrote_to_failed_file_ = false;

  // Type about saving page as only-html or complete-html.
  SavePageType save_type_ = SAVE_PAGE_TYPE_UNKNOWN;

  // Number of all need to be saved resources.
  size_t all_save_items_count_ = 0;

  // This set is used to eliminate duplicated file names in saving directory.
  FileNameSet file_name_set_;

  // This map is used to track serial number for specified filename.
  FileNameCountMap file_name_count_map_;

  // Indicates current waiting state when SavePackage try to get something
  // from outside.
  WaitState wait_state_ = INITIALIZE;

  // Unique ID for this SavePackage.
  const SavePackageId unique_id_;

  DISALLOW_COPY_AND_ASSIGN(SavePackage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_H_
