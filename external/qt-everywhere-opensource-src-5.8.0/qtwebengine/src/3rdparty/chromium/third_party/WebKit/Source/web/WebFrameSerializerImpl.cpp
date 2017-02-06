/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

// How we handle the base tag better.
// Current status:
// At now the normal way we use to handling base tag is
// a) For those links which have corresponding local saved files, such as
// savable CSS, JavaScript files, they will be written to relative URLs which
// point to local saved file. Why those links can not be resolved as absolute
// file URLs, because if they are resolved as absolute URLs, after moving the
// file location from one directory to another directory, the file URLs will
// be dead links.
// b) For those links which have not corresponding local saved files, such as
// links in A, AREA tags, they will be resolved as absolute URLs.
// c) We comment all base tags when serialzing DOM for the page.
// FireFox also uses above way to handle base tag.
//
// Problem:
// This way can not handle the following situation:
// the base tag is written by JavaScript.
// For example. The page "www.yahoo.com" use
// "document.write('<base href="http://www.yahoo.com/"...');" to setup base URL
// of page when loading page. So when saving page as completed-HTML, we assume
// that we save "www.yahoo.com" to "c:\yahoo.htm". After then we load the saved
// completed-HTML page, then the JavaScript will insert a base tag
// <base href="http://www.yahoo.com/"...> to DOM, so all URLs which point to
// local saved resource files will be resolved as
// "http://www.yahoo.com/yahoo_files/...", which will cause all saved  resource
// files can not be loaded correctly. Also the page will be rendered ugly since
// all saved sub-resource files (such as CSS, JavaScript files) and sub-frame
// files can not be fetched.
// Now FireFox, IE and WebKit based Browser all have this problem.
//
// Solution:
// My solution is that we comment old base tag and write new base tag:
// <base href="." ...> after the previous commented base tag. In WebKit, it
// always uses the latest "href" attribute of base tag to set document's base
// URL. Based on this behavior, when we encounter a base tag, we comment it and
// write a new base tag <base href="."> after the previous commented base tag.
// The new added base tag can help engine to locate correct base URL for
// correctly loading local saved resource files. Also I think we need to inherit
// the base target value from document object when appending new base tag.
// If there are multiple base tags in original document, we will comment all old
// base tags and append new base tag after each old base tag because we do not
// know those old base tags are original content or added by JavaScript. If
// they are added by JavaScript, it means when loading saved page, the script(s)
// will still insert base tag(s) to DOM, so the new added base tag(s) can
// override the incorrect base URL and make sure we alway load correct local
// saved resource files.

#include "web/WebFrameSerializerImpl.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/editing/serializers/Serialization.h"
#include "core/frame/FrameSerializer.h"
#include "core/html/HTMLAllCollection.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "public/platform/WebCString.h"
#include "public/platform/WebVector.h"
#include "web/WebLocalFrameImpl.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

// Maximum length of data buffer which is used to temporary save generated
// html content data. This is a soft limit which might be passed if a very large
// contegious string is found in the html document.
static const unsigned dataBufferCapacity = 65536;

WebFrameSerializerImpl::SerializeDomParam::SerializeDomParam(
    const KURL& url,
    const WTF::TextEncoding& textEncoding,
    Document* document)
    : url(url)
    , textEncoding(textEncoding)
    , document(document)
    , isHTMLDocument(document->isHTMLDocument())
    , haveSeenDocType(false)
    , haveAddedCharsetDeclaration(false)
    , skipMetaElement(nullptr)
    , haveAddedXMLProcessingDirective(false)
    , haveAddedContentsBeforeEnd(false)
{
}

