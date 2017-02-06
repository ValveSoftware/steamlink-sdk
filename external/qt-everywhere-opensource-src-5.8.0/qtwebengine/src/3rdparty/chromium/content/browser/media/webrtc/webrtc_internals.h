// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_WEBRTC_WEBRTC_INTERNALS_H_
#define CONTENT_BROWSER_MEDIA_WEBRTC_WEBRTC_INTERNALS_H_

#include <memory>
#include <queue>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace device {
class PowerSaveBlocker;
}  // namespace device

namespace content {

class WebContents;
class WebRTCInternalsUIObserver;

// This is a singleton class running in the browser UI thread.
// It collects peer connection infomation from the renderers,
// forwards the data to WebRTCInternalsUIObserver and
// sends data collecting commands to the renderers.
class CONTENT_EXPORT WebRTCInternals : public RenderProcessHostObserver,
                                       public ui::SelectFileDialog::Listener {
 public:
  static WebRTCInternals* GetInstance();

  // This method is called when a PeerConnection is created.
  // |render_process_id| is the id of the render process (not OS pid), which is
  // needed because we might not be able to get the OS process id when the
  // render process terminates and we want to clean up.
  // |pid| is the renderer process id, |lid| is the renderer local id used to
  // identify a PeerConnection, |url| is the url of the tab owning the
  // PeerConnection, |rtc_configuration| is the serialized RTCConfiguration,
  // |constraints| is the media constraints used to initialize the
  // PeerConnection.
  void OnAddPeerConnection(int render_process_id,
                           base::ProcessId pid,
                           int lid,
                           const std::string& url,
                           const std::string& rtc_configuration,
                           const std::string& constraints);

  // This method is called when PeerConnection is destroyed.
  // |pid| is the renderer process id, |lid| is the renderer local id.
  void OnRemovePeerConnection(base::ProcessId pid, int lid);

  // This method is called when a PeerConnection is updated.
  // |pid| is the renderer process id, |lid| is the renderer local id,
  // |type| is the update type, |value| is the detail of the update.
  void OnUpdatePeerConnection(base::ProcessId pid,
                              int lid,
                              const std::string& type,
                              const std::string& value);

  // This method is called when results from PeerConnectionInterface::GetStats
  // are available. |pid| is the renderer process id, |lid| is the renderer
  // local id, |value| is the list of stats reports.
  void OnAddStats(base::ProcessId pid, int lid, const base::ListValue& value);

  // This method is called when getUserMedia is called. |render_process_id| is
  // the id of the render process (not OS pid), which is needed because we might
  // not be able to get the OS process id when the render process terminates and
  // we want to clean up. |pid| is the renderer OS process id, |origin| is the
  // security origin of the getUserMedia call, |audio| is true if audio stream
  // is requested, |video| is true if the video stream is requested,
  // |audio_constraints| is the constraints for the audio, |video_constraints|
  // is the constraints for the video.
  void OnGetUserMedia(int render_process_id,
                      base::ProcessId pid,
                      const std::string& origin,
                      bool audio,
                      bool video,
                      const std::string& audio_constraints,
                      const std::string& video_constraints);

  // Methods for adding or removing WebRTCInternalsUIObserver.
  void AddObserver(WebRTCInternalsUIObserver* observer);
  void RemoveObserver(WebRTCInternalsUIObserver* observer);

  // Sends all update data to |observer|.
  void UpdateObserver(WebRTCInternalsUIObserver* observer);

  // Enables or disables diagnostic audio recordings for debugging purposes.
  void EnableAudioDebugRecordings(content::WebContents* web_contents);
  void DisableAudioDebugRecordings();

  bool IsAudioDebugRecordingsEnabled() const;
  const base::FilePath& GetAudioDebugRecordingsFilePath() const;

  // Enables or disables diagnostic event log.
  void SetEventLogRecordings(bool enable, content::WebContents* web_contents);

  bool IsEventLogRecordingsEnabled() const;
  const base::FilePath& GetEventLogRecordingsFilePath() const;

 protected:
  // Constructor/Destructor are protected to allow tests to derive from the
  // class and do per-instance testing without having to use the global
  // instance.
  // The default ctor sets |aggregate_updates_ms| to 500ms.
  WebRTCInternals();
  explicit WebRTCInternals(int aggregate_updates_ms);
  ~WebRTCInternals() override;

 private:
  friend struct base::DefaultLazyInstanceTraits<WebRTCInternals>;
  FRIEND_TEST_ALL_PREFIXES(WebRtcAudioDebugRecordingsBrowserTest,
                           CallWithAudioDebugRecordings);
  FRIEND_TEST_ALL_PREFIXES(WebRtcAudioDebugRecordingsBrowserTest,
                           CallWithAudioDebugRecordingsEnabledThenDisabled);
  FRIEND_TEST_ALL_PREFIXES(WebRtcAudioDebugRecordingsBrowserTest,
                           TwoCallsWithAudioDebugRecordings);
  FRIEND_TEST_ALL_PREFIXES(WebRtcInternalsTest,
                           AudioDebugRecordingsFileSelectionCanceled);

  void SendUpdate(const std::string& command,
                  std::unique_ptr<base::Value> value);

  // RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* unused_params) override;
  void FileSelectionCanceled(void* params) override;

  // Called when a renderer exits (including crashes).
  void OnRendererExit(int render_process_id);

#if defined(ENABLE_WEBRTC)
  // Enables diagnostic audio recordings on all render process hosts using
  // |audio_debug_recordings_file_path_|.
  void EnableAudioDebugRecordingsOnAllRenderProcessHosts();

  // Enables event log recordings on all render process hosts using
  // |event_log_recordings_file_path_|.
  void EnableEventLogRecordingsOnAllRenderProcessHosts();
#endif

  // Called whenever an element is added to or removed from
  // |peer_connection_data_| to impose/release a block on suspending the current
  // application for power-saving.
  void CreateOrReleasePowerSaveBlocker();

  // Called on a timer to deliver updates to javascript.
  // We throttle and bulk together updates to avoid DOS like scenarios where
  // a page uses a lot of peerconnection instances with many event
  // notifications.
  void ProcessPendingUpdates();

  base::ObserverList<WebRTCInternalsUIObserver> observers_;

  // |peer_connection_data_| is a list containing all the PeerConnection
  // updates.
  // Each item of the list represents the data for one PeerConnection, which
  // contains these fields:
  // "rid" -- the renderer id.
  // "pid" -- OS process id of the renderer that creates the PeerConnection.
  // "lid" -- local Id assigned to the PeerConnection.
  // "url" -- url of the web page that created the PeerConnection.
  // "servers" and "constraints" -- server configuration and media constraints
  // used to initialize the PeerConnection respectively.
  // "log" -- a ListValue contains all the updates for the PeerConnection. Each
  // list item is a DictionaryValue containing "time", which is the number of
  // milliseconds since epoch as a string, and "type" and "value", both of which
  // are strings representing the event.
  base::ListValue peer_connection_data_;

  // A list of getUserMedia requests. Each item is a DictionaryValue that
  // contains these fields:
  // "rid" -- the renderer id.
  // "pid" -- proceddId of the renderer.
  // "origin" -- the security origin of the request.
  // "audio" -- the serialized audio constraints if audio is requested.
  // "video" -- the serialized video constraints if video is requested.
  base::ListValue get_user_media_requests_;

  // For managing select file dialog.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // Diagnostic audio recording state.
  bool audio_debug_recordings_;
  base::FilePath audio_debug_recordings_file_path_;

  // Diagnostic event log recording state.
  bool event_log_recordings_;
  bool selecting_event_log_;
  base::FilePath event_log_recordings_file_path_;

  // While |peer_connection_data_| is non-empty, hold an instance of
  // PowerSaveBlocker.  This prevents the application from being suspended while
  // remoting.
  std::unique_ptr<device::PowerSaveBlocker> power_save_blocker_;

  // Set of render process hosts that |this| is registered as an observer on.
  base::hash_set<int> render_process_id_set_;

  // Used to bulk up updates that we send to javascript.
  // The class owns the value/dictionary and command name of an update.
  // For each update, a PendingUpdate is stored in the |pending_updates_| queue
  // and deleted as soon as the update has been delivered.
  // The class is moveble and not copyable to avoid copying while still allowing
  // us to use an stl container without needing scoped_ptr or similar.
  // The class is single threaded, so all operations must occur on the same
  // thread.
  class PendingUpdate {
   public:
    PendingUpdate(const std::string& command,
                  std::unique_ptr<base::Value> value);
    PendingUpdate(PendingUpdate&& other);
    ~PendingUpdate();

    const std::string& command() const;
    const base::Value* value() const;

   private:
    base::ThreadChecker thread_checker_;
    std::string command_;
    std::unique_ptr<base::Value> value_;
    DISALLOW_COPY_AND_ASSIGN(PendingUpdate);
  };

  std::queue<PendingUpdate> pending_updates_;
  const int aggregate_updates_ms_;

  // Weak factory for this object that we use for bulking up updates.
  base::WeakPtrFactory<WebRTCInternals> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_WEBRTC_WEBRTC_INTERNALS_H_
