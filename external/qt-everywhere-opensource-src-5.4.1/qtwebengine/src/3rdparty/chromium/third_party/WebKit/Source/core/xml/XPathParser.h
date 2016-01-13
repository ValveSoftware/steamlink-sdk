/*
 * Copyright 2005 Maksim Orlovich <maksim@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef XPathParser_h
#define XPathParser_h

#include "core/xml/XPathPredicate.h"
#include "core/xml/XPathStep.h"

namespace WebCore {

class ExceptionState;
class XPathNSResolver;

namespace XPath {

class Expression;
class LocationPath;
class ParseNode;
class Parser;
class Predicate;

struct Token {
    int type;
    String str;
    Step::Axis axis;
    NumericOp::Opcode numop;
    EqTestOp::Opcode eqop;

    Token(int t) : type(t) { }
    Token(int t, const String& v): type(t), str(v) { }
    Token(int t, Step::Axis v): type(t), axis(v) { }
    Token(int t, NumericOp::Opcode v): type(t), numop(v) { }
    Token(int t, EqTestOp::Opcode v): type(t), eqop(v) { }
};

class Parser {
    WTF_MAKE_NONCOPYABLE(Parser);
    STACK_ALLOCATED();
public:
    Parser();
    ~Parser();

    XPathNSResolver* resolver() const { return m_resolver.get(); }
    bool expandQName(const String& qName, AtomicString& localName, AtomicString& namespaceURI);

    PassOwnPtrWillBeRawPtr<Expression> parseStatement(const String& statement, PassRefPtrWillBeRawPtr<XPathNSResolver>, ExceptionState&);

    static Parser* current() { return currentParser; }

    int lex(void* yylval);

    RawPtrWillBeMember<Expression> m_topExpr;
    bool m_gotNamespaceError;

    void registerParseNode(ParseNode*);
    void unregisterParseNode(ParseNode*);

    void registerPredicateVector(WillBeHeapVector<OwnPtrWillBeMember<Predicate> >*);
    void deletePredicateVector(WillBeHeapVector<OwnPtrWillBeMember<Predicate> >*);

    void registerExpressionVector(WillBeHeapVector<OwnPtrWillBeMember<Expression> >*);
    void deleteExpressionVector(WillBeHeapVector<OwnPtrWillBeMember<Expression> >*);

    void registerString(String*);
    void deleteString(String*);

    void registerNodeTest(Step::NodeTest*);
    void deleteNodeTest(Step::NodeTest*);

private:
    bool isBinaryOperatorContext() const;

    void skipWS();
    Token makeTokenAndAdvance(int type, int advance = 1);
    Token makeTokenAndAdvance(int type, NumericOp::Opcode, int advance = 1);
    Token makeTokenAndAdvance(int type, EqTestOp::Opcode, int advance = 1);
    char peekAheadHelper();
    char peekCurHelper();

    Token lexString();
    Token lexNumber();
    bool lexNCName(String&);
    bool lexQName(String&);

    Token nextToken();
    Token nextTokenInternal();

    void reset(const String& data);

    static Parser* currentParser;

    unsigned m_nextPos;
    String m_data;
    int m_lastTokenType;
    RefPtrWillBeMember<XPathNSResolver> m_resolver;

#if !ENABLE(OILPAN)
    HashSet<ParseNode*> m_parseNodes;
    HashSet<Vector<OwnPtr<Predicate> >*> m_predicateVectors;
    HashSet<Vector<OwnPtr<Expression> >*> m_expressionVectors;
    HashSet<OwnPtr<Step::NodeTest> > m_nodeTests;
#endif
    HashSet<OwnPtr<String> > m_strings;
};

} // namespace XPath

} // namespace WebCore

int xpathyyparse(WebCore::XPath::Parser*);
#endif
