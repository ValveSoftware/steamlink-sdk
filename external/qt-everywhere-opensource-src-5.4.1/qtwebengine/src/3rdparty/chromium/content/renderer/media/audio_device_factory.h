// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace media {
class AudioInputDevice;
class AudioOutputDevice;
}

namespace content {

// A factory for creating AudioOutputDevices and AudioInputDevices.  There is a
// global factory function that can be installed for the purposes of testing to
// provide specialized implementations.
class CONTENT_EXPORT AudioDeviceFactory {
 public:
  // Creates an AudioOutputDevice using the currently registered factory.
  // |render_view_id| and |render_frame_id| refer to the render view and render
   // frame containing the entity producing the audio.
  static scoped_refptr<media::AudioOutputDevice> NewOutputDevice(
      int render_view_id, int render_frame_id);

  // Creates an AudioInputDevice using the currently registered factory.
  // |render_view_id| refers to the render view containing the entity consuming
  // the audio.
  static scoped_refptr<media::AudioInputDevice> NewInputDevice(
      int render_view_id);

 protected:
  AudioDeviceFactory();
  virtual ~AudioDeviceFactory();

  // You can derive from this class and specify an implementation for these
  // functions to provide alternate audio device implementations.
  // If the return value of either of these function is NULL, we fall back
  // on the default implementation.
  virtual media::AudioOutputDevice* CreateOutputDevice(int render_view_id) = 0;
  virtual media::AudioInputDevice* CreateInputDevice(int render_view_id) = 0;

 private:
  // The current globally registered factory. This is NULL when we should
  // create the default AudioRendererSinks.
  static AudioDeviceFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_DEVICE_FACTORY_H_
