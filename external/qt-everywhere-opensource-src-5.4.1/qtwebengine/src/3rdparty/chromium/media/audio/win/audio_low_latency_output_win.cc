// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_low_latency_output_win.h"

#include <Functiondiscoverykeys_devpkey.h>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_propvariant.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/avrt_wrapper_win.h"
#include "media/audio/win/core_audio_util_win.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

using base::win::ScopedComPtr;
using base::win::ScopedCOMInitializer;
using base::win::ScopedCoMem;

namespace media {

// static
AUDCLNT_SHAREMODE WASAPIAudioOutputStream::GetShareMode() {
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableExclusiveAudio))
    return AUDCLNT_SHAREMODE_EXCLUSIVE;
  return AUDCLNT_SHAREMODE_SHARED;
}

// static
int WASAPIAudioOutputStream::HardwareSampleRate(const std::string& device_id) {
  WAVEFORMATPCMEX format;
  ScopedComPtr<IAudioClient> client;
  if (device_id.empty()) {
    client = CoreAudioUtil::CreateDefaultClient(eRender, eConsole);
  } else {
    ScopedComPtr<IMMDevice> device(CoreAudioUtil::CreateDevice(device_id));
    if (!device)
      return 0;
    client = CoreAudioUtil::CreateClient(device);
  }

  if (!client || FAILED(CoreAudioUtil::GetSharedModeMixFormat(client, &format)))
    return 0;

  return static_cast<int>(format.Format.nSamplesPerSec);
}

WASAPIAudioOutputStream::WASAPIAudioOutputStream(AudioManagerWin* manager,
                                                 const std::string& device_id,
                                                 const AudioParameters& params,
                                                 ERole device_role)
    : creating_thread_id_(base::PlatformThread::CurrentId()),
      manager_(manager),
      format_(),
      opened_(false),
      volume_(1.0),
      packet_size_frames_(0),
      packet_size_bytes_(0),
      endpoint_buffer_size_frames_(0),
      device_id_(device_id),
      device_role_(device_role),
      share_mode_(GetShareMode()),
      num_written_frames_(0),
      source_(NULL),
      audio_bus_(AudioBus::Create(params)) {
  DCHECK(manager_);
  VLOG(1) << "WASAPIAudioOutputStream::WASAPIAudioOutputStream()";
  VLOG_IF(1, share_mode_ == AUDCLNT_SHAREMODE_EXCLUSIVE)
      << "Core Audio (WASAPI) EXCLUSIVE MODE is enabled.";

  // Load the Avrt DLL if not already loaded. Required to support MMCSS.
  bool avrt_init = avrt::Initialize();
  DCHECK(avrt_init) << "Failed to load the avrt.dll";

  // Set up the desired render format specified by the client. We use the
  // WAVE_FORMAT_EXTENSIBLE structure to ensure that multiple channel ordering
  // and high precision data can be supported.

  // Begin with the WAVEFORMATEX structure that specifies the basic format.
  WAVEFORMATEX* format = &format_.Format;
  format->wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  format->nChannels = params.channels();
  format->nSamplesPerSec = params.sample_rate();
  format->wBitsPerSample = params.bits_per_sample();
  format->nBlockAlign = (format->wBitsPerSample / 8) * format->nChannels;
  format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
  format->cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

  // Add the parts which are unique to WAVE_FORMAT_EXTENSIBLE.
  format_.Samples.wValidBitsPerSample = params.bits_per_sample();
  format_.dwChannelMask = CoreAudioUtil::GetChannelConfig(device_id, eRender);
  format_.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

  // Store size (in different units) of audio packets which we expect to
  // get from the audio endpoint device in each render event.
  packet_size_frames_ = params.frames_per_buffer();
  packet_size_bytes_ = params.GetBytesPerBuffer();
  VLOG(1) << "Number of bytes per audio frame  : " << format->nBlockAlign;
  VLOG(1) << "Number of audio frames per packet: " << packet_size_frames_;
  VLOG(1) << "Number of bytes per packet       : " << packet_size_bytes_;
  VLOG(1) << "Number of milliseconds per packet: "
          << params.GetBufferDuration().InMillisecondsF();

  // All events are auto-reset events and non-signaled initially.

  // Create the event which the audio engine will signal each time
  // a buffer becomes ready to be processed by the client.
  audio_samples_render_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(audio_samples_render_event_.IsValid());

  // Create the event which will be set in Stop() when capturing shall stop.
  stop_render_event_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
  DCHECK(stop_render_event_.IsValid());
}

WASAPIAudioOutputStream::~WASAPIAudioOutputStream() {}

