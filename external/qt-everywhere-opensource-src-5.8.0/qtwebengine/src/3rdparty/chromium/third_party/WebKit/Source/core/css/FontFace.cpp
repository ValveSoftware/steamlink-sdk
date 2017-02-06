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

#include "core/css/FontFace.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/StringOrArrayBufferOrArrayBufferView.h"
#include "core/CSSValueKeywords.h"
#include "core/css/BinaryDataFontFaceSource.h"
#include "core/css/CSSFontFace.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/FontFaceDescriptors.h"
#include "core/css/LocalFontFaceSource.h"
#include "core/css/RemoteFontFaceSource.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "platform/FontFamilyNames.h"
#include "platform/Histogram.h"
#include "platform/SharedBuffer.h"

namespace blink {

static const CSSValue* parseCSSValue(const Document* document, const String& value, CSSPropertyID propertyID)
{
    CSSParserContext context(*document, UseCounter::getFrom(document));
    return CSSParser::parseFontFaceDescriptor(propertyID, value, context);
}

FontFace* FontFace::create(ExecutionContext* context, const AtomicString& family, StringOrArrayBufferOrArrayBufferView& source, const FontFaceDescriptors& descriptors)
{
    if (source.isString())
        return create(context, family, source.getAsString(), descriptors);
    if (source.isArrayBuffer())
        return create(context, family, source.getAsArrayBuffer(), descriptors);
    if (source.isArrayBufferView())
        return create(context, family, source.getAsArrayBufferView(), descriptors);
    ASSERT_NOT_REACHED();
    return nullptr;
}

FontFace* FontFace::create(ExecutionContext* context, const AtomicString& family, const String& source, const FontFaceDescriptors& descriptors)
{
    FontFace* fontFace = new FontFace(context, family, descriptors);

    const CSSValue* src = parseCSSValue(toDocument(context), source, CSSPropertySrc);
    if (!src || !src->isValueList())
        fontFace->setError(DOMException::create(SyntaxError, "The source provided ('" + source + "') could not be parsed as a value list."));

    fontFace->initCSSFontFace(toDocument(context), src);
    return fontFace;
}

FontFace* FontFace::create(ExecutionContext* context, const AtomicString& family, DOMArrayBuffer* source, const FontFaceDescriptors& descriptors)
{
    FontFace* fontFace = new FontFace(context, family, descriptors);
    fontFace->initCSSFontFace(static_cast<const unsigned char*>(source->data()), source->byteLength());
    return fontFace;
}

FontFace* FontFace::create(ExecutionContext* context, const AtomicString& family, DOMArrayBufferView* source, const FontFaceDescriptors& descriptors)
{
    FontFace* fontFace = new FontFace(context, family, descriptors);
    fontFace->initCSSFontFace(static_cast<const unsigned char*>(source->baseAddress()), source->byteLength());
    return fontFace;
}

FontFace* FontFace::create(Document* document, const StyleRuleFontFace* fontFaceRule)
{
    const StylePropertySet& properties = fontFaceRule->properties();

    // Obtain the font-family property and the src property. Both must be defined.
    CSSValue* family = properties.getPropertyCSSValue(CSSPropertyFontFamily);
    if (!family || (!family->isFontFamilyValue() && !family->isPrimitiveValue()))
        return nullptr;
    CSSValue* src = properties.getPropertyCSSValue(CSSPropertySrc);
    if (!src || !src->isValueList())
        return nullptr;

    FontFace* fontFace = new FontFace(document);

    if (fontFace->setFamilyValue(*family)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontStyle)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontWeight)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontStretch)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyUnicodeRange)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontVariant)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontFeatureSettings)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontDisplay)
        && !fontFace->family().isEmpty()
        && fontFace->traits().bitfield()) {
        fontFace->initCSSFontFace(document, src);
        return fontFace;
    }
    return nullptr;
}

FontFace::FontFace(ExecutionContext* context)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(context)
    , m_status(Unloaded)
{
    suspendIfNeeded();
}

FontFace::FontFace(ExecutionContext* context, const AtomicString& family, const FontFaceDescriptors& descriptors)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(context)
    , m_family(family)
    , m_status(Unloaded)
{
    Document* document = toDocument(context);
    setPropertyFromString(document, descriptors.style(), CSSPropertyFontStyle);
    setPropertyFromString(document, descriptors.weight(), CSSPropertyFontWeight);
    setPropertyFromString(document, descriptors.stretch(), CSSPropertyFontStretch);
    setPropertyFromString(document, descriptors.unicodeRange(), CSSPropertyUnicodeRange);
    setPropertyFromString(document, descriptors.variant(), CSSPropertyFontVariant);
    setPropertyFromString(document, descriptors.featureSettings(), CSSPropertyFontFeatureSettings);

    suspendIfNeeded();
}

