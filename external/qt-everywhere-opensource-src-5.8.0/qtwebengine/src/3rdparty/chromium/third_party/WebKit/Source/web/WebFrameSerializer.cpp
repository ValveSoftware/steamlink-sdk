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

#include "public/web/WebFrameSerializer.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameSerializer.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "core/html/HTMLAllCollection.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTableElement.h"
#include "core/loader/DocumentLoader.h"
#include "platform/SerializedResource.h"
#include "platform/SharedBuffer.h"
#include "platform/mhtml/MHTMLArchive.h"
#include "platform/mhtml/MHTMLParser.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebVector.h"
#include "public/web/WebDataSource.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameSerializerCacheControlPolicy.h"
#include "public/web/WebFrameSerializerClient.h"
#include "web/WebFrameSerializerImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include "wtf/Assertions.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"
#include "wtf/text/StringConcatenate.h"

namespace blink {

namespace {

class MHTMLFrameSerializerDelegate final : public FrameSerializer::Delegate {
    WTF_MAKE_NONCOPYABLE(MHTMLFrameSerializerDelegate);
public:
    explicit MHTMLFrameSerializerDelegate(WebFrameSerializer::MHTMLPartsGenerationDelegate&);
    bool shouldIgnoreAttribute(const Attribute&) override;
    bool rewriteLink(const Element&, String& rewrittenLink) override;
    bool shouldSkipResourceWithURL(const KURL&) override;
    bool shouldSkipResource(const Resource&) override;

private:
    WebFrameSerializer::MHTMLPartsGenerationDelegate& m_webDelegate;
};

MHTMLFrameSerializerDelegate::MHTMLFrameSerializerDelegate(
    WebFrameSerializer::MHTMLPartsGenerationDelegate& webDelegate)
    : m_webDelegate(webDelegate)
{
}

bool MHTMLFrameSerializerDelegate::shouldIgnoreAttribute(const Attribute& attribute)
{
    // TODO(fgorski): Presence of srcset attribute causes MHTML to not display images, as only the value of src
    // is pulled into the archive. Discarding srcset prevents the problem. Long term we should make sure to MHTML
    // plays nicely with srcset.
    return attribute.localName() == HTMLNames::srcsetAttr;
}

bool MHTMLFrameSerializerDelegate::rewriteLink(
    const Element& element,
    String& rewrittenLink)
{
    if (!element.isFrameOwnerElement())
        return false;

    auto* frameOwnerElement = toHTMLFrameOwnerElement(&element);
    Frame* frame = frameOwnerElement->contentFrame();
    if (!frame)
        return false;

    WebString contentID = m_webDelegate.getContentID(WebFrame::fromFrame(frame));
    if (contentID.isNull())
        return false;

    KURL cidURI = MHTMLParser::convertContentIDToURI(contentID);
    DCHECK(cidURI.isValid());

    if (isHTMLFrameElementBase(&element)) {
        rewrittenLink = cidURI.getString();
        return true;
    }

    if (isHTMLObjectElement(&element)) {
        Document* doc = frameOwnerElement->contentDocument();
        bool isHandledBySerializer = doc->isHTMLDocument()
            || doc->isXHTMLDocument() || doc->isImageDocument();
        if (isHandledBySerializer) {
            rewrittenLink = cidURI.getString();
            return true;
        }
    }

    return false;
}

bool MHTMLFrameSerializerDelegate::shouldSkipResourceWithURL(const KURL& url)
{
    return m_webDelegate.shouldSkipResource(url);
}

bool MHTMLFrameSerializerDelegate::shouldSkipResource(const Resource& resource)
{
    return m_webDelegate.cacheControlPolicy() == WebFrameSerializerCacheControlPolicy::SkipAnyFrameOrResourceMarkedNoStore
        && resource.hasCacheControlNoStoreHeader();
}

bool cacheControlNoStoreHeaderPresent(const WebLocalFrameImpl& webLocalFrameImpl)
{
    const ResourceResponse& response = webLocalFrameImpl.dataSource()->response().toResourceResponse();
    if (response.cacheControlContainsNoStore())
        return true;

    const ResourceRequest& request = webLocalFrameImpl.dataSource()->request().toResourceRequest();
    return request.cacheControlContainsNoStore();
}

bool frameShouldBeSerializedAsMHTML(WebLocalFrame* frame, WebFrameSerializerCacheControlPolicy cacheControlPolicy)
{
    WebLocalFrameImpl* webLocalFrameImpl = toWebLocalFrameImpl(frame);
    DCHECK(webLocalFrameImpl);

    if (cacheControlPolicy == WebFrameSerializerCacheControlPolicy::None)
        return true;

    bool needToCheckNoStore = cacheControlPolicy == WebFrameSerializerCacheControlPolicy::SkipAnyFrameOrResourceMarkedNoStore
        || (!frame->parent() && cacheControlPolicy == WebFrameSerializerCacheControlPolicy::FailForNoStoreMainFrame);

    if (!needToCheckNoStore)
        return true;

    return !cacheControlNoStoreHeaderPresent(*webLocalFrameImpl);
}

} // namespace

WebData WebFrameSerializer::generateMHTMLHeader(
    const WebString& boundary, WebLocalFrame* frame, MHTMLPartsGenerationDelegate* delegate)
{
    DCHECK(frame);
    DCHECK(delegate);

    if (!frameShouldBeSerializedAsMHTML(frame, delegate->cacheControlPolicy()))
        return WebData();

    WebLocalFrameImpl* webLocalFrameImpl = toWebLocalFrameImpl(frame);
    DCHECK(webLocalFrameImpl);

    Document* document = webLocalFrameImpl->frame()->document();

    RefPtr<SharedBuffer> buffer = SharedBuffer::create();
    MHTMLArchive::generateMHTMLHeader(
        boundary, document->title(), document->suggestedMIMEType(),
        *buffer);
    return buffer.release();
}

WebData WebFrameSerializer::generateMHTMLParts(
    const WebString& boundary, WebLocalFrame* webFrame, MHTMLPartsGenerationDelegate* webDelegate)
{
    DCHECK(webFrame);
    DCHECK(webDelegate);

    if (!frameShouldBeSerializedAsMHTML(webFrame, webDelegate->cacheControlPolicy()))
        return WebData();

    // Translate arguments from public to internal blink APIs.
    LocalFrame* frame = toWebLocalFrameImpl(webFrame)->frame();
    MHTMLArchive::EncodingPolicy encodingPolicy = webDelegate->useBinaryEncoding()
        ? MHTMLArchive::EncodingPolicy::UseBinaryEncoding
        : MHTMLArchive::EncodingPolicy::UseDefaultEncoding;

    // Serialize.
    Vector<SerializedResource> resources;
    MHTMLFrameSerializerDelegate coreDelegate(*webDelegate);
    FrameSerializer serializer(resources, coreDelegate);
    serializer.serializeFrame(*frame);

    // Get Content-ID for the frame being serialized.
    String frameContentID = webDelegate->getContentID(webFrame);

    // Encode serializer's output as MHTML.
    RefPtr<SharedBuffer> output = SharedBuffer::create();
    bool isFirstResource = true;
    for (const SerializedResource& resource : resources) {
        // Frame is the 1st resource (see FrameSerializer::serializeFrame doc
        // comment). Frames get a Content-ID header.
        String contentID = isFirstResource ? frameContentID : String();

        MHTMLArchive::generateMHTMLPart(
            boundary, contentID, encodingPolicy, resource, *output);

        isFirstResource = false;
    }
    return output.release();
}

WebData WebFrameSerializer::generateMHTMLFooter(const WebString& boundary)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::create();
    MHTMLArchive::generateMHTMLFooter(boundary, *buffer);
    return buffer.release();
}

