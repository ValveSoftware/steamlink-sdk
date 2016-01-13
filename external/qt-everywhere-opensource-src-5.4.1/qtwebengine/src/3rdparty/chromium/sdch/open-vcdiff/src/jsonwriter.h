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
//
// A class for a code table writer which outputs JSON.

#ifndef OPEN_VCDIFF_JSONWRITER_H_
#define OPEN_VCDIFF_JSONWRITER_H_

#include <config.h>
#include <string>
#include "addrcache.h"
#include "checksum.h"
#include "codetable.h"
#include "codetablewriter_interface.h"

namespace open_vcdiff {

// class JSONCodeTableWriter:
//
// A code table writer which outputs a JSON representation of the diff.
// The output is a JSON array of commands.
// * Each ADD is represented by a single JSON string containing
//   the data to add.
// * Each COPY is represented by two numbers. The first is an offset into
//   the dictionary.  The second is a length.
// * Each RUN is represented by a JSON string containing the data to add,
//   similar to the ADD command.
//
class JSONCodeTableWriter : public CodeTableWriterInterface {
 public:
  JSONCodeTableWriter();
  ~JSONCodeTableWriter();

  // Initializes the writer.
  virtual bool Init(size_t dictionary_size);

  // Encode an ADD opcode with the "size" bytes starting at data
  virtual void Add(const char* data, size_t size);

  // Encode a COPY opcode with args "offset" (into dictionary) and "size" bytes.
  virtual void Copy(int32_t offset, size_t size);

  // Encode a RUN opcode for "size" copies of the value "byte".
  virtual void Run(size_t size, unsigned char byte);

  // Writes the header to the output string.
  virtual void WriteHeader(OutputStringInterface* out,
                           VCDiffFormatExtensionFlags format_extensions);

  virtual void AddChecksum(VCDChecksum) { }

  // Appends the encoded delta window to the output
  // string.  The output string is not null-terminated.
  virtual void Output(OutputStringInterface* out);

  // Finishes the encoding.
  virtual void FinishEncoding(OutputStringInterface *out);

  // Returns the number of target bytes processed, which is the sum of all the
  // size arguments passed to Add(), Copy(), and Run().
  // TODO(ajenjo): Eliminate the need for this method.
  virtual size_t target_length() const;
 private:
  typedef std::string string;

  // Escape the input data to conform with the JSON string spec
  // and add it to the 'out' string.
  void JSONEscape(const char* data, size_t size, string* out);

  // Stores the JSON data before it is sent to the OutputString.
  string output_;

  // The sum of all the size arguments passed to Add(), Copy() and Run().
  size_t target_length_;

  // Set if some data has been output.
  bool output_called_;
};

}  // namespace open_vcdiff
#endif  // OPEN_VCDIFF_JSONWRITER_H_
