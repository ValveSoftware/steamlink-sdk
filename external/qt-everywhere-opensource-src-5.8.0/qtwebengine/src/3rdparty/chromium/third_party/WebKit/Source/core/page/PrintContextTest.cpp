// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/PrintContext.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLElement.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerPainter.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/text/TextStream.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include <memory>

namespace blink {

const int kPageWidth = 800;
const int kPageHeight = 600;

class MockPrintContext : public PrintContext {
public:
    MockPrintContext(LocalFrame* frame) : PrintContext(frame) { }

    void outputLinkedDestinations(GraphicsContext& context, const IntRect& pageRect)
    {
        PrintContext::outputLinkedDestinations(context, pageRect);
    }
};

class MockCanvas : public SkCanvas {
public:
    enum OperationType {
        DrawRect,
        DrawPoint
    };

    struct Operation {
        OperationType type;
        SkRect rect;
    };

    MockCanvas() : SkCanvas(kPageWidth, kPageHeight) { }

    void onDrawAnnotation(const SkRect& rect, const char key[], SkData* value) override
    {
        if (rect.width() == 0 && rect.height() == 0) {
            SkPoint point = getTotalMatrix().mapXY(rect.x(), rect.y());
            Operation operation = {
                DrawPoint, SkRect::MakeXYWH(point.x(), point.y(), 0, 0) };
            m_recordedOperations.append(operation);
        } else {
            Operation operation = { DrawRect, rect };
            getTotalMatrix().mapRect(&operation.rect);
            m_recordedOperations.append(operation);
        }
    }

    const Vector<Operation>& recordedOperations() const { return m_recordedOperations; }

private:
    Vector<Operation> m_recordedOperations;
};

class PrintContextTest : public RenderingTest {
protected:
    explicit PrintContextTest(FrameLoaderClient* frameLoaderClient = nullptr)
        : RenderingTest(frameLoaderClient) { }

    void SetUp() override
    {
        RenderingTest::SetUp();
        m_printContext = new MockPrintContext(document().frame());
    }

    MockPrintContext& printContext() { return *m_printContext.get(); }

    void setBodyInnerHTML(String bodyContent)
    {
        document().body()->setAttribute(HTMLNames::styleAttr, "margin: 0");
        document().body()->setInnerHTML(bodyContent, ASSERT_NO_EXCEPTION);
    }

    void printSinglePage(SkCanvas& canvas)
    {
        IntRect pageRect(0, 0, kPageWidth, kPageHeight);
        printContext().begin(pageRect.width(), pageRect.height());
        document().view()->updateAllLifecyclePhases();
        SkPictureBuilder pictureBuilder(pageRect);
        GraphicsContext& context = pictureBuilder.context();
        context.setPrinting(true);
        document().view()->paintContents(context, GlobalPaintPrinting, pageRect);
        {
            DrawingRecorder recorder(context, *document().layoutView(), DisplayItem::PrintedContentDestinationLocations, pageRect);
            printContext().outputLinkedDestinations(context, pageRect);
        }
        pictureBuilder.endRecording()->playback(&canvas);
        printContext().end();
    }

    static String absoluteBlockHtmlForLink(int x, int y, int width, int height, const char* url, const char* children = nullptr)
    {
        TextStream ts;
        ts << "<a style='position: absolute; left: " << x << "px; top: " << y << "px; width: " << width << "px; height: " << height
            << "px' href='" << url << "'>" << (children ? children : url) << "</a>";
        return ts.release();
    }

    static String inlineHtmlForLink(const char* url, const char* children = nullptr)
    {
        TextStream ts;
        ts << "<a href='" << url << "'>" << (children ? children : url) << "</a>";
        return ts.release();
    }

