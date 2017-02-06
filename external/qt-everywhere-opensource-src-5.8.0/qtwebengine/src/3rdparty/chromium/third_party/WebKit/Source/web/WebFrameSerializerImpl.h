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

#ifndef WebFrameSerializerImpl_h
#define WebFrameSerializerImpl_h

#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/web/WebFrameSerializer.h"
#include "public/web/WebFrameSerializerClient.h"
#include "web/WebEntities.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace WTF {
class TextEncoding;
}

namespace blink {

class Document;
class Element;
class Node;
class WebLocalFrame;
class WebLocalFrameImpl;

// Responsible for serializing the specified frame into html
// (replacing links with paths to local files).
class WebFrameSerializerImpl {
    STACK_ALLOCATED();
public:
    // Do serialization action.
    //
    // Returns false to indicate that no data has been serialized (i.e. because
    // the target frame didn't have a valid url).
    //
    // Synchronously calls WebFrameSerializerClient methods to report
    // serialization results.  See WebFrameSerializerClient comments for more
    // details.
    bool serialize();

    // The parameter specifies which frame need to be serialized.
    // The parameter delegate specifies the pointer of interface
    // DomSerializerDelegate provide sink interface which can receive the
    // individual chunks of data to be saved.
    WebFrameSerializerImpl(
        WebLocalFrame*,
        WebFrameSerializerClient*,
        WebFrameSerializer::LinkRewritingDelegate*);

private:
    // Specified frame which need to be serialized;
    Member<WebLocalFrameImpl> m_specifiedWebLocalFrameImpl;
    // Pointer of WebFrameSerializerClient
    WebFrameSerializerClient* m_client;
    // Pointer of WebFrameSerializer::LinkRewritingDelegate
    WebFrameSerializer::LinkRewritingDelegate* m_delegate;
    // Data buffer for saving result of serialized DOM data.
    StringBuilder m_dataBuffer;

    // Web entities conversion maps.
    WebEntities m_htmlEntities;
    WebEntities m_xmlEntities;

    class SerializeDomParam {
        STACK_ALLOCATED();
    public:
        SerializeDomParam(const KURL&, const WTF::TextEncoding&, Document*);

        const KURL& url;
        const WTF::TextEncoding& textEncoding;
        Member<Document> document;
        bool isHTMLDocument; // document.isHTMLDocument()
        bool haveSeenDocType;
        bool haveAddedCharsetDeclaration;
        // This meta element need to be skipped when serializing DOM.
        Member<const Element> skipMetaElement;
        bool haveAddedXMLProcessingDirective;
        // Flag indicates whether we have added additional contents before end tag.
        // This flag will be re-assigned in each call of function
        // PostActionAfterSerializeOpenTag and it could be changed in function
        // PreActionBeforeSerializeEndTag if the function adds new contents into
        // serialization stream.
        bool haveAddedContentsBeforeEnd;
    };

    // Before we begin serializing open tag of a element, we give the target
    // element a chance to do some work prior to add some additional data.
    WTF::String preActionBeforeSerializeOpenTag(
        const Element*,
        SerializeDomParam*,
        bool* needSkip);

    // After we finish serializing open tag of a element, we give the target
    // element a chance to do some post work to add some additional data.
    WTF::String postActionAfterSerializeOpenTag(
        const Element*,
        SerializeDomParam*);

    // Before we begin serializing end tag of a element, we give the target
    // element a chance to do some work prior to add some additional data.
    WTF::String preActionBeforeSerializeEndTag(
        const Element*,
        SerializeDomParam*,
        bool* needSkip);

    // After we finish serializing end tag of a element, we give the target
    // element a chance to do some post work to add some additional data.
    WTF::String postActionAfterSerializeEndTag(
        const Element*,
        SerializeDomParam*);

    // Save generated html content to data buffer.
    void saveHTMLContentToBuffer(
        const WTF::String& content,
        SerializeDomParam*);

    enum FlushOption {
        ForceFlush,
        DoNotForceFlush,
    };

    // Flushes the content buffer by encoding and sending the content to the
    // WebFrameSerializerClient. Content is not flushed if the buffer is not full
    // unless force is 1.
    void encodeAndFlushBuffer(
        WebFrameSerializerClient::FrameSerializationStatus,
        SerializeDomParam*,
        FlushOption);

    // Serialize open tag of an specified element.
    void openTagToString(
        Element*,
        SerializeDomParam*);

    // Serialize end tag of an specified element.
    void endTagToString(
        Element*,
        SerializeDomParam*);

    // Build content for a specified node
    void buildContentForNode(
        Node*,
        SerializeDomParam*);

    // Appends attrName="escapedAttrValue" to result.
    void appendAttribute(
        StringBuilder& result,
        bool isHTMLDocument,
        const String& attrName,
        const String& attrValue);
};

} // namespace blink

#endif
