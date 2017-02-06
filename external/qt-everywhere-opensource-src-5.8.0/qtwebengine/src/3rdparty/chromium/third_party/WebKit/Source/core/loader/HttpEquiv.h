// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HttpEquiv_h
#define HttpEquiv_h

#include "wtf/Allocator.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class Document;

/**
 * Handles a HTTP header equivalent set by a meta tag using <meta http-equiv="..." content="...">. This is called
 * when a meta tag is encountered during document parsing, and also when a script dynamically changes or adds a meta
 * tag. This enables scripts to use meta tags to perform refreshes and set expiry dates in addition to them being
 * specified in a HTML file.
 */
class HttpEquiv {
    STATIC_ONLY(HttpEquiv);
public:
    static void process(Document&, const AtomicString& equiv, const AtomicString& content, bool inDocumentHeadElement);

private:
    static void processHttpEquivDefaultStyle(Document&, const AtomicString& content);
    static void processHttpEquivRefresh(Document&, const AtomicString& content);
    static void processHttpEquivSetCookie(Document&, const AtomicString& content);
    static void processHttpEquivXFrameOptions(Document&, const AtomicString& content);
    static void processHttpEquivContentSecurityPolicy(Document&, const AtomicString& equiv, const AtomicString& content);
    static void processHttpEquivAcceptCH(Document&, const AtomicString& content);
};

} // namespace blink

#endif
