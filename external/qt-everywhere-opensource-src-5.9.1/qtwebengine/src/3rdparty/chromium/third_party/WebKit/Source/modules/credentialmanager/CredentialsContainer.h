// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CredentialsContainer_h
#define CredentialsContainer_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Credential;
class CredentialRequestOptions;
class ScriptPromise;
class ScriptState;

class CredentialsContainer final
    : public GarbageCollected<CredentialsContainer>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CredentialsContainer* create();

  // CredentialsContainer.h
  ScriptPromise get(ScriptState*, const CredentialRequestOptions&);
  ScriptPromise store(ScriptState*, Credential* = 0);
  ScriptPromise requireUserMediation(ScriptState*);

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 private:
  CredentialsContainer();
};

}  // namespace blink

#endif  // CredentialsContainer_h
