/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DateTimeChooser_h
#define DateTimeChooser_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

struct DateTimeSuggestion {
    double value;
    String localizedValue;
    String label;
};

struct DateTimeChooserParameters {
    AtomicString type;
    IntRect anchorRectInRootView;
    // Locale name for which the chooser should be localized. This
    // might be an invalid name because it comes from HTML lang
    // attributes.
    AtomicString locale;
    // FIXME: Remove. Deprecated in favor of doubleValue.
    String currentValue;
    double doubleValue;
    Vector<DateTimeSuggestion> suggestions;
    double minimum;
    double maximum;
    double step;
    double stepBase;
    bool required;
    bool isAnchorElementRTL;
};

class PLATFORM_EXPORT DateTimeChooser : public RefCountedWillBeGarbageCollectedFinalized<DateTimeChooser> {
public:
    virtual ~DateTimeChooser();

    virtual void endChooser() = 0;
    virtual void trace(Visitor*) { }
};

} // namespace WebCore
#endif // DateTimeChooser_h
