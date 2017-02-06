// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlobEvent_h
#define BlobEvent_h

#include "core/fileapi/Blob.h"
#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class Blob;
class BlobEventInit;

class MODULES_EXPORT BlobEvent final : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~BlobEvent() override {}

    static BlobEvent* create();
    static BlobEvent* create(const AtomicString& type, const BlobEventInit& initializer);
    static BlobEvent* create(const AtomicString& type, Blob*);

    Blob* data() const { return m_blob.get(); }

    // Event
    const AtomicString& interfaceName() const final;

    DECLARE_VIRTUAL_TRACE();

private:
    BlobEvent() {}
    BlobEvent(const AtomicString& type, const BlobEventInit& initializer);
    BlobEvent(const AtomicString& type, Blob*);

    Member<Blob> m_blob;
};

} // namespace blink

#endif // BlobEvent_h