FontFace::~FontFace()
{
}

String FontFace::style() const
{
    return m_style ? m_style->cssText() : "normal";
}

String FontFace::weight() const
{
    return m_weight ? m_weight->cssText() : "normal";
}

String FontFace::stretch() const
{
    return m_stretch ? m_stretch->cssText() : "normal";
}

String FontFace::unicodeRange() const
{
    return m_unicodeRange ? m_unicodeRange->cssText() : "U+0-10FFFF";
}

String FontFace::variant() const
{
    return m_variant ? m_variant->cssText() : "normal";
}

String FontFace::featureSettings() const
{
    return m_featureSettings ? m_featureSettings->cssText() : "normal";
}

void FontFace::setStyle(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyFontStyle, &exceptionState);
}

void FontFace::setWeight(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyFontWeight, &exceptionState);
}

void FontFace::setStretch(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyFontStretch, &exceptionState);
}

void FontFace::setUnicodeRange(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyUnicodeRange, &exceptionState);
}

void FontFace::setVariant(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyFontVariant, &exceptionState);
}

void FontFace::setFeatureSettings(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyFontFeatureSettings, &exceptionState);
}

void FontFace::setPropertyFromString(const Document* document, const String& s, CSSPropertyID propertyID, ExceptionState* exceptionState)
{
    const CSSValue* value = parseCSSValue(document, s, propertyID);
    if (value && setPropertyValue(value, propertyID))
        return;

    String message = "Failed to set '" + s + "' as a property value.";
    if (exceptionState)
        exceptionState->throwDOMException(SyntaxError, message);
    else
        setError(DOMException::create(SyntaxError, message));
}

bool FontFace::setPropertyFromStyle(const StylePropertySet& properties, CSSPropertyID propertyID)
{
    return setPropertyValue(properties.getPropertyCSSValue(propertyID), propertyID);
}

