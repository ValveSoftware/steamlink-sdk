// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_audio_input.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/renderer/media/audio_input_message_filter.h"
#include "content/renderer/pepper/pepper_audio_input_host.h"
#include "content/renderer/pepper/pepper_media_device_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "media/audio/audio_device_description.h"
#include "ppapi/shared_impl/ppb_audio_config_shared.h"
#include "url/gurl.h"

namespace content {

// static
PepperPlatformAudioInput* PepperPlatformAudioInput::Create(
    int render_frame_id,
    const std::string& device_id,
    const GURL& document_url,
    int sample_rate,
    int frames_per_buffer,
    PepperAudioInputHost* client) {
  scoped_refptr<PepperPlatformAudioInput> audio_input(
      new PepperPlatformAudioInput());
  if (audio_input->Initialize(render_frame_id,
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
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PepperPlatformAudioInput::StartCaptureOnIOThread, this));
}

void PepperPlatformAudioInput::StopCapture() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PepperPlatformAudioInput::StopCaptureOnIOThread, this));
}

void PepperPlatformAudioInput::ShutDown() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Make sure we don't call shutdown more than once.
  if (!client_)
    return;

  // Called on the main thread to stop all audio callbacks. We must only change
  // the client on the main thread, and the delegates from the I/O thread.
  client_ = NULL;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PepperPlatformAudioInput::ShutDownOnIOThread, this));
}

void PepperPlatformAudioInput::OnStreamCreated(
    base::SharedMemoryHandle handle,
    base::SyncSocket::Handle socket_handle,
    int length,
    int total_segments) {
#if defined(OS_WIN)
  DCHECK(handle.IsValid());
  DCHECK(socket_handle);
#else
  DCHECK(base::SharedMemory::IsHandleValid(handle));
  DCHECK_NE(-1, socket_handle);
#endif
  DCHECK(length);
  // TODO(yzshen): Make use of circular buffer scheme. crbug.com/181449.
  DCHECK_EQ(1, total_segments);

  if (base::ThreadTaskRunnerHandle::Get().get() != main_task_runner_.get()) {
    // If shutdown has occurred, |client_| will be NULL and the handles will be
    // cleaned up on the main thread.
    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(&PepperPlatformAudioInput::OnStreamCreated, this,
                              handle, socket_handle, length, total_segments));
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
    media::AudioInputIPCDelegateState state) {}

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
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(ChildProcess::current()->io_task_runner()),
      render_frame_id_(MSG_ROUTING_NONE),
      create_stream_sent_(false),
      pending_open_device_(false),
      pending_open_device_id_(-1) {
}

bool PepperPlatformAudioInput::Initialize(
    int render_frame_id,
    const std::string& device_id,
    const GURL& document_url,
    int sample_rate,
    int frames_per_buffer,
    PepperAudioInputHost* client) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  RenderFrameImpl* const render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_id);
  if (!render_frame || !client)
    return false;

  render_frame_id_ = render_frame_id;
  client_ = client;

  if (!GetMediaDeviceManager())
    return false;

  ipc_ = RenderThreadImpl::current()
             ->audio_input_message_filter()
             ->CreateAudioInputIPC(render_frame_id);

  params_.Reset(media::AudioParameters::AUDIO_PCM_LINEAR,
                media::CHANNEL_LAYOUT_MONO,
                sample_rate,
                ppapi::kBitsPerAudioInputSample,
                frames_per_buffer);

  // We need to open the device and obtain the label and session ID before
  // initializing.
  pending_open_device_id_ = GetMediaDeviceManager()->OpenDevice(
      PP_DEVICETYPE_DEV_AUDIOCAPTURE,
      device_id.empty() ? media::AudioDeviceDescription::kDefaultDeviceId
                        : device_id,
      document_url,
      base::Bind(&PepperPlatformAudioInput::OnDeviceOpened, this));
  pending_open_device_ = true;

  return true;
}

void PepperPlatformAudioInput::InitializeOnIOThread(int session_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!ipc_)
    return;

  // We will be notified by OnStreamCreated().
  create_stream_sent_ = true;
  ipc_->CreateStream(this, session_id, params_, false, 1);
}

void PepperPlatformAudioInput::StartCaptureOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (ipc_)
    ipc_->RecordStream();
}

void PepperPlatformAudioInput::StopCaptureOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // TODO(yzshen): We cannot re-start capturing if the stream is closed.
  if (ipc_ && create_stream_sent_) {
    ipc_->CloseStream();
  }
  ipc_.reset();
}

void PepperPlatformAudioInput::ShutDownOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  StopCaptureOnIOThread();

  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PepperPlatformAudioInput::CloseDevice, this));

  Release();  // Release for the delegate, balances out the reference taken in
              // PepperPlatformAudioInput::Create.
}

void PepperPlatformAudioInput::OnDeviceOpened(int request_id,
                                              bool succeeded,
                                              const std::string& label) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  pending_open_device_ = false;
  pending_open_device_id_ = -1;

  PepperMediaDeviceManager* const device_manager = GetMediaDeviceManager();
  if (succeeded && device_manager) {
    DCHECK(!label.empty());
    label_ = label;

    if (client_) {
      int session_id = device_manager->GetSessionID(
          PP_DEVICETYPE_DEV_AUDIOCAPTURE, label);
      io_task_runner_->PostTask(
          FROM_HERE, base::Bind(&PepperPlatformAudioInput::InitializeOnIOThread,
                                this, session_id));
    } else {
      // Shutdown has occurred.
      CloseDevice();
    }
  } else {
    NotifyStreamCreationFailed();
  }
}

void PepperPlatformAudioInput::CloseDevice() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (!label_.empty()) {
    PepperMediaDeviceManager* const device_manager = GetMediaDeviceManager();
    if (device_manager)
      device_manager->CloseDevice(label_);
    label_.clear();
  }
  if (pending_open_device_) {
    PepperMediaDeviceManager* const device_manager = GetMediaDeviceManager();
    if (device_manager)
      device_manager->CancelOpenDevice(pending_open_device_id_);
    pending_open_device_ = false;
    pending_open_device_id_ = -1;
  }
}

void PepperPlatformAudioInput::NotifyStreamCreationFailed() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (client_)
    client_->StreamCreationFailed();
}

PepperMediaDeviceManager* PepperPlatformAudioInput::GetMediaDeviceManager() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  RenderFrameImpl* const render_frame =
      RenderFrameImpl::FromRoutingID(render_frame_id_);
  return render_frame ?
      PepperMediaDeviceManager::GetForRenderFrame(render_frame).get() : NULL;
}

}  // namespace content
