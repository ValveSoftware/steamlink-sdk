// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webrtc/webrtc_internals.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "content/browser/media/webrtc/webrtc_internals_ui_observer.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "device/power_save_blocker/power_save_blocker.h"

using base::ProcessId;
using std::string;

namespace content {

namespace {

static base::LazyInstance<WebRTCInternals>::Leaky g_webrtc_internals =
    LAZY_INSTANCE_INITIALIZER;

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

WebRTCInternals::PendingUpdate::PendingUpdate(
    const std::string& command,
    std::unique_ptr<base::Value> value)
    : command_(command), value_(std::move(value)) {}

WebRTCInternals::PendingUpdate::PendingUpdate(PendingUpdate&& other)
    : command_(std::move(other.command_)),
      value_(std::move(other.value_)) {}

WebRTCInternals::PendingUpdate::~PendingUpdate() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

const std::string& WebRTCInternals::PendingUpdate::command() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return command_;
}

const base::Value* WebRTCInternals::PendingUpdate::value() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return value_.get();
}

WebRTCInternals::WebRTCInternals() : WebRTCInternals(500) {}

WebRTCInternals::WebRTCInternals(int aggregate_updates_ms)
    : audio_debug_recordings_(false),
      event_log_recordings_(false),
      selecting_event_log_(false),
      aggregate_updates_ms_(aggregate_updates_ms),
      weak_factory_(this) {
// TODO(grunell): Shouldn't all the webrtc_internals* files be excluded from the
// build if WebRTC is disabled?
#if defined(ENABLE_WEBRTC)
  audio_debug_recordings_file_path_ =
      GetContentClient()->browser()->GetDefaultDownloadDirectory();
  event_log_recordings_file_path_ = audio_debug_recordings_file_path_;

  if (audio_debug_recordings_file_path_.empty()) {
    // In this case the default path (|audio_debug_recordings_file_path_|) will
    // be empty and the platform default path will be used in the file dialog
    // (with no default file name). See SelectFileDialog::SelectFile. On Android
    // where there's no dialog we'll fail to open the file.
    VLOG(1) << "Could not get the download directory.";
  } else {
    audio_debug_recordings_file_path_ =
        audio_debug_recordings_file_path_.Append(
            FILE_PATH_LITERAL("audio_debug"));
    event_log_recordings_file_path_ =
        event_log_recordings_file_path_.Append(FILE_PATH_LITERAL("event_log"));
  }
#endif  // defined(ENABLE_WEBRTC)
}

WebRTCInternals::~WebRTCInternals() {
}

WebRTCInternals* WebRTCInternals::GetInstance() {
  return g_webrtc_internals.Pointer();
}

void WebRTCInternals::OnAddPeerConnection(int render_process_id,
                                          ProcessId pid,
                                          int lid,
                                          const string& url,
                                          const string& rtc_configuration,
                                          const string& constraints) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("rid", render_process_id);
  dict->SetInteger("pid", static_cast<int>(pid));
  dict->SetInteger("lid", lid);
  dict->SetString("rtcConfiguration", rtc_configuration);
  dict->SetString("constraints", constraints);
  dict->SetString("url", url);
  peer_connection_data_.Append(dict);
  CreateOrReleasePowerSaveBlocker();

  if (observers_.might_have_observers())
    SendUpdate("addPeerConnection", dict->CreateDeepCopy());

  if (render_process_id_set_.insert(render_process_id).second) {
    RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
    if (host)
      host->AddObserver(this);
  }
}

void WebRTCInternals::OnRemovePeerConnection(ProcessId pid, int lid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
    CreateOrReleasePowerSaveBlocker();

    if (observers_.might_have_observers()) {
      std::unique_ptr<base::DictionaryValue> id(new base::DictionaryValue());
      id->SetInteger("pid", static_cast<int>(pid));
      id->SetInteger("lid", lid);
      SendUpdate("removePeerConnection", std::move(id));
    }
    break;
  }
}

