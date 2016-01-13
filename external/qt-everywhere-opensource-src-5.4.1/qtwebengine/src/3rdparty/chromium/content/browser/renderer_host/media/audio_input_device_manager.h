// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioInputDeviceManager manages the audio input devices. In particular it
// communicates with MediaStreamManager and AudioInputRendererHost on the
// browser IO thread, handles queries like
// enumerate/open/close/GetOpenedDeviceInfoById from MediaStreamManager and
// GetOpenedDeviceInfoById from AudioInputRendererHost.
// The work for enumerate/open/close is handled asynchronously on Media Stream
// device thread, while GetOpenedDeviceInfoById is synchronous on the IO thread.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/common/media_stream_request.h"
#include "media/audio/audio_device_name.h"

namespace media {
class AudioManager;
}

namespace content {

class CONTENT_EXPORT AudioInputDeviceManager : public MediaStreamProvider {
 public:
  // Calling Start() with this kFakeOpenSessionId will open the default device,
  // even though Open() has not been called. This is used to be able to use the
  // AudioInputDeviceManager before MediaStream is implemented.
  // TODO(xians): Remove it when the webrtc unittest does not need it any more.
  static const int kFakeOpenSessionId;

  explicit AudioInputDeviceManager(media::AudioManager* audio_manager);

  // Gets the opened device info by |session_id|. Returns NULL if the device
  // is not opened, otherwise the opened device. Called on IO thread.
  const StreamDeviceInfo* GetOpenedDeviceInfoById(int session_id);

  // MediaStreamProvider implementation, called on IO thread.
  virtual void Register(MediaStreamProviderListener* listener,
                        const scoped_refptr<base::SingleThreadTaskRunner>&
                            device_task_runner) OVERRIDE;
  virtual void Unregister() OVERRIDE;
  virtual void EnumerateDevices(MediaStreamType stream_type) OVERRIDE;
  virtual int Open(const StreamDeviceInfo& device) OVERRIDE;
  virtual void Close(int session_id) OVERRIDE;

  void UseFakeDevice();
  bool ShouldUseFakeDevice() const;

 private:
  // Used by the unittests to get a list of fake devices.
  friend class MediaStreamDispatcherHostTest;
  void GetFakeDeviceNames(media::AudioDeviceNames* device_names);

  typedef std::vector<StreamDeviceInfo> StreamDeviceList;
  virtual ~AudioInputDeviceManager();

  // Enumerates audio input devices on media stream device thread.
  void EnumerateOnDeviceThread(MediaStreamType stream_type);
  // Opens the device on media stream device thread.
  void OpenOnDeviceThread(int session_id, const StreamDeviceInfo& info);

  // Callback used by EnumerateOnDeviceThread(), called with a list of
  // enumerated devices on IO thread.
  void DevicesEnumeratedOnIOThread(MediaStreamType stream_type,
                                   scoped_ptr<StreamDeviceInfoArray> devices);
  // Callback used by OpenOnDeviceThread(), called with the session_id
  // referencing the opened device on IO thread.
  void OpenedOnIOThread(int session_id, const StreamDeviceInfo& info);
  // Callback used by CloseOnDeviceThread(), called with the session_id
  // referencing the closed device on IO thread.
  void ClosedOnIOThread(MediaStreamType type, int session_id);

  // Verifies that the calling thread is media stream device thread.
  bool IsOnDeviceThread() const;

  // Helper to return iterator to the device referenced by |session_id|. If no
  // device is found, it will return devices_.end().
  StreamDeviceList::iterator GetDevice(int session_id);

  // Only accessed on Browser::IO thread.
  MediaStreamProviderListener* listener_;
  int next_capture_session_id_;
  bool use_fake_device_;
  StreamDeviceList devices_;

  media::AudioManager* const audio_manager_;  // Weak.

  // The message loop of media stream device thread that this object runs on.
  scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputDeviceManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_
