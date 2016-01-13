// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_audio_input.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/pepper/pepper_audio_input_host.h"
#include "content/renderer/pepper/pepper_media_device_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "media/audio/audio_manager_base.h"
#include "ppapi/shared_impl/ppb_audio_config_shared.h"
#include "url/gurl.h"

namespace content {

// static
PepperPlatformAudioInput* PepperPlatformAudioInput::Create(
    const base::WeakPtr<RenderViewImpl>& render_view,
    const std::string& device_id,
    const GURL& document_url,
    int sample_rate,
    int frames_per_buffer,
    PepperAudioInputHost* client) {
  scoped_refptr<PepperPlatformAudioInput> audio_input(
      new PepperPlatformAudioInput());
  if (audio_input->Initialize(render_view,
                              device_id,
                              document_url,
                              sample_rate,
                              frames_per_buffer,
                              client)) {
    // Balanced by Release invoked in
    // PepperPlatformAudioInput::ShutDownOnIOThread().
    audio_input->AddRef();
    return audio_input.get();
  }
  return NULL;
}

void PepperPlatformAudioInput::StartCapture() {
  DCHECK(main_message_loop_proxy_->BelongsToCurrentThread());

  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PepperPlatformAudioInput::StartCaptureOnIOThread, this));
}

void PepperPlatformAudioInput::StopCapture() {
  DCHECK(main_message_loop_proxy_->BelongsToCurrentThread());

  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PepperPlatformAudioInput::StopCaptureOnIOThread, this));
}

void PepperPlatformAudioInput::ShutDown() {
  DCHECK(main_message_loop_proxy_->BelongsToCurrentThread());

  // Make sure we don't call shutdown more than once.
  if (!client_)
    return;

  // Called on the main thread to stop all audio callbacks. We must only change
  // the client on the main thread, and the delegates from the I/O thread.
  client_ = NULL;
  io_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&PepperPlatformAudioInput::ShutDownOnIOThread, this));
}

void PepperPlatformAudioInput::OnStreamCreated(
    base::SharedMemoryHandle handle,
    base::SyncSocket::Handle socket_handle,
    int length,
    int total_segments) {
#if defined(OS_WIN)
  DCHECK(handle);
  DCHECK(socket_handle);
#else
  DCHECK_NE(-1, handle.fd);
  DCHECK_NE(-1, socket_handle);
#endif
  DCHECK(length);
  // TODO(yzshen): Make use of circular buffer scheme. crbug.com/181449.
  DCHECK_EQ(1, total_segments);

  if (base::MessageLoopProxy::current().get() !=
      main_message_loop_proxy_.get()) {
    // If shutdown has occurred, |client_| will be NULL and the handles will be
    // cleaned up on the main thread.
    main_message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&PepperPlatformAudioInput::OnStreamCreated,
                   this,
                   handle,
                   socket_handle,
                   length,
                   total_segments));
  } else {
    // Must dereference the client only on the main thread. Shutdown may have
    // occurred while the request was in-flight, so we need to NULL check.
    if (client_) {
      client_->StreamCreated(handle, length, socket_handle);
    } else {
      // Clean up the handles.
      base::SyncSocket temp_socket(socket_handle);
      base::SharedMemory temp_shared_memory(handle, false);
    }
  }
}

void PepperPlatformAudioInput::OnVolume(double volume) {}

void PepperPlatformAudioInput::OnStateChanged(
    media::AudioInputIPCDelegate::State state) {}

void PepperPlatformAudioInput::OnIPCClosed() { ipc_.reset(); }

PepperPlatformAudioInput::~PepperPlatformAudioInput() {
  // Make sure we have been shut down. Warning: this may happen on the I/O
  // thread!
  // Although these members should be accessed on a specific thread (either the
  // main thread or the I/O thread), it should be fine to examine their value
  // here.
  DCHECK(!ipc_);
  DCHECK(!client_);
  DCHECK(label_.empty());
  DCHECK(!pending_open_device_);
}

PepperPlatformAudioInput::PepperPlatformAudioInput()
    : client_(NULL),
      main_message_loop_proxy_(base::MessageLoopProxy::current()),
      io_message_loop_proxy_(ChildProcess::current()->io_message_loop_proxy()),
      create_stream_sent_(false),
      pending_open_device_(false),
      pending_open_device_id_(-1) {}

