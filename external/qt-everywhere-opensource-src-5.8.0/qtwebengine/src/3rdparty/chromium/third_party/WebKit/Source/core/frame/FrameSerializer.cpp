/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/frame/FrameSerializer.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/css/CSSFontFaceRule.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSValueList.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/editing/serializers/MarkupAccumulator.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ImageResource.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/ImageDocument.h"
#include "core/style/StyleFetchedImage.h"
#include "core/style/StyleImage.h"
#include "platform/SerializedResource.h"
#include "platform/graphics/Image.h"
#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextEncoding.h"
#include "wtf/text/WTFString.h"

namespace blink {

static bool shouldIgnoreElement(const Element& element)
{
    if (isHTMLScriptElement(element))
        return true;
    if (isHTMLNoScriptElement(element))
        return true;
    return isHTMLMetaElement(element) && toHTMLMetaElement(element).computeEncoding().isValid();
}

class SerializerMarkupAccumulator : public MarkupAccumulator {
    STACK_ALLOCATED();
public:
    SerializerMarkupAccumulator(FrameSerializer::Delegate&, const Document&, HeapVector<Member<Node>>&);
    ~SerializerMarkupAccumulator() override;

protected:
    void appendText(StringBuilder& out, Text&) override;
    bool shouldIgnoreAttribute(const Attribute&) override;
    void appendElement(StringBuilder& out, Element&, Namespaces*) override;
    void appendAttribute(StringBuilder& out, const Element&, const Attribute&, Namespaces*) override;
    void appendStartTag(Node&, Namespaces* = nullptr) override;
    void appendEndTag(const Element&) override;

private:
    void appendAttributeValue(StringBuilder& out, const String& attributeValue);
    void appendRewrittenAttribute(
        StringBuilder& out,
        const Element&,
        const String& attributeName,
        const String& attributeValue);

    FrameSerializer::Delegate& m_delegate;
    Member<const Document> m_document;

    // FIXME: |FrameSerializer| uses |m_nodes| for collecting nodes in document
    // included into serialized text then extracts image, object, etc. The size
    // of this vector isn't small for large document. It is better to use
    // callback like functionality.
    HeapVector<Member<Node>>& m_nodes;

