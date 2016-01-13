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

#include "config.h"
#include "core/css/FontFace.h"

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "bindings/v8/ScriptState.h"
#include "core/CSSValueKeywords.h"
#include "core/css/BinaryDataFontFaceSource.h"
#include "core/css/CSSFontFace.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/LocalFontFaceSource.h"
#include "core/css/RemoteFontFaceSource.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/dom/DOMError.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/svg/SVGFontFaceElement.h"
#include "core/svg/SVGFontFaceSource.h"
#include "core/svg/SVGRemoteFontFaceSource.h"
#include "platform/FontFamilyNames.h"
#include "platform/SharedBuffer.h"

namespace WebCore {

class FontFaceReadyPromiseResolver {
public:
    static PassOwnPtr<FontFaceReadyPromiseResolver> create(ScriptState* scriptState)
    {
        return adoptPtr(new FontFaceReadyPromiseResolver(scriptState));
    }

    void resolve(PassRefPtrWillBeRawPtr<FontFace> fontFace)
    {
        switch (fontFace->loadStatus()) {
        case FontFace::Loaded:
            m_resolver->resolve(fontFace);
            break;
        case FontFace::Error:
            m_resolver->reject(fontFace->error());
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    ScriptPromise promise() { return m_resolver->promise(); }

private:
    FontFaceReadyPromiseResolver(ScriptState* scriptState)
        : m_resolver(ScriptPromiseResolverWithContext::create(scriptState))
    {
    }

    RefPtr<ScriptPromiseResolverWithContext> m_resolver;
};

static PassRefPtrWillBeRawPtr<CSSValue> parseCSSValue(const Document* document, const String& s, CSSPropertyID propertyID)
{
    if (s.isEmpty())
        return nullptr;
    RefPtrWillBeRawPtr<MutableStylePropertySet> parsedStyle = MutableStylePropertySet::create();
    BisonCSSParser::parseValue(parsedStyle.get(), propertyID, s, true, *document);
    return parsedStyle->getPropertyCSSValue(propertyID);
}

static bool initFontFace(FontFace* fontFace, ExecutionContext* context, const AtomicString& family, const Dictionary& descriptors, ExceptionState& exceptionState)
{
    fontFace->setFamily(context, family, exceptionState);
    if (exceptionState.hadException())
        return false;

    String value;
    if (descriptors.get("style", value)) {
        fontFace->setStyle(context, value, exceptionState);
        if (exceptionState.hadException())
            return false;
    }
    if (descriptors.get("weight", value)) {
        fontFace->setWeight(context, value, exceptionState);
        if (exceptionState.hadException())
            return false;
    }
    if (descriptors.get("stretch", value)) {
        fontFace->setStretch(context, value, exceptionState);
        if (exceptionState.hadException())
            return false;
    }
    if (descriptors.get("unicodeRange", value)) {
        fontFace->setUnicodeRange(context, value, exceptionState);
        if (exceptionState.hadException())
            return false;
    }
    if (descriptors.get("variant", value)) {
        fontFace->setVariant(context, value, exceptionState);
        if (exceptionState.hadException())
            return false;
    }
    if (descriptors.get("featureSettings", value)) {
        fontFace->setFeatureSettings(context, value, exceptionState);
        if (exceptionState.hadException())
            return false;
    }
    return true;
}

PassRefPtrWillBeRawPtr<FontFace> FontFace::create(ExecutionContext* context, const AtomicString& family, const String& source, const Dictionary& descriptors, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<CSSValue> src = parseCSSValue(toDocument(context), source, CSSPropertySrc);
    if (!src || !src->isValueList()) {
        exceptionState.throwDOMException(SyntaxError, "The source provided ('" + source + "') could not be parsed as a value list.");
        return nullptr;
    }

    RefPtrWillBeRawPtr<FontFace> fontFace = adoptRefWillBeNoop(new FontFace());
    if (initFontFace(fontFace.get(), context, family, descriptors, exceptionState))
        fontFace->initCSSFontFace(toDocument(context), src);
    return fontFace.release();
}

PassRefPtrWillBeRawPtr<FontFace> FontFace::create(ExecutionContext* context, const AtomicString& family, PassRefPtr<ArrayBuffer> source, const Dictionary& descriptors, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<FontFace> fontFace = adoptRefWillBeNoop(new FontFace());
    if (initFontFace(fontFace.get(), context, family, descriptors, exceptionState))
        fontFace->initCSSFontFace(static_cast<const unsigned char*>(source->data()), source->byteLength());
    return fontFace.release();
}

PassRefPtrWillBeRawPtr<FontFace> FontFace::create(ExecutionContext* context, const AtomicString& family, PassRefPtr<ArrayBufferView> source, const Dictionary& descriptors, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<FontFace> fontFace = adoptRefWillBeNoop(new FontFace());
    if (initFontFace(fontFace.get(), context, family, descriptors, exceptionState))
        fontFace->initCSSFontFace(static_cast<const unsigned char*>(source->baseAddress()), source->byteLength());
    return fontFace.release();
}

PassRefPtrWillBeRawPtr<FontFace> FontFace::create(Document* document, const StyleRuleFontFace* fontFaceRule)
{
    const StylePropertySet& properties = fontFaceRule->properties();

    // Obtain the font-family property and the src property. Both must be defined.
    RefPtrWillBeRawPtr<CSSValue> family = properties.getPropertyCSSValue(CSSPropertyFontFamily);
    if (!family || !family->isValueList())
        return nullptr;
    RefPtrWillBeRawPtr<CSSValue> src = properties.getPropertyCSSValue(CSSPropertySrc);
    if (!src || !src->isValueList())
        return nullptr;

    RefPtrWillBeRawPtr<FontFace> fontFace = adoptRefWillBeNoop(new FontFace());

    if (fontFace->setFamilyValue(toCSSValueList(family.get()))
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontStyle)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontWeight)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontStretch)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyUnicodeRange)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyFontVariant)
        && fontFace->setPropertyFromStyle(properties, CSSPropertyWebkitFontFeatureSettings)
        && !fontFace->family().isEmpty()
        && fontFace->traits().mask()) {
        fontFace->initCSSFontFace(document, src);
        return fontFace.release();
    }
    return nullptr;
}

