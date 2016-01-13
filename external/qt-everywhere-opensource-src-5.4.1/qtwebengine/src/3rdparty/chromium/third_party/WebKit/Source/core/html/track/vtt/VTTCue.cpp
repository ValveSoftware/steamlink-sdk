/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/track/vtt/VTTCue.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/NodeTraversal.h"
#include "core/events/Event.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/track/TextTrack.h"
#include "core/html/track/TextTrackCueList.h"
#include "core/html/track/vtt/VTTElement.h"
#include "core/html/track/vtt/VTTParser.h"
#include "core/html/track/vtt/VTTRegionList.h"
#include "core/html/track/vtt/VTTScanner.h"
#include "core/rendering/RenderVTTCue.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/TextRunIterator.h"
#include "wtf/MathExtras.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

static const int undefinedPosition = -1;
static const int undefinedSize = -1;

static const CSSValueID displayWritingModeMap[] = {
    CSSValueHorizontalTb, CSSValueVerticalRl, CSSValueVerticalLr
};
COMPILE_ASSERT(WTF_ARRAY_LENGTH(displayWritingModeMap) == VTTCue::NumberOfWritingDirections,
    displayWritingModeMap_has_wrong_size);

static const CSSValueID displayAlignmentMap[] = {
    CSSValueStart, CSSValueCenter, CSSValueEnd, CSSValueLeft, CSSValueRight
};
COMPILE_ASSERT(WTF_ARRAY_LENGTH(displayAlignmentMap) == VTTCue::NumberOfAlignments,
    displayAlignmentMap_has_wrong_size);

static const String& startKeyword()
{
    DEFINE_STATIC_LOCAL(const String, start, ("start"));
    return start;
}

static const String& middleKeyword()
{
    DEFINE_STATIC_LOCAL(const String, middle, ("middle"));
    return middle;
}

static const String& endKeyword()
{
    DEFINE_STATIC_LOCAL(const String, end, ("end"));
    return end;
}

static const String& leftKeyword()
{
    DEFINE_STATIC_LOCAL(const String, left, ("left"));
    return left;
}

static const String& rightKeyword()
{
    DEFINE_STATIC_LOCAL(const String, right, ("right"));
    return right;
}

static const String& horizontalKeyword()
{
    return emptyString();
}

static const String& verticalGrowingLeftKeyword()
{
    DEFINE_STATIC_LOCAL(const String, verticalrl, ("rl"));
    return verticalrl;
}

static const String& verticalGrowingRightKeyword()
{
    DEFINE_STATIC_LOCAL(const String, verticallr, ("lr"));
    return verticallr;
}

static bool isInvalidPercentage(int value, ExceptionState& exceptionState)
{
    if (value < 0 || value > 100) {
        exceptionState.throwDOMException(IndexSizeError, ExceptionMessages::indexOutsideRange("value", value, 0, ExceptionMessages::InclusiveBound, 100, ExceptionMessages::InclusiveBound));
        return true;
    }
    return false;
}

VTTCueBox::VTTCueBox(Document& document, VTTCue* cue)
    : HTMLDivElement(document)
    , m_cue(cue)
{
    setShadowPseudoId(AtomicString("-webkit-media-text-track-display", AtomicString::ConstructFromLiteral));
}

void VTTCueBox::applyCSSProperties(const IntSize&)
{
    // FIXME: Apply all the initial CSS positioning properties. http://wkb.ug/79916
    if (!m_cue->regionId().isEmpty()) {
        setInlineStyleProperty(CSSPropertyPosition, CSSValueRelative);
        return;
    }

    // 3.5.1 On the (root) List of WebVTT Node Objects:

    // the 'position' property must be set to 'absolute'
    setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);

    //  the 'unicode-bidi' property must be set to 'plaintext'
    setInlineStyleProperty(CSSPropertyUnicodeBidi, CSSValueWebkitPlaintext);

    // the 'direction' property must be set to direction
    setInlineStyleProperty(CSSPropertyDirection, m_cue->getCSSWritingDirection());

    // the 'writing-mode' property must be set to writing-mode
    setInlineStyleProperty(CSSPropertyWebkitWritingMode, m_cue->getCSSWritingMode());

    std::pair<float, float> position = m_cue->getCSSPosition();

    // the 'top' property must be set to top,
    setInlineStyleProperty(CSSPropertyTop, position.second, CSSPrimitiveValue::CSS_PERCENTAGE);

    // the 'left' property must be set to left
    setInlineStyleProperty(CSSPropertyLeft, position.first, CSSPrimitiveValue::CSS_PERCENTAGE);

    // the 'width' property must be set to width, and the 'height' property  must be set to height
    if (m_cue->vertical() == horizontalKeyword()) {
        setInlineStyleProperty(CSSPropertyWidth, static_cast<double>(m_cue->getCSSSize()), CSSPrimitiveValue::CSS_PERCENTAGE);
        setInlineStyleProperty(CSSPropertyHeight, CSSValueAuto);
    } else {
        setInlineStyleProperty(CSSPropertyWidth, CSSValueAuto);
        setInlineStyleProperty(CSSPropertyHeight, static_cast<double>(m_cue->getCSSSize()),  CSSPrimitiveValue::CSS_PERCENTAGE);
    }

    // The 'text-align' property on the (root) List of WebVTT Node Objects must
    // be set to the value in the second cell of the row of the table below
    // whose first cell is the value of the corresponding cue's text track cue
    // alignment:
    setInlineStyleProperty(CSSPropertyTextAlign, m_cue->getCSSAlignment());

    if (!m_cue->snapToLines()) {
        // 10.13.1 Set up x and y:
        // Note: x and y are set through the CSS left and top above.

        // 10.13.2 Position the boxes in boxes such that the point x% along the
        // width of the bounding box of the boxes in boxes is x% of the way
        // across the width of the video's rendering area, and the point y%
        // along the height of the bounding box of the boxes in boxes is y%
        // of the way across the height of the video's rendering area, while
        // maintaining the relative positions of the boxes in boxes to each
        // other.
        setInlineStyleProperty(CSSPropertyWebkitTransform,
            String::format("translate(-%.2f%%, -%.2f%%)", position.first, position.second));

        setInlineStyleProperty(CSSPropertyWhiteSpace, CSSValuePre);
    }
}

