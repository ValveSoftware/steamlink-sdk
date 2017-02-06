// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebFrameContentDumper.h"

#include "core/editing/EphemeralRange.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/serializers/Serialization.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutTreeAsText.h"
#include "core/layout/api/LayoutViewItem.h"
#include "public/web/WebDocument.h"
#include "public/web/WebLocalFrame.h"
#include "public/web/WebView.h"
#include "web/WebLocalFrameImpl.h"
#include "wtf/text/WTFString.h"

namespace blink {

static void frameContentAsPlainText(size_t maxChars, LocalFrame* frame, StringBuilder& output)
{
    Document* document = frame->document();
    if (!document)
        return;

    if (!frame->view() || frame->view()->shouldThrottleRendering())
        return;

    DCHECK(!frame->view()->needsLayout());
    DCHECK(!document->needsLayoutTreeUpdate());

    // Select the document body.
    if (document->body()) {
        const EphemeralRange range = EphemeralRange::rangeOfContents(*document->body());

        // The text iterator will walk nodes giving us text. This is similar to
        // the plainText() function in core/editing/TextIterator.h, but we implement the maximum
        // size and also copy the results directly into a wstring, avoiding the
        // string conversion.
        for (TextIterator it(range.startPosition(), range.endPosition()); !it.atEnd(); it.advance()) {
            it.text().appendTextToStringBuilder(output, 0, maxChars - output.length());
            if (output.length() >= maxChars)
                return; // Filled up the buffer.
        }
    }

    // The separator between frames when the frames are converted to plain text.
    const LChar frameSeparator[] = { '\n', '\n' };
    const size_t frameSeparatorLength = WTF_ARRAY_LENGTH(frameSeparator);

    // Recursively walk the children.
    const FrameTree& frameTree = frame->tree();
    for (Frame* curChild = frameTree.firstChild(); curChild; curChild = curChild->tree().nextSibling()) {
        if (!curChild->isLocalFrame())
            continue;
        LocalFrame* curLocalChild = toLocalFrame(curChild);
        // Ignore the text of non-visible frames.
        LayoutViewItem contentLayoutItem = curLocalChild->contentLayoutItem();
        LayoutPart* ownerLayoutObject = curLocalChild->ownerLayoutObject();
        if (contentLayoutItem.isNull() || !contentLayoutItem.size().width() || !contentLayoutItem.size().height()
            || (contentLayoutItem.location().x() + contentLayoutItem.size().width() <= 0) || (contentLayoutItem.location().y() + contentLayoutItem.size().height() <= 0)
            || (ownerLayoutObject && ownerLayoutObject->style() && ownerLayoutObject->style()->visibility() != VISIBLE)) {
            continue;
        }

        // Make sure the frame separator won't fill up the buffer, and give up if
        // it will. The danger is if the separator will make the buffer longer than
        // maxChars. This will cause the computation above:
        //   maxChars - output->size()
        // to be a negative number which will crash when the subframe is added.
        if (output.length() >= maxChars - frameSeparatorLength)
            return;

        output.append(frameSeparator, frameSeparatorLength);
        frameContentAsPlainText(maxChars, curLocalChild, output);
        if (output.length() >= maxChars)
            return; // Filled up the buffer.
    }
}

WebString WebFrameContentDumper::deprecatedDumpFrameTreeAsText(WebLocalFrame* frame, size_t maxChars)
{
    if (!frame)
        return WebString();
    StringBuilder text;
    frameContentAsPlainText(maxChars, toWebLocalFrameImpl(frame)->frame(), text);
    return text.toString();
}

WebString WebFrameContentDumper::dumpWebViewAsText(WebView* webView, size_t maxChars)
{
    DCHECK(webView);
    webView->updateAllLifecyclePhases();
    return WebFrameContentDumper::deprecatedDumpFrameTreeAsText(webView->mainFrame()->toWebLocalFrame(), maxChars);
}

WebString WebFrameContentDumper::dumpAsMarkup(WebLocalFrame* frame)
{
    if (!frame)
        return WebString();
    return createMarkup(toWebLocalFrameImpl(frame)->frame()->document());
}

WebString WebFrameContentDumper::dumpLayoutTreeAsText(WebLocalFrame* frame, LayoutAsTextControls toShow)
{
    if (!frame)
        return WebString();
    LayoutAsTextBehavior behavior = LayoutAsTextShowAllLayers;

    if (toShow & LayoutAsTextWithLineTrees)
        behavior |= LayoutAsTextShowLineTrees;

    if (toShow & LayoutAsTextDebug)
        behavior |= LayoutAsTextShowCompositedLayers | LayoutAsTextShowAddresses | LayoutAsTextShowIDAndClass | LayoutAsTextShowLayerNesting;

    if (toShow & LayoutAsTextPrinting)
        behavior |= LayoutAsTextPrintingMode;

    return externalRepresentation(toWebLocalFrameImpl(frame)->frame(), behavior);
}
}
