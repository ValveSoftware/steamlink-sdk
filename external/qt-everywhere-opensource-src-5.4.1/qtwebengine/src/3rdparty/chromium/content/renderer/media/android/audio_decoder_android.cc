// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/audio_decoder_android.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

#include "base/file_descriptor_posix.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/posix/eintr_wrapper.h"
#include "content/common/view_messages.h"
#include "media/base/android/webaudio_media_codec_info.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"
#include "third_party/WebKit/public/platform/WebAudioBus.h"

namespace content {

class AudioDecoderIO {
 public:
  AudioDecoderIO(const char* data, size_t data_size);
  ~AudioDecoderIO();
  bool ShareEncodedToProcess(base::SharedMemoryHandle* handle);

  // Returns true if AudioDecoderIO was successfully created.
  bool IsValid() const;

  int read_fd() const { return read_fd_; }
  int write_fd() const { return write_fd_; }

 private:
  // Shared memory that will hold the encoded audio data.  This is
  // used by MediaCodec for decoding.
  base::SharedMemory encoded_shared_memory_;

  // A pipe used to communicate with MediaCodec.  MediaCodec owns
  // write_fd_ and writes to it.
  int read_fd_;
  int write_fd_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderIO);
};

AudioDecoderIO::AudioDecoderIO(const char* data, size_t data_size)
    : read_fd_(-1),
      write_fd_(-1) {

  if (!data || !data_size || data_size > 0x80000000)
    return;

  // Create the shared memory and copy our data to it so that
  // MediaCodec can access it.
  encoded_shared_memory_.CreateAndMapAnonymous(data_size);

  if (!encoded_shared_memory_.memory())
    return;

  memcpy(encoded_shared_memory_.memory(), data, data_size);

  // Create a pipe for reading/writing the decoded PCM data
  int pipefd[2];

  if (pipe(pipefd))
    return;

  read_fd_ = pipefd[0];
  write_fd_ = pipefd[1];
}

AudioDecoderIO::~AudioDecoderIO() {
  // Close the read end of the pipe.  The write end should have been
  // closed by MediaCodec.
  if (read_fd_ >= 0 && close(read_fd_)) {
    DVLOG(1) << "Cannot close read fd " << read_fd_
             << ": " << strerror(errno);
  }
}

bool AudioDecoderIO::IsValid() const {
  return read_fd_ >= 0 && write_fd_ >= 0 &&
      encoded_shared_memory_.memory();
}

bool AudioDecoderIO::ShareEncodedToProcess(base::SharedMemoryHandle* handle) {
  return encoded_shared_memory_.ShareToProcess(
      base::Process::Current().handle(),
      handle);
}

static float ConvertSampleToFloat(int16_t sample) {
  const float kMaxScale = 1.0f / std::numeric_limits<int16_t>::max();
  const float kMinScale = -1.0f / std::numeric_limits<int16_t>::min();

  return sample * (sample < 0 ? kMinScale : kMaxScale);
}

// A basic WAVE file decoder.  See
// https://ccrma.stanford.edu/courses/422/projects/WaveFormat/ for a
// basic guide to the WAVE file format.
class WAVEDecoder {
 public:
  WAVEDecoder(const uint8* data, size_t data_size);
  ~WAVEDecoder();

  // Try to decode the data as a WAVE file.  If the data is a supported
  // WAVE file, |destination_bus| is filled with the decoded data and
  // DecodeWAVEFile returns true.  Otherwise, DecodeWAVEFile returns
  // false.
  bool DecodeWAVEFile(blink::WebAudioBus* destination_bus);

 private:
  // Minimum number of bytes in a WAVE file to hold all of the data we
  // need to interpret it as a WAVE file.
  static const unsigned kMinimumWAVLength = 44;

  // Number of bytes in the chunk ID field.
  static const unsigned kChunkIDLength = 4;

  // Number of bytes in the chunk size field.
  static const unsigned kChunkSizeLength = 4;

  // Number of bytes in the format field of the "RIFF" chunk.
  static const unsigned kFormatFieldLength = 4;

  // Number of bytes in a valid "fmt" chunk.
  static const unsigned kFMTChunkLength = 16;

