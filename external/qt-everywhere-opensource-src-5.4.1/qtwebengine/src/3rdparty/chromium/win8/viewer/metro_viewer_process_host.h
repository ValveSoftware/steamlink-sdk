// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_VIEWER_METRO_VIEWER_PROCESS_HOST_H_
#define WIN8_VIEWER_METRO_VIEWER_PROCESS_HOST_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/threading/non_thread_safe.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ipc/message_filter.h"
#include "ui/gfx/native_widget_types.h"
#include "win8/viewer/metro_viewer_exports.h"

namespace base {
class SingleThreadTaskRunner;
class WaitableEvent;
}

namespace IPC {
class ChannelProxy;
class Message;
}

namespace win8 {

// Abstract base class for various Metro viewer process host implementations.
class METRO_VIEWER_EXPORT MetroViewerProcessHost : public IPC::Listener,
                                                   public IPC::Sender,
                                                   public base::NonThreadSafe {
 public:
  typedef base::Callback<void(const base::FilePath&, int, void*)>
      OpenFileCompletion;

  typedef base::Callback<void(const std::vector<base::FilePath>&, void*)>
      OpenMultipleFilesCompletion;

  typedef base::Callback<void(const base::FilePath&, int, void*)>
      SaveFileCompletion;

  typedef base::Callback<void(const base::FilePath&, int, void*)>
      SelectFolderCompletion;

  typedef base::Callback<void(void*)> FileSelectionCanceled;

  // Initializes a viewer process host to connect to the Metro viewer process
  // over IPC. The given task runner correspond to a thread on which
  // IPC::Channel is created and used (e.g. IO thread). Instantly connects to
  // the viewer process if one is already connected to |ipc_channel_name|; a
  // viewer can otherwise be launched synchronously via
  // LaunchViewerAndWaitForConnection().
  explicit MetroViewerProcessHost(
      base::SingleThreadTaskRunner* ipc_task_runner);
  virtual ~MetroViewerProcessHost();

  // Returns the process id of the viewer process if one is connected to this
  // host, returns base::kNullProcessId otherwise.
  base::ProcessId GetViewerProcessId();

  // Launches the viewer process associated with the given |app_user_model_id|
  // and blocks until that viewer process connects or until a timeout is
  // reached. Returns true if the viewer process connects before the timeout is
  // reached. NOTE: this assumes that the app referred to by |app_user_model_id|
  // is registered as the default browser.
  bool LaunchViewerAndWaitForConnection(
      const base::string16& app_user_model_id);

  // Handles the activate desktop command for Metro Chrome Ash. The |ash_exit|
  // parameter indicates whether the Ash process would be shutdown after
  // activating the desktop.
  static void HandleActivateDesktop(const base::FilePath& shortcut,
                                    bool ash_exit);

  // Handles the metro exit command.  Notifies the metro viewer to shutdown
  // gracefully.
  static void HandleMetroExit();

  // Handles the open file operation for Metro Chrome Ash. The on_success
  // callback passed in is invoked when we receive the opened file name from
  // the metro viewer. The on failure callback is invoked on failure.
  static void HandleOpenFile(const base::string16& title,
                             const base::FilePath& default_path,
                             const base::string16& filter,
                             const OpenFileCompletion& on_success,
                             const FileSelectionCanceled& on_failure);

  // Handles the open multiple file operation for Metro Chrome Ash. The
  // on_success callback passed in is invoked when we receive the opened file
  // names from the metro viewer. The on failure callback is invoked on failure.
  static void HandleOpenMultipleFiles(
      const base::string16& title,
      const base::FilePath& default_path,
      const base::string16& filter,
      const OpenMultipleFilesCompletion& on_success,
      const FileSelectionCanceled& on_failure);

  // Handles the save file operation for Metro Chrome Ash. The on_success
  // callback passed in is invoked when we receive the saved file name from
  // the metro viewer. The on failure callback is invoked on failure.
  static void HandleSaveFile(const base::string16& title,
                             const base::FilePath& default_path,
                             const base::string16& filter,
                             int filter_index,
                             const base::string16& default_extension,
                             const SaveFileCompletion& on_success,
                             const FileSelectionCanceled& on_failure);

  // Handles the select folder for Metro Chrome Ash. The on_success
  // callback passed in is invoked when we receive the folder name from the
  // metro viewer. The on failure callback is invoked on failure.
  static void HandleSelectFolder(const base::string16& title,
                                 const SelectFolderCompletion& on_success,
                                 const FileSelectionCanceled& on_failure);

 protected:
  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE = 0;

 private:
  // The following are the implementation for the corresponding static methods
  // above, see them for descriptions.
  void HandleOpenFileImpl(const base::string16& title,
                          const base::FilePath& default_path,
                          const base::string16& filter,
                          const OpenFileCompletion& on_success,
                          const FileSelectionCanceled& on_failure);
  void HandleOpenMultipleFilesImpl(
      const base::string16& title,
      const base::FilePath& default_path,
      const base::string16& filter,
      const OpenMultipleFilesCompletion& on_success,
      const FileSelectionCanceled& on_failure);
  void HandleSaveFileImpl(const base::string16& title,
                          const base::FilePath& default_path,
                          const base::string16& filter,
                          int filter_index,
                          const base::string16& default_extension,
                          const SaveFileCompletion& on_success,
                          const FileSelectionCanceled& on_failure);
  void HandleSelectFolderImpl(const base::string16& title,
                              const SelectFolderCompletion& on_success,
                              const FileSelectionCanceled& on_failure);

  // Called over IPC by the viewer process to tell this host that it should be
  // drawing to |target_surface|.
  virtual void OnSetTargetSurface(gfx::NativeViewId target_surface,
                                  float device_scale) = 0;

  // Called over IPC by the viewer process to request that the url passed in be
  // opened.
  virtual void OnOpenURL(const base::string16& url) = 0;

  // Called over IPC by the viewer process to request that the search string
  // passed in is passed to the default search provider and a URL navigation be
  // performed.
  virtual void OnHandleSearchRequest(const base::string16& search_string) = 0;

  // Called over IPC by the viewer process when the window size has changed.
  virtual void OnWindowSizeChanged(uint32 width, uint32 height) = 0;

  void NotifyChannelConnected();

  // IPC message handing methods:
  void OnFileSaveAsDone(bool success,
                        const base::FilePath& filename,
                        int filter_index);
  void OnFileOpenDone(bool success, const base::FilePath& filename);
  void OnMultiFileOpenDone(bool success,
                           const std::vector<base::FilePath>& files);
  void OnSelectFolderDone(bool success, const base::FilePath& folder);

  // Inner message filter used to handle connection event on the IPC channel
  // proxy's background thread. This prevents consumers of
  // MetroViewerProcessHost from having to pump messages on their own message
  // loop.
  class InternalMessageFilter : public IPC::MessageFilter {
   public:
    InternalMessageFilter(MetroViewerProcessHost* owner);

    // IPC::MessageFilter implementation.
    virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

   private:
    MetroViewerProcessHost* owner_;
    DISALLOW_COPY_AND_ASSIGN(InternalMessageFilter);
  };

  scoped_ptr<IPC::ChannelProxy> channel_;
  scoped_ptr<base::WaitableEvent> channel_connected_event_;
  scoped_refptr<InternalMessageFilter> message_filter_;

  static MetroViewerProcessHost* instance_;

  // Saved callbacks which inform the caller about the result of the open file/
  // save file/select operations.
  OpenFileCompletion file_open_completion_callback_;
  OpenMultipleFilesCompletion multi_file_open_completion_callback_;
  SaveFileCompletion file_saveas_completion_callback_;
  SelectFolderCompletion select_folder_completion_callback_;
  FileSelectionCanceled failure_callback_;

  DISALLOW_COPY_AND_ASSIGN(MetroViewerProcessHost);
};

}  // namespace win8

#endif  // WIN8_VIEWER_METRO_VIEWER_PROCESS_HOST_H_