bool WASAPIAudioOutputStream::Open() {
  VLOG(1) << "WASAPIAudioOutputStream::Open()";
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  if (opened_)
    return true;

  // Create an IAudioClient interface for the default rendering IMMDevice.
  ScopedComPtr<IAudioClient> audio_client;
  if (device_id_.empty() ||
      CoreAudioUtil::DeviceIsDefault(eRender, device_role_, device_id_)) {
    audio_client = CoreAudioUtil::CreateDefaultClient(eRender, device_role_);
  } else {
    ScopedComPtr<IMMDevice> device(CoreAudioUtil::CreateDevice(device_id_));
    DLOG_IF(ERROR, !device) << "Failed to open device: " << device_id_;
    if (device)
      audio_client = CoreAudioUtil::CreateClient(device);
  }

  if (!audio_client)
    return false;

  // Extra sanity to ensure that the provided device format is still valid.
  if (!CoreAudioUtil::IsFormatSupported(audio_client,
                                        share_mode_,
                                        &format_)) {
    LOG(ERROR) << "Audio parameters are not supported.";
    return false;
  }

  HRESULT hr = S_FALSE;
  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    // Initialize the audio stream between the client and the device in shared
    // mode and using event-driven buffer handling.
    hr = CoreAudioUtil::SharedModeInitialize(
        audio_client, &format_, audio_samples_render_event_.Get(),
        &endpoint_buffer_size_frames_);
    if (FAILED(hr))
      return false;

    // We know from experience that the best possible callback sequence is
    // achieved when the packet size (given by the native device period)
    // is an even divisor of the endpoint buffer size.
    // Examples: 48kHz => 960 % 480, 44.1kHz => 896 % 448 or 882 % 441.
    if (endpoint_buffer_size_frames_ % packet_size_frames_ != 0) {
      LOG(ERROR)
          << "Bailing out due to non-perfect timing.  Buffer size of "
          << packet_size_frames_ << " is not an even divisor of "
          << endpoint_buffer_size_frames_;
      return false;
    }
  } else {
    // TODO(henrika): break out to CoreAudioUtil::ExclusiveModeInitialize()
    // when removing the enable-exclusive-audio flag.
    hr = ExclusiveModeInitialization(audio_client,
                                     audio_samples_render_event_.Get(),
                                     &endpoint_buffer_size_frames_);
    if (FAILED(hr))
      return false;

    // The buffer scheme for exclusive mode streams is not designed for max
    // flexibility. We only allow a "perfect match" between the packet size set
    // by the user and the actual endpoint buffer size.
    if (endpoint_buffer_size_frames_ != packet_size_frames_) {
      LOG(ERROR) << "Bailing out due to non-perfect timing.";
      return false;
    }
  }

  // Create an IAudioRenderClient client for an initialized IAudioClient.
  // The IAudioRenderClient interface enables us to write output data to
  // a rendering endpoint buffer.
  ScopedComPtr<IAudioRenderClient> audio_render_client =
      CoreAudioUtil::CreateRenderClient(audio_client);
  if (!audio_render_client)
    return false;

   // Store valid COM interfaces.
  audio_client_ = audio_client;
  audio_render_client_ = audio_render_client;

  hr = audio_client_->GetService(__uuidof(IAudioClock),
                                 audio_clock_.ReceiveVoid());
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get IAudioClock service.";
    return false;
  }

  opened_ = true;
  return true;
}

void WASAPIAudioOutputStream::Start(AudioSourceCallback* callback) {
  VLOG(1) << "WASAPIAudioOutputStream::Start()";
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  CHECK(callback);
  CHECK(opened_);

  if (render_thread_) {
    CHECK_EQ(callback, source_);
    return;
  }

  source_ = callback;

  // Ensure that the endpoint buffer is prepared with silence.
  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    if (!CoreAudioUtil::FillRenderEndpointBufferWithSilence(
             audio_client_, audio_render_client_)) {
      LOG(ERROR) << "Failed to prepare endpoint buffers with silence.";
      callback->OnError(this);
      return;
    }
  }
  num_written_frames_ = endpoint_buffer_size_frames_;

  // Create and start the thread that will drive the rendering by waiting for
  // render events.
  render_thread_.reset(
      new base::DelegateSimpleThread(this, "wasapi_render_thread"));
  render_thread_->Start();
  if (!render_thread_->HasBeenStarted()) {
    LOG(ERROR) << "Failed to start WASAPI render thread.";
    StopThread();
    callback->OnError(this);
    return;
  }

  // Start streaming data between the endpoint buffer and the audio engine.
  HRESULT hr = audio_client_->Start();
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to start output streaming: " << std::hex << hr;
    StopThread();
    callback->OnError(this);
  }
}

