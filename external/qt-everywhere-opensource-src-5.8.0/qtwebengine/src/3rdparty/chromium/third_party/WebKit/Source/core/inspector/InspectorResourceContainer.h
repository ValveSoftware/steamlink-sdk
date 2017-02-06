// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorResourceContainer_h
#define InspectorResourceContainer_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

class InspectedFrames;
class LocalFrame;

class CORE_EXPORT InspectorResourceContainer : public GarbageCollectedFinalized<InspectorResourceContainer> {
    WTF_MAKE_NONCOPYABLE(InspectorResourceContainer);
public:
    explicit InspectorResourceContainer(InspectedFrames*);
    ~InspectorResourceContainer();
    DECLARE_TRACE();

    void didCommitLoadForLocalFrame(LocalFrame*);

    void storeStyleSheetContent(const String& url, const String& content);
    bool loadStyleSheetContent(const String& url, String* content);

    void storeStyleElementContent(int backendNodeId, const String& content);
    bool loadStyleElementContent(int backendNodeId, String* content);

private:
    Member<InspectedFrames> m_inspectedFrames;
    HashMap<String, String> m_styleSheetContents;
    HashMap<int, String> m_styleElementContents;
};

} // namespace blink


#endif // !defined(InspectorResourceContainer_h)