    // Elements with links rewritten via appendAttribute method.
    HeapHashSet<Member<const Element>> m_elementsWithRewrittenLinks;
};

SerializerMarkupAccumulator::SerializerMarkupAccumulator(FrameSerializer::Delegate& delegate, const Document& document, HeapVector<Member<Node>>& nodes)
    : MarkupAccumulator(ResolveAllURLs)
    , m_delegate(delegate)
    , m_document(&document)
    , m_nodes(nodes)
{
}

SerializerMarkupAccumulator::~SerializerMarkupAccumulator()
{
}

void SerializerMarkupAccumulator::appendText(StringBuilder& result, Text& text)
{
    Element* parent = text.parentElement();
    if (parent && !shouldIgnoreElement(*parent))
        MarkupAccumulator::appendText(result, text);
}

bool SerializerMarkupAccumulator::shouldIgnoreAttribute(const Attribute& attribute)
{
    return m_delegate.shouldIgnoreAttribute(attribute);
}

void SerializerMarkupAccumulator::appendElement(StringBuilder& result, Element& element, Namespaces* namespaces)
{
    if (!shouldIgnoreElement(element))
        MarkupAccumulator::appendElement(result, element, namespaces);

    // TODO(tiger): Refactor MarkupAccumulator so it is easier to append an element like this, without special cases for XHTML
    if (isHTMLHeadElement(element)) {
        result.append("<meta http-equiv=\"Content-Type\" content=\"");
        appendAttributeValue(result, m_document->suggestedMIMEType());
        result.append("; charset=");
        appendAttributeValue(result, m_document->characterSet());
        if (m_document->isXHTMLDocument())
            result.append("\" />");
        else
            result.append("\">");
    }

    // FIXME: For object (plugins) tags and video tag we could replace them by an image of their current contents.
}

void SerializerMarkupAccumulator::appendAttribute(
    StringBuilder& out,
    const Element& element,
    const Attribute& attribute,
    Namespaces* namespaces)
{
    // Check if link rewriting can affect the attribute.
    bool isLinkAttribute = element.hasLegalLinkAttribute(attribute.name());
    bool isSrcDocAttribute = isHTMLFrameElementBase(element)
        && attribute.name() == HTMLNames::srcdocAttr;
    if (isLinkAttribute || isSrcDocAttribute) {
        // Check if the delegate wants to do link rewriting for the element.
        String newLinkForTheElement;
        if (m_delegate.rewriteLink(element, newLinkForTheElement)) {
            if (isLinkAttribute) {
                // Rewrite element links.
                appendRewrittenAttribute(
                    out, element, attribute.name().toString(), newLinkForTheElement);
            } else {
                ASSERT(isSrcDocAttribute);
                // Emit src instead of srcdoc attribute for frame elements - we want the
                // serialized subframe to use html contents from the link provided by
                // Delegate::rewriteLink rather than html contents from srcdoc
                // attribute.
                appendRewrittenAttribute(
                    out, element, HTMLNames::srcAttr.localName(), newLinkForTheElement);
            }
            return;
        }
    }

    // Fallback to appending the original attribute.
    MarkupAccumulator::appendAttribute(out, element, attribute, namespaces);
}

void SerializerMarkupAccumulator::appendStartTag(Node& node, Namespaces* namespaces)
{
    MarkupAccumulator::appendStartTag(node, namespaces);
    m_nodes.append(&node);
}

void SerializerMarkupAccumulator::appendEndTag(const Element& element)
{
    if (!shouldIgnoreElement(element))
        MarkupAccumulator::appendEndTag(element);
}

void SerializerMarkupAccumulator::appendAttributeValue(
    StringBuilder& out,
    const String& attributeValue)
{
    MarkupFormatter::appendAttributeValue(out, attributeValue, m_document->isHTMLDocument());
}

void SerializerMarkupAccumulator::appendRewrittenAttribute(
    StringBuilder& out,
    const Element& element,
    const String& attributeName,
    const String& attributeValue)
{
    if (m_elementsWithRewrittenLinks.contains(&element))
        return;
    m_elementsWithRewrittenLinks.add(&element);

    // Append the rewritten attribute.
    // TODO(tiger): Refactor MarkupAccumulator so it is easier to append an attribute like this.
    out.append(' ');
    out.append(attributeName);
    out.append("=\"");
    appendAttributeValue(out, attributeValue);
    out.append("\"");
}

// TODO(tiger): Right now there is no support for rewriting URLs inside CSS
// documents which leads to bugs like <https://crbug.com/251898>. Not being
// able to rewrite URLs inside CSS documents means that resources imported from
// url(...) statements in CSS might not work when rewriting links for the
// "Webpage, Complete" method of saving a page. It will take some work but it
// needs to be done if we want to continue to support non-MHTML saved pages.

FrameSerializer::FrameSerializer(
    Vector<SerializedResource>& resources,
    Delegate& delegate)
    : m_resources(&resources)
    , m_delegate(delegate)
{
}

void FrameSerializer::serializeFrame(const LocalFrame& frame)
{
    ASSERT(frame.document());
    Document& document = *frame.document();
    KURL url = document.url();

    // If frame is an image document, add the image and don't continue
    if (document.isImageDocument()) {
        ImageDocument& imageDocument = toImageDocument(document);
        addImageToResources(imageDocument.cachedImage(), url);
        return;
    }

    HeapVector<Member<Node>> serializedNodes;
    SerializerMarkupAccumulator accumulator(m_delegate, document, serializedNodes);
    String text = serializeNodes<EditingStrategy>(accumulator, document, IncludeNode);

    CString frameHTML = document.encoding().encode(text, WTF::EntitiesForUnencodables);
    m_resources->append(SerializedResource(url, document.suggestedMIMEType(), SharedBuffer::create(frameHTML.data(), frameHTML.length())));

    for (Node* node: serializedNodes) {
        ASSERT(node);
        if (!node->isElementNode())
            continue;

        Element& element = toElement(*node);
        // We have to process in-line style as it might contain some resources (typically background images).
        if (element.isStyledElement()) {
            retrieveResourcesForProperties(element.inlineStyle(), document);
            retrieveResourcesForProperties(element.presentationAttributeStyle(), document);
        }

        if (isHTMLImageElement(element)) {
            HTMLImageElement& imageElement = toHTMLImageElement(element);
            KURL url = document.completeURL(imageElement.getAttribute(HTMLNames::srcAttr));
            ImageResource* cachedImage = imageElement.cachedImage();
            addImageToResources(cachedImage, url);
        } else if (isHTMLInputElement(element)) {
            HTMLInputElement& inputElement = toHTMLInputElement(element);
            if (inputElement.type() == InputTypeNames::image && inputElement.imageLoader()) {
                KURL url = inputElement.src();
                ImageResource* cachedImage = inputElement.imageLoader()->image();
                addImageToResources(cachedImage, url);
            }
        } else if (isHTMLLinkElement(element)) {
            HTMLLinkElement& linkElement = toHTMLLinkElement(element);
            if (CSSStyleSheet* sheet = linkElement.sheet()) {
                KURL url = document.completeURL(linkElement.getAttribute(HTMLNames::hrefAttr));
                serializeCSSStyleSheet(*sheet, url);
            }
        } else if (isHTMLStyleElement(element)) {
            HTMLStyleElement& styleElement = toHTMLStyleElement(element);
            if (CSSStyleSheet* sheet = styleElement.sheet())
                serializeCSSStyleSheet(*sheet, KURL());
        }
    }
}

void FrameSerializer::serializeCSSStyleSheet(CSSStyleSheet& styleSheet, const KURL& url)
{
    StringBuilder cssText;
    cssText.append("@charset \"");
    cssText.append(styleSheet.contents()->charset().lower());
    cssText.append("\";\n\n");

    for (unsigned i = 0; i < styleSheet.length(); ++i) {
        CSSRule* rule = styleSheet.item(i);
        String itemText = rule->cssText();
        if (!itemText.isEmpty()) {
            cssText.append(itemText);
            if (i < styleSheet.length() - 1)
                cssText.append("\n\n");
        }

        // Some rules have resources associated with them that we need to retrieve.
        serializeCSSRule(rule);
    }

    if (shouldAddURL(url)) {
        WTF::TextEncoding textEncoding(styleSheet.contents()->charset());
        ASSERT(textEncoding.isValid());
        String textString = cssText.toString();
        CString text = textEncoding.encode(textString, WTF::CSSEncodedEntitiesForUnencodables);
        m_resources->append(SerializedResource(url, String("text/css"), SharedBuffer::create(text.data(), text.length())));
        m_resourceURLs.add(url);
    }
}

void FrameSerializer::serializeCSSRule(CSSRule* rule)
{
    ASSERT(rule->parentStyleSheet()->ownerDocument());
    Document& document = *rule->parentStyleSheet()->ownerDocument();

    switch (rule->type()) {
    case CSSRule::STYLE_RULE:
        retrieveResourcesForProperties(&toCSSStyleRule(rule)->styleRule()->properties(), document);
        break;

    case CSSRule::IMPORT_RULE: {
        CSSImportRule* importRule = toCSSImportRule(rule);
        KURL sheetBaseURL = rule->parentStyleSheet()->baseURL();
        ASSERT(sheetBaseURL.isValid());
        KURL importURL = KURL(sheetBaseURL, importRule->href());
        if (m_resourceURLs.contains(importURL))
            break;
        if (importRule->styleSheet())
            serializeCSSStyleSheet(*importRule->styleSheet(), importURL);
        break;
    }

    // Rules inheriting CSSGroupingRule
    case CSSRule::MEDIA_RULE:
    case CSSRule::SUPPORTS_RULE: {
        CSSRuleList* ruleList = rule->cssRules();
        for (unsigned i = 0; i < ruleList->length(); ++i)
            serializeCSSRule(ruleList->item(i));
        break;
    }

    case CSSRule::FONT_FACE_RULE:
        retrieveResourcesForProperties(&toCSSFontFaceRule(rule)->styleRule()->properties(), document);
        break;

    // Rules in which no external resources can be referenced
    case CSSRule::CHARSET_RULE:
    case CSSRule::PAGE_RULE:
    case CSSRule::KEYFRAMES_RULE:
    case CSSRule::KEYFRAME_RULE:
    case CSSRule::NAMESPACE_RULE:
    case CSSRule::VIEWPORT_RULE:
        break;
    }
}

bool FrameSerializer::shouldAddURL(const KURL& url)
{
    return url.isValid() && !m_resourceURLs.contains(url) && !url.protocolIsData()
        && !m_delegate.shouldSkipResourceWithURL(url);
}

void FrameSerializer::addToResources(const Resource& resource, PassRefPtr<SharedBuffer> data, const KURL& url)
{
    if (m_delegate.shouldSkipResource(resource))
        return;

    if (!data) {
        DLOG(ERROR) << "No data for resource " << url.getString();
        return;
    }

    String mimeType = resource.response().mimeType();
    m_resources->append(SerializedResource(url, mimeType, data));
    m_resourceURLs.add(url);
}

void FrameSerializer::addImageToResources(ImageResource* image, const KURL& url)
{
    if (!image || !image->hasImage() || image->errorOccurred() || !shouldAddURL(url))
        return;

    RefPtr<SharedBuffer> data = image->getImage()->data();
    addToResources(*image, data, url);
}

void FrameSerializer::addFontToResources(FontResource* font)
{
    if (!font || !font->isLoaded() || !font->resourceBuffer() || !shouldAddURL(font->url()))
        return;

    RefPtr<SharedBuffer> data(font->resourceBuffer());

    addToResources(*font, data, font->url());
}

void FrameSerializer::retrieveResourcesForProperties(const StylePropertySet* styleDeclaration, Document& document)
{
    if (!styleDeclaration)
        return;

    // The background-image and list-style-image (for ul or ol) are the CSS properties
    // that make use of images. We iterate to make sure we include any other
    // image properties there might be.
    unsigned propertyCount = styleDeclaration->propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        CSSValue* cssValue = styleDeclaration->propertyAt(i).value();
        retrieveResourcesForCSSValue(*cssValue, document);
    }
}

