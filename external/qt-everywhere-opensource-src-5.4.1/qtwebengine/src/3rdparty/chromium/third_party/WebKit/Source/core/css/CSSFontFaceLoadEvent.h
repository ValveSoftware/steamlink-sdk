/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef CSSFontFaceLoadEvent_h
#define CSSFontFaceLoadEvent_h

#include "core/css/FontFace.h"
#include "core/dom/DOMError.h"
#include "core/events/Event.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

struct CSSFontFaceLoadEventInit : public EventInit {
    FontFaceArray fontfaces;
};

class CSSFontFaceLoadEvent FINAL : public Event {
public:
    static PassRefPtrWillBeRawPtr<CSSFontFaceLoadEvent> create()
    {
        return adoptRefWillBeNoop(new CSSFontFaceLoadEvent());
    }

    static PassRefPtrWillBeRawPtr<CSSFontFaceLoadEvent> create(const AtomicString& type, const CSSFontFaceLoadEventInit& initializer)
    {
        return adoptRefWillBeNoop(new CSSFontFaceLoadEvent(type, initializer));
    }

    static PassRefPtrWillBeRawPtr<CSSFontFaceLoadEvent> createForFontFaces(const AtomicString& type, const FontFaceArray& fontfaces = FontFaceArray())
    {
        return adoptRefWillBeNoop(new CSSFontFaceLoadEvent(type, fontfaces));
    }

    virtual ~CSSFontFaceLoadEvent();

    FontFaceArray fontfaces() const { return m_fontfaces; }

    virtual const AtomicString& interfaceName() const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    CSSFontFaceLoadEvent();
    CSSFontFaceLoadEvent(const AtomicString&, const FontFaceArray&);
    CSSFontFaceLoadEvent(const AtomicString&, const CSSFontFaceLoadEventInit&);

    FontFaceArray m_fontfaces;
};

} // namespace WebCore

#endif // CSSFontFaceLoadEvent_h