FontFace::FontFace()
    : m_status(Unloaded)
{
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
    setPropertyFromString(toDocument(context), s, CSSPropertyFontStyle, exceptionState);
}

void FontFace::setWeight(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyFontWeight, exceptionState);
}

void FontFace::setStretch(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyFontStretch, exceptionState);
}

void FontFace::setUnicodeRange(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyUnicodeRange, exceptionState);
}

void FontFace::setVariant(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyFontVariant, exceptionState);
}

void FontFace::setFeatureSettings(ExecutionContext* context, const String& s, ExceptionState& exceptionState)
{
    setPropertyFromString(toDocument(context), s, CSSPropertyWebkitFontFeatureSettings, exceptionState);
}

void FontFace::setPropertyFromString(const Document* document, const String& s, CSSPropertyID propertyID, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<CSSValue> value = parseCSSValue(document, s, propertyID);
    if (!value || !setPropertyValue(value, propertyID))
        exceptionState.throwDOMException(SyntaxError, "Failed to set '" + s + "' as a property value.");
}

bool FontFace::setPropertyFromStyle(const StylePropertySet& properties, CSSPropertyID propertyID)
{
    return setPropertyValue(properties.getPropertyCSSValue(propertyID), propertyID);
}

bool FontFace::setPropertyValue(PassRefPtrWillBeRawPtr<CSSValue> value, CSSPropertyID propertyID)
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
    case CSSPropertyWebkitFontFeatureSettings:
        m_featureSettings = value;
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

