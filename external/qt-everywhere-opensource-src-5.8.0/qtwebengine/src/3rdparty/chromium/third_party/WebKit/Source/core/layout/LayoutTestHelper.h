// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTestHelper_h
#define LayoutTestHelper_h

#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLElement.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "wtf/Allocator.h"
#include <gtest/gtest.h>
#include <memory>

namespace blink {

class RenderingTest : public testing::Test {
    USING_FAST_MALLOC(RenderingTest);
public:
    virtual FrameSettingOverrideFunction settingOverrider() const { return nullptr; }

    RenderingTest(FrameLoaderClient* = nullptr);

protected:
    void SetUp() override;
    void TearDown() override;

    Document& document() const { return m_pageHolder->document(); }

    // Both sets the inner html and runs the document lifecycle.
    void setBodyInnerHTML(const String& htmlContent)
    {
        document().body()->setInnerHTML(htmlContent, ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
    }

    // Returns the Document for the iframe.
    Document& setupChildIframe(const AtomicString& iframeElementId, const String& htmlContentOfIframe);

    // Both enables compositing and runs the document lifecycle.
    void enableCompositing()
    {
        m_pageHolder->page().settings().setAcceleratedCompositingEnabled(true);
        document().view()->setParentVisible(true);
        document().view()->setSelfVisible(true);
        document().view()->updateAllLifecyclePhases();
    }

    LayoutObject* getLayoutObjectByElementId(const char* id) const
    {
        Node* node = document().getElementById(id);
        return node ? node->layoutObject() : nullptr;
    }

private:
    Persistent<LocalFrame> m_subframe;
    Persistent<FrameLoaderClient> m_frameLoaderClient;
    Persistent<FrameLoaderClient> m_childFrameLoaderClient;
    std::unique_ptr<DummyPageHolder> m_pageHolder;
};

class SingleChildFrameLoaderClient final : public EmptyFrameLoaderClient {
public:
    static SingleChildFrameLoaderClient* create() { return new SingleChildFrameLoaderClient; }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_child);
        EmptyFrameLoaderClient::trace(visitor);
    }

    Frame* firstChild() const override { return m_child.get(); }
    Frame* lastChild() const override { return m_child.get(); }

    void setChild(Frame* child) { m_child = child; }

private:
    SingleChildFrameLoaderClient() : m_child(nullptr) { }

    Member<Frame> m_child;
};

class FrameLoaderClientWithParent final : public EmptyFrameLoaderClient {
public:
    static FrameLoaderClientWithParent* create(Frame* parent)
    {
        return new FrameLoaderClientWithParent(parent);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_parent);
        EmptyFrameLoaderClient::trace(visitor);
    }

    Frame* parent() const override { return m_parent.get(); }

private:
    explicit FrameLoaderClientWithParent(Frame* parent) : m_parent(parent) { }

    Member<Frame> m_parent;
};

} // namespace blink

#endif // LayoutTestHelper_h
