/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/markers/DocumentMarkerController.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/markers/RenderedDocumentMarker.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class DocumentMarkerControllerTest : public ::testing::Test {
protected:
    DocumentMarkerControllerTest()
        : m_dummyPageHolder(DummyPageHolder::create(IntSize(800, 600)))
    {
    }

    Document& document() const { return m_dummyPageHolder->document(); }
    DocumentMarkerController& markerController() const { return document().markers(); }

    Text* createTextNode(const char*);
    void markNodeContents(Node*);
    void markNodeContentsWithComposition(Node*);
    void setBodyInnerHTML(const char*);

private:
    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

Text* DocumentMarkerControllerTest::createTextNode(const char* textContents)
{
    return document().createTextNode(String::fromUTF8(textContents));
}

void DocumentMarkerControllerTest::markNodeContents(Node* node)
{
    // Force layoutObjects to be created; TextIterator, which is used in
    // DocumentMarkerControllerTest::addMarker(), needs them.
    document().updateStyleAndLayout();
    auto range = EphemeralRange::rangeOfContents(*node);
    markerController().addMarker(range.startPosition(), range.endPosition(), DocumentMarker::Spelling);
}

void DocumentMarkerControllerTest::markNodeContentsWithComposition(Node* node)
{
    // Force layoutObjects to be created; TextIterator, which is used in
    // DocumentMarkerControllerTest::addMarker(), needs them.
    document().updateStyleAndLayout();
    auto range = EphemeralRange::rangeOfContents(*node);
    markerController().addCompositionMarker(range.startPosition(), range.endPosition(), Color::black, false, Color::black);
}

void DocumentMarkerControllerTest::setBodyInnerHTML(const char* bodyContent)
{
    document().body()->setInnerHTML(String::fromUTF8(bodyContent), ASSERT_NO_EXCEPTION);
}

TEST_F(DocumentMarkerControllerTest, DidMoveToNewDocument)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    Element* parent = toElement(document().body()->firstChild()->firstChild());
    markNodeContents(parent);
    EXPECT_EQ(1u, markerController().markers().size());
    Persistent<Document> anotherDocument = Document::create();
    anotherDocument->adoptNode(parent, ASSERT_NO_EXCEPTION);

    // No more reference to marked node.
    ThreadHeap::collectAllGarbage();
    EXPECT_EQ(0u, markerController().markers().size());
    EXPECT_EQ(0u, anotherDocument->markers().markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByNormalize)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    {
        Element* parent = toElement(document().body()->firstChild()->firstChild());
        parent->appendChild(createTextNode("bar"));
        markNodeContents(parent);
        EXPECT_EQ(2u, markerController().markers().size());
        parent->normalize();
    }
    // No more reference to marked node.
    ThreadHeap::collectAllGarbage();
    EXPECT_EQ(1u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveChildren)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    Element* parent = toElement(document().body()->firstChild()->firstChild());
    markNodeContents(parent);
    EXPECT_EQ(1u, markerController().markers().size());
    parent->removeChildren();
    // No more reference to marked node.
    ThreadHeap::collectAllGarbage();
    EXPECT_EQ(0u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedByRemoveMarked)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    {
        Element* parent = toElement(document().body()->firstChild()->firstChild());
        markNodeContents(parent);
        EXPECT_EQ(1u, markerController().markers().size());
        parent->removeChild(parent->firstChild());
    }
    // No more reference to marked node.
    ThreadHeap::collectAllGarbage();
    EXPECT_EQ(0u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveAncestor)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    {
        Element* parent = toElement(document().body()->firstChild()->firstChild());
        markNodeContents(parent);
        EXPECT_EQ(1u, markerController().markers().size());
        parent->parentNode()->parentNode()->removeChild(parent->parentNode());
    }
    // No more reference to marked node.
    ThreadHeap::collectAllGarbage();
    EXPECT_EQ(0u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveParent)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    {
        Element* parent = toElement(document().body()->firstChild()->firstChild());
        markNodeContents(parent);
        EXPECT_EQ(1u, markerController().markers().size());
        parent->parentNode()->removeChild(parent);
    }
    // No more reference to marked node.
    ThreadHeap::collectAllGarbage();
    EXPECT_EQ(0u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByReplaceChild)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    {
        Element* parent = toElement(document().body()->firstChild()->firstChild());
        markNodeContents(parent);
        EXPECT_EQ(1u, markerController().markers().size());
        parent->replaceChild(createTextNode("bar"), parent->firstChild());
    }
    // No more reference to marked node.
    ThreadHeap::collectAllGarbage();
    EXPECT_EQ(0u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedBySetInnerHTML)
{
    setBodyInnerHTML("<b><i>foo</i></b>");
    {
        Element* parent = toElement(document().body()->firstChild()->firstChild());
        markNodeContents(parent);
        EXPECT_EQ(1u, markerController().markers().size());
        setBodyInnerHTML("");
    }
    // No more reference to marked node.
    ThreadHeap::collectAllGarbage();
    EXPECT_EQ(0u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, UpdateRenderedRects)
{
    setBodyInnerHTML("<div style='margin: 100px'>foo</div>");
    Element* div = toElement(document().body()->firstChild());
    markNodeContents(div);
    Vector<IntRect> renderedRects = markerController().renderedRectsForMarkers(DocumentMarker::Spelling);
    EXPECT_EQ(1u, renderedRects.size());

    div->setAttribute(HTMLNames::styleAttr, "margin: 200px");
    document().updateStyleAndLayout();
    Vector<IntRect> newRenderedRects = markerController().renderedRectsForMarkers(DocumentMarker::Spelling);
    EXPECT_EQ(1u, newRenderedRects.size());
    EXPECT_NE(renderedRects[0], newRenderedRects[0]);
}

TEST_F(DocumentMarkerControllerTest, UpdateRenderedRectsForComposition)
{
    setBodyInnerHTML("<div style='margin: 100px'>foo</div>");
    Element* div = toElement(document().body()->firstChild());
    markNodeContentsWithComposition(div);
    Vector<IntRect> renderedRects = markerController().renderedRectsForMarkers(DocumentMarker::Composition);
    EXPECT_EQ(1u, renderedRects.size());

    div->setAttribute(HTMLNames::styleAttr, "margin: 200px");
    document().updateStyleAndLayout();
    Vector<IntRect> newRenderedRects = markerController().renderedRectsForMarkers(DocumentMarker::Composition);
    EXPECT_EQ(1u, newRenderedRects.size());
    EXPECT_NE(renderedRects[0], newRenderedRects[0]);
}

TEST_F(DocumentMarkerControllerTest, CompositionMarkersNotMerged)
{
    setBodyInnerHTML("<div style='margin: 100px'>foo</div>");
    Node* text = document().body()->firstChild()->firstChild();
    document().updateStyleAndLayout();
    markerController().addCompositionMarker(Position(text, 0), Position(text, 1), Color::black, false, Color::black);
    markerController().addCompositionMarker(Position(text, 1), Position(text, 3), Color::black, true, Color::black);

    EXPECT_EQ(2u, markerController().markers().size());
}

TEST_F(DocumentMarkerControllerTest, SetMarkerActiveTest)
{
    setBodyInnerHTML("<b>foo</b>");
    Element* bElement = toElement(document().body()->firstChild());
    EphemeralRange ephemeralRange = EphemeralRange::rangeOfContents(*bElement);
    Position startBElement = toPositionInDOMTree(ephemeralRange.startPosition());
    Position endBElement = toPositionInDOMTree(ephemeralRange.endPosition());
    Range* range = Range::create(document(), startBElement, endBElement);
    // Try to make active a marker that doesn't exist.
    EXPECT_FALSE(markerController().setMarkersActive(range, true));

    // Add a marker and try it once more.
    markerController().addTextMatchMarker(range, false);
    EXPECT_EQ(1u, markerController().markers().size());
    EXPECT_TRUE(markerController().setMarkersActive(range, true));
}

} // namespace blink