  // Supported audio format in a WAVE file.
  // TODO(rtoy): Consider supporting other formats here, if necessary.
  static const int16_t kAudioFormatPCM = 1;

  // Maximum number (inclusive) of bytes per sample supported by this
  // decoder.
  static const unsigned kMaximumBytesPerSample = 3;

  // Read an unsigned integer of |length| bytes from |buffer|.  The
  // integer is interpreted as being in little-endian order.
  uint32_t ReadUnsignedInteger(const uint8_t* buffer, size_t length);

  // Read a PCM sample from the WAVE data at |pcm_data|.
  int16_t ReadPCMSample(const uint8_t* pcm_data);

  // Read a WAVE chunk header including the chunk ID and chunk size.
  // Returns false if the header could not be read.
  bool ReadChunkHeader();

  // Read and parse the "fmt" chunk.  Returns false if the fmt chunk
  // could not be read or contained unsupported formats.
  bool ReadFMTChunk();

  // Read data chunk and save it to |destination_bus|.  Returns false
  // if the data chunk could not be read correctly.
  bool CopyDataChunkToBus(blink::WebAudioBus* destination_bus);

  // The WAVE chunk ID that identifies the chunk.
  uint8_t chunk_id_[kChunkIDLength];

  // The number of bytes in the data portion of the chunk.
  size_t chunk_size_;

  // The total number of bytes in the encoded data.
  size_t data_size_;

  // The current position within the WAVE file.
  const uint8_t* buffer_;

  // Points one byte past the end of the in-memory WAVE file.  Used for
  // detecting if we've reached the end of the file.
  const uint8_t* buffer_end_;

  size_t bytes_per_sample_;

  uint16_t number_of_channels_;

  // Sample rate of the WAVE data, in Hz.
  uint32_t sample_rate_;

  DISALLOW_COPY_AND_ASSIGN(WAVEDecoder);
};

WAVEDecoder::WAVEDecoder(const uint8_t* encoded_data, size_t data_size)
    : data_size_(data_size),
      buffer_(encoded_data),
      buffer_end_(encoded_data + 1),
      bytes_per_sample_(0),
      number_of_channels_(0),
      sample_rate_(0) {
  if (buffer_ + data_size > buffer_)
    buffer_end_ = buffer_ + data_size;
}

WAVEDecoder::~WAVEDecoder() {}

uint32_t WAVEDecoder::ReadUnsignedInteger(const uint8_t* buffer,
                                          size_t length) {
  unsigned value = 0;

  if (length == 0 || length > sizeof(value)) {
    DCHECK(false) << "ReadUnsignedInteger: Invalid length: " << length;
    return 0;
  }

  // All integer fields in a WAVE file are little-endian.
  for (size_t k = length; k > 0; --k)
    value = (value << 8) + buffer[k - 1];

  return value;
}

int16_t WAVEDecoder::ReadPCMSample(const uint8_t* pcm_data) {
  uint32_t unsigned_sample = ReadUnsignedInteger(pcm_data, bytes_per_sample_);
  int16_t sample;

  // Convert the unsigned data into a 16-bit PCM sample.
  switch (bytes_per_sample_) {
    case 1:
      sample = (unsigned_sample - 128) << 8;
      break;
    case 2:
      sample = static_cast<int16_t>(unsigned_sample);
      break;
    case 3:
      // Android currently converts 24-bit WAVE data into 16-bit
      // samples by taking the high-order 16 bits without rounding.
      // We do the same here for consistency.
      sample = static_cast<int16_t>(unsigned_sample >> 8);
      break;
    default:
      sample = 0;
      break;
  }
  return sample;
}

bool WAVEDecoder::ReadChunkHeader() {
  if (buffer_ + kChunkIDLength + kChunkSizeLength >= buffer_end_)
    return false;

  memcpy(chunk_id_, buffer_, kChunkIDLength);

  chunk_size_ = ReadUnsignedInteger(buffer_ + kChunkIDLength, kChunkSizeLength);

  // Adjust for padding
  if (chunk_size_ % 2)
    ++chunk_size_;

  // Check for completely bogus chunk size.
  if (chunk_size_ > data_size_)
    return false;

  return true;
}