RenderObject* VTTCueBox::createRenderer(RenderStyle*)
{
    return new RenderVTTCue(this);
}

void VTTCueBox::trace(Visitor* visitor)
{
    visitor->trace(m_cue);
    HTMLDivElement::trace(visitor);
}

VTTCue::VTTCue(Document& document, double startTime, double endTime, const String& text)
    : TextTrackCue(startTime, endTime)
    , m_text(text)
    , m_linePosition(undefinedPosition)
    , m_computedLinePosition(undefinedPosition)
    , m_textPosition(50)
    , m_cueSize(100)
    , m_writingDirection(Horizontal)
    , m_cueAlignment(Middle)
    , m_vttNodeTree(nullptr)
    , m_cueBackgroundBox(HTMLDivElement::create(document))
    , m_displayDirection(CSSValueLtr)
    , m_displaySize(undefinedSize)
    , m_snapToLines(true)
    , m_displayTreeShouldChange(true)
    , m_notifyRegion(true)
{
    ScriptWrappable::init(this);
    UseCounter::count(document, UseCounter::VTTCue);
}

VTTCue::~VTTCue()
{
    // Using oilpan, if m_displayTree is in the document it will strongly keep
    // the cue alive. Thus, if the cue is dead, either m_displayTree is not in
    // the document or the entire document is dead too.
#if !ENABLE(OILPAN)
    // FIXME: This is scary, we should make the life cycle smarter so the destructor
    // doesn't need to do DOM mutations.
    if (m_displayTree)
        m_displayTree->remove(ASSERT_NO_EXCEPTION);
#endif
}

#ifndef NDEBUG
String VTTCue::toString() const
{
    return String::format("%p id=%s interval=%f-->%f cue=%s)", this, id().utf8().data(), startTime(), endTime(), text().utf8().data());
}
#endif

VTTCueBox& VTTCue::ensureDisplayTree()
{
    if (!m_displayTree)
        m_displayTree = VTTCueBox::create(document(), this);
    return *m_displayTree;
}

void VTTCue::cueDidChange()
{
    TextTrackCue::cueDidChange();
    m_displayTreeShouldChange = true;
}

const String& VTTCue::vertical() const
{
    switch (m_writingDirection) {
    case Horizontal:
        return horizontalKeyword();
    case VerticalGrowingLeft:
        return verticalGrowingLeftKeyword();
    case VerticalGrowingRight:
        return verticalGrowingRightKeyword();
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

void VTTCue::setVertical(const String& value)
{
    WritingDirection direction = m_writingDirection;
    if (value == horizontalKeyword())
        direction = Horizontal;
    else if (value == verticalGrowingLeftKeyword())
        direction = VerticalGrowingLeft;
    else if (value == verticalGrowingRightKeyword())
        direction = VerticalGrowingRight;
    else
        ASSERT_NOT_REACHED();

    if (direction == m_writingDirection)
        return;

    cueWillChange();
    m_writingDirection = direction;
    cueDidChange();
}

void VTTCue::setSnapToLines(bool value)
{
    if (m_snapToLines == value)
        return;

    cueWillChange();
    m_snapToLines = value;
    cueDidChange();
}

void VTTCue::setLine(int position, ExceptionState& exceptionState)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-line
    // On setting, if the text track cue snap-to-lines flag is not set, and the new
    // value is negative or greater than 100, then throw an IndexSizeError exception.
    if (!m_snapToLines && (position < 0 || position > 100)) {
        exceptionState.throwDOMException(IndexSizeError, "The snap-to-lines flag is not set, and the value provided (" + String::number(position) + ") is not between 0 and 100.");
        return;
    }

    // Otherwise, set the text track cue line position to the new value.
    if (m_linePosition == position)
        return;

    cueWillChange();
    m_linePosition = position;
    m_computedLinePosition = calculateComputedLinePosition();
    cueDidChange();
}

void VTTCue::setPosition(int position, ExceptionState& exceptionState)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-position
    // On setting, if the new value is negative or greater than 100, then throw an IndexSizeError exception.
    // Otherwise, set the text track cue text position to the new value.
    if (isInvalidPercentage(position, exceptionState))
        return;

    // Otherwise, set the text track cue line position to the new value.
    if (m_textPosition == position)
        return;

    cueWillChange();
    m_textPosition = position;
    cueDidChange();
}

