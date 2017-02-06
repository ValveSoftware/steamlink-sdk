// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentWriteEvaluator_h
#define DocumentWriteEvaluator_h

#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/frame/Navigator.h"
#include "core/html/parser/CompactHTMLToken.h"
#include "core/html/parser/HTMLToken.h"
#include "core/html/parser/HTMLTokenizer.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

// This class is used by the preload scanner on the background parser thread to
// execute inline Javascript and preload resources that would have been written
// by document.write(). It takes a script string and outputs a vector of start
// tag tokens.
class CORE_EXPORT DocumentWriteEvaluator {
    WTF_MAKE_NONCOPYABLE(DocumentWriteEvaluator);
    USING_FAST_MALLOC(DocumentWriteEvaluator);

public:
    // For unit testing.
    DocumentWriteEvaluator(const String& pathName, const String& hostName, const String& protocol, const String& userAgent);

    static std::unique_ptr<DocumentWriteEvaluator> create(const Document& document)
    {
        return wrapUnique(new DocumentWriteEvaluator(document));
    }
    virtual ~DocumentWriteEvaluator();

    // Initializes the V8 context for this document. Returns whether
    // initialization was needed.
    bool ensureEvaluationContext();
    String evaluateAndEmitWrittenSource(const String& scriptSource);
    bool shouldEvaluate(const String& scriptSource);

    void recordDocumentWrite(const String& documentWrittenString);

private:
    explicit DocumentWriteEvaluator(const Document&);
    // Returns true if the evaluation succeeded with no errors.
    bool evaluate(const String& scriptSource);

    // All the strings that are document.written in the script tag that is being
    // scanned.
    StringBuilder m_documentWrittenStrings;

    ScopedPersistent<v8::Context> m_persistentContext;
    ScopedPersistent<v8::Object> m_window;
    ScopedPersistent<v8::Object> m_document;
    ScopedPersistent<v8::Object> m_location;
    ScopedPersistent<v8::Object> m_navigator;

    String m_pathName;
    String m_hostName;
    String m_protocol;
    String m_userAgent;
};

} // namespace blink

#endif // DocumentWriteEvaluator_h
