// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(dcheng): For efficiency reasons, consider passing custom data around
// as a vector instead. It allows us to append a
// std::pair<base::string16, base::string16> and swap the deserialized values.

#include "ui/base/clipboard/custom_data_helper.h"

#include <utility>

#include "base/pickle.h"

namespace ui {

namespace {

class SkippablePickle : public Pickle {
 public:
  SkippablePickle(const void* data, size_t data_len);
  bool SkipString16(PickleIterator* iter);
};

SkippablePickle::SkippablePickle(const void* data, size_t data_len)
    : Pickle(reinterpret_cast<const char*>(data), data_len) {
}

bool SkippablePickle::SkipString16(PickleIterator* iter) {
  DCHECK(iter);

  int len;
  if (!ReadLength(iter, &len))
    return false;
  return iter->SkipBytes(len * sizeof(base::char16));
}

}  // namespace

void ReadCustomDataTypes(const void* data,
                         size_t data_length,
                         std::vector<base::string16>* types) {
  SkippablePickle pickle(data, data_length);
  PickleIterator iter(pickle);

  uint64 size = 0;
  if (!pickle.ReadUInt64(&iter, &size))
    return;

  // Keep track of the original elements in the types vector. On failure, we
  // truncate the vector to the original size since we want to ignore corrupt
  // custom data pickles.
  uint64 original_size = types->size();

  for (uint64 i = 0; i < size; ++i) {
    types->push_back(base::string16());
    if (!pickle.ReadString16(&iter, &types->back()) ||
        !pickle.SkipString16(&iter)) {
      types->resize(original_size);
      return;
    }
  }
}

void ReadCustomDataForType(const void* data,
                           size_t data_length,
                           const base::string16& type,
                           base::string16* result) {
  SkippablePickle pickle(data, data_length);
  PickleIterator iter(pickle);

  uint64 size = 0;
  if (!pickle.ReadUInt64(&iter, &size))
    return;

  for (uint64 i = 0; i < size; ++i) {
    base::string16 deserialized_type;
    if (!pickle.ReadString16(&iter, &deserialized_type))
      return;
    if (deserialized_type == type) {
      ignore_result(pickle.ReadString16(&iter, result));
      return;
    }
    if (!pickle.SkipString16(&iter))
      return;
  }
}

void ReadCustomDataIntoMap(const void* data,
                           size_t data_length,
                           std::map<base::string16, base::string16>* result) {
  Pickle pickle(reinterpret_cast<const char*>(data), data_length);
  PickleIterator iter(pickle);

  uint64 size = 0;
  if (!pickle.ReadUInt64(&iter, &size))
    return;

  for (uint64 i = 0; i < size; ++i) {
    base::string16 type;
    if (!pickle.ReadString16(&iter, &type)) {
      // Data is corrupt, return an empty map.
      result->clear();
      return;
    }
    std::pair<std::map<base::string16, base::string16>::iterator, bool>
        insert_result = result->insert(std::make_pair(type, base::string16()));
    if (!pickle.ReadString16(&iter, &insert_result.first->second)) {
      // Data is corrupt, return an empty map.
      result->clear();
      return;
    }
  }
}

void WriteCustomDataToPickle(
    const std::map<base::string16, base::string16>& data,
    Pickle* pickle) {
  pickle->WriteUInt64(data.size());
  for (std::map<base::string16, base::string16>::const_iterator it =
           data.begin();
       it != data.end();
       ++it) {
    pickle->WriteString16(it->first);
    pickle->WriteString16(it->second);
  }
}

}  // namespace ui
