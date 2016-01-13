// Copyright 2009 Google Inc.
// Author: James deBoer
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <string>
#include "jsonwriter.h"
#include "google/output_string.h"

namespace open_vcdiff {

JSONCodeTableWriter::JSONCodeTableWriter()
  : output_called_(false) {}
JSONCodeTableWriter::~JSONCodeTableWriter() {}

bool JSONCodeTableWriter::Init(size_t /*dictionary_size*/) {
  output_ = "[";
  target_length_ = 0;
  return true;
}

void JSONCodeTableWriter::Output(OutputStringInterface* out) {
  output_called_ = true;
  out->append(output_.data(), output_.size());
  output_ = "";
  target_length_ = 0;
}

void JSONCodeTableWriter::FinishEncoding(OutputStringInterface* out) {
  if (output_called_) {
    out->append("]", 1);
  }
}

void JSONCodeTableWriter::JSONEscape(const char* data,
                                     size_t size,
                                     string* out) {
  for (size_t i = 0; i < size; ++i) {
    const char c = data[i];
    switch (c) {
      case '"': out->append("\\\"", 2); break;
      case '\\': out->append("\\\\", 2); break;
      case '\b': out->append("\\b", 2); break;
      case '\f': out->append("\\f", 2); break;
      case '\n': out->append("\\n", 2); break;
      case '\r': out->append("\\r", 2); break;
      case '\t': out->append("\\t", 2); break;
      default:
        // encode zero as unicode, also all control codes.
        if (c < 32 || c >= 127) {
          char unicode_code[8] = "";
          snprintf(unicode_code, sizeof(unicode_code), "\\u%04x", c);
          out->append(unicode_code, strlen(unicode_code));
        } else {
          out->push_back(c);
        }
    }
  }
}

void JSONCodeTableWriter::Add(const char* data, size_t size) {
  output_.push_back('\"');
  JSONEscape(data, size, &output_);
  output_.append("\",", 2);
  target_length_ += size;
}

void JSONCodeTableWriter::Copy(int32_t offset, size_t size) {
  std::ostringstream copy_code;
  copy_code << offset << "," << size << ",";
  output_.append(copy_code.str());
  target_length_ += size;
}

void JSONCodeTableWriter::Run(size_t size, unsigned char byte) {
  output_.push_back('\"');
  output_.append(string(size, byte).data(), size);
  output_.append("\",", 2);
  target_length_ += size;
}

size_t JSONCodeTableWriter::target_length() const {
  return target_length_;
}

void JSONCodeTableWriter::WriteHeader(OutputStringInterface *,
                                      VCDiffFormatExtensionFlags) {
  // The JSON format does not need a header.
}

}  // namespace open_vcdiff
