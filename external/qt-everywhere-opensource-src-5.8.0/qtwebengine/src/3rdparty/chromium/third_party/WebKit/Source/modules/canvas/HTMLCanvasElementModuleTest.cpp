// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas/HTMLCanvasElementModule.h"

#include "core/dom/DOMNodeIds.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLDocument.h"
#include "core/loader/EmptyClients.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class HTMLCanvasElementModuleTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        Page::PageClients pageClients;
        fillWithEmptyClients(pageClients);
        std::unique_ptr<DummyPageHolder> m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600), &pageClients);
        Persistent<HTMLDocument> m_document = toHTMLDocument(&m_dummyPageHolder->document());
        m_document->documentElement()->setInnerHTML("<body><canvas id='c'></canvas></body>", ASSERT_NO_EXCEPTION);
        m_document->view()->updateAllLifecyclePhases();
        m_canvasElement = toHTMLCanvasElement(m_document->getElementById("c"));
    }

    HTMLCanvasElement& canvasElement() const { return *m_canvasElement; }
    OffscreenCanvas* transferControlToOffscreen(ExceptionState&);

private:
    Persistent<HTMLCanvasElement> m_canvasElement;
};

OffscreenCanvas* HTMLCanvasElementModuleTest::transferControlToOffscreen(ExceptionState& exceptionState)
{
    // This unit test only tests if the Canvas Id is associated correctly, so we exclude the part that
    // creates surface layer bridge because a mojo message pipe cannot be tested using webkit unit tests.
    return HTMLCanvasElementModule::transferControlToOffscreenInternal(canvasElement(), exceptionState);
}

TEST_F(HTMLCanvasElementModuleTest, TransferControlToOffscreen)
{
    NonThrowableExceptionState exceptionState;
    OffscreenCanvas* offscreenCanvas = transferControlToOffscreen(exceptionState);
    int canvasId = offscreenCanvas->getAssociatedCanvasId();
    EXPECT_EQ(canvasId, DOMNodeIds::idForNode(&(canvasElement())));
}

} // namespace blink
