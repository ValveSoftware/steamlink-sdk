// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ElementVisibilityObserver.h"

#include "core/dom/Element.h"
#include "core/dom/IntersectionObserverEntry.h"
#include "wtf/Functional.h"

namespace blink {

ElementVisibilityObserver::ElementVisibilityObserver(Element* element, std::unique_ptr<VisibilityCallback> callback)
    : m_element(element)
    , m_callback(std::move(callback))
{
}

ElementVisibilityObserver::~ElementVisibilityObserver() = default;

void ElementVisibilityObserver::start()
{
    DCHECK(!m_intersectionObserver);
    m_intersectionObserver = IntersectionObserver::create(
        Vector<Length>(), Vector<float>({std::numeric_limits<float>::min()}), &m_element->document(),
        WTF::bind(&ElementVisibilityObserver::onVisibilityChanged, wrapWeakPersistent(this)));
    DCHECK(m_intersectionObserver);
    m_intersectionObserver->observe(m_element);
}

void ElementVisibilityObserver::stop()
{
    DCHECK(m_intersectionObserver);

    m_intersectionObserver->unobserve(m_element);
    m_intersectionObserver = nullptr;
}

DEFINE_TRACE(ElementVisibilityObserver)
{
    visitor->trace(m_element);
    visitor->trace(m_intersectionObserver);
}

void ElementVisibilityObserver::onVisibilityChanged(const HeapVector<Member<IntersectionObserverEntry>>& entries)
{
    bool isVisible = entries.last()->intersectionRatio() > 0.f;
    (*m_callback.get())(isVisible);
}

} // namespace blink
