// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SetSinkIdCallbacks_h
#define SetSinkIdCallbacks_h

#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebSetSinkIdCallbacks.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class HTMLMediaElement;
class ScriptPromiseResolver;

class SetSinkIdCallbacks final : public WebSetSinkIdCallbacks {
  // FIXME(tasak): When making public/platform classes to use PartitionAlloc,
  // the following macro should be moved to WebCallbacks defined in
  // public/platform/WebCallbacks.h.
  USING_FAST_MALLOC(SetSinkIdCallbacks);
  WTF_MAKE_NONCOPYABLE(SetSinkIdCallbacks);

 public:
  SetSinkIdCallbacks(ScriptPromiseResolver*,
                     HTMLMediaElement&,
                     const String& sinkId);
  ~SetSinkIdCallbacks() override;

  void onSuccess() override;
  void onError(WebSetSinkIdError) override;

 private:
  Persistent<ScriptPromiseResolver> m_resolver;
  Persistent<HTMLMediaElement> m_element;
  String m_sinkId;
};

}  // namespace blink

#endif  // SetSinkIdCallbacks_h