void WASAPIAudioOutputStream::Stop() {
  VLOG(1) << "WASAPIAudioOutputStream::Stop()";
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);
  if (!render_thread_)
    return;

  // Stop output audio streaming.
  HRESULT hr = audio_client_->Stop();
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to stop output streaming: " << std::hex << hr;
    source_->OnError(this);
  }

  // Make a local copy of |source_| since StopThread() will clear it.
  AudioSourceCallback* callback = source_;
  StopThread();

  // Flush all pending data and reset the audio clock stream position to 0.
  hr = audio_client_->Reset();
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to reset streaming: " << std::hex << hr;
    callback->OnError(this);
  }

  // Extra safety check to ensure that the buffers are cleared.
  // If the buffers are not cleared correctly, the next call to Start()
  // would fail with AUDCLNT_E_BUFFER_ERROR at IAudioRenderClient::GetBuffer().
  // This check is is only needed for shared-mode streams.
  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    UINT32 num_queued_frames = 0;
    audio_client_->GetCurrentPadding(&num_queued_frames);
    DCHECK_EQ(0u, num_queued_frames);
  }
}

void WASAPIAudioOutputStream::Close() {
  VLOG(1) << "WASAPIAudioOutputStream::Close()";
  DCHECK_EQ(GetCurrentThreadId(), creating_thread_id_);

  // It is valid to call Close() before calling open or Start().
  // It is also valid to call Close() after Start() has been called.
  Stop();

  // Inform the audio manager that we have been closed. This will cause our
  // destruction.
  manager_->ReleaseOutputStream(this);
}

void WASAPIAudioOutputStream::SetVolume(double volume) {
  VLOG(1) << "SetVolume(volume=" << volume << ")";
  float volume_float = static_cast<float>(volume);
  if (volume_float < 0.0f || volume_float > 1.0f) {
    return;
  }
  volume_ = volume_float;
}

void WASAPIAudioOutputStream::GetVolume(double* volume) {
  VLOG(1) << "GetVolume()";
  *volume = static_cast<double>(volume_);
}

void WASAPIAudioOutputStream::Run() {
  ScopedCOMInitializer com_init(ScopedCOMInitializer::kMTA);

  // Increase the thread priority.
  render_thread_->SetThreadPriority(base::kThreadPriority_RealtimeAudio);

  // Enable MMCSS to ensure that this thread receives prioritized access to
  // CPU resources.
  DWORD task_index = 0;
  HANDLE mm_task = avrt::AvSetMmThreadCharacteristics(L"Pro Audio",
                                                      &task_index);
  bool mmcss_is_ok =
      (mm_task && avrt::AvSetMmThreadPriority(mm_task, AVRT_PRIORITY_CRITICAL));
  if (!mmcss_is_ok) {
    // Failed to enable MMCSS on this thread. It is not fatal but can lead
    // to reduced QoS at high load.
    DWORD err = GetLastError();
    LOG(WARNING) << "Failed to enable MMCSS (error code=" << err << ").";
  }

  HRESULT hr = S_FALSE;

  bool playing = true;
  bool error = false;
  HANDLE wait_array[] = { stop_render_event_,
                          audio_samples_render_event_ };
  UINT64 device_frequency = 0;

  // The device frequency is the frequency generated by the hardware clock in
  // the audio device. The GetFrequency() method reports a constant frequency.
  hr = audio_clock_->GetFrequency(&device_frequency);
  error = FAILED(hr);
  PLOG_IF(ERROR, error) << "Failed to acquire IAudioClock interface: "
                        << std::hex << hr;

  // Keep rendering audio until the stop event or the stream-switch event
  // is signaled. An error event can also break the main thread loop.
  while (playing && !error) {
    // Wait for a close-down event, stream-switch event or a new render event.
    DWORD wait_result = WaitForMultipleObjects(arraysize(wait_array),
                                               wait_array,
                                               FALSE,
                                               INFINITE);

    switch (wait_result) {
      case WAIT_OBJECT_0 + 0:
        // |stop_render_event_| has been set.
        playing = false;
        break;
      case WAIT_OBJECT_0 + 1:
        // |audio_samples_render_event_| has been set.
        error = !RenderAudioFromSource(device_frequency);
        break;
      default:
        error = true;
        break;
    }
  }

  if (playing && error) {
    // Stop audio rendering since something has gone wrong in our main thread
    // loop. Note that, we are still in a "started" state, hence a Stop() call
    // is required to join the thread properly.
    audio_client_->Stop();
    PLOG(ERROR) << "WASAPI rendering failed.";
  }

  // Disable MMCSS.
  if (mm_task && !avrt::AvRevertMmThreadCharacteristics(mm_task)) {
    PLOG(WARNING) << "Failed to disable MMCSS";
  }
}

