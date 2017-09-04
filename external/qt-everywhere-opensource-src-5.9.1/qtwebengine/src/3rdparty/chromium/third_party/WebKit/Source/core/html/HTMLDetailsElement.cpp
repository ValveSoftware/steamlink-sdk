/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#include "core/html/HTMLDetailsElement.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/Event.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLSummaryElement.h"
#include "core/html/shadow/DetailsMarkerControl.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/layout/LayoutBlockFlow.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

using namespace HTMLNames;

class FirstSummarySelectFilter final : public HTMLContentSelectFilter {
 public:
  virtual ~FirstSummarySelectFilter() {}

  static FirstSummarySelectFilter* create() {
    return new FirstSummarySelectFilter();
  }

  bool canSelectNode(const HeapVector<Member<Node>, 32>& siblings,
                     int nth) const override {
    if (!siblings[nth]->hasTagName(HTMLNames::summaryTag))
      return false;
    for (int i = nth - 1; i >= 0; --i) {
      if (siblings[i]->hasTagName(HTMLNames::summaryTag))
        return false;
    }
    return true;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { HTMLContentSelectFilter::trace(visitor); }

 private:
  FirstSummarySelectFilter() {}
};

HTMLDetailsElement* HTMLDetailsElement::create(Document& document) {
  HTMLDetailsElement* details = new HTMLDetailsElement(document);
  details->ensureUserAgentShadowRoot();
  return details;
}

HTMLDetailsElement::HTMLDetailsElement(Document& document)
    : HTMLElement(detailsTag, document), m_isOpen(false) {
  UseCounter::count(document, UseCounter::DetailsElement);
}

HTMLDetailsElement::~HTMLDetailsElement() {}

void HTMLDetailsElement::dispatchPendingEvent() {
  dispatchEvent(Event::create(EventTypeNames::toggle));
}

LayoutObject* HTMLDetailsElement::createLayoutObject(const ComputedStyle&) {
  return new LayoutBlockFlow(this);
}

void HTMLDetailsElement::didAddUserAgentShadowRoot(ShadowRoot& root) {
  HTMLSummaryElement* defaultSummary = HTMLSummaryElement::create(document());
  defaultSummary->appendChild(Text::create(
      document(), locale().queryString(WebLocalizedString::DetailsLabel)));

  HTMLContentElement* summary = HTMLContentElement::create(
      document(), FirstSummarySelectFilter::create());
  summary->setIdAttribute(ShadowElementNames::detailsSummary());
  summary->appendChild(defaultSummary);
  root.appendChild(summary);

  HTMLDivElement* content = HTMLDivElement::create(document());
  content->setIdAttribute(ShadowElementNames::detailsContent());
  content->appendChild(HTMLContentElement::create(document()));
  content->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
  root.appendChild(content);
}

Element* HTMLDetailsElement::findMainSummary() const {
  if (HTMLSummaryElement* summary =
          Traversal<HTMLSummaryElement>::firstChild(*this))
    return summary;

  HTMLContentElement* content =
      toHTMLContentElementOrDie(userAgentShadowRoot()->firstChild());
  DCHECK(content->firstChild());
  CHECK(isHTMLSummaryElement(*content->firstChild()));
  return toElement(content->firstChild());
}

void HTMLDetailsElement::parseAttribute(const QualifiedName& name,
                                        const AtomicString& oldValue,
                                        const AtomicString& value) {
  if (name == openAttr) {
    bool oldValue = m_isOpen;
    m_isOpen = !value.isNull();
    if (m_isOpen == oldValue)
      return;

    // Dispatch toggle event asynchronously.
    m_pendingEvent =
        TaskRunnerHelper::get(TaskType::DOMManipulation, &document())
            ->postCancellableTask(
                BLINK_FROM_HERE,
                WTF::bind(&HTMLDetailsElement::dispatchPendingEvent,
                          wrapPersistent(this)));

    Element* content = ensureUserAgentShadowRoot().getElementById(
        ShadowElementNames::detailsContent());
    DCHECK(content);
    if (m_isOpen)
      content->removeInlineStyleProperty(CSSPropertyDisplay);
    else
      content->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);

    // Invalidate the LayoutDetailsMarker in order to turn the arrow signifying
    // if the details element is open or closed.
    Element* summary = findMainSummary();
    DCHECK(summary);

    Element* control = toHTMLSummaryElement(summary)->markerControl();
    if (control && control->layoutObject())
      control->layoutObject()->setShouldDoFullPaintInvalidation();

    return;
  }
  HTMLElement::parseAttribute(name, oldValue, value);
}

void HTMLDetailsElement::toggleOpen() {
  setAttribute(openAttr, m_isOpen ? nullAtom : emptyAtom);
}

bool HTMLDetailsElement::isInteractiveContent() const {
  return true;
}

}  // namespace blink
