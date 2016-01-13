// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webrtc_internals.h"

#include "base/path_service.h"
#include "content/browser/media/webrtc_internals_ui_observer.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using base::ProcessId;
using std::string;

namespace content {

namespace {
// Makes sure that |dict| has a ListValue under path "log".
static base::ListValue* EnsureLogList(base::DictionaryValue* dict) {
  base::ListValue* log = NULL;
  if (!dict->GetList("log", &log)) {
    log = new base::ListValue();
    if (log)
      dict->Set("log", log);
  }
  return log;
}

}  // namespace

WebRTCInternals::WebRTCInternals()
    : aec_dump_enabled_(false) {
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllBrowserContextsAndSources());
// TODO(grunell): Shouldn't all the webrtc_internals* files be excluded from the
// build if WebRTC is disabled?
#if defined(ENABLE_WEBRTC)
  aec_dump_file_path_ =
      GetContentClient()->browser()->GetDefaultDownloadDirectory();
  if (aec_dump_file_path_.empty()) {
    // In this case the default path (|aec_dump_file_path_|) will be empty and
    // the platform default path will be used in the file dialog (with no
    // default file name). See SelectFileDialog::SelectFile. On Android where
    // there's no dialog we'll fail to open the file.
    VLOG(1) << "Could not get the download directory.";
  } else {
    aec_dump_file_path_ =
        aec_dump_file_path_.Append(FILE_PATH_LITERAL("audio.aecdump"));
  }
#endif  // defined(ENABLE_WEBRTC)
}

WebRTCInternals::~WebRTCInternals() {
}

WebRTCInternals* WebRTCInternals::GetInstance() {
  return Singleton<WebRTCInternals>::get();
}

void WebRTCInternals::OnAddPeerConnection(int render_process_id,
                                          ProcessId pid,
                                          int lid,
                                          const string& url,
                                          const string& servers,
                                          const string& constraints) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::DictionaryValue* dict = new base::DictionaryValue();
  if (!dict)
    return;

  dict->SetInteger("rid", render_process_id);
  dict->SetInteger("pid", static_cast<int>(pid));
  dict->SetInteger("lid", lid);
  dict->SetString("servers", servers);
  dict->SetString("constraints", constraints);
  dict->SetString("url", url);
  peer_connection_data_.Append(dict);

  if (observers_.might_have_observers())
    SendUpdate("addPeerConnection", dict);
}

void WebRTCInternals::OnRemovePeerConnection(ProcessId pid, int lid) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (size_t i = 0; i < peer_connection_data_.GetSize(); ++i) {
    base::DictionaryValue* dict = NULL;
    peer_connection_data_.GetDictionary(i, &dict);

    int this_pid = 0;
    int this_lid = 0;
    dict->GetInteger("pid", &this_pid);
    dict->GetInteger("lid", &this_lid);

    if (this_pid != static_cast<int>(pid) || this_lid != lid)
      continue;

    peer_connection_data_.Remove(i, NULL);

    if (observers_.might_have_observers()) {
      base::DictionaryValue id;
      id.SetInteger("pid", static_cast<int>(pid));
      id.SetInteger("lid", lid);
      SendUpdate("removePeerConnection", &id);
    }
    break;
  }
}

void WebRTCInternals::OnUpdatePeerConnection(
    ProcessId pid, int lid, const string& type, const string& value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (size_t i = 0; i < peer_connection_data_.GetSize(); ++i) {
    base::DictionaryValue* record = NULL;
    peer_connection_data_.GetDictionary(i, &record);

    int this_pid = 0, this_lid = 0;
    record->GetInteger("pid", &this_pid);
    record->GetInteger("lid", &this_lid);

    if (this_pid != static_cast<int>(pid) || this_lid != lid)
      continue;

    // Append the update to the end of the log.
    base::ListValue* log = EnsureLogList(record);
    if (!log)
      return;

    base::DictionaryValue* log_entry = new base::DictionaryValue();
    if (!log_entry)
      return;

    log_entry->SetString("type", type);
    log_entry->SetString("value", value);
    log->Append(log_entry);

    if (observers_.might_have_observers()) {
      base::DictionaryValue update;
      update.SetInteger("pid", static_cast<int>(pid));
      update.SetInteger("lid", lid);
      update.SetString("type", type);
      update.SetString("value", value);

      SendUpdate("updatePeerConnection", &update);
    }
    return;
  }
}

void WebRTCInternals::OnAddStats(base::ProcessId pid, int lid,
                                 const base::ListValue& value) {
  if (!observers_.might_have_observers())
    return;

  base::DictionaryValue dict;
  dict.SetInteger("pid", static_cast<int>(pid));
  dict.SetInteger("lid", lid);

  base::ListValue* list = value.DeepCopy();
  if (!list)
    return;

  dict.Set("reports", list);

  SendUpdate("addStats", &dict);
}