bool PepperPlatformAudioInput::Initialize(
    const base::WeakPtr<RenderViewImpl>& render_view,
    const std::string& device_id,
    const GURL& document_url,
    int sample_rate,
    int frames_per_buffer,
    PepperAudioInputHost* client) {
  DCHECK(main_message_loop_proxy_->BelongsToCurrentThread());

  if (!render_view.get() || !client)
    return false;

  ipc_ = RenderThreadImpl::current()
             ->audio_input_message_filter()
             ->CreateAudioInputIPC(render_view->GetRoutingID());

  render_view_ = render_view;
  client_ = client;

  params_.Reset(media::AudioParameters::AUDIO_PCM_LINEAR,
                media::CHANNEL_LAYOUT_MONO,
                ppapi::kAudioInputChannels,
                0,
                sample_rate,
                ppapi::kBitsPerAudioInputSample,
                frames_per_buffer);

  // We need to open the device and obtain the label and session ID before
  // initializing.
  pending_open_device_id_ = GetMediaDeviceManager()->OpenDevice(
      PP_DEVICETYPE_DEV_AUDIOCAPTURE,
      device_id.empty() ? media::AudioManagerBase::kDefaultDeviceId : device_id,
      document_url,
      base::Bind(&PepperPlatformAudioInput::OnDeviceOpened, this));
  pending_open_device_ = true;

  return true;
}

void PepperPlatformAudioInput::InitializeOnIOThread(int session_id) {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (!ipc_)
    return;

  // We will be notified by OnStreamCreated().
  create_stream_sent_ = true;
  ipc_->CreateStream(this, session_id, params_, false, 1);
}

void PepperPlatformAudioInput::StartCaptureOnIOThread() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  if (ipc_)
    ipc_->RecordStream();
}

void PepperPlatformAudioInput::StopCaptureOnIOThread() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  // TODO(yzshen): We cannot re-start capturing if the stream is closed.
  if (ipc_ && create_stream_sent_) {
    ipc_->CloseStream();
  }
  ipc_.reset();
}

void PepperPlatformAudioInput::ShutDownOnIOThread() {
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  StopCaptureOnIOThread();

  main_message_loop_proxy_->PostTask(
      FROM_HERE, base::Bind(&PepperPlatformAudioInput::CloseDevice, this));

  Release();  // Release for the delegate, balances out the reference taken in
              // PepperPlatformAudioInput::Create.
}

void PepperPlatformAudioInput::OnDeviceOpened(int request_id,
                                              bool succeeded,
                                              const std::string& label) {
  DCHECK(main_message_loop_proxy_->BelongsToCurrentThread());

  pending_open_device_ = false;
  pending_open_device_id_ = -1;

  if (succeeded && render_view_.get()) {
    DCHECK(!label.empty());
    label_ = label;

    if (client_) {
      int session_id = GetMediaDeviceManager()->GetSessionID(
          PP_DEVICETYPE_DEV_AUDIOCAPTURE, label);
      io_message_loop_proxy_->PostTask(
          FROM_HERE,
          base::Bind(&PepperPlatformAudioInput::InitializeOnIOThread,
                     this,
                     session_id));
    } else {
      // Shutdown has occurred.
      CloseDevice();
    }
  } else {
    NotifyStreamCreationFailed();
  }
}

void PepperPlatformAudioInput::CloseDevice() {
  DCHECK(main_message_loop_proxy_->BelongsToCurrentThread());

  if (render_view_.get()) {
    if (!label_.empty()) {
      GetMediaDeviceManager()->CloseDevice(label_);
      label_.clear();
    }
    if (pending_open_device_) {
      GetMediaDeviceManager()->CancelOpenDevice(pending_open_device_id_);
      pending_open_device_ = false;
      pending_open_device_id_ = -1;
    }
  }
}

void PepperPlatformAudioInput::NotifyStreamCreationFailed() {
  DCHECK(main_message_loop_proxy_->BelongsToCurrentThread());

  if (client_)
    client_->StreamCreationFailed();
}

PepperMediaDeviceManager* PepperPlatformAudioInput::GetMediaDeviceManager() {
  return PepperMediaDeviceManager::GetForRenderView(render_view_.get());
}

}  // namespace content
