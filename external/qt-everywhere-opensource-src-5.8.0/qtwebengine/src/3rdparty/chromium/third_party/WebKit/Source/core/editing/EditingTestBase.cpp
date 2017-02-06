// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/EditingTestBase.h"

#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"

namespace blink {

EditingTestBase::EditingTestBase()
{
}

EditingTestBase::~EditingTestBase()
{
}

Document& EditingTestBase::document() const
{
    return m_dummyPageHolder->document();
}

void EditingTestBase::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

ShadowRoot* EditingTestBase::createShadowRootForElementWithIDAndSetInnerHTML(TreeScope& scope, const char* hostElementID, const char* shadowRootContent)
{
    ShadowRoot* shadowRoot = scope.getElementById(AtomicString::fromUTF8(hostElementID))->createShadowRootInternal(ShadowRootType::V0, ASSERT_NO_EXCEPTION);
    shadowRoot->setInnerHTML(String::fromUTF8(shadowRootContent), ASSERT_NO_EXCEPTION);
    scope.document().view()->updateAllLifecyclePhases();
    return shadowRoot;
}

void EditingTestBase::setBodyContent(const std::string& bodyContent)
{
    document().body()->setInnerHTML(String::fromUTF8(bodyContent.c_str()), ASSERT_NO_EXCEPTION);
    updateAllLifecyclePhases();
}

ShadowRoot* EditingTestBase::setShadowContent(const char* shadowContent, const char* host)
{
    ShadowRoot* shadowRoot = createShadowRootForElementWithIDAndSetInnerHTML(document(), host, shadowContent);
    return shadowRoot;
}

void EditingTestBase::updateAllLifecyclePhases()
{
    document().view()->updateAllLifecyclePhases();
}

} // namespace blink
