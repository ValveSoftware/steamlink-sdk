/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
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

#ifndef XPathEvaluator_h
#define XPathEvaluator_h

#include "bindings/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ExceptionState;
class Node;
class XPathExpression;
class XPathNSResolver;
class XPathResult;

class XPathEvaluator : public RefCountedWillBeGarbageCollectedFinalized<XPathEvaluator>, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<XPathEvaluator> create()
    {
        return adoptRefWillBeNoop(new XPathEvaluator);
    }

    PassRefPtrWillBeRawPtr<XPathExpression> createExpression(const String& expression, PassRefPtrWillBeRawPtr<XPathNSResolver>, ExceptionState&);
    PassRefPtrWillBeRawPtr<XPathNSResolver> createNSResolver(Node* nodeResolver);
    PassRefPtrWillBeRawPtr<XPathResult> evaluate(const String& expression, Node* contextNode,
        PassRefPtrWillBeRawPtr<XPathNSResolver>, unsigned short type, XPathResult*, ExceptionState&);

    void trace(Visitor*) { }

private:
    XPathEvaluator()
    {
        ScriptWrappable::init(this);
    }
};

}

#endif // XPathEvaluator_h