void VTTCue::setSize(int size, ExceptionState& exceptionState)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#dom-texttrackcue-size
    // On setting, if the new value is negative or greater than 100, then throw an IndexSizeError
    // exception. Otherwise, set the text track cue size to the new value.
    if (isInvalidPercentage(size, exceptionState))
        return;

    // Otherwise, set the text track cue line position to the new value.
    if (m_cueSize == size)
        return;

    cueWillChange();
    m_cueSize = size;
    cueDidChange();
}

const String& VTTCue::align() const
{
    switch (m_cueAlignment) {
    case Start:
        return startKeyword();
    case Middle:
        return middleKeyword();
    case End:
        return endKeyword();
    case Left:
        return leftKeyword();
    case Right:
        return rightKeyword();
    default:
        ASSERT_NOT_REACHED();
        return emptyString();
    }
}

void VTTCue::setAlign(const String& value)
{
    CueAlignment alignment = m_cueAlignment;
    if (value == startKeyword())
        alignment = Start;
    else if (value == middleKeyword())
        alignment = Middle;
    else if (value == endKeyword())
        alignment = End;
    else if (value == leftKeyword())
        alignment = Left;
    else if (value == rightKeyword())
        alignment = Right;
    else
        ASSERT_NOT_REACHED();

    if (alignment == m_cueAlignment)
        return;

    cueWillChange();
    m_cueAlignment = alignment;
    cueDidChange();
}

void VTTCue::setText(const String& text)
{
    if (m_text == text)
        return;

    cueWillChange();
    // Clear the document fragment but don't bother to create it again just yet as we can do that
    // when it is requested.
    m_vttNodeTree = nullptr;
    m_text = text;
    cueDidChange();
}

void VTTCue::createVTTNodeTree()
{
    if (!m_vttNodeTree)
        m_vttNodeTree = VTTParser::createDocumentFragmentFromCueText(document(), m_text);
}

void VTTCue::copyVTTNodeToDOMTree(ContainerNode* vttNode, ContainerNode* parent)
{
    for (Node* node = vttNode->firstChild(); node; node = node->nextSibling()) {
        RefPtrWillBeRawPtr<Node> clonedNode;
        if (node->isVTTElement())
            clonedNode = toVTTElement(node)->createEquivalentHTMLElement(document());
        else
            clonedNode = node->cloneNode(false);
        parent->appendChild(clonedNode);
        if (node->isContainerNode())
            copyVTTNodeToDOMTree(toContainerNode(node), toContainerNode(clonedNode));
    }
}

PassRefPtrWillBeRawPtr<DocumentFragment> VTTCue::getCueAsHTML()
{
    createVTTNodeTree();
    RefPtrWillBeRawPtr<DocumentFragment> clonedFragment = DocumentFragment::create(document());
    copyVTTNodeToDOMTree(m_vttNodeTree.get(), clonedFragment.get());
    return clonedFragment.release();
}

PassRefPtrWillBeRawPtr<DocumentFragment> VTTCue::createCueRenderingTree()
{
    createVTTNodeTree();
    RefPtrWillBeRawPtr<DocumentFragment> clonedFragment = DocumentFragment::create(document());
    m_vttNodeTree->cloneChildNodes(clonedFragment.get());
    return clonedFragment.release();
}

void VTTCue::setRegionId(const String& regionId)
{
    if (m_regionId == regionId)
        return;

    cueWillChange();
    m_regionId = regionId;
    cueDidChange();
}

void VTTCue::notifyRegionWhenRemovingDisplayTree(bool notifyRegion)
{
    m_notifyRegion = notifyRegion;
}