bool WebFrameSerializer::serialize(
    WebLocalFrame* frame,
    WebFrameSerializerClient* client,
    WebFrameSerializer::LinkRewritingDelegate* delegate)
{
    WebFrameSerializerImpl serializerImpl(frame, client, delegate);
    return serializerImpl.serialize();
}

WebString WebFrameSerializer::generateMetaCharsetDeclaration(const WebString& charset)
{
    // TODO(yosin) We should call |FrameSerializer::metaCharsetDeclarationOf()|.
    String charsetString = "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=" + static_cast<const String&>(charset) + "\">";
    return charsetString;
}

WebString WebFrameSerializer::generateMarkOfTheWebDeclaration(const WebURL& url)
{
    StringBuilder builder;
    builder.append("\n<!-- ");
    builder.append(FrameSerializer::markOfTheWebDeclaration(url));
    builder.append(" -->\n");
    return builder.toString();
}

WebString WebFrameSerializer::generateBaseTagDeclaration(const WebString& baseTarget)
{
    // TODO(yosin) We should call |FrameSerializer::baseTagDeclarationOf()|.
    if (baseTarget.isEmpty())
        return String("<base href=\".\">");
    String baseString = "<base href=\".\" target=\"" + static_cast<const String&>(baseTarget) + "\">";
    return baseString;
}

} // namespace blink
