// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/strings/pattern.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/zlib/zlib.h"

namespace content {

namespace {

const char kChromeTraceLabel[] = "traceEvents";
const char kMetadataTraceLabel[] = "metadata";

class StringTraceDataEndpoint : public TracingController::TraceDataEndpoint {
 public:
  typedef base::Callback<void(std::unique_ptr<const base::DictionaryValue>,
                              base::RefCountedString*)>
      CompletionCallback;

  explicit StringTraceDataEndpoint(CompletionCallback callback)
      : completion_callback_(callback) {}

  void ReceiveTraceFinalContents(
      std::unique_ptr<const base::DictionaryValue> metadata,
      const std::string& contents) override {
    std::string tmp = contents;
    scoped_refptr<base::RefCountedString> str =
        base::RefCountedString::TakeString(&tmp);

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(completion_callback_, base::Passed(std::move(metadata)),
                   base::RetainedRef(str)));
  }

 private:
  ~StringTraceDataEndpoint() override {}

  CompletionCallback completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(StringTraceDataEndpoint);
};

class FileTraceDataEndpoint : public TracingController::TraceDataEndpoint {
 public:
  explicit FileTraceDataEndpoint(const base::FilePath& trace_file_path,
                                 const base::Closure& callback)
      : file_path_(trace_file_path),
        completion_callback_(callback),
        file_(NULL) {}

  void ReceiveTraceChunk(const std::string& chunk) override {
    std::string tmp = chunk;
    scoped_refptr<base::RefCountedString> chunk_ptr =
        base::RefCountedString::TakeString(&tmp);
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&FileTraceDataEndpoint::ReceiveTraceChunkOnFileThread, this,
                   chunk_ptr));
  }

  void ReceiveTraceFinalContents(std::unique_ptr<const base::DictionaryValue>,
                                 const std::string& contents) override {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&FileTraceDataEndpoint::CloseOnFileThread, this));
  }

 private:
  ~FileTraceDataEndpoint() override { DCHECK(file_ == NULL); }

  void ReceiveTraceChunkOnFileThread(
      const scoped_refptr<base::RefCountedString> chunk) {
    if (!OpenFileIfNeededOnFileThread())
      return;
    ignore_result(
        fwrite(chunk->data().c_str(), chunk->data().size(), 1, file_));
  }

  bool OpenFileIfNeededOnFileThread() {
    if (file_ != NULL)
      return true;
    file_ = base::OpenFile(file_path_, "w");
    if (file_ == NULL) {
      LOG(ERROR) << "Failed to open " << file_path_.value();
      return false;
    }
    return true;
  }

  void CloseOnFileThread() {
    if (OpenFileIfNeededOnFileThread()) {
      base::CloseFile(file_);
      file_ = NULL;
    }
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&FileTraceDataEndpoint::FinalizeOnUIThread, this));
  }

  void FinalizeOnUIThread() { completion_callback_.Run(); }

  base::FilePath file_path_;
  base::Closure completion_callback_;
  FILE* file_;

  DISALLOW_COPY_AND_ASSIGN(FileTraceDataEndpoint);
};

class StringTraceDataSink : public TracingController::TraceDataSink {
 public:
  explicit StringTraceDataSink(
      scoped_refptr<TracingController::TraceDataEndpoint> endpoint)
      : endpoint_(endpoint) {}

  void AddTraceChunk(const std::string& chunk) override {
    std::string trace_string;
    if (trace_.empty())
      trace_string = "{\"" + std::string(kChromeTraceLabel) + "\":[";
    else
      trace_string = ",";
    trace_string += chunk;

    AddTraceChunkAndPassToEndpoint(trace_string);
  }

  void AddTraceChunkAndPassToEndpoint(const std::string& chunk) {
    trace_ += chunk;

    endpoint_->ReceiveTraceChunk(chunk);
  }