bool WAVEDecoder::ReadFMTChunk() {
  // The fmt chunk has basic info about the format of the audio
  // data.  Only a basic PCM format is supported.
  if (chunk_size_ < kFMTChunkLength) {
    DVLOG(1) << "FMT chunk too short: " << chunk_size_;
    return 0;
  }

  uint16_t audio_format = ReadUnsignedInteger(buffer_, 2);

  if (audio_format != kAudioFormatPCM) {
    DVLOG(1) << "Audio format not supported: " << audio_format;
    return false;
  }

  number_of_channels_ = ReadUnsignedInteger(buffer_ + 2, 2);
  sample_rate_ = ReadUnsignedInteger(buffer_ + 4, 4);
  unsigned bits_per_sample = ReadUnsignedInteger(buffer_ + 14, 2);

  // Sanity checks.

  if (!number_of_channels_ ||
      number_of_channels_ > media::limits::kMaxChannels) {
    DVLOG(1) << "Unsupported number of channels: " << number_of_channels_;
    return false;
  }

  if (sample_rate_ < media::limits::kMinSampleRate ||
      sample_rate_ > media::limits::kMaxSampleRate) {
    DVLOG(1) << "Unsupported sample rate: " << sample_rate_;
    return false;
  }

  // We only support 8, 16, and 24 bits per sample.
  if (bits_per_sample == 8 || bits_per_sample == 16 || bits_per_sample == 24) {
    bytes_per_sample_ = bits_per_sample / 8;
    return true;
  }

  DVLOG(1) << "Unsupported bits per sample: " << bits_per_sample;
  return false;
}

bool WAVEDecoder::CopyDataChunkToBus(blink::WebAudioBus* destination_bus) {
  // The data chunk contains the audio data itself.
  if (!bytes_per_sample_ || bytes_per_sample_ > kMaximumBytesPerSample) {
    DVLOG(1) << "WARNING: data chunk without preceeding fmt chunk,"
             << " or invalid bytes per sample.";
    return false;
  }

  VLOG(0) << "Decoding WAVE file: " << number_of_channels_ << " channels, "
          << sample_rate_ << " kHz, "
          << chunk_size_ / bytes_per_sample_ / number_of_channels_
          << " frames, " << 8 * bytes_per_sample_ << " bits/sample";

  // Create the destination bus of the appropriate size and then decode
  // the data into the bus.
  size_t number_of_frames =
      chunk_size_ / bytes_per_sample_ / number_of_channels_;

  destination_bus->initialize(
      number_of_channels_, number_of_frames, sample_rate_);

  for (size_t m = 0; m < number_of_frames; ++m) {
    for (uint16_t k = 0; k < number_of_channels_; ++k) {
      int16_t sample = ReadPCMSample(buffer_);

      buffer_ += bytes_per_sample_;
      destination_bus->channelData(k)[m] = ConvertSampleToFloat(sample);
    }
  }

  return true;
}

bool WAVEDecoder::DecodeWAVEFile(blink::WebAudioBus* destination_bus) {
  // Parse and decode WAVE file. If we can't parse it, return false.

  if (buffer_ + kMinimumWAVLength > buffer_end_) {
    DVLOG(1) << "Buffer too small to contain full WAVE header: ";
    return false;
  }

  // Do we have a RIFF file?
  ReadChunkHeader();
  if (memcmp(chunk_id_, "RIFF", kChunkIDLength) != 0) {
    DVLOG(1) << "RIFF missing";
    return false;
  }
  buffer_ += kChunkIDLength + kChunkSizeLength;

  // Check the format field of the RIFF chunk
  memcpy(chunk_id_, buffer_, kFormatFieldLength);
  if (memcmp(chunk_id_, "WAVE", kFormatFieldLength) != 0) {
    DVLOG(1) << "Invalid WAVE file:  missing WAVE header";
    return false;
  }
  // Advance past the format field
  buffer_ += kFormatFieldLength;

  // We have a WAVE file.  Start parsing the chunks.

  while (buffer_ < buffer_end_) {
    if (!ReadChunkHeader()) {
      DVLOG(1) << "Couldn't read chunk header";
      return false;
    }

    // Consume the chunk ID and chunk size
    buffer_ += kChunkIDLength + kChunkSizeLength;

    // Make sure we can read all chunk_size bytes.
    if (buffer_ + chunk_size_ > buffer_end_) {
      DVLOG(1) << "Insufficient bytes to read chunk of size " << chunk_size_;
      return false;
    }

    if (memcmp(chunk_id_, "fmt ", kChunkIDLength) == 0) {
      if (!ReadFMTChunk())
        return false;
    } else if (memcmp(chunk_id_, "data", kChunkIDLength) == 0) {
      // Return after reading the data chunk, whether we succeeded or
      // not.
      return CopyDataChunkToBus(destination_bus);
    } else {
      // Ignore these chunks that we don't know about.
      DVLOG(0) << "Ignoring WAVE chunk `" << chunk_id_ << "' size "
               << chunk_size_;
    }

    // Advance to next chunk.
    buffer_ += chunk_size_;
  }

  // If we get here, that means we didn't find a data chunk, so we
  // couldn't handle this WAVE file.

  return false;
}