void WebRTCInternals::OnUpdatePeerConnection(
    ProcessId pid, int lid, const string& type, const string& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

    double epoch_time = base::Time::Now().ToJsTime();
    string time = base::DoubleToString(epoch_time);
    log_entry->SetString("time", time);
    log_entry->SetString("type", type);
    log_entry->SetString("value", value);
    log->Append(log_entry);

    if (observers_.might_have_observers()) {
      std::unique_ptr<base::DictionaryValue> update(
          new base::DictionaryValue());
      update->SetInteger("pid", static_cast<int>(pid));
      update->SetInteger("lid", lid);
      update->MergeDictionary(log_entry);

      SendUpdate("updatePeerConnection", std::move(update));
    }
    return;
  }
}

void WebRTCInternals::OnAddStats(base::ProcessId pid, int lid,
                                 const base::ListValue& value) {
  if (!observers_.might_have_observers())
    return;

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger("pid", static_cast<int>(pid));
  dict->SetInteger("lid", lid);

  dict->Set("reports", value.CreateDeepCopy());

  SendUpdate("addStats", std::move(dict));
}

void WebRTCInternals::OnGetUserMedia(int rid,
                                     base::ProcessId pid,
                                     const std::string& origin,
                                     bool audio,
                                     bool video,
                                     const std::string& audio_constraints,
                                     const std::string& video_constraints) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
    SendUpdate("addGetUserMedia", dict->CreateDeepCopy());

  if (render_process_id_set_.insert(rid).second) {
    RenderProcessHost* host = RenderProcessHost::FromID(rid);
    if (host)
      host->AddObserver(this);
  }
}

void WebRTCInternals::AddObserver(WebRTCInternalsUIObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void WebRTCInternals::RemoveObserver(WebRTCInternalsUIObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);

  // Disables audio debug recordings if it is enabled and the last
  // webrtc-internals page is going away.
  if (audio_debug_recordings_ && !observers_.might_have_observers())
    DisableAudioDebugRecordings();
}

void WebRTCInternals::UpdateObserver(WebRTCInternalsUIObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (peer_connection_data_.GetSize() > 0)
    observer->OnUpdate("updateAllPeerConnections", &peer_connection_data_);

  for (const auto& request : get_user_media_requests_) {
    observer->OnUpdate("addGetUserMedia", request.get());
  }
}

void WebRTCInternals::EnableAudioDebugRecordings(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(ENABLE_WEBRTC)
#if defined(OS_ANDROID)
  EnableAudioDebugRecordingsOnAllRenderProcessHosts();
#else
  selecting_event_log_ = false;
  DCHECK(!select_file_dialog_);
  select_file_dialog_ = ui::SelectFileDialog::Create(this, NULL);
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      base::string16(),
      audio_debug_recordings_file_path_,
      NULL,
      0,
      FILE_PATH_LITERAL(""),
      web_contents->GetTopLevelNativeWindow(),
      NULL);
#endif
#endif
}

void WebRTCInternals::DisableAudioDebugRecordings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(ENABLE_WEBRTC)
  audio_debug_recordings_ = false;

  // Tear down the dialog since the user has unchecked the audio debug
  // recordings box.
  select_file_dialog_ = NULL;

  for (RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->DisableAudioDebugRecordings();
  }
#endif
}

bool WebRTCInternals::IsAudioDebugRecordingsEnabled() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return audio_debug_recordings_;
}

const base::FilePath& WebRTCInternals::GetAudioDebugRecordingsFilePath() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return audio_debug_recordings_file_path_;
}

void WebRTCInternals::SetEventLogRecordings(
    bool enable,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(ENABLE_WEBRTC)
  if (enable) {
#if defined(OS_ANDROID)
    EnableEventLogRecordingsOnAllRenderProcessHosts();
#else
    DCHECK(web_contents);
    DCHECK(!select_file_dialog_);
    selecting_event_log_ = true;
    select_file_dialog_ = ui::SelectFileDialog::Create(this, nullptr);
    select_file_dialog_->SelectFile(
        ui::SelectFileDialog::SELECT_SAVEAS_FILE, base::string16(),
        event_log_recordings_file_path_, nullptr, 0, FILE_PATH_LITERAL(""),
        web_contents->GetTopLevelNativeWindow(), nullptr);
#endif
  } else {
    event_log_recordings_ = false;
    // Tear down the dialog since the user has unchecked the audio debug
    // recordings box.
    select_file_dialog_ = nullptr;
    DCHECK(select_file_dialog_->HasOneRef());

    for (RenderProcessHost::iterator i(
             content::RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance()) {
      i.GetCurrentValue()->DisableEventLogRecordings();
    }
  }
#endif
}

bool WebRTCInternals::IsEventLogRecordingsEnabled() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return event_log_recordings_;
}

