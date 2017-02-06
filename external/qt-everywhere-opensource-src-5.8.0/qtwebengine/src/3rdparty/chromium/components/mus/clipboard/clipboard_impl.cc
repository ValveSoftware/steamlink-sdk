// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/clipboard/clipboard_impl.h"

#include <string.h>
#include <utility>

#include "base/macros.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/string.h"

using mojo::Array;
using mojo::Map;
using mojo::String;

namespace mus {
namespace clipboard {

// ClipboardData contains data copied to the Clipboard for a variety of formats.
// It mostly just provides APIs to cleanly access and manipulate this data.
class ClipboardImpl::ClipboardData {
 public:
  ClipboardData() : sequence_number_(0) {}
  ~ClipboardData() {}

  uint64_t sequence_number() const {
    return sequence_number_;
  }

  Array<String> GetMimeTypes() const {
    Array<String> types(data_types_.size());
    int i = 0;
    for (auto it = data_types_.begin(); it != data_types_.end(); ++it, ++i)
      types[i] = it->first;

    return types;
  }

  void SetData(Map<String, Array<uint8_t>> data) {
    sequence_number_++;
    data_types_ = std::move(data);
  }

  void GetData(const String& mime_type, Array<uint8_t>* data) const {
    auto it = data_types_.find(mime_type);
    if (it != data_types_.end())
      *data = it->second.Clone();
  }

 private:
  uint64_t sequence_number_;
  Map<String, Array<uint8_t>> data_types_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardData);
};

ClipboardImpl::ClipboardImpl() {
  for (int i = 0; i < kNumClipboards; ++i)
    clipboard_state_[i].reset(new ClipboardData);
}

ClipboardImpl::~ClipboardImpl() {
}

void ClipboardImpl::AddBinding(mojom::ClipboardRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ClipboardImpl::GetSequenceNumber(
    Clipboard::Type clipboard_type,
    const GetSequenceNumberCallback& callback) {
  callback.Run(
      clipboard_state_[static_cast<int>(clipboard_type)]->sequence_number());
}

void ClipboardImpl::GetAvailableMimeTypes(
    Clipboard::Type clipboard_type,
    const GetAvailableMimeTypesCallback& callback) {
  int clipboard_num = static_cast<int>(clipboard_type);
  callback.Run(clipboard_state_[clipboard_num]->sequence_number(),
               clipboard_state_[clipboard_num]->GetMimeTypes());
}

void ClipboardImpl::ReadClipboardData(
    Clipboard::Type clipboard_type,
    const String& mime_type,
    const ReadClipboardDataCallback& callback) {
  int clipboard_num = static_cast<int>(clipboard_type);
  Array<uint8_t> mime_data(nullptr);
  uint64_t sequence = clipboard_state_[clipboard_num]->sequence_number();
  clipboard_state_[clipboard_num]->GetData(mime_type, &mime_data);
  callback.Run(sequence, std::move(mime_data));
}

void ClipboardImpl::WriteClipboardData(
    Clipboard::Type clipboard_type,
    Map<String, Array<uint8_t>> data,
    const WriteClipboardDataCallback& callback) {
  int clipboard_num = static_cast<int>(clipboard_type);
  clipboard_state_[clipboard_num]->SetData(std::move(data));
  callback.Run(clipboard_state_[clipboard_num]->sequence_number());
}

}  // namespace clipboard
}  // namespace mus
