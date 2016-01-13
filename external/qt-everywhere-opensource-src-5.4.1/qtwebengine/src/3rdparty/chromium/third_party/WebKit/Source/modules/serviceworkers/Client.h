// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Client_h
#define Client_h

#include "bindings/v8/ScriptWrappable.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "wtf/Forward.h"

namespace WebCore {

class Client FINAL : public RefCounted<Client>, public ScriptWrappable {
public:
    static PassRefPtr<Client> create(unsigned id);
    ~Client();

    // Client.idl
    unsigned id() const { return m_id; }
    void postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue> message, const MessagePortArray*, ExceptionState&);

private:
    explicit Client(unsigned id);

    unsigned m_id;
};

} // namespace WebCore

#endif // Client_h