void FrameSerializer::retrieveResourcesForCSSValue(const CSSValue& cssValue, Document& document)
{
    if (cssValue.isImageValue()) {
        const CSSImageValue& imageValue = toCSSImageValue(cssValue);
        if (imageValue.isCachePending())
            return;
        StyleImage* styleImage = imageValue.cachedImage();
        if (!styleImage || !styleImage->isImageResource())
            return;

        addImageToResources(styleImage->cachedImage(), styleImage->cachedImage()->url());
    } else if (cssValue.isFontFaceSrcValue()) {
        const CSSFontFaceSrcValue& fontFaceSrcValue = toCSSFontFaceSrcValue(cssValue);
        if (fontFaceSrcValue.isLocal()) {
            return;
        }

        addFontToResources(fontFaceSrcValue.fetch(&document));
    } else if (cssValue.isValueList()) {
        const CSSValueList& cssValueList = toCSSValueList(cssValue);
        for (unsigned i = 0; i < cssValueList.length(); i++)
            retrieveResourcesForCSSValue(cssValueList.item(i), document);
    }
}

// Returns MOTW (Mark of the Web) declaration before html tag which is in
// HTML comment, e.g. "<!-- saved from url=(%04d)%s -->"
// See http://msdn2.microsoft.com/en-us/library/ms537628(VS.85).aspx.
String FrameSerializer::markOfTheWebDeclaration(const KURL& url)
{
    StringBuilder builder;
    bool emitsMinus = false;
    CString orignalUrl = url.getString().ascii();
    for (const char* string = orignalUrl.data(); *string; ++string) {
        const char ch = *string;
        if (ch == '-' && emitsMinus) {
            builder.append("%2D");
            emitsMinus = false;
            continue;
        }
        emitsMinus = ch == '-';
        builder.append(ch);
    }
    CString escapedUrl = builder.toString().ascii();
    return String::format("saved from url=(%04d)%s", static_cast<int>(escapedUrl.length()), escapedUrl.data());
}

} // namespace blink
