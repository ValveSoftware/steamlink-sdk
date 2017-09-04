// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_debug_writer.h"
#include <stdint.h>
#include <array>
#include <utility>
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sys_byteorder.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/audio_bus.h"

namespace content {

namespace {

// Windows WAVE format header
// Byte order: Little-endian
// Offset Length  Content
//  0      4     "RIFF"
//  4      4     <file length - 8>
//  8      4     "WAVE"
// 12      4     "fmt "
// 16      4     <length of the fmt data> (=16)
// 20      2     <WAVE file encoding tag>
// 22      2     <channels>
// 24      4     <sample rate>
// 28      4     <bytes per second> (sample rate * block align)
// 32      2     <block align>  (channels * bits per sample / 8)
// 34      2     <bits per sample>
// 36      4     "data"
// 40      4     <sample data size(n)>
// 44     (n)    <sample data>

// We write 16 bit PCM only.
static const uint16_t kBytesPerSample = 2;

static const uint32_t kWavHeaderSize = 44;
static const uint32_t kFmtChunkSize = 16;
// 4 bytes for ID + 4 bytes for size.
static const uint32_t kChunkHeaderSize = 8;
static const uint16_t kWavFormatPcm = 1;

static const char kRiff[] = {'R', 'I', 'F', 'F'};
static const char kWave[] = {'W', 'A', 'V', 'E'};
static const char kFmt[] = {'f', 'm', 't', ' '};
static const char kData[] = {'d', 'a', 't', 'a'};

typedef std::array<char, kWavHeaderSize> WavHeaderBuffer;

class CharBufferWriter {
 public:
  CharBufferWriter(char* buf, int max_size)
      : buf_(buf), max_size_(max_size), size_(0) {}

  void Write(const char* data, int data_size) {
    CHECK_LE(size_ + data_size, max_size_);
    memcpy(&buf_[size_], data, data_size);
    size_ += data_size;
  }

  void Write(const char(&data)[4]) { Write(static_cast<const char*>(data), 4); }

  void WriteLE16(uint16_t data) {
    uint16_t val = base::ByteSwapToLE16(data);
    Write(reinterpret_cast<const char*>(&val), sizeof(val));
  }

  void WriteLE32(uint32_t data) {
    uint32_t val = base::ByteSwapToLE32(data);
    Write(reinterpret_cast<const char*>(&val), sizeof(val));
  }

 private:
  char* buf_;
  const int max_size_;
  int size_;

  DISALLOW_COPY_AND_ASSIGN(CharBufferWriter);
};

// Writes Wave header to the specified address, there should be at least
// kWavHeaderSize bytes allocated for it.
void WriteWavHeader(WavHeaderBuffer* buf,
                    uint32_t channels,
                    uint32_t sample_rate,
                    uint64_t samples) {
  // We'll need to add (kWavHeaderSize - kChunkHeaderSize) to payload to
  // calculate Riff chunk size.
  static const uint32_t kMaxBytesInPayload =
      std::numeric_limits<uint32_t>::max() -
      (kWavHeaderSize - kChunkHeaderSize);
  const uint64_t bytes_in_payload_64 = samples * kBytesPerSample;

  // In case payload is too large and causes uint32_t overflow, we just specify
  // the maximum possible value; all the payload above that count will be
  // interpreted as garbage.
  const uint32_t bytes_in_payload = bytes_in_payload_64 > kMaxBytesInPayload
                                        ? kMaxBytesInPayload
                                        : bytes_in_payload_64;
  LOG_IF(WARNING, bytes_in_payload < bytes_in_payload_64)
      << "Number of samples is too large and will be clipped by Wave header,"
      << " all the data above " << kMaxBytesInPayload
      << " bytes will appear as junk";
  const uint32_t block_align = channels * kBytesPerSample;
  const uint32_t byte_rate = channels * sample_rate * kBytesPerSample;
  const uint32_t riff_chunk_size =
      bytes_in_payload + kWavHeaderSize - kChunkHeaderSize;

  CharBufferWriter writer(&(*buf)[0], kWavHeaderSize);

  writer.Write(kRiff);
  writer.WriteLE32(riff_chunk_size);
  writer.Write(kWave);
  writer.Write(kFmt);
  writer.WriteLE32(kFmtChunkSize);
  writer.WriteLE16(kWavFormatPcm);
  writer.WriteLE16(channels);
  writer.WriteLE32(sample_rate);
  writer.WriteLE32(byte_rate);
  writer.WriteLE16(block_align);
  writer.WriteLE16(kBytesPerSample * 8);
  writer.Write(kData);
  writer.WriteLE32(bytes_in_payload);
}

}  // namespace

// Manages the debug recording file and writes to it. Can be created on any
// thread. All the operations must be executed on FILE thread. Must be destroyed
// on FILE thread.
class AudioInputDebugWriter::AudioFileWriter {
 public:
  static AudioFileWriterUniquePtr Create(const base::FilePath& file_name,
                                         const media::AudioParameters& params);

  ~AudioFileWriter();

  // Write data from |data| to file.
  void Write(const media::AudioBus* data);

 private:
  AudioFileWriter(const media::AudioParameters& params);

  // Write wave header to file. Called on the FILE thread twice: on construction
  // of AudioFileWriter size of the wave data is unknown, so the header is
  // written with zero sizes; then on destruction it is re-written with the
  // actual size info accumulated throughout the object lifetime.
  void WriteHeader();