String WebFrameSerializerImpl::preActionBeforeSerializeOpenTag(
    const Element* element, SerializeDomParam* param, bool* needSkip)
{
    StringBuilder result;

    *needSkip = false;
    if (param->isHTMLDocument) {
        // Skip the open tag of original META tag which declare charset since we
        // have overrided the META which have correct charset declaration after
        // serializing open tag of HEAD element.
        DCHECK(element);
        if (isHTMLMetaElement(element) && toHTMLMetaElement(element)->computeEncoding().isValid()) {
            // Found META tag declared charset, we need to skip it when
            // serializing DOM.
            param->skipMetaElement = element;
            *needSkip = true;
        } else if (isHTMLHtmlElement(*element)) {
            // Check something before processing the open tag of HEAD element.
            // First we add doc type declaration if original document has it.
            if (!param->haveSeenDocType) {
                param->haveSeenDocType = true;
                result.append(createMarkup(param->document->doctype()));
            }

            // Add MOTW declaration before html tag.
            // See http://msdn2.microsoft.com/en-us/library/ms537628(VS.85).aspx.
            result.append(WebFrameSerializer::generateMarkOfTheWebDeclaration(param->url));
        } else if (isHTMLBaseElement(*element)) {
            // Comment the BASE tag when serializing dom.
            result.append("<!--");
        }
    } else {
        // Write XML declaration.
        if (!param->haveAddedXMLProcessingDirective) {
            param->haveAddedXMLProcessingDirective = true;
            // Get encoding info.
            String xmlEncoding = param->document->xmlEncoding();
            if (xmlEncoding.isEmpty())
                xmlEncoding = param->document->encodingName();
            if (xmlEncoding.isEmpty())
                xmlEncoding = UTF8Encoding().name();
            result.append("<?xml version=\"");
            result.append(param->document->xmlVersion());
            result.append("\" encoding=\"");
            result.append(xmlEncoding);
            if (param->document->xmlStandalone())
                result.append("\" standalone=\"yes");
            result.append("\"?>\n");
        }
        // Add doc type declaration if original document has it.
        if (!param->haveSeenDocType) {
            param->haveSeenDocType = true;
            result.append(createMarkup(param->document->doctype()));
        }
    }
    return result.toString();
}

String WebFrameSerializerImpl::postActionAfterSerializeOpenTag(
    const Element* element, SerializeDomParam* param)
{
    StringBuilder result;

    param->haveAddedContentsBeforeEnd = false;
    if (!param->isHTMLDocument)
        return result.toString();
    // Check after processing the open tag of HEAD element
    if (!param->haveAddedCharsetDeclaration
        && isHTMLHeadElement(*element)) {
        param->haveAddedCharsetDeclaration = true;
        // Check meta element. WebKit only pre-parse the first 512 bytes of the
        // document. If the whole <HEAD> is larger and meta is the end of head
        // part, then this kind of html documents aren't decoded correctly
        // because of this issue. So when we serialize the DOM, we need to make
        // sure the meta will in first child of head tag.
        // See http://bugs.webkit.org/show_bug.cgi?id=16621.
        // First we generate new content for writing correct META element.
        result.append(WebFrameSerializer::generateMetaCharsetDeclaration(
            String(param->textEncoding.name())));

        param->haveAddedContentsBeforeEnd = true;
        // Will search each META which has charset declaration, and skip them all
        // in PreActionBeforeSerializeOpenTag.
    }

    return result.toString();
}

String WebFrameSerializerImpl::preActionBeforeSerializeEndTag(
    const Element* element, SerializeDomParam* param, bool* needSkip)
{
    String result;

    *needSkip = false;
    if (!param->isHTMLDocument)
        return result;
    // Skip the end tag of original META tag which declare charset.
    // Need not to check whether it's META tag since we guarantee
    // skipMetaElement is definitely META tag if it's not 0.
    if (param->skipMetaElement == element) {
        *needSkip = true;
    }

    return result;
}

// After we finish serializing end tag of a element, we give the target
// element a chance to do some post work to add some additional data.
String WebFrameSerializerImpl::postActionAfterSerializeEndTag(
    const Element* element, SerializeDomParam* param)
{
    StringBuilder result;

    if (!param->isHTMLDocument)
        return result.toString();
    // Comment the BASE tag when serializing DOM.
    if (isHTMLBaseElement(*element)) {
        result.append("-->");
        // Append a new base tag declaration.
        result.append(WebFrameSerializer::generateBaseTagDeclaration(
            param->document->baseTarget()));
    }

    return result.toString();
}

void WebFrameSerializerImpl::saveHTMLContentToBuffer(
    const String& result, SerializeDomParam* param)
{
    m_dataBuffer.append(result);
    encodeAndFlushBuffer(
        WebFrameSerializerClient::CurrentFrameIsNotFinished,
        param,
        DoNotForceFlush);
}