bool FontFace::setPropertyValue(const CSSValue* value, CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyFontStyle:
        m_style = value;
        break;
    case CSSPropertyFontWeight:
        m_weight = value;
        break;
    case CSSPropertyFontStretch:
        m_stretch = value;
        break;
    case CSSPropertyUnicodeRange:
        if (value && !value->isValueList())
            return false;
        m_unicodeRange = value;
        break;
    case CSSPropertyFontVariant:
        m_variant = value;
        break;
    case CSSPropertyFontFeatureSettings:
        m_featureSettings = value;
        break;
    case CSSPropertyFontDisplay:
        m_display = value;
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

bool FontFace::setFamilyValue(const CSSValue& familyValue)
{
    AtomicString family;
    if (familyValue.isFontFamilyValue()) {
        family = AtomicString(toCSSFontFamilyValue(familyValue).value());
    } else if (toCSSPrimitiveValue(familyValue).isValueID()) {
        // We need to use the raw text for all the generic family types, since @font-face is a way of actually
        // defining what font to use for those types.
        switch (toCSSPrimitiveValue(familyValue).getValueID()) {
        case CSSValueSerif:
            family =  FontFamilyNames::webkit_serif;
            break;
        case CSSValueSansSerif:
            family =  FontFamilyNames::webkit_sans_serif;
            break;
        case CSSValueCursive:
            family =  FontFamilyNames::webkit_cursive;
            break;
        case CSSValueFantasy:
            family =  FontFamilyNames::webkit_fantasy;
            break;
        case CSSValueMonospace:
            family =  FontFamilyNames::webkit_monospace;
            break;
        case CSSValueWebkitPictograph:
            family =  FontFamilyNames::webkit_pictograph;
            break;
        default:
            return false;
        }
    }
    m_family = family;
    return true;
}

String FontFace::status() const
{
    switch (m_status) {
    case Unloaded:
        return "unloaded";
    case Loading:
        return "loading";
    case Loaded:
        return "loaded";
    case Error:
        return "error";
    default:
        ASSERT_NOT_REACHED();
    }
    return emptyString();
}

void FontFace::setLoadStatus(LoadStatusType status)
{
    m_status = status;
    ASSERT(m_status != Error || m_error);

    if (m_status == Loaded || m_status == Error) {
        if (m_loadedProperty) {
            if (m_status == Loaded)
                m_loadedProperty->resolve(this);
            else
                m_loadedProperty->reject(m_error.get());
        }

        HeapVector<Member<LoadFontCallback>> callbacks;
        m_callbacks.swap(callbacks);
        for (size_t i = 0; i < callbacks.size(); ++i) {
            if (m_status == Loaded)
                callbacks[i]->notifyLoaded(this);
            else
                callbacks[i]->notifyError(this);
        }
    }
}

void FontFace::setError(DOMException* error)
{
    if (!m_error)
        m_error = error ? error : DOMException::create(NetworkError);
    setLoadStatus(Error);
}

ScriptPromise FontFace::fontStatusPromise(ScriptState* scriptState)
{
    if (!m_loadedProperty) {
        m_loadedProperty = new LoadedProperty(scriptState->getExecutionContext(), this, LoadedProperty::Loaded);
        if (m_status == Loaded)
            m_loadedProperty->resolve(this);
        else if (m_status == Error)
            m_loadedProperty->reject(m_error.get());
    }
    return m_loadedProperty->promise(scriptState->world());
}

ScriptPromise FontFace::load(ScriptState* scriptState)
{
    loadInternal(scriptState->getExecutionContext());
    return fontStatusPromise(scriptState);
}

void FontFace::loadWithCallback(LoadFontCallback* callback, ExecutionContext* context)
{
    loadInternal(context);
    if (m_status == Loaded)
        callback->notifyLoaded(this);
    else if (m_status == Error)
        callback->notifyError(this);
    else
        m_callbacks.append(callback);
}

void FontFace::loadInternal(ExecutionContext* context)
{
    if (m_status != Unloaded)
        return;

    m_cssFontFace->load();
}

FontTraits FontFace::traits() const
{
    FontStretch stretch = FontStretchNormal;
    if (m_stretch) {
        if (!m_stretch->isPrimitiveValue())
            return 0;

        switch (toCSSPrimitiveValue(m_stretch.get())->getValueID()) {
        case CSSValueUltraCondensed:
            stretch = FontStretchUltraCondensed;
            break;
        case CSSValueExtraCondensed:
            stretch = FontStretchExtraCondensed;
            break;
        case CSSValueCondensed:
            stretch = FontStretchCondensed;
            break;
        case CSSValueSemiCondensed:
            stretch = FontStretchSemiCondensed;
            break;
        case CSSValueSemiExpanded:
            stretch = FontStretchSemiExpanded;
            break;
        case CSSValueExpanded:
            stretch = FontStretchExpanded;
            break;
        case CSSValueExtraExpanded:
            stretch = FontStretchExtraExpanded;
            break;
        case CSSValueUltraExpanded:
            stretch = FontStretchUltraExpanded;
            break;
        default:
            break;
        }
    }

    FontStyle style = FontStyleNormal;
    if (m_style) {
        if (!m_style->isPrimitiveValue())
            return 0;

        switch (toCSSPrimitiveValue(m_style.get())->getValueID()) {
        case CSSValueNormal:
            style = FontStyleNormal;
            break;
        case CSSValueOblique:
            style = FontStyleOblique;
            break;
        case CSSValueItalic:
            style = FontStyleItalic;
            break;
        default:
            break;
        }
    }

    FontWeight weight = FontWeight400;
    if (m_weight) {
        if (!m_weight->isPrimitiveValue())
            return 0;

        switch (toCSSPrimitiveValue(m_weight.get())->getValueID()) {
        case CSSValueBold:
        case CSSValue700:
            weight = FontWeight700;
            break;
        case CSSValueNormal:
        case CSSValue400:
            weight = FontWeight400;
            break;
        case CSSValue900:
            weight = FontWeight900;
            break;
        case CSSValue800:
            weight = FontWeight800;
            break;
        case CSSValue600:
            weight = FontWeight600;
            break;
        case CSSValue500:
            weight = FontWeight500;
            break;
        case CSSValue300:
            weight = FontWeight300;
            break;
        case CSSValue200:
            weight = FontWeight200;
            break;
        case CSSValue100:
            weight = FontWeight100;
            break;
        // Although 'lighter' and 'bolder' are valid keywords for font-weights, they are invalid
        // inside font-face rules so they are ignored. Reference: http://www.w3.org/TR/css3-fonts/#descdef-font-weight.
        case CSSValueLighter:
        case CSSValueBolder:
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    return FontTraits(style, weight, stretch);
}

size_t FontFace::approximateBlankCharacterCount() const
{
    if (m_status == Loading)
        return m_cssFontFace->approximateBlankCharacterCount();
    return 0;
}

static FontDisplay CSSValueToFontDisplay(const CSSValue* value)
{
    if (value && value->isPrimitiveValue()) {
        switch (toCSSPrimitiveValue(value)->getValueID()) {
        case CSSValueAuto:
            return FontDisplayAuto;
        case CSSValueBlock:
            return FontDisplayBlock;
        case CSSValueSwap:
            return FontDisplaySwap;
        case CSSValueFallback:
            return FontDisplayFallback;
        case CSSValueOptional:
            return FontDisplayOptional;
        default:
            break;
        }
    }
    return FontDisplayAuto;
}

static CSSFontFace* createCSSFontFace(FontFace* fontFace, const CSSValue* unicodeRange)
{
    Vector<UnicodeRange> ranges;
    if (const CSSValueList* rangeList = toCSSValueList(unicodeRange)) {
        unsigned numRanges = rangeList->length();
        for (unsigned i = 0; i < numRanges; i++) {
            const CSSUnicodeRangeValue& range = toCSSUnicodeRangeValue(rangeList->item(i));
            ranges.append(UnicodeRange(range.from(), range.to()));
        }
    }

    return new CSSFontFace(fontFace, ranges);
}

void FontFace::initCSSFontFace(Document* document, const CSSValue* src)
{
    m_cssFontFace = createCSSFontFace(this, m_unicodeRange.get());
    if (m_error)
        return;

    // Each item in the src property's list is a single CSSFontFaceSource. Put them all into a CSSFontFace.
    ASSERT(src);
    ASSERT(src->isValueList());
    const CSSValueList* srcList = toCSSValueList(src);
    int srcLength = srcList->length();

    for (int i = 0; i < srcLength; i++) {
        // An item in the list either specifies a string (local font name) or a URL (remote font to download).
        const CSSFontFaceSrcValue& item = toCSSFontFaceSrcValue(srcList->item(i));
        CSSFontFaceSource* source = nullptr;

        if (!item.isLocal()) {
            const Settings* settings = document ? document->settings() : nullptr;
            bool allowDownloading = settings && settings->downloadableBinaryFontsEnabled();
            if (allowDownloading && item.isSupportedFormat() && document) {
                FontResource* fetched = item.fetch(document);
                if (fetched) {
                    CSSFontSelector* fontSelector = document->styleEngine().fontSelector();
                    source = new RemoteFontFaceSource(fetched, fontSelector, CSSValueToFontDisplay(m_display.get()));
                }
            }
        } else {
            source = new LocalFontFaceSource(item.resource());
        }

        if (source)
            m_cssFontFace->addSource(source);
    }

    if (m_display) {
        DEFINE_STATIC_LOCAL(EnumerationHistogram, fontDisplayHistogram, ("WebFont.FontDisplayValue", FontDisplayEnumMax));
        fontDisplayHistogram.count(CSSValueToFontDisplay(m_display.get()));
    }
}

void FontFace::initCSSFontFace(const unsigned char* data, size_t size)
{
    m_cssFontFace = createCSSFontFace(this, m_unicodeRange.get());
    if (m_error)
        return;

    RefPtr<SharedBuffer> buffer = SharedBuffer::create(data, size);
    BinaryDataFontFaceSource* source = new BinaryDataFontFaceSource(buffer.get(), m_otsParseMessage);
    if (source->isValid())
        setLoadStatus(Loaded);
    else
        setError(DOMException::create(SyntaxError, "Invalid font data in ArrayBuffer."));
    m_cssFontFace->addSource(source);
}

DEFINE_TRACE(FontFace)
{
    visitor->trace(m_style);
    visitor->trace(m_weight);
    visitor->trace(m_stretch);
    visitor->trace(m_unicodeRange);
    visitor->trace(m_variant);
    visitor->trace(m_featureSettings);
    visitor->trace(m_display);
    visitor->trace(m_error);
    visitor->trace(m_loadedProperty);
    visitor->trace(m_cssFontFace);
    visitor->trace(m_callbacks);
    ActiveDOMObject::trace(visitor);
}

bool FontFace::hadBlankText() const
{
    return m_cssFontFace->hadBlankText();
}

bool FontFace::hasPendingActivity() const
{
    return m_status == Loading && getExecutionContext() && !getExecutionContext()->activeDOMObjectsAreStopped();
}

} // namespace blink
