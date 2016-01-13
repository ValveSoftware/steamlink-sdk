// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/resource_message_params.h"

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

ResourceMessageParams::SerializedHandles::SerializedHandles()
    : should_close_(false) {
}

ResourceMessageParams::SerializedHandles::~SerializedHandles() {
  if (should_close_) {
    for (std::vector<SerializedHandle>::iterator iter = data_.begin();
         iter != data_.end(); ++iter) {
      iter->Close();
    }
  }
}

ResourceMessageParams::ResourceMessageParams()
    : pp_resource_(0),
      sequence_(0),
      handles_(new SerializedHandles()) {
}

ResourceMessageParams::ResourceMessageParams(PP_Resource resource,
                                             int32_t sequence)
    : pp_resource_(resource),
      sequence_(sequence),
      handles_(new SerializedHandles()) {
}

ResourceMessageParams::~ResourceMessageParams() {
}

void ResourceMessageParams::Serialize(IPC::Message* msg) const {
  WriteHeader(msg);
  WriteHandles(msg);
}

bool ResourceMessageParams::Deserialize(const IPC::Message* msg,
                                        PickleIterator* iter) {
  return ReadHeader(msg, iter) && ReadHandles(msg, iter);
}

void ResourceMessageParams::WriteHeader(IPC::Message* msg) const {
  IPC::ParamTraits<PP_Resource>::Write(msg, pp_resource_);
  IPC::ParamTraits<int32_t>::Write(msg, sequence_);
}

void ResourceMessageParams::WriteHandles(IPC::Message* msg) const {
  IPC::ParamTraits<std::vector<SerializedHandle> >::Write(msg,
                                                          handles_->data());
}

bool ResourceMessageParams::ReadHeader(const IPC::Message* msg,
                                       PickleIterator* iter) {
  DCHECK(handles_->data().empty());
  handles_->set_should_close(true);
  return IPC::ParamTraits<PP_Resource>::Read(msg, iter, &pp_resource_) &&
         IPC::ParamTraits<int32_t>::Read(msg, iter, &sequence_);
}

bool ResourceMessageParams::ReadHandles(const IPC::Message* msg,
                                        PickleIterator* iter) {
  return IPC::ParamTraits<std::vector<SerializedHandle> >::Read(
             msg, iter, &handles_->data());
}

void ResourceMessageParams::ConsumeHandles() const {
  // Note: we must not invalidate the handles. This is used for converting
  // handles from the host OS to NaCl, and that conversion will not work if we
  // invalidate the handles (see HandleConverter).
  handles_->set_should_close(false);
}

SerializedHandle ResourceMessageParams::TakeHandleOfTypeAtIndex(
    size_t index,
    SerializedHandle::Type type) const {
  SerializedHandle handle;
  std::vector<SerializedHandle>& data = handles_->data();
  if (index < data.size() && data[index].type() == type) {
    handle = data[index];
    data[index] = SerializedHandle();
  }
  return handle;
}

bool ResourceMessageParams::TakeSharedMemoryHandleAtIndex(
    size_t index,
    base::SharedMemoryHandle* handle) const {
  SerializedHandle serialized = TakeHandleOfTypeAtIndex(
      index, SerializedHandle::SHARED_MEMORY);
  if (!serialized.is_shmem())
    return false;
  *handle = serialized.shmem();
  return true;
}

bool ResourceMessageParams::TakeSocketHandleAtIndex(
    size_t index,
    IPC::PlatformFileForTransit* handle) const {
  SerializedHandle serialized = TakeHandleOfTypeAtIndex(
      index, SerializedHandle::SOCKET);
  if (!serialized.is_socket())
    return false;
  *handle = serialized.descriptor();
  return true;
}

bool ResourceMessageParams::TakeFileHandleAtIndex(
    size_t index,
    IPC::PlatformFileForTransit* handle) const {
  SerializedHandle serialized = TakeHandleOfTypeAtIndex(
      index, SerializedHandle::FILE);
  if (!serialized.is_file())
    return false;
  *handle = serialized.descriptor();
  return true;
}

void ResourceMessageParams::TakeAllSharedMemoryHandles(
    std::vector<base::SharedMemoryHandle>* handles) const {
  for (size_t i = 0; i < handles_->data().size(); ++i) {
    base::SharedMemoryHandle handle;
    if (TakeSharedMemoryHandleAtIndex(i, &handle))
      handles->push_back(handle);
  }
}

void ResourceMessageParams::AppendHandle(const SerializedHandle& handle) const {
  handles_->data().push_back(handle);
}

ResourceMessageCallParams::ResourceMessageCallParams()
    : ResourceMessageParams(),
      has_callback_(0) {
}

ResourceMessageCallParams::ResourceMessageCallParams(PP_Resource resource,
                                                     int32_t sequence)
    : ResourceMessageParams(resource, sequence),
      has_callback_(0) {
}

ResourceMessageCallParams::~ResourceMessageCallParams() {
}

void ResourceMessageCallParams::Serialize(IPC::Message* msg) const {
  ResourceMessageParams::Serialize(msg);
  IPC::ParamTraits<bool>::Write(msg, has_callback_);
}

bool ResourceMessageCallParams::Deserialize(const IPC::Message* msg,
                                            PickleIterator* iter) {
  if (!ResourceMessageParams::Deserialize(msg, iter))
    return false;
  return IPC::ParamTraits<bool>::Read(msg, iter, &has_callback_);
}

ResourceMessageReplyParams::ResourceMessageReplyParams()
    : ResourceMessageParams(),
      result_(PP_OK) {
}

ResourceMessageReplyParams::ResourceMessageReplyParams(PP_Resource resource,
                                                       int32_t sequence)
    : ResourceMessageParams(resource, sequence),
      result_(PP_OK) {
}

ResourceMessageReplyParams::~ResourceMessageReplyParams() {
}

void ResourceMessageReplyParams::Serialize(IPC::Message* msg) const {
  // Rather than serialize all of ResourceMessageParams first, we serialize all
  // non-handle data first, then the handles. When transferring to NaCl on
  // Windows, we need to be able to translate Windows-style handles to POSIX-
  // style handles, and it's easier to put all the regular stuff at the front.
  WriteReplyHeader(msg);
  WriteHandles(msg);
}

bool ResourceMessageReplyParams::Deserialize(const IPC::Message* msg,
                                             PickleIterator* iter) {
  return (ReadHeader(msg, iter) &&
          IPC::ParamTraits<int32_t>::Read(msg, iter, &result_) &&
          ReadHandles(msg, iter));
}

void ResourceMessageReplyParams::WriteReplyHeader(IPC::Message* msg) const {
  WriteHeader(msg);
  IPC::ParamTraits<int32_t>::Write(msg, result_);
}

}  // namespace proxy
}  // namespace ppapi