  void Close() override {
    AddTraceChunkAndPassToEndpoint("]");

    for (auto const &it : GetAgentTrace())
      AddTraceChunkAndPassToEndpoint(",\"" + it.first + "\": " + it.second);

    std::string metadataJSON;
    if (base::JSONWriter::Write(*GetMetadataCopy(), &metadataJSON) &&
        !metadataJSON.empty()) {
      AddTraceChunkAndPassToEndpoint(
          ",\"" + std::string(kMetadataTraceLabel) + "\": " + metadataJSON);
    }

    AddTraceChunkAndPassToEndpoint("}");

    endpoint_->ReceiveTraceFinalContents(GetMetadataCopy(), trace_);
  }

 private:
  ~StringTraceDataSink() override {}

  scoped_refptr<TracingController::TraceDataEndpoint> endpoint_;
  std::string trace_;

  DISALLOW_COPY_AND_ASSIGN(StringTraceDataSink);
};

class CompressedStringTraceDataSink : public TracingController::TraceDataSink {
 public:
  explicit CompressedStringTraceDataSink(
      scoped_refptr<TracingController::TraceDataEndpoint> endpoint)
      : endpoint_(endpoint), already_tried_open_(false) {}

  void AddTraceChunk(const std::string& chunk) override {
    std::string tmp = chunk;
    scoped_refptr<base::RefCountedString> chunk_ptr =
        base::RefCountedString::TakeString(&tmp);
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&CompressedStringTraceDataSink::AddTraceChunkOnFileThread,
                   this, chunk_ptr));
  }

  void Close() override {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&CompressedStringTraceDataSink::CloseOnFileThread, this));
  }

 private:
  ~CompressedStringTraceDataSink() override {}

  bool OpenZStreamOnFileThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    if (stream_)
      return true;

    if (already_tried_open_)
      return false;

    already_tried_open_ = true;
    stream_.reset(new z_stream);
    *stream_ = {0};
    stream_->zalloc = Z_NULL;
    stream_->zfree = Z_NULL;
    stream_->opaque = Z_NULL;

    int result = deflateInit2(stream_.get(), Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                              // 16 is added to produce a gzip header + trailer.
                              MAX_WBITS + 16,
                              8,  // memLevel = 8 is default.
                              Z_DEFAULT_STRATEGY);
    return result == 0;
  }

  void AddTraceChunkOnFileThread(
      const scoped_refptr<base::RefCountedString> chunk_ptr) {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    std::string trace;
    if (compressed_trace_data_.empty())
      trace = "{\"" + std::string(kChromeTraceLabel) + "\":[";
    else
      trace = ",";
    trace += chunk_ptr->data();
    AddTraceChunkAndCompressOnFileThread(trace, false);
  }

  void AddTraceChunkAndCompressOnFileThread(const std::string& chunk,
                                            bool finished) {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    if (!OpenZStreamOnFileThread())
      return;

    const int kChunkSize = 0x4000;

    char buffer[kChunkSize];
    int err;
    stream_->avail_in = chunk.size();
    stream_->next_in = (unsigned char*)chunk.data();
    do {
      stream_->avail_out = kChunkSize;
      stream_->next_out = (unsigned char*)buffer;
      err = deflate(stream_.get(), finished ? Z_FINISH : Z_NO_FLUSH);
      if (err != Z_OK && (err != Z_STREAM_END && finished)) {
        stream_.reset();
        return;
      }

      int bytes = kChunkSize - stream_->avail_out;
      if (bytes) {
        std::string compressed_chunk = std::string(buffer, bytes);
        compressed_trace_data_ += compressed_chunk;
        endpoint_->ReceiveTraceChunk(compressed_chunk);
      }
    } while (stream_->avail_out == 0);
  }

  void CloseOnFileThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    if (!OpenZStreamOnFileThread())
      return;

    if (compressed_trace_data_.empty()) {
      AddTraceChunkAndCompressOnFileThread(
          "{\"" + std::string(kChromeTraceLabel) + "\":[", false);
    }
    AddTraceChunkAndCompressOnFileThread("]", false);

    for (auto const &it : GetAgentTrace()) {
      AddTraceChunkAndCompressOnFileThread(
          ",\"" + it.first + "\": " + it.second, false);
    }

    std::string metadataJSON;
    if (base::JSONWriter::Write(*GetMetadataCopy(), &metadataJSON) &&
        !metadataJSON.empty()) {
      AddTraceChunkAndCompressOnFileThread(
          ",\"" + std::string(kMetadataTraceLabel) + "\": " + metadataJSON,
          false);
    }
    AddTraceChunkAndCompressOnFileThread("}", true);

    deflateEnd(stream_.get());
    stream_.reset();

    endpoint_->ReceiveTraceFinalContents(GetMetadataCopy(),
                                         compressed_trace_data_);
  }

  scoped_refptr<TracingController::TraceDataEndpoint> endpoint_;
  std::unique_ptr<z_stream> stream_;
  bool already_tried_open_;
  std::string compressed_trace_data_;

  DISALLOW_COPY_AND_ASSIGN(CompressedStringTraceDataSink);
};

}  // namespace

