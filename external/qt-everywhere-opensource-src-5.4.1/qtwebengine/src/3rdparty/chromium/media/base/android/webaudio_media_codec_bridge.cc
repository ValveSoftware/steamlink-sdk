// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/webaudio_media_codec_bridge.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "jni/WebAudioMediaCodecBridge_jni.h"
#include "media/base/android/webaudio_media_codec_info.h"


using base::android::AttachCurrentThread;

namespace media {

void WebAudioMediaCodecBridge::RunWebAudioMediaCodec(
    base::SharedMemoryHandle encoded_audio_handle,
    base::FileDescriptor pcm_output,
    uint32_t data_size) {
  WebAudioMediaCodecBridge bridge(encoded_audio_handle, pcm_output, data_size);

  bridge.DecodeInMemoryAudioFile();
}

WebAudioMediaCodecBridge::WebAudioMediaCodecBridge(
    base::SharedMemoryHandle encoded_audio_handle,
    base::FileDescriptor pcm_output,
    uint32_t data_size)
    : encoded_audio_handle_(encoded_audio_handle),
      pcm_output_(pcm_output.fd),
      data_size_(data_size) {
  DVLOG(1) << "WebAudioMediaCodecBridge start **********************"
           << " output fd = " << pcm_output.fd;
}

WebAudioMediaCodecBridge::~WebAudioMediaCodecBridge() {
  if (close(pcm_output_)) {
    DVLOG(1) << "Couldn't close output fd " << pcm_output_
             << ": " << strerror(errno);
  }
}

int WebAudioMediaCodecBridge::SaveEncodedAudioToFile(
    JNIEnv* env,
    jobject context) {
  // Create a temporary file where we can save the encoded audio data.
  std::string temporaryFile =
      base::android::ConvertJavaStringToUTF8(
          env,
          Java_WebAudioMediaCodecBridge_CreateTempFile(env, context).obj());

  // Open the file and unlink it, so that it will be actually removed
  // when we close the file.
  int fd = open(temporaryFile.c_str(), O_RDWR);
  if (unlink(temporaryFile.c_str())) {
    VLOG(0) << "Couldn't unlink temp file " << temporaryFile
            << ": " << strerror(errno);
  }

  if (fd < 0) {
    return -1;
  }

  // Create a local mapping of the shared memory containing the
  // encoded audio data, and save the contents to the temporary file.
  base::SharedMemory encoded_data(encoded_audio_handle_, true);

  if (!encoded_data.Map(data_size_)) {
    VLOG(0) << "Unable to map shared memory!";
    return -1;
  }

  if (static_cast<uint32_t>(write(fd, encoded_data.memory(), data_size_))
      != data_size_) {
    VLOG(0) << "Failed to write all audio data to temp file!";
    return -1;
  }

  lseek(fd, 0, SEEK_SET);

  return fd;
}

bool WebAudioMediaCodecBridge::DecodeInMemoryAudioFile() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);

  jobject context = base::android::GetApplicationContext();

  int sourceFd = SaveEncodedAudioToFile(env, context);

  if (sourceFd < 0)
    return false;

  jboolean decoded = Java_WebAudioMediaCodecBridge_decodeAudioFile(
      env,
      context,
      reinterpret_cast<intptr_t>(this),
      sourceFd,
      data_size_);

  close(sourceFd);

  DVLOG(1) << "decoded = " << (decoded ? "true" : "false");

  return decoded;
}

void WebAudioMediaCodecBridge::InitializeDestination(
    JNIEnv* env,
    jobject /*java object*/,
    jint channel_count,
    jint sample_rate,
    jlong duration_microsec) {
  // Send information about this audio file: number of channels,
  // sample rate (Hz), and the number of frames.
  struct WebAudioMediaCodecInfo info = {
    static_cast<unsigned long>(channel_count),
    static_cast<unsigned long>(sample_rate),
    // The number of frames is the duration of the file
    // (in microseconds) times the sample rate.
    static_cast<unsigned long>(
        0.5 + (duration_microsec * 0.000001 *
               sample_rate))
  };

  DVLOG(1) << "InitializeDestination:"
           << "  channel count = " << channel_count
           << "  rate = " << sample_rate
           << "  duration = " << duration_microsec << " microsec";

  HANDLE_EINTR(write(pcm_output_, &info, sizeof(info)));
}

void WebAudioMediaCodecBridge::OnChunkDecoded(
    JNIEnv* env,
    jobject /*java object*/,
    jobject buf,
    jint buf_size,
    jint input_channel_count,
    jint output_channel_count) {

  if (buf_size <= 0 || !buf)
    return;

  int8_t* buffer =
      static_cast<int8_t*>(env->GetDirectBufferAddress(buf));
  size_t count = static_cast<size_t>(buf_size);
  std::vector<int16_t> decoded_data;

  if (input_channel_count == 1 && output_channel_count == 2) {
    // See crbug.com/266006.  The file has one channel, but the
    // decoder decided to return two channels.  To be consistent with
    // the number of channels in the file, only send one channel (the
    // first).
    int16_t* data = static_cast<int16_t*>(env->GetDirectBufferAddress(buf));
    int frame_count  = buf_size / sizeof(*data) / 2;

    decoded_data.resize(frame_count);
    for (int k = 0; k < frame_count; ++k) {
      decoded_data[k] = *data;
      data += 2;
    }
    buffer = reinterpret_cast<int8_t*>(vector_as_array(&decoded_data));
    DCHECK(buffer);
    count = frame_count * sizeof(*data);
  }

  // Write out the data to the pipe in small chunks if necessary.
  while (count > 0) {
    int bytes_to_write = (count >= PIPE_BUF) ? PIPE_BUF : count;
    ssize_t bytes_written = HANDLE_EINTR(write(pcm_output_,
                                               buffer,
                                               bytes_to_write));
    if (bytes_written == -1)
      break;
    count -= bytes_written;
    buffer += bytes_written;
  }
}

bool WebAudioMediaCodecBridge::RegisterWebAudioMediaCodecBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

} // namespace
