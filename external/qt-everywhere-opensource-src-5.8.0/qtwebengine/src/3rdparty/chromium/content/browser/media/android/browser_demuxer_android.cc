// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/browser_demuxer_android.h"

#include "base/macros.h"
#include "content/common/media/media_player_messages_android.h"
#include "media/base/android/media_task_runner.h"
#include "media/base/media_switches.h"

namespace content {

class BrowserDemuxerAndroid::Internal : public media::DemuxerAndroid {
 public:
  Internal(const scoped_refptr<BrowserDemuxerAndroid>& demuxer,
           int demuxer_client_id)
      : demuxer_(demuxer),
        demuxer_client_id_(demuxer_client_id) {}

  ~Internal() override {
    DCHECK(ClientIDExists()) << demuxer_client_id_;
    demuxer_->RemoveDemuxerClient(demuxer_client_id_);
  }

  // media::DemuxerAndroid implementation.
  void Initialize(media::DemuxerAndroidClient* client) override {
    DCHECK(!ClientIDExists()) << demuxer_client_id_;
    demuxer_->AddDemuxerClient(demuxer_client_id_, client);
  }

  void RequestDemuxerData(media::DemuxerStream::Type type) override {
    DCHECK(ClientIDExists()) << demuxer_client_id_;
    demuxer_->Send(new MediaPlayerMsg_ReadFromDemuxer(
        demuxer_client_id_, type));
  }

  void RequestDemuxerSeek(const base::TimeDelta& time_to_seek,
                          bool is_browser_seek) override {
    DCHECK(ClientIDExists()) << demuxer_client_id_;
    demuxer_->Send(new MediaPlayerMsg_DemuxerSeekRequest(
        demuxer_client_id_, time_to_seek, is_browser_seek));
  }

 private:
  // Helper for DCHECKing that the ID is still registered.
  bool ClientIDExists() {
    return demuxer_->demuxer_clients_.Lookup(demuxer_client_id_);
  }

  scoped_refptr<BrowserDemuxerAndroid> demuxer_;
  int demuxer_client_id_;

  DISALLOW_COPY_AND_ASSIGN(Internal);
};

BrowserDemuxerAndroid::BrowserDemuxerAndroid()
    : BrowserMessageFilter(MediaPlayerMsgStart) {
  const bool use_media_thread = media::UseMediaThreadForMediaPlayback();
  VLOG(1) << "Using " << (use_media_thread ? "media" : "UI") << " thread"
          << " for MSE playback";
  task_runner_ =
      use_media_thread ?
      media::GetMediaTaskRunner().get() :
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI).get();

}

BrowserDemuxerAndroid::~BrowserDemuxerAndroid() {}

base::TaskRunner* BrowserDemuxerAndroid::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {

  switch (message.type()) {
    case MediaPlayerHostMsg_DemuxerReady::ID:
    case MediaPlayerHostMsg_ReadFromDemuxerAck::ID:
    case MediaPlayerHostMsg_DurationChanged::ID:
    case MediaPlayerHostMsg_DemuxerSeekDone::ID:
      return task_runner_;
  }

  return nullptr;
}

bool BrowserDemuxerAndroid::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserDemuxerAndroid, message)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DemuxerReady, OnDemuxerReady)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_ReadFromDemuxerAck,
                        OnReadFromDemuxerAck)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DurationChanged,
                        OnDurationChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_DemuxerSeekDone,
                        OnDemuxerSeekDone)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

std::unique_ptr<media::DemuxerAndroid> BrowserDemuxerAndroid::CreateDemuxer(
    int demuxer_client_id) {
  return std::unique_ptr<media::DemuxerAndroid>(
      new Internal(this, demuxer_client_id));
}

void BrowserDemuxerAndroid::AddDemuxerClient(
    int demuxer_client_id,
    media::DemuxerAndroidClient* client) {
  DVLOG(1) << __FUNCTION__ << " peer_pid=" << peer_pid()
           << " demuxer_client_id=" << demuxer_client_id;
  DCHECK(task_runner_->BelongsToCurrentThread());

  demuxer_clients_.AddWithID(client, demuxer_client_id);

  if (pending_configs_.count(demuxer_client_id)) {
    client->OnDemuxerConfigsAvailable(pending_configs_[demuxer_client_id]);
    pending_configs_.erase(demuxer_client_id);
  }
}

void BrowserDemuxerAndroid::RemoveDemuxerClient(int demuxer_client_id) {
  DVLOG(1) << __FUNCTION__ << " peer_pid=" << peer_pid()
           << " demuxer_client_id=" << demuxer_client_id;
  DCHECK(task_runner_->BelongsToCurrentThread());

  demuxer_clients_.Remove(demuxer_client_id);
}

void BrowserDemuxerAndroid::OnDemuxerReady(
    int demuxer_client_id,
    const media::DemuxerConfigs& configs) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  media::DemuxerAndroidClient* client =
      demuxer_clients_.Lookup(demuxer_client_id);

  if (client)
    client->OnDemuxerConfigsAvailable(configs);
  else
    pending_configs_[demuxer_client_id] = configs;
}

void BrowserDemuxerAndroid::OnReadFromDemuxerAck(
    int demuxer_client_id,
    const media::DemuxerData& data) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  media::DemuxerAndroidClient* client =
      demuxer_clients_.Lookup(demuxer_client_id);

  if (client)
    client->OnDemuxerDataAvailable(data);
}

void BrowserDemuxerAndroid::OnDemuxerSeekDone(
    int demuxer_client_id,
    const base::TimeDelta& actual_browser_seek_time) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  media::DemuxerAndroidClient* client =
      demuxer_clients_.Lookup(demuxer_client_id);

  if (client)
    client->OnDemuxerSeekDone(actual_browser_seek_time);
}

void BrowserDemuxerAndroid::OnDurationChanged(int demuxer_client_id,
                                              const base::TimeDelta& duration) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  media::DemuxerAndroidClient* client =
      demuxer_clients_.Lookup(demuxer_client_id);

  if (client)
    client->OnDemuxerDurationChanged(duration);
  else
    pending_configs_[demuxer_client_id].duration = duration;
}

}  // namespace content
