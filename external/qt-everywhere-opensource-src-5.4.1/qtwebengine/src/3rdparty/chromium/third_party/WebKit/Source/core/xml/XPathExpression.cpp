/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/xml/XPathExpression.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/xml/XPathExpressionNode.h"
#include "core/xml/XPathNSResolver.h"
#include "core/xml/XPathParser.h"
#include "core/xml/XPathResult.h"
#include "core/xml/XPathUtil.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

using namespace XPath;

XPathExpression::XPathExpression()
{
    ScriptWrappable::init(this);
}

PassRefPtrWillBeRawPtr<XPathExpression> XPathExpression::createExpression(const String& expression, PassRefPtrWillBeRawPtr<XPathNSResolver> resolver, ExceptionState& exceptionState)
{
    RefPtrWillBeRawPtr<XPathExpression> expr = XPathExpression::create();
    Parser parser;

    expr->m_topExpression = parser.parseStatement(expression, resolver, exceptionState);
    if (!expr->m_topExpression)
        return nullptr;

    return expr.release();
}

XPathExpression::~XPathExpression()
{
}

void XPathExpression::trace(Visitor* visitor)
{
    visitor->trace(m_topExpression);
}

PassRefPtrWillBeRawPtr<XPathResult> XPathExpression::evaluate(Node* contextNode, unsigned short type, XPathResult*, ExceptionState& exceptionState)
{
    if (!contextNode) {
        exceptionState.throwDOMException(NotSupportedError, "The context node provided is null.");
        return nullptr;
    }

    if (!isValidContextNode(contextNode)) {
        exceptionState.throwDOMException(NotSupportedError, "The node provided is '" + contextNode->nodeName() + "', which is not a valid context node type.");
        return nullptr;
    }

    EvaluationContext& evaluationContext = Expression::evaluationContext();
    evaluationContext.node = contextNode;
    evaluationContext.size = 1;
    evaluationContext.position = 1;
    evaluationContext.hadTypeConversionError = false;
    RefPtrWillBeRawPtr<XPathResult> result = XPathResult::create(&contextNode->document(), m_topExpression->evaluate());
    evaluationContext.node = nullptr; // Do not hold a reference to the context node, as this may prevent the whole document from being destroyed in time.

    if (evaluationContext.hadTypeConversionError) {
        // It is not specified what to do if type conversion fails while evaluating an expression.
        exceptionState.throwDOMException(SyntaxError, "Type conversion failed while evaluating the expression.");
        return nullptr;
    }

    if (type != XPathResult::ANY_TYPE) {
        result->convertTo(type, exceptionState);
        if (exceptionState.hadException())
            return nullptr;
    }

    return result;
}

}
