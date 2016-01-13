// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/bindings/js/core.h"

#include "base/bind.h"
#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/function_template.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/public/wrapper_info.h"
#include "gin/wrappable.h"
#include "mojo/bindings/js/handle.h"

namespace mojo {
namespace js {

namespace {

MojoResult CloseHandle(gin::Handle<gin::HandleWrapper> handle) {
  if (!handle->get().is_valid())
    return MOJO_RESULT_INVALID_ARGUMENT;
  handle->Close();
  return MOJO_RESULT_OK;
}

MojoResult WaitHandle(mojo::Handle handle,
                      MojoHandleSignals signals,
                      MojoDeadline deadline) {
  return MojoWait(handle.value(), signals, deadline);
}

MojoResult WaitMany(
    const std::vector<mojo::Handle>& handles,
    const std::vector<MojoHandleSignals>& signals,
    MojoDeadline deadline) {
  return mojo::WaitMany(handles, signals, deadline);
}

gin::Dictionary CreateMessagePipe(const gin::Arguments& args) {
  MojoHandle handle0 = MOJO_HANDLE_INVALID;
  MojoHandle handle1 = MOJO_HANDLE_INVALID;
  // TODO(vtl): Add support for the options struct.
  MojoResult result = MojoCreateMessagePipe(NULL, &handle0, &handle1);
  CHECK(result == MOJO_RESULT_OK);

  gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(args.isolate());
  dictionary.Set("handle0", mojo::Handle(handle0));
  dictionary.Set("handle1", mojo::Handle(handle1));
  return dictionary;
}

MojoResult WriteMessage(
    mojo::Handle handle,
    const gin::ArrayBufferView& buffer,
    const std::vector<gin::Handle<gin::HandleWrapper> >& handles,
    MojoWriteMessageFlags flags) {
  std::vector<MojoHandle> raw_handles(handles.size());
  for (size_t i = 0; i < handles.size(); ++i)
    raw_handles[i] = handles[i]->get().value();
  MojoResult rv = MojoWriteMessage(handle.value(),
                          buffer.bytes(),
                          static_cast<uint32_t>(buffer.num_bytes()),
                          raw_handles.empty() ? NULL : &raw_handles[0],
                          static_cast<uint32_t>(raw_handles.size()),
                          flags);
  // MojoWriteMessage takes ownership of the handles upon success, so
  // release them here.
  if (rv == MOJO_RESULT_OK) {
    for (size_t i = 0; i < handles.size(); ++i)
      ignore_result(handles[i]->release());
  }
  return rv;
}

gin::Dictionary ReadMessage(const gin::Arguments& args,
                            mojo::Handle handle,
                            MojoReadMessageFlags flags) {
  uint32_t num_bytes = 0;
  uint32_t num_handles = 0;
  MojoResult result = MojoReadMessage(
      handle.value(), NULL, &num_bytes, NULL, &num_handles, flags);
  if (result != MOJO_RESULT_RESOURCE_EXHAUSTED) {
    gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(args.isolate());
    dictionary.Set("result", result);
    return dictionary;
  }

  v8::Handle<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(args.isolate(), num_bytes);
  std::vector<mojo::Handle> handles(num_handles);

  gin::ArrayBuffer buffer;
  ConvertFromV8(args.isolate(), array_buffer, &buffer);
  CHECK(buffer.num_bytes() == num_bytes);

  result = MojoReadMessage(handle.value(),
                           buffer.bytes(),
                           &num_bytes,
                           handles.empty() ? NULL :
                               reinterpret_cast<MojoHandle*>(&handles[0]),
                           &num_handles,
                           flags);

  CHECK(buffer.num_bytes() == num_bytes);
  CHECK(handles.size() == num_handles);

  gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(args.isolate());
  dictionary.Set("result", result);
  dictionary.Set("buffer", array_buffer);
  dictionary.Set("handles", handles);
  return dictionary;
}

gin::Dictionary CreateDataPipe(const gin::Arguments& args,
                               v8::Handle<v8::Value> options_value) {
  gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(args.isolate());
  dictionary.Set("result", MOJO_RESULT_INVALID_ARGUMENT);

  MojoHandle producer_handle = MOJO_HANDLE_INVALID;
  MojoHandle consumer_handle = MOJO_HANDLE_INVALID;
  MojoResult result = MOJO_RESULT_OK;

  if (options_value->IsObject()) {
    gin::Dictionary options_dict(args.isolate(), options_value->ToObject());
    MojoCreateDataPipeOptions options;
    // For future struct_size, we can probably infer that from the presence of
    // properties in options_dict. For now, it's always 16.
    options.struct_size = 16;
    // Ideally these would be optional. But the interface makes it hard to
    // typecheck them then.
    if (!options_dict.Get("flags", &options.flags) ||
        !options_dict.Get("elementNumBytes", &options.element_num_bytes) ||
        !options_dict.Get("capacityNumBytes", &options.capacity_num_bytes)) {
      return dictionary;
    }

    result = MojoCreateDataPipe(&options, &producer_handle, &consumer_handle);
  } else if (options_value->IsNull() || options_value->IsUndefined()) {
    result = MojoCreateDataPipe(NULL, &producer_handle, &consumer_handle);
  } else {
    return dictionary;
  }

  CHECK_EQ(MOJO_RESULT_OK, result);

  dictionary.Set("result", result);
  dictionary.Set("producerHandle", mojo::Handle(producer_handle));
  dictionary.Set("consumerHandle", mojo::Handle(consumer_handle));
  return dictionary;
}

gin::Dictionary WriteData(const gin::Arguments& args,
                          mojo::Handle handle,
                          const gin::ArrayBufferView& buffer,
                          MojoWriteDataFlags flags) {
  uint32_t num_bytes = static_cast<uint32_t>(buffer.num_bytes());
  MojoResult result =
      MojoWriteData(handle.value(), buffer.bytes(), &num_bytes, flags);
  gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(args.isolate());
  dictionary.Set("result", result);
  dictionary.Set("numBytes", num_bytes);
  return dictionary;
}

gin::Dictionary ReadData(const gin::Arguments& args,
                         mojo::Handle handle,
                         MojoReadDataFlags flags) {
  uint32_t num_bytes = 0;
  MojoResult result = MojoReadData(
      handle.value(), NULL, &num_bytes, MOJO_READ_DATA_FLAG_QUERY);
  if (result != MOJO_RESULT_OK) {
    gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(args.isolate());
    dictionary.Set("result", result);
    return dictionary;
  }

  v8::Handle<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(args.isolate(), num_bytes);
  gin::ArrayBuffer buffer;
  ConvertFromV8(args.isolate(), array_buffer, &buffer);
  CHECK_EQ(num_bytes, buffer.num_bytes());

  result = MojoReadData(handle.value(), buffer.bytes(), &num_bytes, flags);
  CHECK_EQ(num_bytes, buffer.num_bytes());

  gin::Dictionary dictionary = gin::Dictionary::CreateEmpty(args.isolate());
  dictionary.Set("result", result);
  dictionary.Set("buffer", array_buffer);
  return dictionary;
}

gin::WrapperInfo g_wrapper_info = { gin::kEmbedderNativeGin };

}  // namespace

const char Core::kModuleName[] = "mojo/public/js/bindings/core";

v8::Local<v8::Value> Core::GetModule(v8::Isolate* isolate) {
  gin::PerIsolateData* data = gin::PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(
      &g_wrapper_info);

  if (templ.IsEmpty()) {
     templ = gin::ObjectTemplateBuilder(isolate)
        // TODO(mpcomplete): Should these just be methods on the JS Handle
        // object?
        .SetMethod("close", CloseHandle)
        .SetMethod("wait", WaitHandle)
        .SetMethod("waitMany", WaitMany)
        .SetMethod("createMessagePipe", CreateMessagePipe)
        .SetMethod("writeMessage", WriteMessage)
        .SetMethod("readMessage", ReadMessage)
        .SetMethod("createDataPipe", CreateDataPipe)
        .SetMethod("writeData", WriteData)
        .SetMethod("readData", ReadData)

        .SetValue("RESULT_OK", MOJO_RESULT_OK)
        .SetValue("RESULT_CANCELLED", MOJO_RESULT_CANCELLED)
        .SetValue("RESULT_UNKNOWN", MOJO_RESULT_UNKNOWN)
        .SetValue("RESULT_INVALID_ARGUMENT", MOJO_RESULT_INVALID_ARGUMENT)
        .SetValue("RESULT_DEADLINE_EXCEEDED", MOJO_RESULT_DEADLINE_EXCEEDED)
        .SetValue("RESULT_NOT_FOUND", MOJO_RESULT_NOT_FOUND)
        .SetValue("RESULT_ALREADY_EXISTS", MOJO_RESULT_ALREADY_EXISTS)
        .SetValue("RESULT_PERMISSION_DENIED", MOJO_RESULT_PERMISSION_DENIED)
        .SetValue("RESULT_RESOURCE_EXHAUSTED", MOJO_RESULT_RESOURCE_EXHAUSTED)
        .SetValue("RESULT_FAILED_PRECONDITION", MOJO_RESULT_FAILED_PRECONDITION)
        .SetValue("RESULT_ABORTED", MOJO_RESULT_ABORTED)
        .SetValue("RESULT_OUT_OF_RANGE", MOJO_RESULT_OUT_OF_RANGE)
        .SetValue("RESULT_UNIMPLEMENTED", MOJO_RESULT_UNIMPLEMENTED)
        .SetValue("RESULT_INTERNAL", MOJO_RESULT_INTERNAL)
        .SetValue("RESULT_UNAVAILABLE", MOJO_RESULT_UNAVAILABLE)
        .SetValue("RESULT_DATA_LOSS", MOJO_RESULT_DATA_LOSS)
        .SetValue("RESULT_BUSY", MOJO_RESULT_BUSY)
        .SetValue("RESULT_SHOULD_WAIT", MOJO_RESULT_SHOULD_WAIT)

        .SetValue("DEADLINE_INDEFINITE", MOJO_DEADLINE_INDEFINITE)

        .SetValue("HANDLE_SIGNAL_NONE", MOJO_HANDLE_SIGNAL_NONE)
        .SetValue("HANDLE_SIGNAL_READABLE", MOJO_HANDLE_SIGNAL_READABLE)
        .SetValue("HANDLE_SIGNAL_READABLE", MOJO_HANDLE_SIGNAL_READABLE)

        .SetValue("WRITE_MESSAGE_FLAG_NONE", MOJO_WRITE_MESSAGE_FLAG_NONE)

        .SetValue("READ_MESSAGE_FLAG_NONE", MOJO_READ_MESSAGE_FLAG_NONE)
        .SetValue("READ_MESSAGE_FLAG_MAY_DISCARD",
                  MOJO_READ_MESSAGE_FLAG_MAY_DISCARD)

        .SetValue("CREATE_DATA_PIPE_OPTIONS_FLAG_NONE",
                  MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE)
        .SetValue("CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD",
                  MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD)

        .SetValue("WRITE_DATA_FLAG_NONE", MOJO_WRITE_DATA_FLAG_NONE)
        .SetValue("WRITE_DATA_FLAG_ALL_OR_NONE",
                  MOJO_WRITE_DATA_FLAG_ALL_OR_NONE)

        .SetValue("READ_DATA_FLAG_NONE", MOJO_READ_DATA_FLAG_NONE)
        .SetValue("READ_DATA_FLAG_ALL_OR_NONE",
                  MOJO_READ_DATA_FLAG_ALL_OR_NONE)
        .SetValue("READ_DATA_FLAG_DISCARD", MOJO_READ_DATA_FLAG_DISCARD)
        .SetValue("READ_DATA_FLAG_QUERY", MOJO_READ_DATA_FLAG_QUERY)
        .Build();

    data->SetObjectTemplate(&g_wrapper_info, templ);
  }

  return templ->NewInstance();
}

}  // namespace js
}  // namespace mojo