const base::FilePath& WebRTCInternals::GetEventLogRecordingsFilePath() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return event_log_recordings_file_path_;
}

void WebRTCInternals::SendUpdate(const string& command,
                                 std::unique_ptr<base::Value> value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(observers_.might_have_observers());

  bool queue_was_empty = pending_updates_.empty();
  pending_updates_.push(PendingUpdate(command, std::move(value)));

  if (queue_was_empty) {
    BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&WebRTCInternals::ProcessPendingUpdates,
                   weak_factory_.GetWeakPtr()),
                   base::TimeDelta::FromMilliseconds(aggregate_updates_ms_));
  }
}

void WebRTCInternals::RenderProcessHostDestroyed(RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnRendererExit(host->GetID());

  render_process_id_set_.erase(host->GetID());
  host->RemoveObserver(this);
}

void WebRTCInternals::FileSelected(const base::FilePath& path,
                                   int /* unused_index */,
                                   void* /*unused_params */) {
#if defined(ENABLE_WEBRTC)
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (selecting_event_log_) {
    event_log_recordings_file_path_ = path;
    EnableEventLogRecordingsOnAllRenderProcessHosts();
  } else {
    audio_debug_recordings_file_path_ = path;
    EnableAudioDebugRecordingsOnAllRenderProcessHosts();
  }
#endif
}

void WebRTCInternals::FileSelectionCanceled(void* params) {
#if defined(ENABLE_WEBRTC)
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (selecting_event_log_) {
    SendUpdate("eventLogRecordingsFileSelectionCancelled", nullptr);
  } else {
    SendUpdate("audioDebugRecordingsFileSelectionCancelled", nullptr);
  }
#endif
}

void WebRTCInternals::OnRendererExit(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

        std::unique_ptr<base::DictionaryValue> update(
            new base::DictionaryValue());
        update->SetInteger("lid", lid);
        update->SetInteger("pid", pid);
        SendUpdate("removePeerConnection", std::move(update));
      }
      peer_connection_data_.Remove(i, NULL);
    }
  }
  CreateOrReleasePowerSaveBlocker();

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
    std::unique_ptr<base::DictionaryValue> update(new base::DictionaryValue());
    update->SetInteger("rid", render_process_id);
    SendUpdate("removeGetUserMediaForRenderer", std::move(update));
  }
}

#if defined(ENABLE_WEBRTC)
void WebRTCInternals::EnableAudioDebugRecordingsOnAllRenderProcessHosts() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  audio_debug_recordings_ = true;
  for (RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->EnableAudioDebugRecordings(
        audio_debug_recordings_file_path_);
  }
}

void WebRTCInternals::EnableEventLogRecordingsOnAllRenderProcessHosts() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  event_log_recordings_ = true;
  for (RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->EnableEventLogRecordings(
        event_log_recordings_file_path_);
  }
}
#endif

void WebRTCInternals::CreateOrReleasePowerSaveBlocker() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (peer_connection_data_.empty() && power_save_blocker_) {
    DVLOG(1) << ("Releasing the block on application suspension since no "
                 "PeerConnections are active anymore.");
    power_save_blocker_.reset();
  } else if (!peer_connection_data_.empty() && !power_save_blocker_) {
    DVLOG(1) << ("Preventing the application from being suspended while one or "
                 "more PeerConnections are active.");
    power_save_blocker_.reset(new device::PowerSaveBlocker(
        device::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
        device::PowerSaveBlocker::kReasonOther,
        "WebRTC has active PeerConnections",
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)));
  }
}

void WebRTCInternals::ProcessPendingUpdates() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  while (!pending_updates_.empty()) {
    const auto& update = pending_updates_.front();
    FOR_EACH_OBSERVER(WebRTCInternalsUIObserver,
                      observers_,
                      OnUpdate(update.command(), update.value()));
    pending_updates_.pop();
  }
}

}  // namespace content