void WebFrameSerializerImpl::encodeAndFlushBuffer(
    WebFrameSerializerClient::FrameSerializationStatus status,
    SerializeDomParam* param,
    FlushOption flushOption)
{
    // Data buffer is not full nor do we want to force flush.
    if (flushOption != ForceFlush && m_dataBuffer.length() <= dataBufferCapacity)
        return;

    String content = m_dataBuffer.toString();
    m_dataBuffer.clear();

    CString encodedContent = param->textEncoding.encode(content, WTF::EntitiesForUnencodables);

    // Send result to the client.
    m_client->didSerializeDataForFrame(WebCString(encodedContent), status);
}

// TODO(yosin): We should utilize |MarkupFormatter| here to share code,
// especially escaping attribute values, done by |WebEntities| |m_htmlEntities|
// and |m_xmlEntities|.
void WebFrameSerializerImpl::appendAttribute(
    StringBuilder& result,
    bool isHTMLDocument,
    const String& attrName,
    const String& attrValue) {
    result.append(' ');
    result.append(attrName);
    result.append("=\"");
    if (isHTMLDocument)
        result.append(m_htmlEntities.convertEntitiesInString(attrValue));
    else
        result.append(m_xmlEntities.convertEntitiesInString(attrValue));
    result.append('\"');
}

void WebFrameSerializerImpl::openTagToString(
    Element* element,
    SerializeDomParam* param)
{
    bool needSkip;
    StringBuilder result;
    // Do pre action for open tag.
    result.append(preActionBeforeSerializeOpenTag(element, param, &needSkip));
    if (needSkip)
        return;
    // Add open tag
    result.append('<');
    result.append(element->nodeName().lower());

    // Find out if we need to do frame-specific link rewriting.
    WebFrame* frame = nullptr;
    if (element->isFrameOwnerElement()) {
        frame = WebFrame::fromFrame(
            toHTMLFrameOwnerElement(element)->contentFrame());
    }
    WebString rewrittenFrameLink;
    bool shouldRewriteFrameSrc =
        frame && m_delegate->rewriteFrameSource(frame, &rewrittenFrameLink);
    bool didRewriteFrameSrc = false;

    // Go through all attributes and serialize them.
    for (const auto& it : element->attributes()) {
        const QualifiedName& attrName = it.name();
        String attrValue = it.value();

        // Skip srcdoc attribute if we will emit src attribute (for frames).
        if (shouldRewriteFrameSrc && attrName == HTMLNames::srcdocAttr)
            continue;

        // Rewrite the attribute value if requested.
        if (element->hasLegalLinkAttribute(attrName)) {
            // For links start with "javascript:", we do not change it.
            if (!attrValue.startsWith("javascript:", TextCaseInsensitive)) {
                // Get the absolute link.
                KURL completeURL = param->document->completeURL(attrValue);

                // Check whether we have a local file to link to.
                WebString rewrittenURL;
                if (shouldRewriteFrameSrc) {
                    attrValue = rewrittenFrameLink;
                    didRewriteFrameSrc = true;
                } else if (m_delegate->rewriteLink(completeURL, &rewrittenURL)) {
                    attrValue = rewrittenURL;
                } else {
                    attrValue = completeURL;
                }
            }
        }

        appendAttribute(result, param->isHTMLDocument, attrName.toString(), attrValue);
    }

    // For frames where link rewriting was requested, ensure that src attribute
    // is written even if the original document didn't have that attribute
    // (mainly needed for iframes with srcdoc, but with no src attribute).
    if (shouldRewriteFrameSrc && !didRewriteFrameSrc && isHTMLIFrameElement(element)) {
        appendAttribute(
            result, param->isHTMLDocument, HTMLNames::srcAttr.toString(), rewrittenFrameLink);
    }

    // Do post action for open tag.
    String addedContents = postActionAfterSerializeOpenTag(element, param);
    // Complete the open tag for element when it has child/children.
    if (element->hasChildren() || param->haveAddedContentsBeforeEnd)
        result.append('>');
    // Append the added contents generate in  post action of open tag.
    result.append(addedContents);
    // Save the result to data buffer.
    saveHTMLContentToBuffer(result.toString(), param);
}

