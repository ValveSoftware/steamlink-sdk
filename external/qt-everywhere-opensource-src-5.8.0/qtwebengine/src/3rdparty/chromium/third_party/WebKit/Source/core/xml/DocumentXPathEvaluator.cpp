/*
 * Copyright (C) 2013 Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/xml/DocumentXPathEvaluator.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/xml/XPathExpression.h"
#include "core/xml/XPathResult.h"

namespace blink {

DocumentXPathEvaluator::DocumentXPathEvaluator()
{
}

DocumentXPathEvaluator& DocumentXPathEvaluator::from(Supplementable<Document>& document)
{
    DocumentXPathEvaluator* cache = static_cast<DocumentXPathEvaluator*>(Supplement<Document>::from(document, supplementName()));
    if (!cache) {
        cache = new DocumentXPathEvaluator;
        Supplement<Document>::provideTo(document, supplementName(), cache);
    }
    return *cache;
}

XPathExpression* DocumentXPathEvaluator::createExpression(Supplementable<Document>& document, const String& expression, XPathNSResolver* resolver, ExceptionState& exceptionState)
{
    DocumentXPathEvaluator& suplement = from(document);
    if (!suplement.m_xpathEvaluator)
        suplement.m_xpathEvaluator = XPathEvaluator::create();
    return suplement.m_xpathEvaluator->createExpression(expression, resolver, exceptionState);
}

XPathNSResolver* DocumentXPathEvaluator::createNSResolver(Supplementable<Document>& document, Node* nodeResolver)
{
    DocumentXPathEvaluator& suplement = from(document);
    if (!suplement.m_xpathEvaluator)
        suplement.m_xpathEvaluator = XPathEvaluator::create();
    return suplement.m_xpathEvaluator->createNSResolver(nodeResolver);
}

XPathResult* DocumentXPathEvaluator::evaluate(Supplementable<Document>& document, const String& expression,
    Node* contextNode, XPathNSResolver* resolver, unsigned short type,
    const ScriptValue&, ExceptionState& exceptionState)
{
    DocumentXPathEvaluator& suplement = from(document);
    if (!suplement.m_xpathEvaluator)
        suplement.m_xpathEvaluator = XPathEvaluator::create();
    return suplement.m_xpathEvaluator->evaluate(expression, contextNode, resolver, type, ScriptValue(), exceptionState);
}

DEFINE_TRACE(DocumentXPathEvaluator)
{
    visitor->trace(m_xpathEvaluator);
    Supplement<Document>::trace(visitor);
}

} // namespace blink