bool WASAPIAudioOutputStream::RenderAudioFromSource(UINT64 device_frequency) {
  TRACE_EVENT0("audio", "RenderAudioFromSource");

  HRESULT hr = S_FALSE;
  UINT32 num_queued_frames = 0;
  uint8* audio_data = NULL;

  // Contains how much new data we can write to the buffer without
  // the risk of overwriting previously written data that the audio
  // engine has not yet read from the buffer.
  size_t num_available_frames = 0;

  if (share_mode_ == AUDCLNT_SHAREMODE_SHARED) {
    // Get the padding value which represents the amount of rendering
    // data that is queued up to play in the endpoint buffer.
    hr = audio_client_->GetCurrentPadding(&num_queued_frames);
    num_available_frames =
        endpoint_buffer_size_frames_ - num_queued_frames;
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to retrieve amount of available space: "
                  << std::hex << hr;
      return false;
    }
  } else {
    // While the stream is running, the system alternately sends one
    // buffer or the other to the client. This form of double buffering
    // is referred to as "ping-ponging". Each time the client receives
    // a buffer from the system (triggers this event) the client must
    // process the entire buffer. Calls to the GetCurrentPadding method
    // are unnecessary because the packet size must always equal the
    // buffer size. In contrast to the shared mode buffering scheme,
    // the latency for an event-driven, exclusive-mode stream depends
    // directly on the buffer size.
    num_available_frames = endpoint_buffer_size_frames_;
  }

  // Check if there is enough available space to fit the packet size
  // specified by the client.
  if (num_available_frames < packet_size_frames_)
    return true;

  DLOG_IF(ERROR, num_available_frames % packet_size_frames_ != 0)
      << "Non-perfect timing detected (num_available_frames="
      << num_available_frames << ", packet_size_frames="
      << packet_size_frames_ << ")";

  // Derive the number of packets we need to get from the client to
  // fill up the available area in the endpoint buffer.
  // |num_packets| will always be one for exclusive-mode streams and
  // will be one in most cases for shared mode streams as well.
  // However, we have found that two packets can sometimes be
  // required.
  size_t num_packets = (num_available_frames / packet_size_frames_);

  for (size_t n = 0; n < num_packets; ++n) {
    // Grab all available space in the rendering endpoint buffer
    // into which the client can write a data packet.
    hr = audio_render_client_->GetBuffer(packet_size_frames_,
                                         &audio_data);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to use rendering audio buffer: "
                 << std::hex << hr;
      return false;
    }

    // Derive the audio delay which corresponds to the delay between
    // a render event and the time when the first audio sample in a
    // packet is played out through the speaker. This delay value
    // can typically be utilized by an acoustic echo-control (AEC)
    // unit at the render side.
    UINT64 position = 0;
    int audio_delay_bytes = 0;
    hr = audio_clock_->GetPosition(&position, NULL);
    if (SUCCEEDED(hr)) {
      // Stream position of the sample that is currently playing
      // through the speaker.
      double pos_sample_playing_frames = format_.Format.nSamplesPerSec *
          (static_cast<double>(position) / device_frequency);

      // Stream position of the last sample written to the endpoint
      // buffer. Note that, the packet we are about to receive in
      // the upcoming callback is also included.
      size_t pos_last_sample_written_frames =
          num_written_frames_ + packet_size_frames_;

      // Derive the actual delay value which will be fed to the
      // render client using the OnMoreData() callback.
      audio_delay_bytes = (pos_last_sample_written_frames -
          pos_sample_playing_frames) *  format_.Format.nBlockAlign;
    }

    // Read a data packet from the registered client source and
    // deliver a delay estimate in the same callback to the client.
    // A time stamp is also stored in the AudioBuffersState. This
    // time stamp can be used at the client side to compensate for
    // the delay between the usage of the delay value and the time
    // of generation.

    int frames_filled = source_->OnMoreData(
        audio_bus_.get(), AudioBuffersState(0, audio_delay_bytes));
    uint32 num_filled_bytes = frames_filled * format_.Format.nBlockAlign;
    DCHECK_LE(num_filled_bytes, packet_size_bytes_);

    // Note: If this ever changes to output raw float the data must be
    // clipped and sanitized since it may come from an untrusted
    // source such as NaCl.
    const int bytes_per_sample = format_.Format.wBitsPerSample >> 3;
    audio_bus_->Scale(volume_);
    audio_bus_->ToInterleaved(
        frames_filled, bytes_per_sample, audio_data);


    // Release the buffer space acquired in the GetBuffer() call.
    // Render silence if we were not able to fill up the buffer totally.
    DWORD flags = (num_filled_bytes < packet_size_bytes_) ?
        AUDCLNT_BUFFERFLAGS_SILENT : 0;
    audio_render_client_->ReleaseBuffer(packet_size_frames_, flags);

    num_written_frames_ += packet_size_frames_;
  }

  return true;
}

