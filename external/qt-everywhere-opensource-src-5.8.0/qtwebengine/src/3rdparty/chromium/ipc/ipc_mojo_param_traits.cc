// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_mojo_param_traits.h"

#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_mojo_message_helper.h"

namespace IPC {

void ParamTraits<mojo::MessagePipeHandle>::GetSize(base::PickleSizer* sizer,
                                                   const param_type& p) {
  GetParamSize(sizer, p.is_valid());
  if (p.is_valid())
    sizer->AddAttachment();
}

void ParamTraits<mojo::MessagePipeHandle>::Write(base::Pickle* m,
                                                 const param_type& p) {
  WriteParam(m, p.is_valid());
  if (p.is_valid())
    MojoMessageHelper::WriteMessagePipeTo(m, mojo::ScopedMessagePipeHandle(p));
}

bool ParamTraits<mojo::MessagePipeHandle>::Read(const base::Pickle* m,
                                                base::PickleIterator* iter,
                                                param_type* r) {
  bool is_valid;
  if (!ReadParam(m, iter, &is_valid))
    return false;
  if (!is_valid)
    return true;

  mojo::ScopedMessagePipeHandle handle;
  if (!MojoMessageHelper::ReadMessagePipeFrom(m, iter, &handle))
    return false;
  DCHECK(handle.is_valid());
  *r = handle.release();
  return true;
}

void ParamTraits<mojo::MessagePipeHandle>::Log(const param_type& p,
                                               std::string* l) {
  l->append("mojo::MessagePipeHandle(");
  LogParam(p.value(), l);
  l->append(")");
}

}  // namespace IPC