int VTTCue::calculateComputedLinePosition()
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#text-track-cue-computed-line-position

    // If the text track cue line position is numeric, then that is the text
    // track cue computed line position.
    if (m_linePosition != undefinedPosition)
        return m_linePosition;

    // If the text track cue snap-to-lines flag of the text track cue is not
    // set, the text track cue computed line position is the value 100;
    if (!m_snapToLines)
        return 100;

    // Otherwise, it is the value returned by the following algorithm:

    // If cue is not associated with a text track, return -1 and abort these
    // steps.
    if (!track())
        return -1;

    // Let n be the number of text tracks whose text track mode is showing or
    // showing by default and that are in the media element's list of text
    // tracks before track.
    int n = track()->trackIndexRelativeToRenderedTracks();

    // Increment n by one.
    n++;

    // Negate n.
    n = -n;

    return n;
}

class VTTTextRunIterator : public TextRunIterator {
public:
    VTTTextRunIterator() { }
    VTTTextRunIterator(const TextRun* textRun, unsigned offset) : TextRunIterator(textRun, offset) { }

    bool atParagraphSeparator() const
    {
        // Within a cue, paragraph boundaries are only denoted by Type B characters,
        // such as U+000A LINE FEED (LF), U+0085 NEXT LINE (NEL), and U+2029 PARAGRAPH SEPARATOR.
        return WTF::Unicode::category(current()) & WTF::Unicode::Separator_Paragraph;
    }
};

// Almost the same as determineDirectionality in core/html/HTMLElement.cpp, but
// that one uses a "plain" TextRunIterator (which only checks for '\n').
static TextDirection determineDirectionality(const String& value, bool& hasStrongDirectionality)
{
    TextRun run(value);
    BidiResolver<VTTTextRunIterator, BidiCharacterRun> bidiResolver;
    bidiResolver.setStatus(BidiStatus(LTR, false));
    bidiResolver.setPositionIgnoringNestedIsolates(VTTTextRunIterator(&run, 0));
    return bidiResolver.determineParagraphDirectionality(&hasStrongDirectionality);
}

static CSSValueID determineTextDirection(DocumentFragment* vttRoot)
{
    DEFINE_STATIC_LOCAL(const String, rtTag, ("rt"));
    ASSERT(vttRoot);

    // Apply the Unicode Bidirectional Algorithm's Paragraph Level steps to the
    // concatenation of the values of each WebVTT Text Object in nodes, in a
    // pre-order, depth-first traversal, excluding WebVTT Ruby Text Objects and
    // their descendants.
    TextDirection textDirection = LTR;
    for (Node* node = vttRoot->firstChild(); node; node = NodeTraversal::next(*node, vttRoot)) {
        if (!node->isTextNode() || node->localName() == rtTag)
            continue;

        bool hasStrongDirectionality;
        textDirection = determineDirectionality(node->nodeValue(), hasStrongDirectionality);
        if (hasStrongDirectionality)
            break;
    }
    return isLeftToRightDirection(textDirection) ? CSSValueLtr : CSSValueRtl;
}

