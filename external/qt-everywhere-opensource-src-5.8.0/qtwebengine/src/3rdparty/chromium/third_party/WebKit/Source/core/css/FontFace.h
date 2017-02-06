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

#ifndef FontFace_h
#define FontFace_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSValue.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/DOMException.h"
#include "platform/fonts/FontTraits.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSFontFace;
class CSSValue;
class DOMArrayBuffer;
class DOMArrayBufferView;
class Document;
class ExceptionState;
class FontFaceDescriptors;
class StringOrArrayBufferOrArrayBufferView;
class StylePropertySet;
class StyleRuleFontFace;

class FontFace : public GarbageCollectedFinalized<FontFace>, public ScriptWrappable, public ActiveScriptWrappable, public ActiveDOMObject {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(FontFace);
    WTF_MAKE_NONCOPYABLE(FontFace);
public:
    enum LoadStatusType { Unloaded, Loading, Loaded, Error };

    static FontFace* create(ExecutionContext*, const AtomicString& family, StringOrArrayBufferOrArrayBufferView&, const FontFaceDescriptors&);
    static FontFace* create(Document*, const StyleRuleFontFace*);

    ~FontFace();

    const AtomicString& family() const { return m_family; }
    String style() const;
    String weight() const;
    String stretch() const;
    String unicodeRange() const;
    String variant() const;
    String featureSettings() const;

    // FIXME: Changing these attributes should affect font matching.
    void setFamily(ExecutionContext*, const AtomicString& s, ExceptionState&) { m_family = s; }
    void setStyle(ExecutionContext*, const String&, ExceptionState&);
    void setWeight(ExecutionContext*, const String&, ExceptionState&);
    void setStretch(ExecutionContext*, const String&, ExceptionState&);
    void setUnicodeRange(ExecutionContext*, const String&, ExceptionState&);
    void setVariant(ExecutionContext*, const String&, ExceptionState&);
    void setFeatureSettings(ExecutionContext*, const String&, ExceptionState&);

    String status() const;
    ScriptPromise loaded(ScriptState* scriptState) { return fontStatusPromise(scriptState); }

    ScriptPromise load(ScriptState*);

    LoadStatusType loadStatus() const { return m_status; }
    void setLoadStatus(LoadStatusType);
    void setError(DOMException* = nullptr);
    DOMException* error() const { return m_error; }
    FontTraits traits() const;
    CSSFontFace* cssFontFace() { return m_cssFontFace.get(); }
    size_t approximateBlankCharacterCount() const;

    DECLARE_VIRTUAL_TRACE();

    bool hadBlankText() const;

    class LoadFontCallback : public GarbageCollectedFinalized<LoadFontCallback> {
    public:
        virtual ~LoadFontCallback() { }
        virtual void notifyLoaded(FontFace*) = 0;
        virtual void notifyError(FontFace*) = 0;
        DEFINE_INLINE_VIRTUAL_TRACE() { }
    };
    void loadWithCallback(LoadFontCallback*, ExecutionContext*);

    // ActiveScriptWrappable.
    bool hasPendingActivity() const final;

private:
    static FontFace* create(ExecutionContext*, const AtomicString& family, DOMArrayBuffer* source, const FontFaceDescriptors&);
    static FontFace* create(ExecutionContext*, const AtomicString& family, DOMArrayBufferView*, const FontFaceDescriptors&);
    static FontFace* create(ExecutionContext*, const AtomicString& family, const String& source, const FontFaceDescriptors&);

    explicit FontFace(ExecutionContext*);
    FontFace(ExecutionContext*, const AtomicString& family, const FontFaceDescriptors&);

    void initCSSFontFace(Document*, const CSSValue* src);
    void initCSSFontFace(const unsigned char* data, size_t);
    void setPropertyFromString(const Document*, const String&, CSSPropertyID, ExceptionState* = 0);
    bool setPropertyFromStyle(const StylePropertySet&, CSSPropertyID);
    bool setPropertyValue(const CSSValue*, CSSPropertyID);
    bool setFamilyValue(const CSSValue&);
    void loadInternal(ExecutionContext*);
    ScriptPromise fontStatusPromise(ScriptState*);

    using LoadedProperty = ScriptPromiseProperty<Member<FontFace>, Member<FontFace>, Member<DOMException>>;

    AtomicString m_family;
    String m_otsParseMessage;
    Member<const CSSValue> m_style;
    Member<const CSSValue> m_weight;
    Member<const CSSValue> m_stretch;
    Member<const CSSValue> m_unicodeRange;
    Member<const CSSValue> m_variant;
    Member<const CSSValue> m_featureSettings;
    Member<const CSSValue> m_display;
    LoadStatusType m_status;
    Member<DOMException> m_error;

    Member<LoadedProperty> m_loadedProperty;
    Member<CSSFontFace> m_cssFontFace;
    HeapVector<Member<LoadFontCallback>> m_callbacks;
};

using FontFaceArray = HeapVector<Member<FontFace>>;

} // namespace blink

#endif // FontFace_h