void WebRTCInternals::OnGetUserMedia(int rid,
                                     base::ProcessId pid,
                                     const std::string& origin,
                                     bool audio,
                                     bool video,
                                     const std::string& audio_constraints,
                                     const std::string& video_constraints) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("rid", rid);
  dict->SetInteger("pid", static_cast<int>(pid));
  dict->SetString("origin", origin);
  if (audio)
    dict->SetString("audio", audio_constraints);
  if (video)
    dict->SetString("video", video_constraints);

  get_user_media_requests_.Append(dict);

  if (observers_.might_have_observers())
    SendUpdate("addGetUserMedia", dict);
}

void WebRTCInternals::AddObserver(WebRTCInternalsUIObserver *observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.AddObserver(observer);
}

void WebRTCInternals::RemoveObserver(WebRTCInternalsUIObserver *observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.RemoveObserver(observer);

  // Disables the AEC recording if it is enabled and the last webrtc-internals
  // page is going away.
  if (aec_dump_enabled_ && !observers_.might_have_observers())
    DisableAecDump();
}

void WebRTCInternals::UpdateObserver(WebRTCInternalsUIObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (peer_connection_data_.GetSize() > 0)
    observer->OnUpdate("updateAllPeerConnections", &peer_connection_data_);

  for (base::ListValue::iterator it = get_user_media_requests_.begin();
       it != get_user_media_requests_.end();
       ++it) {
    observer->OnUpdate("addGetUserMedia", *it);
  }
}

void WebRTCInternals::EnableAecDump(content::WebContents* web_contents) {
#if defined(ENABLE_WEBRTC)
#if defined(OS_ANDROID)
  EnableAecDumpOnAllRenderProcessHosts();
#else
  select_file_dialog_ = ui::SelectFileDialog::Create(this, NULL);
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      base::string16(),
      aec_dump_file_path_,
      NULL,
      0,
      FILE_PATH_LITERAL(""),
      web_contents->GetTopLevelNativeWindow(),
      NULL);
#endif
#endif
}

void WebRTCInternals::DisableAecDump() {
#if defined(ENABLE_WEBRTC)
  aec_dump_enabled_ = false;

  // Tear down the dialog since the user has unchecked the AEC dump box.
  select_file_dialog_ = NULL;

  for (RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->DisableAecDump();
  }
#endif
}

void WebRTCInternals::ResetForTesting() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_.Clear();
  peer_connection_data_.Clear();
  get_user_media_requests_.Clear();
  aec_dump_enabled_ = false;
}

void WebRTCInternals::SendUpdate(const string& command, base::Value* value) {
  DCHECK(observers_.might_have_observers());

  FOR_EACH_OBSERVER(WebRTCInternalsUIObserver,
                    observers_,
                    OnUpdate(command, value));
}

void WebRTCInternals::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(type, NOTIFICATION_RENDERER_PROCESS_TERMINATED);
  OnRendererExit(Source<RenderProcessHost>(source)->GetID());
}

void WebRTCInternals::FileSelected(const base::FilePath& path,
                                   int /* unused_index */,
                                   void* /*unused_params */) {
#if defined(ENABLE_WEBRTC)
  aec_dump_file_path_ = path;
  EnableAecDumpOnAllRenderProcessHosts();
#endif
}

void WebRTCInternals::FileSelectionCanceled(void* params) {
#if defined(ENABLE_WEBRTC)
  SendUpdate("aecRecordingFileSelectionCancelled", NULL);
#endif
}

void WebRTCInternals::OnRendererExit(int render_process_id) {
  // Iterates from the end of the list to remove the PeerConnections created
  // by the exitting renderer.
  for (int i = peer_connection_data_.GetSize() - 1; i >= 0; --i) {
    base::DictionaryValue* record = NULL;
    peer_connection_data_.GetDictionary(i, &record);

    int this_rid = 0;
    record->GetInteger("rid", &this_rid);

    if (this_rid == render_process_id) {
      if (observers_.might_have_observers()) {
        int lid = 0, pid = 0;
        record->GetInteger("lid", &lid);
        record->GetInteger("pid", &pid);

        base::DictionaryValue update;
        update.SetInteger("lid", lid);
        update.SetInteger("pid", pid);
        SendUpdate("removePeerConnection", &update);
      }
      peer_connection_data_.Remove(i, NULL);
    }
  }

  bool found_any = false;
  // Iterates from the end of the list to remove the getUserMedia requests
  // created by the exiting renderer.
  for (int i = get_user_media_requests_.GetSize() - 1; i >= 0; --i) {
    base::DictionaryValue* record = NULL;
    get_user_media_requests_.GetDictionary(i, &record);

    int this_rid = 0;
    record->GetInteger("rid", &this_rid);

    if (this_rid == render_process_id) {
      get_user_media_requests_.Remove(i, NULL);
      found_any = true;
    }
  }

  if (found_any && observers_.might_have_observers()) {
    base::DictionaryValue update;
    update.SetInteger("rid", render_process_id);
    SendUpdate("removeGetUserMediaForRenderer", &update);
  }
}

#if defined(ENABLE_WEBRTC)
void WebRTCInternals::EnableAecDumpOnAllRenderProcessHosts() {
  aec_dump_enabled_ = true;
  for (RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->EnableAecDump(aec_dump_file_path_);
  }
}
#endif

}  // namespace content
