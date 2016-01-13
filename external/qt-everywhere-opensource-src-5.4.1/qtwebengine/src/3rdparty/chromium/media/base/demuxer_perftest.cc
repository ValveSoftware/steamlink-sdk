// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "media/base/media.h"
#include "media/base/media_log.h"
#include "media/base/test_data_util.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/file_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace media {

static const int kBenchmarkIterations = 500;

class DemuxerHostImpl : public media::DemuxerHost {
 public:
  DemuxerHostImpl() {}
  virtual ~DemuxerHostImpl() {}

  // DemuxerHost implementation.
  virtual void AddBufferedTimeRange(base::TimeDelta start,
                                    base::TimeDelta end) OVERRIDE {}
  virtual void SetDuration(base::TimeDelta duration) OVERRIDE {}
  virtual void OnDemuxerError(media::PipelineStatus error) OVERRIDE {}
  virtual void AddTextStream(media::DemuxerStream* text_stream,
                             const media::TextTrackConfig& config) OVERRIDE {}
  virtual void RemoveTextStream(media::DemuxerStream* text_stream) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DemuxerHostImpl);
};

static void QuitLoopWithStatus(base::MessageLoop* message_loop,
                        media::PipelineStatus status) {
  CHECK_EQ(status, media::PIPELINE_OK);
  message_loop->PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

static void NeedKey(const std::string& type,
                    const std::vector<uint8>& init_data) {
  VLOG(0) << "File is encrypted.";
}

typedef std::vector<media::DemuxerStream* > Streams;

// Simulates playback reading requirements by reading from each stream
// present in |demuxer| in as-close-to-monotonically-increasing timestamp order.
class StreamReader {
 public:
  StreamReader(media::Demuxer* demuxer, bool enable_bitstream_converter);
  ~StreamReader();

  // Performs a single step read.
  void Read();

  // Returns true when all streams have reached end of stream.
  bool IsDone();

  int number_of_streams() { return static_cast<int>(streams_.size()); }
  const Streams& streams() { return streams_; }
  const std::vector<int>& counts() { return counts_; }

 private:
  void OnReadDone(base::MessageLoop* message_loop,
                  bool* end_of_stream,
                  base::TimeDelta* timestamp,
                  media::DemuxerStream::Status status,
                  const scoped_refptr<media::DecoderBuffer>& buffer);
  int GetNextStreamIndexToRead();

  Streams streams_;
  std::vector<bool> end_of_stream_;
  std::vector<base::TimeDelta> last_read_timestamp_;
  std::vector<int> counts_;

  DISALLOW_COPY_AND_ASSIGN(StreamReader);
};

StreamReader::StreamReader(media::Demuxer* demuxer,
                           bool enable_bitstream_converter) {
  media::DemuxerStream* stream =
      demuxer->GetStream(media::DemuxerStream::AUDIO);
  if (stream) {
    streams_.push_back(stream);
    end_of_stream_.push_back(false);
    last_read_timestamp_.push_back(media::kNoTimestamp());
    counts_.push_back(0);
  }

  stream = demuxer->GetStream(media::DemuxerStream::VIDEO);
  if (stream) {
    streams_.push_back(stream);
    end_of_stream_.push_back(false);
    last_read_timestamp_.push_back(media::kNoTimestamp());
    counts_.push_back(0);

    if (enable_bitstream_converter)
      stream->EnableBitstreamConverter();
  }
}

StreamReader::~StreamReader() {}

void StreamReader::Read() {
  int index = GetNextStreamIndexToRead();
  bool end_of_stream = false;
  base::TimeDelta timestamp;

  streams_[index]->Read(base::Bind(
      &StreamReader::OnReadDone, base::Unretained(this),
      base::MessageLoop::current(), &end_of_stream, &timestamp));
  base::MessageLoop::current()->Run();

  CHECK(end_of_stream || timestamp != media::kNoTimestamp());
  end_of_stream_[index] = end_of_stream;
  last_read_timestamp_[index] = timestamp;
  counts_[index]++;
}

bool StreamReader::IsDone() {
  for (size_t i = 0; i < end_of_stream_.size(); ++i) {
    if (!end_of_stream_[i])
      return false;
  }
  return true;
}

void StreamReader::OnReadDone(
    base::MessageLoop* message_loop,
    bool* end_of_stream,
    base::TimeDelta* timestamp,
    media::DemuxerStream::Status status,
    const scoped_refptr<media::DecoderBuffer>& buffer) {
  CHECK_EQ(status, media::DemuxerStream::kOk);
  CHECK(buffer.get());
  *end_of_stream = buffer->end_of_stream();
  *timestamp = *end_of_stream ? media::kNoTimestamp() : buffer->timestamp();
  message_loop->PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

int StreamReader::GetNextStreamIndexToRead() {
  int index = -1;
  for (int i = 0; i < number_of_streams(); ++i) {
    // Ignore streams at EOS.
    if (end_of_stream_[i])
      continue;

    // Use a stream if it hasn't been read from yet.
    if (last_read_timestamp_[i] == media::kNoTimestamp())
      return i;

    if (index < 0 ||
        last_read_timestamp_[i] < last_read_timestamp_[index]) {
      index = i;
    }
  }
  CHECK_GE(index, 0) << "Couldn't find a stream to read";
  return index;
}

static void RunDemuxerBenchmark(const std::string& filename) {
  base::FilePath file_path(GetTestDataFilePath(filename));
  double total_time = 0.0;
  for (int i = 0; i < kBenchmarkIterations; ++i) {
    // Setup.
    base::MessageLoop message_loop;
    DemuxerHostImpl demuxer_host;
    FileDataSource data_source;
    ASSERT_TRUE(data_source.Initialize(file_path));

    Demuxer::NeedKeyCB need_key_cb = base::Bind(&NeedKey);
    FFmpegDemuxer demuxer(message_loop.message_loop_proxy(),
                          &data_source,
                          need_key_cb,
                          new MediaLog());

    demuxer.Initialize(&demuxer_host,
                       base::Bind(&QuitLoopWithStatus, &message_loop),
                       false);
    message_loop.Run();
    StreamReader stream_reader(&demuxer, false);

    // Benchmark.
    base::TimeTicks start = base::TimeTicks::HighResNow();
    while (!stream_reader.IsDone()) {
      stream_reader.Read();
    }
    base::TimeTicks end = base::TimeTicks::HighResNow();
    total_time += (end - start).InSecondsF();
    demuxer.Stop(base::Bind(
        &QuitLoopWithStatus, &message_loop, PIPELINE_OK));
    message_loop.Run();
  }

  perf_test::PrintResult("demuxer_bench",
                         "",
                         filename,
                         kBenchmarkIterations / total_time,
                         "runs/s",
                         true);
}

TEST(DemuxerPerfTest, Demuxer) {
  RunDemuxerBenchmark("bear.ogv");
  RunDemuxerBenchmark("bear-640x360.webm");
  RunDemuxerBenchmark("sfx_s16le.wav");
#if defined(USE_PROPRIETARY_CODECS)
  RunDemuxerBenchmark("bear-1280x720.mp4");
  RunDemuxerBenchmark("sfx.mp3");
#endif
#if defined(OS_CHROMEOS)
  RunDemuxerBenchmark("bear.flac");
#endif
#if defined(USE_PROPRIETARY_CODECS) && defined(OS_CHROMEOS)
  RunDemuxerBenchmark("bear.avi");
#endif
}

}  // namespace media