// Serialize end tag of an specified element.
void WebFrameSerializerImpl::endTagToString(
    Element* element,
    SerializeDomParam* param)
{
    bool needSkip;
    StringBuilder result;
    // Do pre action for end tag.
    result.append(preActionBeforeSerializeEndTag(element, param, &needSkip));
    if (needSkip)
        return;
    // Write end tag when element has child/children.
    if (element->hasChildren() || param->haveAddedContentsBeforeEnd) {
        result.append("</");
        result.append(element->nodeName().lower());
        result.append('>');
    } else {
        // Check whether we have to write end tag for empty element.
        if (param->isHTMLDocument) {
            result.append('>');
            // FIXME: This code is horribly wrong.  WebFrameSerializerImpl must die.
            if (!element->isHTMLElement() || !toHTMLElement(element)->ieForbidsInsertHTML()) {
                // We need to write end tag when it is required.
                result.append("</");
                result.append(element->nodeName().lower());
                result.append('>');
            }
        } else {
            // For xml base document.
            result.append(" />");
        }
    }
    // Do post action for end tag.
    result.append(postActionAfterSerializeEndTag(element, param));
    // Save the result to data buffer.
    saveHTMLContentToBuffer(result.toString(), param);
}

void WebFrameSerializerImpl::buildContentForNode(
    Node* node,
    SerializeDomParam* param)
{
    switch (node->getNodeType()) {
    case Node::ELEMENT_NODE:
        // Process open tag of element.
        openTagToString(toElement(node), param);
        // Walk through the children nodes and process it.
        for (Node *child = node->firstChild(); child; child = child->nextSibling())
            buildContentForNode(child, param);
        // Process end tag of element.
        endTagToString(toElement(node), param);
        break;
    case Node::TEXT_NODE:
        saveHTMLContentToBuffer(createMarkup(node), param);
        break;
    case Node::ATTRIBUTE_NODE:
    case Node::DOCUMENT_NODE:
    case Node::DOCUMENT_FRAGMENT_NODE:
        // Should not exist.
        NOTREACHED();
        break;
    // Document type node can be in DOM?
    case Node::DOCUMENT_TYPE_NODE:
        param->haveSeenDocType = true;
    default:
        // For other type node, call default action.
        saveHTMLContentToBuffer(createMarkup(node), param);
        break;
    }
}

WebFrameSerializerImpl::WebFrameSerializerImpl(
    WebLocalFrame* frame,
    WebFrameSerializerClient* client,
    WebFrameSerializer::LinkRewritingDelegate* delegate)
    : m_client(client)
    , m_delegate(delegate)
    , m_htmlEntities(false)
    , m_xmlEntities(true)
{
    // Must specify available webframe.
    DCHECK(frame);
    m_specifiedWebLocalFrameImpl = toWebLocalFrameImpl(frame);
    // Make sure we have non null client and delegate.
    DCHECK(client);
    DCHECK(delegate);

    DCHECK(m_dataBuffer.isEmpty());
}

bool WebFrameSerializerImpl::serialize()
{
    bool didSerialization = false;

    Document* document = m_specifiedWebLocalFrameImpl->frame()->document();
    const KURL& url = document->url();

    if (url.isValid()) {
        didSerialization = true;

        const WTF::TextEncoding& textEncoding = document->encoding().isValid() ? document->encoding() : UTF8Encoding();
        if (textEncoding.isNonByteBasedEncoding()) {
            const UChar byteOrderMark = 0xFEFF;
            m_dataBuffer.append(byteOrderMark);
        }

        SerializeDomParam param(url, textEncoding, document);

        Element* documentElement = document->documentElement();
        if (documentElement)
            buildContentForNode(documentElement, &param);

        encodeAndFlushBuffer(WebFrameSerializerClient::CurrentFrameIsFinished, &param, ForceFlush);
    } else {
        // Report empty contents for invalid URLs.
        m_client->didSerializeDataForFrame(
            WebCString(), WebFrameSerializerClient::CurrentFrameIsFinished);
    }

    DCHECK(m_dataBuffer.isEmpty());
    return didSerialization;
}

} // namespace blink