    static String htmlForAnchor(int x, int y, const char* name, const char* textContent)
    {
        TextStream ts;
        ts << "<a name='" << name << "' style='position: absolute; left: " << x << "px; top: " << y << "px'>" << textContent << "</a>";
        return ts.release();
    }

private:
    std::unique_ptr<DummyPageHolder> m_pageHolder;
    Persistent<MockPrintContext> m_printContext;
};

class PrintContextFrameTest : public PrintContextTest {
public:
    PrintContextFrameTest() : PrintContextTest(SingleChildFrameLoaderClient::create()) { }
};

#define EXPECT_SKRECT_EQ(expectedX, expectedY, expectedWidth, expectedHeight, actualRect) \
    EXPECT_EQ(expectedX, actualRect.x()); \
    EXPECT_EQ(expectedY, actualRect.y()); \
    EXPECT_EQ(expectedWidth, actualRect.width()); \
    EXPECT_EQ(expectedHeight, actualRect.height());

TEST_F(PrintContextTest, LinkTarget)
{
    MockCanvas canvas;
    setBodyInnerHTML(absoluteBlockHtmlForLink(50, 60, 70, 80, "http://www.google.com")
        + absoluteBlockHtmlForLink(150, 160, 170, 180, "http://www.google.com#fragment"));
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(2u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect);
    EXPECT_EQ(MockCanvas::DrawRect, operations[1].type);
    EXPECT_SKRECT_EQ(150, 160, 170, 180, operations[1].rect);
}

TEST_F(PrintContextTest, LinkTargetUnderAnonymousBlockBeforeBlock)
{
    MockCanvas canvas;
    setBodyInnerHTML("<div style='padding-top: 50px'>"
        + inlineHtmlForLink("http://www.google.com", "<img style='width: 111; height: 10'>")
        + "<div> " + inlineHtmlForLink("http://www.google1.com", "<img style='width: 122; height: 20'>") + "</div>"
        + "</div>");
    printSinglePage(canvas);
    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(2u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(0, 50, 111, 10, operations[0].rect);
    EXPECT_EQ(MockCanvas::DrawRect, operations[1].type);
    EXPECT_SKRECT_EQ(0, 60, 122, 20, operations[1].rect);
}

TEST_F(PrintContextTest, LinkTargetContainingABlock)
{
    MockCanvas canvas;
    setBodyInnerHTML("<div style='padding-top: 50px'>"
        + inlineHtmlForLink("http://www.google2.com", "<div style='width:133; height: 30'>BLOCK</div>")
        + "</div>");
    printSinglePage(canvas);
    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(1u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(0, 50, 133, 30, operations[0].rect);
}

TEST_F(PrintContextTest, LinkTargetUnderInInlines)
{
    MockCanvas canvas;
    setBodyInnerHTML("<span><b><i><img style='width: 40px; height: 40px'><br>"
        + inlineHtmlForLink("http://www.google3.com", "<img style='width: 144px; height: 40px'>")
        + "</i></b></span>");
    printSinglePage(canvas);
    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(1u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(0, 40, 144, 40, operations[0].rect);
}

TEST_F(PrintContextTest, LinkTargetUnderRelativelyPositionedInline)
{
    MockCanvas canvas;
    setBodyInnerHTML(
        + "<span style='position: relative; top: 50px; left: 50px'><b><i><img style='width: 1px; height: 40px'><br>"
        + inlineHtmlForLink("http://www.google3.com", "<img style='width: 155px; height: 50px'>")
        + "</i></b></span>");
    printSinglePage(canvas);
    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(1u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(50, 90, 155, 50, operations[0].rect);
}

TEST_F(PrintContextTest, LinkTargetSvg)
{
    MockCanvas canvas;
    setBodyInnerHTML("<svg width='100' height='100'>"
        "<a xlink:href='http://www.w3.org'><rect x='20' y='20' width='50' height='50'/></a>"
        "<text x='10' y='90'><a xlink:href='http://www.google.com'><tspan>google</tspan></a></text>"
        "</svg>");
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(2u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(20, 20, 50, 50, operations[0].rect);
    EXPECT_EQ(MockCanvas::DrawRect, operations[1].type);
    EXPECT_EQ(10, operations[1].rect.x());
    EXPECT_GE(90, operations[1].rect.y());
}

TEST_F(PrintContextTest, LinkedTarget)
{
    MockCanvas canvas;
    document().setBaseURLOverride(KURL(ParsedURLString, "http://a.com/"));
    setBodyInnerHTML(absoluteBlockHtmlForLink(50, 60, 70, 80, "#fragment") // Generates a Link_Named_Dest_Key annotation
        + absoluteBlockHtmlForLink(150, 160, 170, 180, "#not-found") // Generates no annotation
        + htmlForAnchor(250, 260, "fragment", "fragment") // Generates a Define_Named_Dest_Key annotation
        + htmlForAnchor(350, 360, "fragment-not-used", "fragment-not-used")); // Generates no annotation
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(2u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect);
    EXPECT_EQ(MockCanvas::DrawPoint, operations[1].type);
    EXPECT_SKRECT_EQ(250, 260, 0, 0, operations[1].rect);
}

TEST_F(PrintContextTest, EmptyLinkedTarget)
{
    MockCanvas canvas;
    document().setBaseURLOverride(KURL(ParsedURLString, "http://a.com/"));
    setBodyInnerHTML(absoluteBlockHtmlForLink(50, 60, 70, 80, "#fragment")
        + htmlForAnchor(250, 260, "fragment", ""));
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(2u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect);
    EXPECT_EQ(MockCanvas::DrawPoint, operations[1].type);
    EXPECT_SKRECT_EQ(250, 260, 0, 0, operations[1].rect);
}

TEST_F(PrintContextTest, LinkTargetBoundingBox)
{
    MockCanvas canvas;
    setBodyInnerHTML(absoluteBlockHtmlForLink(50, 60, 70, 20, "http://www.google.com", "<img style='width: 200px; height: 100px'>"));
    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(1u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(50, 60, 200, 100, operations[0].rect);
}

TEST_F(PrintContextFrameTest, WithSubframe)
{
    MockCanvas canvas;
    document().setBaseURLOverride(KURL(ParsedURLString, "http://a.com/"));
    setBodyInnerHTML("<style>::-webkit-scrollbar { display: none }</style>"
        "<iframe id='frame' src='http://b.com/' width='500' height='500'"
        " style='border-width: 5px; margin: 5px; position: absolute; top: 90px; left: 90px'></iframe>");

    setupChildIframe("frame", absoluteBlockHtmlForLink(50, 60, 70, 80, "#fragment")
        + absoluteBlockHtmlForLink(150, 160, 170, 180, "http://www.google.com")
        + absoluteBlockHtmlForLink(250, 260, 270, 280, "http://www.google.com#fragment"));

    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(2u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(250, 260, 170, 180, operations[0].rect);
    EXPECT_EQ(MockCanvas::DrawRect, operations[1].type);
    EXPECT_SKRECT_EQ(350, 360, 270, 280, operations[1].rect);
}

TEST_F(PrintContextFrameTest, WithScrolledSubframe)
{
    MockCanvas canvas;
    document().setBaseURLOverride(KURL(ParsedURLString, "http://a.com/"));
    setBodyInnerHTML("<style>::-webkit-scrollbar { display: none }</style>"
        "<iframe id='frame' src='http://b.com/' width='500' height='500'"
        " style='border-width: 5px; margin: 5px; position: absolute; top: 90px; left: 90px'></iframe>");

    Document& frameDocument = setupChildIframe("frame", absoluteBlockHtmlForLink(10, 10, 20, 20, "http://invisible.com")
        + absoluteBlockHtmlForLink(50, 60, 70, 80, "http://partly.visible.com")
        + absoluteBlockHtmlForLink(150, 160, 170, 180, "http://www.google.com")
        + absoluteBlockHtmlForLink(250, 260, 270, 280, "http://www.google.com#fragment")
        + absoluteBlockHtmlForLink(850, 860, 70, 80, "http://another.invisible.com"));

    frameDocument.domWindow()->scrollTo(100, 100);

    printSinglePage(canvas);

    const Vector<MockCanvas::Operation>& operations = canvas.recordedOperations();
    ASSERT_EQ(3u, operations.size());
    EXPECT_EQ(MockCanvas::DrawRect, operations[0].type);
    EXPECT_SKRECT_EQ(50, 60, 70, 80, operations[0].rect); // FIXME: the rect should be clipped.
    EXPECT_EQ(MockCanvas::DrawRect, operations[1].type);
    EXPECT_SKRECT_EQ(150, 160, 170, 180, operations[1].rect);
    EXPECT_EQ(MockCanvas::DrawRect, operations[2].type);
    EXPECT_SKRECT_EQ(250, 260, 270, 280, operations[2].rect);
}

} // namespace blink