void VTTCue::calculateDisplayParameters()
{
    createVTTNodeTree();

    // Steps 10.2, 10.3
    m_displayDirection = determineTextDirection(m_vttNodeTree.get());

    if (m_displayDirection == CSSValueRtl)
        UseCounter::count(document(), UseCounter::VTTCueRenderRtl);

    // 10.4 If the text track cue writing direction is horizontal, then let
    // block-flow be 'tb'. Otherwise, if the text track cue writing direction is
    // vertical growing left, then let block-flow be 'lr'. Otherwise, the text
    // track cue writing direction is vertical growing right; let block-flow be
    // 'rl'.

    // The above step is done through the writing direction static map.

    // 10.5 Determine the value of maximum size for cue as per the appropriate
    // rules from the following list:
    int maximumSize = m_textPosition;
    if ((m_writingDirection == Horizontal && m_cueAlignment == Start && m_displayDirection == CSSValueLtr)
        || (m_writingDirection == Horizontal && m_cueAlignment == End && m_displayDirection == CSSValueRtl)
        || (m_writingDirection == Horizontal && m_cueAlignment == Left)
        || (m_writingDirection == VerticalGrowingLeft && (m_cueAlignment == Start || m_cueAlignment == Left))
        || (m_writingDirection == VerticalGrowingRight && (m_cueAlignment == Start || m_cueAlignment == Left))) {
        maximumSize = 100 - m_textPosition;
    } else if ((m_writingDirection == Horizontal && m_cueAlignment == End && m_displayDirection == CSSValueLtr)
        || (m_writingDirection == Horizontal && m_cueAlignment == Start && m_displayDirection == CSSValueRtl)
        || (m_writingDirection == Horizontal && m_cueAlignment == Right)
        || (m_writingDirection == VerticalGrowingLeft && (m_cueAlignment == End || m_cueAlignment == Right))
        || (m_writingDirection == VerticalGrowingRight && (m_cueAlignment == End || m_cueAlignment == Right))) {
        maximumSize = m_textPosition;
    } else if (m_cueAlignment == Middle) {
        maximumSize = m_textPosition <= 50 ? m_textPosition : (100 - m_textPosition);
        maximumSize = maximumSize * 2;
    } else {
        ASSERT_NOT_REACHED();
    }

    // 10.6 If the text track cue size is less than maximum size, then let size
    // be text track cue size. Otherwise, let size be maximum size.
    m_displaySize = std::min(m_cueSize, maximumSize);

    // FIXME: Understand why step 10.7 is missing (just a copy/paste error?)
    // Could be done within a spec implementation check - http://crbug.com/301580

    // 10.8 Determine the value of x-position or y-position for cue as per the
    // appropriate rules from the following list:
    if (m_writingDirection == Horizontal) {
        switch (m_cueAlignment) {
        case Start:
            if (m_displayDirection == CSSValueLtr)
                m_displayPosition.first = m_textPosition;
            else
                m_displayPosition.first = 100 - m_textPosition - m_displaySize;
            break;
        case End:
            if (m_displayDirection == CSSValueRtl)
                m_displayPosition.first = 100 - m_textPosition;
            else
                m_displayPosition.first = m_textPosition - m_displaySize;
            break;
        case Left:
            if (m_displayDirection == CSSValueLtr)
                m_displayPosition.first = m_textPosition;
            else
                m_displayPosition.first = 100 - m_textPosition;
            break;
        case Right:
            if (m_displayDirection == CSSValueLtr)
                m_displayPosition.first = m_textPosition - m_displaySize;
            else
                m_displayPosition.first = 100 - m_textPosition - m_displaySize;
            break;
        case Middle:
            if (m_displayDirection == CSSValueLtr)
                m_displayPosition.first = m_textPosition - m_displaySize / 2;
            else
                m_displayPosition.first = 100 - m_textPosition - m_displaySize / 2;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    } else {
        // Cases for m_writingDirection being VerticalGrowing{Left|Right}
        switch (m_cueAlignment) {
        case Start:
        case Left:
            m_displayPosition.second = m_textPosition;
            break;
        case End:
        case Right:
            m_displayPosition.second = m_textPosition - m_displaySize;
            break;
        case Middle:
            m_displayPosition.second = m_textPosition - m_displaySize / 2;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    // A text track cue has a text track cue computed line position whose value
    // is defined in terms of the other aspects of the cue.
    m_computedLinePosition = calculateComputedLinePosition();

    // 10.9 Determine the value of whichever of x-position or y-position is not
    // yet calculated for cue as per the appropriate rules from the following
    // list:
    if (m_snapToLines && m_displayPosition.second == undefinedPosition && m_writingDirection == Horizontal)
        m_displayPosition.second = 0;

    if (!m_snapToLines && m_displayPosition.second == undefinedPosition && m_writingDirection == Horizontal)
        m_displayPosition.second = m_computedLinePosition;

    if (m_snapToLines && m_displayPosition.first == undefinedPosition
        && (m_writingDirection == VerticalGrowingLeft || m_writingDirection == VerticalGrowingRight))
        m_displayPosition.first = 0;

    if (!m_snapToLines && (m_writingDirection == VerticalGrowingLeft || m_writingDirection == VerticalGrowingRight))
        m_displayPosition.first = m_computedLinePosition;
}

void VTTCue::markFutureAndPastNodes(ContainerNode* root, double previousTimestamp, double movieTime)
{
    DEFINE_STATIC_LOCAL(const String, timestampTag, ("timestamp"));

    bool isPastNode = true;
    double currentTimestamp = previousTimestamp;
    if (currentTimestamp > movieTime)
        isPastNode = false;

    for (Node* child = root->firstChild(); child; child = NodeTraversal::next(*child, root)) {
        if (child->nodeName() == timestampTag) {
            double currentTimestamp;
            bool check = VTTParser::collectTimeStamp(child->nodeValue(), currentTimestamp);
            ASSERT_UNUSED(check, check);

            if (currentTimestamp > movieTime)
                isPastNode = false;
        }

        if (child->isVTTElement()) {
            toVTTElement(child)->setIsPastNode(isPastNode);
            // Make an elemenet id match a cue id for style matching purposes.
            if (!id().isEmpty())
                toElement(child)->setIdAttribute(id());
        }
    }
}

void VTTCue::updateDisplayTree(double movieTime)
{
    // The display tree may contain WebVTT timestamp objects representing
    // timestamps (processing instructions), along with displayable nodes.

    if (!track()->isRendered())
        return;

    // Clear the contents of the set.
    m_cueBackgroundBox->removeChildren();

    // Update the two sets containing past and future WebVTT objects.
    RefPtrWillBeRawPtr<DocumentFragment> referenceTree = createCueRenderingTree();
    markFutureAndPastNodes(referenceTree.get(), startTime(), movieTime);
    m_cueBackgroundBox->appendChild(referenceTree, ASSERT_NO_EXCEPTION);
}

PassRefPtrWillBeRawPtr<VTTCueBox> VTTCue::getDisplayTree(const IntSize& videoSize)
{
    RefPtrWillBeRawPtr<VTTCueBox> displayTree(ensureDisplayTree());
    if (!m_displayTreeShouldChange || !track()->isRendered())
        return displayTree.release();

    // 10.1 - 10.10
    calculateDisplayParameters();

    // 10.11. Apply the terms of the CSS specifications to nodes within the
    // following constraints, thus obtaining a set of CSS boxes positioned
    // relative to an initial containing block:
    displayTree->removeChildren();

    // The document tree is the tree of WebVTT Node Objects rooted at nodes.

    // The children of the nodes must be wrapped in an anonymous box whose
    // 'display' property has the value 'inline'. This is the WebVTT cue
    // background box.

    // Note: This is contained by default in m_cueBackgroundBox.
    m_cueBackgroundBox->setShadowPseudoId(cueShadowPseudoId());
    displayTree->appendChild(m_cueBackgroundBox);

    // FIXME(BUG 79916): Runs of children of WebVTT Ruby Objects that are not
    // WebVTT Ruby Text Objects must be wrapped in anonymous boxes whose
    // 'display' property has the value 'ruby-base'.

    // FIXME(BUG 79916): Text runs must be wrapped according to the CSS
    // line-wrapping rules, except that additionally, regardless of the value of
    // the 'white-space' property, lines must be wrapped at the edge of their
    // containing blocks, even if doing so requires splitting a word where there
    // is no line breaking opportunity. (Thus, normally text wraps as needed,
    // but if there is a particularly long word, it does not overflow as it
    // normally would in CSS, it is instead forcibly wrapped at the box's edge.)
    displayTree->applyCSSProperties(videoSize);

    m_displayTreeShouldChange = false;

    // 10.15. Let cue's text track cue display state have the CSS boxes in
    // boxes.
    return displayTree.release();
}

void VTTCue::removeDisplayTree()
{
    if (m_notifyRegion && track()->regions()) {
        // The region needs to be informed about the cue removal.
        VTTRegion* region = track()->regions()->getRegionById(m_regionId);
        if (region)
            region->willRemoveVTTCueBox(m_displayTree.get());
    }

    if (m_displayTree)
        m_displayTree->remove(ASSERT_NO_EXCEPTION);
}

void VTTCue::updateDisplay(const IntSize& videoSize, HTMLDivElement& container)
{
    UseCounter::count(document(), UseCounter::VTTCueRender);

    if (m_writingDirection != Horizontal)
        UseCounter::count(document(), UseCounter::VTTCueRenderVertical);

    if (!m_snapToLines)
        UseCounter::count(document(), UseCounter::VTTCueRenderSnapToLinesFalse);

    if (m_linePosition != undefinedPosition)
        UseCounter::count(document(), UseCounter::VTTCueRenderLineNotAuto);

    if (m_textPosition != 50)
        UseCounter::count(document(), UseCounter::VTTCueRenderPositionNot50);

    if (m_cueSize != 100)
        UseCounter::count(document(), UseCounter::VTTCueRenderSizeNot100);

    if (m_cueAlignment != Middle)
        UseCounter::count(document(), UseCounter::VTTCueRenderAlignNotMiddle);

    RefPtrWillBeRawPtr<VTTCueBox> displayBox = getDisplayTree(videoSize);
    VTTRegion* region = 0;
    if (track()->regions())
        region = track()->regions()->getRegionById(regionId());

    if (!region) {
        // If cue has an empty text track cue region identifier or there is no
        // WebVTT region whose region identifier is identical to cue's text
        // track cue region identifier, run the following substeps:
        if (displayBox->hasChildren() && !container.contains(displayBox.get())) {
            // Note: the display tree of a cue is removed when the active flag of the cue is unset.
            container.appendChild(displayBox);
        }
    } else {
        // Let region be the WebVTT region whose region identifier
        // matches the text track cue region identifier of cue.
        RefPtrWillBeRawPtr<HTMLDivElement> regionNode = region->getDisplayTree(document());

        // Append the region to the viewport, if it was not already.
        if (!container.contains(regionNode.get()))
            container.appendChild(regionNode);

        region->appendVTTCueBox(displayBox);
    }
}

std::pair<double, double> VTTCue::getPositionCoordinates() const
{
    // This method is used for setting x and y when snap to lines is not set.
    std::pair<double, double> coordinates;

    if (m_writingDirection == Horizontal && m_displayDirection == CSSValueLtr) {
        coordinates.first = m_textPosition;
        coordinates.second = m_computedLinePosition;

        return coordinates;
    }

    if (m_writingDirection == Horizontal && m_displayDirection == CSSValueRtl) {
        coordinates.first = 100 - m_textPosition;
        coordinates.second = m_computedLinePosition;

        return coordinates;
    }

    if (m_writingDirection == VerticalGrowingLeft) {
        coordinates.first = 100 - m_computedLinePosition;
        coordinates.second = m_textPosition;

        return coordinates;
    }

    if (m_writingDirection == VerticalGrowingRight) {
        coordinates.first = m_computedLinePosition;
        coordinates.second = m_textPosition;

        return coordinates;
    }

    ASSERT_NOT_REACHED();

    return coordinates;
}

VTTCue::CueSetting VTTCue::settingName(VTTScanner& input)
{
    CueSetting parsedSetting = None;
    if (input.scan("vertical"))
        parsedSetting = Vertical;
    else if (input.scan("line"))
        parsedSetting = Line;
    else if (input.scan("position"))
        parsedSetting = Position;
    else if (input.scan("size"))
        parsedSetting = Size;
    else if (input.scan("align"))
        parsedSetting = Align;
    else if (RuntimeEnabledFeatures::webVTTRegionsEnabled() && input.scan("region"))
        parsedSetting = RegionId;
    // Verify that a ':' follows.
    if (parsedSetting != None && input.scan(':'))
        return parsedSetting;
    return None;
}

// Used for 'position' and 'size'.
static bool scanPercentage(VTTScanner& input, const VTTScanner::Run& valueRun, int& number)
{
    // 1. If value contains any characters other than U+0025 PERCENT SIGN
    //    characters (%) and characters in the range U+0030 DIGIT ZERO (0) to
    //    U+0039 DIGIT NINE (9), then jump to the step labeled next setting.
    // 2. If value does not contain at least one character in the range U+0030
    //    DIGIT ZERO (0) to U+0039 DIGIT NINE (9), then jump to the step
    //    labeled next setting.
    if (!input.scanDigits(number))
        return false;

    // 3. If any character in value other than the last character is a U+0025
    //    PERCENT SIGN character (%), then jump to the step labeled next
    //    setting.
    // 4. If the last character in value is not a U+0025 PERCENT SIGN character
    //    (%), then jump to the step labeled next setting.
    if (!input.scan('%') || !input.isAt(valueRun.end()))
        return false;

    // 5. Ignoring the trailing percent sign, interpret value as an integer,
    //    and let number be that number.
    // 6. If number is not in the range 0 ≤ number ≤ 100, then jump to the step
    //    labeled next setting.
    return number >= 0 && number <= 100;
}

void VTTCue::parseSettings(const String& inputString)
{
    VTTScanner input(inputString);

    while (!input.isAtEnd()) {

        // The WebVTT cue settings part of a WebVTT cue consists of zero or more of the following components, in any order,
        // separated from each other by one or more U+0020 SPACE characters or U+0009 CHARACTER TABULATION (tab) characters.
        input.skipWhile<VTTParser::isValidSettingDelimiter>();

        if (input.isAtEnd())
            break;

        // When the user agent is to parse the WebVTT settings given by a string input for a text track cue cue,
        // the user agent must run the following steps:
        // 1. Let settings be the result of splitting input on spaces.
        // 2. For each token setting in the list settings, run the following substeps:
        //    1. If setting does not contain a U+003A COLON character (:), or if the first U+003A COLON character (:)
        //       in setting is either the first or last character of setting, then jump to the step labeled next setting.
        //    2. Let name be the leading substring of setting up to and excluding the first U+003A COLON character (:) in that string.
        CueSetting name = settingName(input);

        // 3. Let value be the trailing substring of setting starting from the character immediately after the first U+003A COLON character (:) in that string.
        VTTScanner::Run valueRun = input.collectUntil<VTTParser::isValidSettingDelimiter>();

        // 4. Run the appropriate substeps that apply for the value of name, as follows:
        switch (name) {
        case Vertical: {
            // If name is a case-sensitive match for "vertical"
            // 1. If value is a case-sensitive match for the string "rl", then let cue's text track cue writing direction
            //    be vertical growing left.
            if (input.scanRun(valueRun, verticalGrowingLeftKeyword()))
                m_writingDirection = VerticalGrowingLeft;

            // 2. Otherwise, if value is a case-sensitive match for the string "lr", then let cue's text track cue writing
            //    direction be vertical growing right.
            else if (input.scanRun(valueRun, verticalGrowingRightKeyword()))
                m_writingDirection = VerticalGrowingRight;
            break;
        }
        case Line: {
            // 1-2 - Collect chars that are either '-', '%', or a digit.
            // 1. If value contains any characters other than U+002D HYPHEN-MINUS characters (-), U+0025 PERCENT SIGN
            //    characters (%), and characters in the range U+0030 DIGIT ZERO (0) to U+0039 DIGIT NINE (9), then jump
            //    to the step labeled next setting.
            bool isNegative = input.scan('-');
            int linePosition;
            unsigned numDigits = input.scanDigits(linePosition);
            bool isPercentage = input.scan('%');

            if (!input.isAt(valueRun.end()))
                break;

            // 2. If value does not contain at least one character in the range U+0030 DIGIT ZERO (0) to U+0039 DIGIT
            //    NINE (9), then jump to the step labeled next setting.
            // 3. If any character in value other than the first character is a U+002D HYPHEN-MINUS character (-), then
            //    jump to the step labeled next setting.
            // 4. If any character in value other than the last character is a U+0025 PERCENT SIGN character (%), then
            //    jump to the step labeled next setting.

            // 5. If the first character in value is a U+002D HYPHEN-MINUS character (-) and the last character in value is a
            //    U+0025 PERCENT SIGN character (%), then jump to the step labeled next setting.
            if (!numDigits || (isPercentage && isNegative))
                break;

            // 6. Ignoring the trailing percent sign, if any, interpret value as a (potentially signed) integer, and
            //    let number be that number.
            // 7. If the last character in value is a U+0025 PERCENT SIGN character (%), but number is not in the range
            //    0 ≤ number ≤ 100, then jump to the step labeled next setting.
            // 8. Let cue's text track cue line position be number.
            // 9. If the last character in value is a U+0025 PERCENT SIGN character (%), then let cue's text track cue
            //    snap-to-lines flag be false. Otherwise, let it be true.
            if (isPercentage) {
                if (linePosition < 0 || linePosition > 100)
                    break;
                // 10 - If '%' then set snap-to-lines flag to false.
                m_snapToLines = false;
            } else {
                if (isNegative)
                    linePosition = -linePosition;
                m_snapToLines = true;
            }
            m_linePosition = linePosition;
            break;
        }
        case Position: {
            int number;
            // Steps 1 - 6.
            if (!scanPercentage(input, valueRun, number))
                break;

            // 7. Let cue's text track cue text position be number.
            m_textPosition = number;
            break;
        }
        case Size: {
            int number;
            // Steps 1 - 6.
            if (!scanPercentage(input, valueRun, number))
                break;

            // 7. Let cue's text track cue size be number.
            m_cueSize = number;
            break;
        }
        case Align: {
            // 1. If value is a case-sensitive match for the string "start", then let cue's text track cue alignment be start alignment.
            if (input.scanRun(valueRun, startKeyword()))
                m_cueAlignment = Start;

            // 2. If value is a case-sensitive match for the string "middle", then let cue's text track cue alignment be middle alignment.
            else if (input.scanRun(valueRun, middleKeyword()))
                m_cueAlignment = Middle;

            // 3. If value is a case-sensitive match for the string "end", then let cue's text track cue alignment be end alignment.
            else if (input.scanRun(valueRun, endKeyword()))
                m_cueAlignment = End;

            // 4. If value is a case-sensitive match for the string "left", then let cue's text track cue alignment be left alignment.
            else if (input.scanRun(valueRun, leftKeyword()))
                m_cueAlignment = Left;

            // 5. If value is a case-sensitive match for the string "right", then let cue's text track cue alignment be right alignment.
            else if (input.scanRun(valueRun, rightKeyword()))
                m_cueAlignment = Right;
            break;
        }
        case RegionId:
            m_regionId = input.extractString(valueRun);
            break;
        case None:
            break;
        }

        // Make sure the entire run is consumed.
        input.skipRun(valueRun);
    }

    // If cue's line position is not auto or cue's size is not 100 or cue's
    // writing direction is not horizontal, but cue's region identifier is not
    // the empty string, let cue's region identifier be the empty string.
    if (m_regionId.isEmpty())
        return;

    if (m_linePosition != undefinedPosition || m_cueSize != 100 || m_writingDirection != Horizontal)
        m_regionId = emptyString();
}

CSSValueID VTTCue::getCSSAlignment() const
{
    return displayAlignmentMap[m_cueAlignment];
}

CSSValueID VTTCue::getCSSWritingDirection() const
{
    return m_displayDirection;
}

CSSValueID VTTCue::getCSSWritingMode() const
{
    return displayWritingModeMap[m_writingDirection];
}

int VTTCue::getCSSSize() const
{
    ASSERT(m_displaySize != undefinedSize);
    return m_displaySize;
}

std::pair<double, double> VTTCue::getCSSPosition() const
{
    if (!m_snapToLines)
        return getPositionCoordinates();

    return m_displayPosition;
}

ExecutionContext* VTTCue::executionContext() const
{
    ASSERT(m_cueBackgroundBox);
    return m_cueBackgroundBox->executionContext();
}

Document& VTTCue::document() const
{
    ASSERT(m_cueBackgroundBox);
    return m_cueBackgroundBox->document();
}

void VTTCue::trace(Visitor* visitor)
{
    visitor->trace(m_vttNodeTree);
    visitor->trace(m_cueBackgroundBox);
    visitor->trace(m_displayTree);
    TextTrackCue::trace(visitor);
}

} // namespace WebCore
