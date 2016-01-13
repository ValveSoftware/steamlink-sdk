// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_WEBRTC_INTERNALS_H_
#define CONTENT_BROWSER_MEDIA_WEBRTC_INTERNALS_H_

#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class WebContents;
class WebRTCInternalsUIObserver;

// This is a singleton class running in the browser UI thread.
// It collects peer connection infomation from the renderers,
// forwards the data to WebRTCInternalsUIObserver and
// sends data collecting commands to the renderers.
class CONTENT_EXPORT WebRTCInternals : public NotificationObserver,
                                       public ui::SelectFileDialog::Listener {
 public:
  static WebRTCInternals* GetInstance();

  // This method is called when a PeerConnection is created.
  // |render_process_id| is the id of the render process (not OS pid), which is
  // needed because we might not be able to get the OS process id when the
  // render process terminates and we want to clean up.
  // |pid| is the renderer process id, |lid| is the renderer local id used to
  // identify a PeerConnection, |url| is the url of the tab owning the
  // PeerConnection, |servers| is the servers configuration, |constraints| is
  // the media constraints used to initialize the PeerConnection.
  void OnAddPeerConnection(int render_process_id,
                           base::ProcessId pid,
                           int lid,
                           const std::string& url,
                           const std::string& servers,
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
  void AddObserver(WebRTCInternalsUIObserver *observer);
  void RemoveObserver(WebRTCInternalsUIObserver *observer);

  // Sends all update data to |observer|.
  void UpdateObserver(WebRTCInternalsUIObserver* observer);

  // Enables or disables AEC dump (diagnostic echo canceller recording).
  void EnableAecDump(content::WebContents* web_contents);
  void DisableAecDump();

  bool aec_dump_enabled() {
    return aec_dump_enabled_;
  }

  base::FilePath aec_dump_file_path() {
    return aec_dump_file_path_;
  }

  void ResetForTesting();

 private:
  friend struct DefaultSingletonTraits<WebRTCInternals>;
  FRIEND_TEST_ALL_PREFIXES(WebRtcBrowserTest, CallWithAecDump);
  FRIEND_TEST_ALL_PREFIXES(WebRtcBrowserTest,
                           CallWithAecDumpEnabledThenDisabled);
  FRIEND_TEST_ALL_PREFIXES(WebRTCInternalsTest,
                           AecRecordingFileSelectionCanceled);

  WebRTCInternals();
  virtual ~WebRTCInternals();

  void SendUpdate(const std::string& command, base::Value* value);

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // ui::SelectFileDialog::Listener implementation.
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* unused_params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

  // Called when a renderer exits (including crashes).
  void OnRendererExit(int render_process_id);

#if defined(ENABLE_WEBRTC)
  // Enables AEC dump on all render process hosts using |aec_dump_file_path_|.
  void EnableAecDumpOnAllRenderProcessHosts();
#endif

  ObserverList<WebRTCInternalsUIObserver> observers_;

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
  // list item is a DictionaryValue containing "type" and "value", both of which
  // are strings.
  base::ListValue peer_connection_data_;

  // A list of getUserMedia requests. Each item is a DictionaryValue that
  // contains these fields:
  // "rid" -- the renderer id.
  // "pid" -- proceddId of the renderer.
  // "origin" -- the security origin of the request.
  // "audio" -- the serialized audio constraints if audio is requested.
  // "video" -- the serialized video constraints if video is requested.
  base::ListValue get_user_media_requests_;

  NotificationRegistrar registrar_;

  // For managing select file dialog.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // AEC dump (diagnostic echo canceller recording) state.
  bool aec_dump_enabled_;
  base::FilePath aec_dump_file_path_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_WEBRTC_INTERNALS_H_