HRESULT WASAPIAudioOutputStream::ExclusiveModeInitialization(
    IAudioClient* client, HANDLE event_handle, uint32* endpoint_buffer_size) {
  DCHECK_EQ(share_mode_, AUDCLNT_SHAREMODE_EXCLUSIVE);

  float f = (1000.0 * packet_size_frames_) / format_.Format.nSamplesPerSec;
  REFERENCE_TIME requested_buffer_duration =
      static_cast<REFERENCE_TIME>(f * 10000.0 + 0.5);

  DWORD stream_flags = AUDCLNT_STREAMFLAGS_NOPERSIST;
  bool use_event = (event_handle != NULL &&
                    event_handle != INVALID_HANDLE_VALUE);
  if (use_event)
    stream_flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  VLOG(2) << "stream_flags: 0x" << std::hex << stream_flags;

  // Initialize the audio stream between the client and the device.
  // For an exclusive-mode stream that uses event-driven buffering, the
  // caller must specify nonzero values for hnsPeriodicity and
  // hnsBufferDuration, and the values of these two parameters must be equal.
  // The Initialize method allocates two buffers for the stream. Each buffer
  // is equal in duration to the value of the hnsBufferDuration parameter.
  // Following the Initialize call for a rendering stream, the caller should
  // fill the first of the two buffers before starting the stream.
  HRESULT hr = S_FALSE;
  hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                          stream_flags,
                          requested_buffer_duration,
                          requested_buffer_duration,
                          reinterpret_cast<WAVEFORMATEX*>(&format_),
                          NULL);
  if (FAILED(hr)) {
    if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
      LOG(ERROR) << "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED";

      UINT32 aligned_buffer_size = 0;
      client->GetBufferSize(&aligned_buffer_size);
      VLOG(1) << "Use aligned buffer size instead: " << aligned_buffer_size;

      // Calculate new aligned periodicity. Each unit of reference time
      // is 100 nanoseconds.
      REFERENCE_TIME aligned_buffer_duration = static_cast<REFERENCE_TIME>(
          (10000000.0 * aligned_buffer_size / format_.Format.nSamplesPerSec)
          + 0.5);

      // It is possible to re-activate and re-initialize the audio client
      // at this stage but we bail out with an error code instead and
      // combine it with a log message which informs about the suggested
      // aligned buffer size which should be used instead.
      VLOG(1) << "aligned_buffer_duration: "
              << static_cast<double>(aligned_buffer_duration / 10000.0)
              << " [ms]";
    } else if (hr == AUDCLNT_E_INVALID_DEVICE_PERIOD) {
      // We will get this error if we try to use a smaller buffer size than
      // the minimum supported size (usually ~3ms on Windows 7).
      LOG(ERROR) << "AUDCLNT_E_INVALID_DEVICE_PERIOD";
    }
    return hr;
  }

  if (use_event) {
    hr = client->SetEventHandle(event_handle);
    if (FAILED(hr)) {
      VLOG(1) << "IAudioClient::SetEventHandle: " << std::hex << hr;
      return hr;
    }
  }

  UINT32 buffer_size_in_frames = 0;
  hr = client->GetBufferSize(&buffer_size_in_frames);
  if (FAILED(hr)) {
    VLOG(1) << "IAudioClient::GetBufferSize: " << std::hex << hr;
    return hr;
  }

  *endpoint_buffer_size = buffer_size_in_frames;
  VLOG(2) << "endpoint buffer size: " << buffer_size_in_frames;
  return hr;
}

void WASAPIAudioOutputStream::StopThread() {
  if (render_thread_ ) {
    if (render_thread_->HasBeenStarted()) {
      // Wait until the thread completes and perform cleanup.
      SetEvent(stop_render_event_.Get());
      render_thread_->Join();
    }

    render_thread_.reset();

    // Ensure that we don't quit the main thread loop immediately next
    // time Start() is called.
    ResetEvent(stop_render_event_.Get());
  }

  source_ = NULL;
}

}  // namespace media
