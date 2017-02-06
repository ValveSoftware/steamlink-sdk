/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef XMLDocumentParser_h
#define XMLDocumentParser_h

#include "core/dom/ParserContentPolicy.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/fetch/ResourceClient.h"
#include "core/fetch/ScriptResource.h"
#include "core/xml/parser/XMLErrors.h"
#include "platform/heap/Handle.h"
#include "platform/text/SegmentedString.h"
#include "wtf/HashMap.h"
#include "wtf/RefCounted.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringHash.h"
#include <libxml/tree.h>
#include <memory>

namespace blink {

class ContainerNode;
class ResourceFetcher;
class DocumentFragment;
class Document;
class Element;
class FrameView;
class Text;

class XMLParserContext : public RefCounted<XMLParserContext> {
public:
    static PassRefPtr<XMLParserContext> createMemoryParser(xmlSAXHandlerPtr, void* userData, const CString& chunk);
    static PassRefPtr<XMLParserContext> createStringParser(xmlSAXHandlerPtr, void* userData);
    ~XMLParserContext();
    xmlParserCtxtPtr context() const { return m_context; }

private:
    XMLParserContext(xmlParserCtxtPtr context)
        : m_context(context)
    {
    }

    xmlParserCtxtPtr m_context;
};

class XMLDocumentParser final : public ScriptableDocumentParser, public ScriptResourceClient {
    USING_GARBAGE_COLLECTED_MIXIN(XMLDocumentParser);
public:
    static XMLDocumentParser* create(Document& document, FrameView* view)
    {
        return new XMLDocumentParser(document, view);
    }
    static XMLDocumentParser* create(DocumentFragment* fragment, Element* element, ParserContentPolicy parserContentPolicy)
    {
        return new XMLDocumentParser(fragment, element, parserContentPolicy);
    }
    ~XMLDocumentParser() override;
    DECLARE_VIRTUAL_TRACE();

    // Exposed for callbacks:
    void handleError(XMLErrors::ErrorType, const char* message, TextPosition);

    void setIsXHTMLDocument(bool isXHTML) { m_isXHTMLDocument = isXHTML; }
    bool isXHTMLDocument() const { return m_isXHTMLDocument; }

    bool isCurrentlyParsing8BitChunk() { return m_isCurrentlyParsing8BitChunk; }

    static bool parseDocumentFragment(const String&, DocumentFragment*, Element* parent = 0, ParserContentPolicy = AllowScriptingContent);

    // Used by the XMLHttpRequest to check if the responseXML was well formed.
    bool wellFormed() const override { return !m_sawError; }

    TextPosition textPosition() const override;

    static bool supportsXMLVersion(const String&);

    class PendingCallback {
        USING_FAST_MALLOC(PendingCallback);
    public:
        virtual ~PendingCallback() { }
        virtual void call(XMLDocumentParser*) = 0;
    };

    void setScriptStartPosition(TextPosition);

private:
    explicit XMLDocumentParser(Document&, FrameView* = 0);
    XMLDocumentParser(DocumentFragment*, Element*, ParserContentPolicy);

    // From DocumentParser
    void insert(const SegmentedString&) override;
    void append(const String&) override;
    void finish() override;
    bool isWaitingForScripts() const override;
    void stopParsing() override;
    void detach() override;
    OrdinalNumber lineNumber() const override;
    OrdinalNumber columnNumber() const;

    // from ResourceClient
    void notifyFinished(Resource*) override;
    String debugName() const override { return "XMLDocumentParser"; }

    void end();

    void pauseParsing();
    void resumeParsing();

    bool appendFragmentSource(const String&);

public:
    // Callbacks from parser SAX
    void error(XMLErrors::ErrorType, const char* message, va_list args) WTF_ATTRIBUTE_PRINTF(3, 0);
    void startElementNs(const AtomicString& localName, const AtomicString& prefix, const AtomicString& uri, int namespaceCount,
        const xmlChar** namespaces, int attributeCount, int defaultedCount, const xmlChar** libxmlAttributes);
    void endElementNs();
    void characters(const xmlChar* chars, int length);
    void processingInstruction(const String& target, const String& data);
    void cdataBlock(const String&);
    void comment(const String&);
    void startDocument(const String& version, const String& encoding, int standalone);
    void internalSubset(const String& name, const String& externalID, const String& systemID);
    void endDocument();

private:
    void initializeParserContext(const CString& chunk = CString());

    void pushCurrentNode(ContainerNode*);
    void popCurrentNode();
    void clearCurrentNodeStack();

    void insertErrorMessageBlock();

    void createLeafTextNodeIfNeeded();
    bool updateLeafTextNode();

    void doWrite(const String&);
    void doEnd();

    bool m_hasView;

    SegmentedString m_originalSourceForTransform;

    xmlParserCtxtPtr context() const { return m_context ? m_context->context() : 0; }
    RefPtr<XMLParserContext> m_context;
    Deque<std::unique_ptr<PendingCallback>> m_pendingCallbacks;
    Vector<xmlChar> m_bufferedText;

    Member<ContainerNode> m_currentNode;
    HeapVector<Member<ContainerNode>> m_currentNodeStack;

    Member<Text> m_leafTextNode;

    bool m_isCurrentlyParsing8BitChunk;
    bool m_sawError;
    bool m_sawCSS;
    bool m_sawXSLTransform;
    bool m_sawFirstElement;
    bool m_isXHTMLDocument;
    bool m_parserPaused;
    bool m_requestingScript;
    bool m_finishCalled;

    XMLErrors m_xmlErrors;

    Member<ScriptResource> m_pendingScript;
    Member<Element> m_scriptElement;
    TextPosition m_scriptStartPosition;

    bool m_parsingFragment;
    AtomicString m_defaultNamespaceURI;

    typedef HashMap<AtomicString, AtomicString> PrefixForNamespaceMap;
    PrefixForNamespaceMap m_prefixToNamespaceMap;
    SegmentedString m_pendingSrc;
};

xmlDocPtr xmlDocPtrForString(Document*, const String& source, const String& url);
HashMap<String, String> parseAttributes(const String&, bool& attrsOK);

} // namespace blink

#endif // XMLDocumentParser_h
