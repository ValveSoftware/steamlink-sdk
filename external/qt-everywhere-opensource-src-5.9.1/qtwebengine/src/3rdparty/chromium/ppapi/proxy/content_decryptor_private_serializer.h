// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_CONTENT_DECRYPTOR_PRIVATE_SERIALIZER_H_
#define PPAPI_PROXY_CONTENT_DECRYPTOR_PRIVATE_SERIALIZER_H_

#include <cstring>
#include <string>

#include "ppapi/c/private/pp_content_decryptor.h"

namespace ppapi {
namespace proxy {

// Serialization/deserialization utility functions for storing/extracting
// PP_DecryptedBlockInfo, PP_EncryptedBlockInfo, and PP_DecompressedFrameInfo
// structs within std::string's for passing through IPC. Both functions return
// true upon success, and false upon failure.
//
// Note, these functions check the size of |block_info| against the size of
// the "serialized" data stored within |serialized_block_info|, and will report
// failure if expectations are not met. Use of CHECK/DCHECK has been avoided
// because the functions are intended for use on both sides of the IPC proxy.

template <typename T>
bool SerializeBlockInfo(const T& block_info,
                        std::string* serialized_block_info) {
  if (!serialized_block_info)
    return false;

  serialized_block_info->assign(reinterpret_cast<const char*>(&block_info),
                                sizeof(block_info));

  if (serialized_block_info->size() != sizeof(block_info))
    return false;

  return true;
}

template <typename T>
bool DeserializeBlockInfo(const std::string& serialized_block_info,
                          T* block_info) {
  if (!block_info)
    return false;

  if (serialized_block_info.size() != sizeof(*block_info))
    return false;

  std::memcpy(block_info, serialized_block_info.data(), sizeof(*block_info));
  return true;
}

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_CONTENT_DECRYPTOR_PRIVATE_SERIALIZER_H_