// The number of frames is known so preallocate the destination
// bus and copy the pcm data to the destination bus as it's being
// received.
static void CopyPcmDataToBus(int input_fd,
                             blink::WebAudioBus* destination_bus,
                             size_t number_of_frames,
                             unsigned number_of_channels,
                             double file_sample_rate) {
  destination_bus->initialize(number_of_channels,
                              number_of_frames,
                              file_sample_rate);

  int16_t pipe_data[PIPE_BUF / sizeof(int16_t)];
  size_t decoded_frames = 0;
  size_t current_sample_in_frame = 0;
  ssize_t nread;

  while ((nread = HANDLE_EINTR(read(input_fd, pipe_data, sizeof(pipe_data)))) >
         0) {
    size_t samples_in_pipe = nread / sizeof(int16_t);

    // The pipe may not contain a whole number of frames.  This is
    // especially true if the number of channels is greater than
    // 2. Thus, keep track of which sample in a frame is being
    // processed, so we handle the boundary at the end of the pipe
    // correctly.
    for (size_t m = 0; m < samples_in_pipe; ++m) {
      if (decoded_frames >= number_of_frames)
        break;

      destination_bus->channelData(current_sample_in_frame)[decoded_frames] =
          ConvertSampleToFloat(pipe_data[m]);
      ++current_sample_in_frame;

      if (current_sample_in_frame >= number_of_channels) {
        current_sample_in_frame = 0;
        ++decoded_frames;
      }
    }
  }

  // number_of_frames is only an estimate.  Resize the buffer with the
  // actual number of received frames.
  if (decoded_frames < number_of_frames)
    destination_bus->resizeSmaller(decoded_frames);
}

// The number of frames is unknown, so keep reading and buffering
// until there's no more data and then copy the data to the
// destination bus.
static void BufferAndCopyPcmDataToBus(int input_fd,
                                      blink::WebAudioBus* destination_bus,
                                      unsigned number_of_channels,
                                      double file_sample_rate) {
  int16_t pipe_data[PIPE_BUF / sizeof(int16_t)];
  std::vector<int16_t> decoded_samples;
  ssize_t nread;

  while ((nread = HANDLE_EINTR(read(input_fd, pipe_data, sizeof(pipe_data)))) >
         0) {
    size_t samples_in_pipe = nread / sizeof(int16_t);
    if (decoded_samples.size() + samples_in_pipe > decoded_samples.capacity()) {
      decoded_samples.reserve(std::max(samples_in_pipe,
                                       2 * decoded_samples.capacity()));
    }
    std::copy(pipe_data,
              pipe_data + samples_in_pipe,
              back_inserter(decoded_samples));
  }

  DVLOG(1) << "Total samples read = " << decoded_samples.size();

  // Convert the samples and save them in the audio bus.
  size_t number_of_samples = decoded_samples.size();
  size_t number_of_frames = decoded_samples.size() / number_of_channels;
  size_t decoded_frames = 0;

  destination_bus->initialize(number_of_channels,
                              number_of_frames,
                              file_sample_rate);

  for (size_t m = 0; m < number_of_samples; m += number_of_channels) {
    for (size_t k = 0; k < number_of_channels; ++k) {
      int16_t sample = decoded_samples[m + k];
      destination_bus->channelData(k)[decoded_frames] =
          ConvertSampleToFloat(sample);
    }
    ++decoded_frames;
  }

  // number_of_frames is only an estimate.  Resize the buffer with the
  // actual number of received frames.
  if (decoded_frames < number_of_frames)
    destination_bus->resizeSmaller(decoded_frames);
}