bool FontFace::setFamilyValue(CSSValueList* familyList)
{
    // The font-family descriptor has to have exactly one family name.
    if (familyList->length() != 1)
        return false;

    CSSPrimitiveValue* familyValue = toCSSPrimitiveValue(familyList->itemWithoutBoundsCheck(0));
    AtomicString family;
    if (familyValue->isString()) {
        family = AtomicString(familyValue->getStringValue());
    } else if (familyValue->isValueID()) {
        // We need to use the raw text for all the generic family types, since @font-face is a way of actually
        // defining what font to use for those types.
        switch (familyValue->getValueID()) {
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

void FontFace::setLoadStatus(LoadStatus status)
{
    m_status = status;
    if (m_status == Error)
        m_error = DOMError::create(NetworkError);
    if (m_status == Loaded || m_status == Error) {
        resolveReadyPromises();

        WillBeHeapVector<RefPtrWillBeMember<LoadFontCallback> > callbacks;
        m_callbacks.swap(callbacks);
        for (size_t i = 0; i < callbacks.size(); ++i) {
            if (m_status == Loaded)
                callbacks[i]->notifyLoaded(this);
            else
                callbacks[i]->notifyError(this);
        }
    }
}

ScriptPromise FontFace::fontStatusPromise(ScriptState* scriptState)
{
    // Since we cannot hold a ScriptPromise as a member of FontFace (that will
    // cause a circular reference), this creates new Promise every time.
    OwnPtr<FontFaceReadyPromiseResolver> resolver = FontFaceReadyPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    if (m_status == Loaded || m_status == Error)
        resolver->resolve(this);
    else
        m_readyResolvers.append(resolver.release());

    return promise;
}

ScriptPromise FontFace::load(ScriptState* scriptState)
{
    loadInternal(scriptState->executionContext());
    return fontStatusPromise(scriptState);
}

void FontFace::loadWithCallback(PassRefPtrWillBeRawPtr<LoadFontCallback> callback, ExecutionContext* context)
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
    toDocument(context)->styleEngine()->fontSelector()->fontLoader()->loadPendingFonts();
}

void FontFace::resolveReadyPromises()
{
    for (size_t i = 0; i < m_readyResolvers.size(); i++)
        m_readyResolvers[i]->resolve(this);
    m_readyResolvers.clear();
}

FontTraits FontFace::traits() const
{
    FontStyle style = FontStyleNormal;
    if (m_style) {
        if (!m_style->isPrimitiveValue())
            return 0;

        switch (toCSSPrimitiveValue(m_style.get())->getValueID()) {
        case CSSValueNormal:
            style = FontStyleNormal;
            break;
        case CSSValueItalic:
        case CSSValueOblique:
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
        case CSSValueLighter:
        case CSSValue100:
            weight = FontWeight100;
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    FontVariant variant = FontVariantNormal;
    if (RefPtrWillBeRawPtr<CSSValue> fontVariant = m_variant) {
        // font-variant descriptor can be a value list.
        if (fontVariant->isPrimitiveValue()) {
            RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            list->append(fontVariant);
            fontVariant = list;
        } else if (!fontVariant->isValueList()) {
            return 0;
        }

        CSSValueList* variantList = toCSSValueList(fontVariant.get());
        unsigned numVariants = variantList->length();
        if (!numVariants)
            return 0;

        for (unsigned i = 0; i < numVariants; ++i) {
            switch (toCSSPrimitiveValue(variantList->itemWithoutBoundsCheck(i))->getValueID()) {
            case CSSValueNormal:
                variant = FontVariantNormal;
                break;
            case CSSValueSmallCaps:
                variant = FontVariantSmallCaps;
                break;
            default:
                break;
            }
        }
    }

    return FontTraits(style, variant, weight, FontStretchNormal);
}

static PassOwnPtrWillBeRawPtr<CSSFontFace> createCSSFontFace(FontFace* fontFace, CSSValue* unicodeRange)
{
    Vector<CSSFontFace::UnicodeRange> ranges;
    if (CSSValueList* rangeList = toCSSValueList(unicodeRange)) {
        unsigned numRanges = rangeList->length();
        for (unsigned i = 0; i < numRanges; i++) {
            CSSUnicodeRangeValue* range = toCSSUnicodeRangeValue(rangeList->itemWithoutBoundsCheck(i));
            ranges.append(CSSFontFace::UnicodeRange(range->from(), range->to()));
        }
    }

    return adoptPtrWillBeNoop(new CSSFontFace(fontFace, ranges));
}

void FontFace::initCSSFontFace(Document* document, PassRefPtrWillBeRawPtr<CSSValue> src)
{
    m_cssFontFace = createCSSFontFace(this, m_unicodeRange.get());

    // Each item in the src property's list is a single CSSFontFaceSource. Put them all into a CSSFontFace.
    CSSValueList* srcList = toCSSValueList(src.get());
    int srcLength = srcList->length();

    bool foundSVGFont = false;

    for (int i = 0; i < srcLength; i++) {
        // An item in the list either specifies a string (local font name) or a URL (remote font to download).
        CSSFontFaceSrcValue* item = toCSSFontFaceSrcValue(srcList->itemWithoutBoundsCheck(i));
        OwnPtrWillBeRawPtr<CSSFontFaceSource> source = nullptr;

#if ENABLE(SVG_FONTS)
        foundSVGFont = item->isSVGFontFaceSrc() || item->svgFontFaceElement();
#endif
        if (!item->isLocal()) {
            Settings* settings = document ? document->frame() ? document->frame()->settings() : 0 : 0;
            bool allowDownloading = foundSVGFont || (settings && settings->downloadableBinaryFontsEnabled());
            if (allowDownloading && item->isSupportedFormat() && document) {
                FontResource* fetched = item->fetch(document);
                if (fetched) {
                    FontLoader* fontLoader = document->styleEngine()->fontSelector()->fontLoader();

#if ENABLE(SVG_FONTS)
                    if (foundSVGFont) {
                        source = adoptPtrWillBeNoop(new SVGRemoteFontFaceSource(item->resource(), fetched, fontLoader));
                    } else
#endif
                    {
                        source = adoptPtrWillBeNoop(new RemoteFontFaceSource(fetched, fontLoader));
                    }
                }
            }
        } else {
#if ENABLE(SVG_FONTS)
            if (item->svgFontFaceElement()) {
                RefPtrWillBeRawPtr<SVGFontFaceElement> fontfaceElement = item->svgFontFaceElement();
                // SVGFontFaceSource assumes that it is the case where <font-face> element resides in the same document.
                // We put a RELEASE_ASSERT here as it will cause UAF if the assumption is false.
                RELEASE_ASSERT(fontfaceElement->inDocument());
                RELEASE_ASSERT(fontfaceElement->document() == document);
                source = adoptPtrWillBeNoop(new SVGFontFaceSource(fontfaceElement.get()));
            } else
#endif
            {
                source = adoptPtrWillBeNoop(new LocalFontFaceSource(item->resource()));
            }
        }

        if (source)
            m_cssFontFace->addSource(source.release());
    }
}

void FontFace::initCSSFontFace(const unsigned char* data, unsigned size)
{
    m_cssFontFace = createCSSFontFace(this, m_unicodeRange.get());

    RefPtr<SharedBuffer> buffer = SharedBuffer::create(data, size);
    OwnPtrWillBeRawPtr<BinaryDataFontFaceSource> source = adoptPtrWillBeNoop(new BinaryDataFontFaceSource(buffer.get()));
    if (source->isValid()) {
        m_status = Loaded;
    } else {
        m_status = Error;
        m_error = DOMError::create(SyntaxError, "Invalid font data in ArrayBuffer.");
    }
    m_cssFontFace->addSource(source.release());
}

void FontFace::trace(Visitor* visitor)
{
    visitor->trace(m_src);
    visitor->trace(m_style);
    visitor->trace(m_weight);
    visitor->trace(m_stretch);
    visitor->trace(m_unicodeRange);
    visitor->trace(m_variant);
    visitor->trace(m_featureSettings);
    visitor->trace(m_error);
    visitor->trace(m_cssFontFace);
    visitor->trace(m_callbacks);
}

bool FontFace::hadBlankText() const
{
    return m_cssFontFace->hadBlankText();
}

} // namespace WebCore