  void CreateRecordingFile(const base::FilePath& file_name);

  // The file to write to.
  base::File file_;

  // Number of written samples.
  uint64_t samples_;

  // Input audio parameters required to build wave header.
  const media::AudioParameters params_;

  // Intermediate buffer to be written to file. Interleaved 16 bit audio data.
  std::unique_ptr<int16_t[]> interleaved_data_;
  int interleaved_data_size_;
};

// static
AudioInputDebugWriter::AudioFileWriterUniquePtr
AudioInputDebugWriter::AudioFileWriter::Create(
    const base::FilePath& file_name,
    const media::AudioParameters& params) {
  AudioFileWriterUniquePtr file_writer(new AudioFileWriter(params));

  // base::Unretained is safe, because destructor is called on FILE thread or on
  // FILE message loop destruction.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&AudioFileWriter::CreateRecordingFile,
                 base::Unretained(file_writer.get()), file_name));
  return file_writer;
}

AudioInputDebugWriter::AudioFileWriter::AudioFileWriter(
    const media::AudioParameters& params)
    : samples_(0), params_(params), interleaved_data_size_(0) {
  DCHECK_EQ(params.bits_per_sample(), kBytesPerSample * 8);
}

AudioInputDebugWriter::AudioFileWriter::~AudioFileWriter() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (file_.IsValid())
    WriteHeader();
}

void AudioInputDebugWriter::AudioFileWriter::Write(
    const media::AudioBus* data) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (!file_.IsValid())
    return;

  // Convert to 16 bit audio and write to file.
  int data_size = data->frames() * data->channels();
  if (!interleaved_data_ || interleaved_data_size_ < data_size) {
    interleaved_data_.reset(new int16_t[data_size]);
    interleaved_data_size_ = data_size;
  }
  samples_ += data_size;
  data->ToInterleaved(data->frames(), sizeof(interleaved_data_[0]),
                      interleaved_data_.get());

#ifndef ARCH_CPU_LITTLE_ENDIAN
  static_assert(sizeof(interleaved_data_[0]) == sizeof(uint16_t),
                "Only 2 bytes per channel is supported.");
  for (int i = 0; i < data_size; ++i)
    interleaved_data_[i] = base::ByteSwapToLE16(interleaved_data_[i]);
#endif

  file_.WriteAtCurrentPos(reinterpret_cast<char*>(interleaved_data_.get()),
                          data_size * sizeof(interleaved_data_[0]));
}

void AudioInputDebugWriter::AudioFileWriter::WriteHeader() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  if (!file_.IsValid())
    return;
  WavHeaderBuffer buf;
  WriteWavHeader(&buf, params_.channels(), params_.sample_rate(), samples_);
  file_.Write(0, &buf[0], kWavHeaderSize);

  // Write() does not move the cursor if file is not in APPEND mode; Seek() so
  // that the header is not overwritten by the following writes.
  file_.Seek(base::File::FROM_BEGIN, kWavHeaderSize);
}

void AudioInputDebugWriter::AudioFileWriter::CreateRecordingFile(
    const base::FilePath& file_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  DCHECK(!file_.IsValid());

  file_ = base::File(file_name,
                     base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  if (file_.IsValid()) {
    WriteHeader();
    return;
  }

  // Note that we do not inform AudioInputDebugWriter that the file creation
  // fails, so it will continue to post data to be recorded, which won't
  // be written to the file. This also won't be reflected in WillWrite(). It's
  // fine, because this situation is rare, and all the posting is expected to
  // happen in case of success anyways. This allows us to save on thread hops
  // for error reporting and to avoid dealing with lifetime issues. It also
  // means file_.IsValid() should always be checked before issuing writes to it.
  PLOG(ERROR) << "Could not open debug recording file, error="
              << file_.error_details();
}

AudioInputDebugWriter::AudioInputDebugWriter(
    const media::AudioParameters& params)
    : params_(params) {
  client_sequence_checker_.DetachFromSequence();
}

AudioInputDebugWriter::~AudioInputDebugWriter() {
  // |file_writer_| will be deleted on FILE thread.
}

void AudioInputDebugWriter::Start(const base::FilePath& file_name) {
  DCHECK(client_sequence_checker_.CalledOnValidSequence());
  DCHECK(!file_writer_);
  file_writer_ = AudioFileWriter::Create(file_name, params_);
}

void AudioInputDebugWriter::Stop() {
  DCHECK(client_sequence_checker_.CalledOnValidSequence());
  // |file_writer_| is deleted on FILE thread.
  file_writer_.reset();
  client_sequence_checker_.DetachFromSequence();
}

void AudioInputDebugWriter::Write(std::unique_ptr<media::AudioBus> data) {
  DCHECK(client_sequence_checker_.CalledOnValidSequence());
  if (!file_writer_)
    return;

  // base::Unretained for |file_writer_| is safe, see the destructor.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      // Callback takes ownership of |data|:
      base::Bind(&AudioFileWriter::Write, base::Unretained(file_writer_.get()),
                 base::Owned(data.release())));
}

bool AudioInputDebugWriter::WillWrite() {
  // Note that if this is called from any place other than
  // |client_sequence_checker_| then there is a data race here, but it's fine,
  // because Write() will check for |file_writer_|. So, we are not very precise
  // here, but it's fine: we can afford missing some data or scheduling some
  // no-op writes.
  return !!file_writer_;
}

}  // namspace content