static bool TryWAVEFileDecoder(blink::WebAudioBus* destination_bus,
                               const uint8_t* encoded_data,
                               size_t data_size) {
  WAVEDecoder decoder(encoded_data, data_size);

  return decoder.DecodeWAVEFile(destination_bus);
}

// To decode audio data, we want to use the Android MediaCodec class.
// But this can't run in a sandboxed process so we need initiate the
// request to MediaCodec in the browser.  To do this, we create a
// shared memory buffer that holds the audio data.  We send a message
// to the browser to start the decoder using this buffer and one end
// of a pipe.  The MediaCodec class will decode the data from the
// shared memory and write the PCM samples back to us over a pipe.
bool DecodeAudioFileData(blink::WebAudioBus* destination_bus, const char* data,
                         size_t data_size,
                         scoped_refptr<ThreadSafeSender> sender) {
  // Try to decode the data as a WAVE file first.  If it can't be
  // decoded, use MediaCodec.  See crbug.com/259048.
  if (TryWAVEFileDecoder(
          destination_bus, reinterpret_cast<const uint8_t*>(data), data_size)) {
    return true;
  }

  AudioDecoderIO audio_decoder(data, data_size);

  if (!audio_decoder.IsValid())
    return false;

  base::SharedMemoryHandle encoded_data_handle;
  audio_decoder.ShareEncodedToProcess(&encoded_data_handle);
  base::FileDescriptor fd(audio_decoder.write_fd(), true);

  DVLOG(1) << "DecodeAudioFileData: Starting MediaCodec";

  // Start MediaCodec processing in the browser which will read from
  // encoded_data_handle for our shared memory and write the decoded
  // PCM samples (16-bit integer) to our pipe.

  sender->Send(new ViewHostMsg_RunWebAudioMediaCodec(
      encoded_data_handle, fd, data_size));

  // First, read the number of channels, the sample rate, and the
  // number of frames and a flag indicating if the file is an
  // ogg/vorbis file.  This must be coordinated with
  // WebAudioMediaCodecBridge!
  //
  // If we know the number of samples, we can create the destination
  // bus directly and do the conversion directly to the bus instead of
  // buffering up everything before saving the data to the bus.

  int input_fd = audio_decoder.read_fd();
  struct media::WebAudioMediaCodecInfo info;

  DVLOG(1) << "Reading audio file info from fd " << input_fd;
  ssize_t nread = HANDLE_EINTR(read(input_fd, &info, sizeof(info)));
  DVLOG(1) << "read:  " << nread << " bytes:\n"
           << " 0: number of channels = " << info.channel_count << "\n"
           << " 1: sample rate        = " << info.sample_rate << "\n"
           << " 2: number of frames   = " << info.number_of_frames << "\n";

  if (nread != sizeof(info))
    return false;

  unsigned number_of_channels = info.channel_count;
  double file_sample_rate = static_cast<double>(info.sample_rate);
  size_t number_of_frames = info.number_of_frames;

  // Sanity checks
  if (!number_of_channels ||
      number_of_channels > media::limits::kMaxChannels ||
      file_sample_rate < media::limits::kMinSampleRate ||
      file_sample_rate > media::limits::kMaxSampleRate) {
    return false;
  }

  if (number_of_frames > 0) {
    CopyPcmDataToBus(input_fd,
                     destination_bus,
                     number_of_frames,
                     number_of_channels,
                     file_sample_rate);
  } else {
    BufferAndCopyPcmDataToBus(input_fd,
                              destination_bus,
                              number_of_channels,
                              file_sample_rate);
  }

  return true;
}

}  // namespace content