TracingController::TraceDataSink::TraceDataSink() {}

TracingController::TraceDataSink::~TraceDataSink() {}

void TracingController::TraceDataSink::AddAgentTrace(
    const std::string& trace_label,
    const std::string& trace_data) {
  DCHECK(additional_tracing_agent_trace_.find(trace_label) ==
         additional_tracing_agent_trace_.end());
  additional_tracing_agent_trace_[trace_label] = trace_data;
}

const std::map<std::string, std::string>&
    TracingController::TraceDataSink::GetAgentTrace() const {
  return additional_tracing_agent_trace_;
}

void TracingController::TraceDataSink::AddMetadata(
    const base::DictionaryValue& data) {
  metadata_.MergeDictionary(&data);
}

void TracingController::TraceDataSink::SetMetadataFilterPredicate(
    const MetadataFilterPredicate& metadata_filter_predicate) {
  metadata_filter_predicate_ = metadata_filter_predicate;
}

std::unique_ptr<const base::DictionaryValue>
TracingController::TraceDataSink::GetMetadataCopy() const {
  if (metadata_filter_predicate_.is_null())
    return std::unique_ptr<const base::DictionaryValue>(metadata_.DeepCopy());

  std::unique_ptr<base::DictionaryValue> metadata_copy(
      new base::DictionaryValue);
  for (base::DictionaryValue::Iterator it(metadata_); !it.IsAtEnd();
       it.Advance()) {
    if (metadata_filter_predicate_.Run(it.key()))
      metadata_copy->Set(it.key(), it.value().DeepCopy());
    else
      metadata_copy->SetString(it.key(), "__stripped__");
  }
  return std::move(metadata_copy);
}

scoped_refptr<TracingController::TraceDataSink>
TracingController::CreateStringSink(
    const base::Callback<void(std::unique_ptr<const base::DictionaryValue>,
                              base::RefCountedString*)>& callback) {
  return new StringTraceDataSink(new StringTraceDataEndpoint(callback));
}

scoped_refptr<TracingController::TraceDataSink>
TracingController::CreateCompressedStringSink(
    scoped_refptr<TracingController::TraceDataEndpoint> endpoint) {
  return new CompressedStringTraceDataSink(endpoint);
}

scoped_refptr<TracingController::TraceDataSink>
TracingController::CreateFileSink(const base::FilePath& file_path,
                                  const base::Closure& callback) {
  return new StringTraceDataSink(
      CreateFileEndpoint(file_path, callback));
}

scoped_refptr<TracingController::TraceDataEndpoint>
TracingController::CreateCallbackEndpoint(
    const base::Callback<void(std::unique_ptr<const base::DictionaryValue>,
                              base::RefCountedString*)>& callback) {
  return new StringTraceDataEndpoint(callback);
}

scoped_refptr<TracingController::TraceDataEndpoint>
TracingController::CreateFileEndpoint(const base::FilePath& file_path,
                                      const base::Closure& callback) {
  return new FileTraceDataEndpoint(file_path, callback);
}

}  // namespace content
