// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "DocumentStatisticsCollector.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/Text.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMetaElement.h"
#include "platform/Histogram.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDistillability.h"

namespace blink {

using namespace HTMLNames;

namespace {

// Saturate the length of a paragraph to save time.
const int kTextContentLengthSaturation = 1000;

// Filter out short P elements. The threshold is set to around 2 English sentences.
const unsigned kParagraphLengthThreshold = 140;

// Saturate the scores to save time. The max is the score of 6 long paragraphs.
const double kMozScoreSaturation = 175.954539583; // 6 * sqrt(kTextContentLengthSaturation - kParagraphLengthThreshold)
const double kMozScoreAllSqrtSaturation = 189.73665961; // 6 * sqrt(kTextContentLengthSaturation);
const double kMozScoreAllLinearSaturation = 6 * kTextContentLengthSaturation;

unsigned textContentLengthSaturated(const Element& root)
{
    unsigned length = 0;
    // This skips shadow DOM intentionally, to match the JavaScript implementation.
    // We would like to use the same statistics extracted by the JavaScript implementation
    // on iOS, and JavaScript cannot peek deeply into shadow DOM except on modern Chrome
    // versions.
    // Given shadow DOM rarely appears in <P> elements in long-form articles, the overall
    // accuracy should not be largely affected.
    for (Node& node : NodeTraversal::inclusiveDescendantsOf(root)) {
        if (!node.isTextNode()) {
            continue;
        }
        length += toText(node).length();
        if (length > kTextContentLengthSaturation) {
            return kTextContentLengthSaturation;
        }
    }
    return length;
}

bool isVisible(const Element& element)
{
    const ComputedStyle* style = element.computedStyle();
    if (!style)
        return false;
    return (
        style->display() != NONE
        && style->visibility() != HIDDEN
        && style->opacity() != 0
    );
}

bool matchAttributes(const Element& element, const Vector<String>& words)
{
    const String& classes = element.getClassAttribute();
    const String& id = element.getIdAttribute();
    for (const String& word : words) {
        if (classes.findIgnoringCase(word) != WTF::kNotFound
            || id.findIgnoringCase(word) != WTF::kNotFound) {
            return true;
        }
    }
    return false;
}

bool isGoodForScoring(const WebDistillabilityFeatures& features, const Element& element)
{
    DEFINE_STATIC_LOCAL(Vector<String>, unlikelyCandidates, ());
    if (unlikelyCandidates.isEmpty()) {
        auto words = {
            "banner",
            "combx",
            "comment",
            "community",
            "disqus",
            "extra",
            "foot",
            "header",
            "menu",
            "related",
            "remark",
            "rss",
            "share",
            "shoutbox",
            "sidebar",
            "skyscraper",
            "sponsor",
            "ad-break",
            "agegate",
            "pagination",
            "pager",
            "popup"
        };
        for (auto word : words) {
            unlikelyCandidates.append(word);
        }
    }
    DEFINE_STATIC_LOCAL(Vector<String>, highlyLikelyCandidates, ());
    if (highlyLikelyCandidates.isEmpty()) {
        auto words = {
            "and",
            "article",
            "body",
            "column",
            "main",
            "shadow"
        };
        for (auto word : words) {
            highlyLikelyCandidates.append(word);
        }
    }

    if (!isVisible(element))
        return false;
    if (features.mozScore >= kMozScoreSaturation
        && features.mozScoreAllSqrt >= kMozScoreAllSqrtSaturation
        && features.mozScoreAllLinear >= kMozScoreAllLinearSaturation)
        return false;
    if (matchAttributes(element, unlikelyCandidates)
        && !matchAttributes(element, highlyLikelyCandidates))
        return false;
    return true;
}

// underListItem denotes that at least one of the ancesters is <li> element.
void collectFeatures(Element& root, WebDistillabilityFeatures& features, bool underListItem = false)
{
    for (Node& node : NodeTraversal::childrenOf(root)) {
        bool isListItem = false;
        if (!node.isElementNode()) {
            continue;
        }

        features.elementCount++;
        Element& element = toElement(node);
        if (element.hasTagName(aTag)) {
            features.anchorCount++;
        } else if (element.hasTagName(formTag)) {
            features.formCount++;
        } else if (element.hasTagName(inputTag)) {
            const HTMLInputElement& input = toHTMLInputElement(element);
            if (input.type() == InputTypeNames::text) {
                features.textInputCount++;
            } else if (input.type() == InputTypeNames::password) {
                features.passwordInputCount++;
            }
        } else if (element.hasTagName(pTag) || element.hasTagName(preTag)) {
            if (element.hasTagName(pTag)) {
                features.pCount++;
            } else {
                features.preCount++;
            }
            if (!underListItem && isGoodForScoring(features, element)) {
                unsigned length = textContentLengthSaturated(element);
                if (length >= kParagraphLengthThreshold) {
                    features.mozScore += sqrt(length - kParagraphLengthThreshold);
                    features.mozScore = std::min(features.mozScore, kMozScoreSaturation);
                }
                features.mozScoreAllSqrt += sqrt(length);
                features.mozScoreAllSqrt = std::min(features.mozScoreAllSqrt, kMozScoreAllSqrtSaturation);

                features.mozScoreAllLinear += length;
                features.mozScoreAllLinear = std::min(features.mozScoreAllLinear, kMozScoreAllLinearSaturation);
            }
        } else if (element.hasTagName(liTag)) {
            isListItem = true;
        }
        collectFeatures(element, features, underListItem || isListItem);
    }
}

bool hasOpenGraphArticle(const Element& head)
{
    DEFINE_STATIC_LOCAL(AtomicString, ogType, ("og:type"));
    DEFINE_STATIC_LOCAL(AtomicString, propertyAttr, ("property"));
    for (const Element* child = ElementTraversal::firstChild(head); child; child = ElementTraversal::nextSibling(*child)) {
        if (!isHTMLMetaElement(*child))
            continue;
        const HTMLMetaElement& meta = toHTMLMetaElement(*child);

        if (meta.name() == ogType || meta.getAttribute(propertyAttr) == ogType) {
            if (equalIgnoringCase(meta.content(), "article")) {
                return true;
            }
        }
    }
    return false;
}

bool isMobileFriendly(Document& document)
{
    if (FrameHost* frameHost = document.frameHost())
        return frameHost->visualViewport().shouldDisableDesktopWorkarounds();
    return false;
}

} // namespace

WebDistillabilityFeatures DocumentStatisticsCollector::collectStatistics(Document& document)
{
    TRACE_EVENT0("blink", "DocumentStatisticsCollector::collectStatistics");

    WebDistillabilityFeatures features = WebDistillabilityFeatures();

    if (!document.frame() || !document.frame()->isMainFrame())
        return features;

    DCHECK(document.hasFinishedParsing());

    HTMLElement* body = document.body();
    HTMLElement* head = document.head();

    if (!body || !head)
        return features;

    features.isMobileFriendly = isMobileFriendly(document);

    double startTime = monotonicallyIncreasingTime();

    // This should be cheap since collectStatistics is only called right after layout.
    document.updateStyleAndLayoutTree();

    // Traverse the DOM tree and collect statistics.
    collectFeatures(*body, features);
    features.openGraph = hasOpenGraphArticle(*head);

    double elapsedTime = monotonicallyIncreasingTime() - startTime;

    DEFINE_STATIC_LOCAL(CustomCountHistogram, distillabilityHistogram, ("WebCore.DistillabilityUs", 1, 1000000, 50));
    distillabilityHistogram.count(static_cast<int>(1e6 * elapsedTime));

    return features;
}

} // namespace blink
