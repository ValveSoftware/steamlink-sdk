/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLElementStack_h
#define HTMLElementStack_h

#include "core/html/parser/HTMLStackItem.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"

namespace blink {

class ContainerNode;
class Element;
class QualifiedName;

// NOTE: The HTML5 spec uses a backwards (grows downward) stack.  We're using
// more standard (grows upwards) stack terminology here.
class HTMLElementStack {
  WTF_MAKE_NONCOPYABLE(HTMLElementStack);
  DISALLOW_NEW();

 public:
  HTMLElementStack();
  ~HTMLElementStack();

  class ElementRecord final : public GarbageCollected<ElementRecord> {
    WTF_MAKE_NONCOPYABLE(ElementRecord);

   public:
    Element* element() const { return m_item->element(); }
    ContainerNode* node() const { return m_item->node(); }
    const AtomicString& namespaceURI() const { return m_item->namespaceURI(); }
    HTMLStackItem* stackItem() const { return m_item; }
    void replaceElement(HTMLStackItem*);

    bool isAbove(ElementRecord*) const;

    ElementRecord* next() const { return m_next.get(); }

    DECLARE_TRACE();

   private:
    friend class HTMLElementStack;

    ElementRecord(HTMLStackItem*, ElementRecord*);

    ElementRecord* releaseNext() { return m_next.release(); }
    void setNext(ElementRecord* next) { m_next = next; }

    Member<HTMLStackItem> m_item;
    Member<ElementRecord> m_next;
  };

  unsigned stackDepth() const { return m_stackDepth; }

  // Inlining this function is a (small) performance win on the parsing
  // benchmark.
  Element* top() const {
    ASSERT(m_top->element());
    return m_top->element();
  }

  ContainerNode* topNode() const {
    ASSERT(m_top->node());
    return m_top->node();
  }

  HTMLStackItem* topStackItem() const {
    ASSERT(m_top->stackItem());
    return m_top->stackItem();
  }

  HTMLStackItem* oneBelowTop() const;
  ElementRecord* topRecord() const;
  ElementRecord* find(Element*) const;
  ElementRecord* furthestBlockForFormattingElement(Element*) const;
  ElementRecord* topmost(const AtomicString& tagName) const;

  void insertAbove(HTMLStackItem*, ElementRecord*);

  void push(HTMLStackItem*);
  void pushRootNode(HTMLStackItem*);
  void pushHTMLHtmlElement(HTMLStackItem*);
  void pushHTMLHeadElement(HTMLStackItem*);
  void pushHTMLBodyElement(HTMLStackItem*);

  void pop();
  void popUntil(const AtomicString& tagName);
  void popUntil(Element*);
  void popUntilPopped(const AtomicString& tagName);
  void popUntilPopped(const QualifiedName& tagName) {
    popUntilPopped(tagName.localName());
  }

  void popUntilPopped(Element*);
  void popUntilNumberedHeaderElementPopped();

  // "clear the stack back to a table context" in the spec.
  void popUntilTableScopeMarker();

  // "clear the stack back to a table body context" in the spec.
  void popUntilTableBodyScopeMarker();

  // "clear the stack back to a table row context" in the spec.
  void popUntilTableRowScopeMarker();

  void popUntilForeignContentScopeMarker();
  void popHTMLHeadElement();
  void popHTMLBodyElement();
  void popAll();

  static bool isMathMLTextIntegrationPoint(HTMLStackItem*);
  static bool isHTMLIntegrationPoint(HTMLStackItem*);

  void remove(Element*);
  void removeHTMLHeadElement(Element*);

  bool contains(Element*) const;
  bool contains(const AtomicString& tagName) const;

  bool inScope(Element*) const;
  bool inScope(const AtomicString& tagName) const;
  bool inScope(const QualifiedName&) const;
  bool inListItemScope(const AtomicString& tagName) const;
  bool inListItemScope(const QualifiedName&) const;
  bool inTableScope(const AtomicString& tagName) const;
  bool inTableScope(const QualifiedName&) const;
  bool inButtonScope(const AtomicString& tagName) const;
  bool inButtonScope(const QualifiedName&) const;
  bool inSelectScope(const AtomicString& tagName) const;
  bool inSelectScope(const QualifiedName&) const;

  bool hasNumberedHeaderElementInScope() const;

  bool hasOnlyOneElement() const;
  bool secondElementIsHTMLBodyElement() const;
  bool hasTemplateInHTMLScope() const;
  Element* htmlElement() const;
  Element* headElement() const;
  Element* bodyElement() const;

  ContainerNode* rootNode() const;

  DECLARE_TRACE();

#ifndef NDEBUG
  void show();
#endif

 private:
  void pushCommon(HTMLStackItem*);
  void pushRootNodeCommon(HTMLStackItem*);
  void popCommon();
  void removeNonTopCommon(Element*);

  Member<ElementRecord> m_top;

  // We remember the root node, <head> and <body> as they are pushed. Their
  // ElementRecords keep them alive. The root node is never popped.
  // FIXME: We don't currently require type-specific information about these
  // elements so we haven't yet bothered to plumb the types all the way down
  // through createElement, etc.
  Member<ContainerNode> m_rootNode;
  Member<Element> m_headElement;
  Member<Element> m_bodyElement;
  unsigned m_stackDepth;
};

WILL_NOT_BE_EAGERLY_TRACED_CLASS(HTMLElementStack::ElementRecord);

}  // namespace blink

#endif  // HTMLElementStack_h
