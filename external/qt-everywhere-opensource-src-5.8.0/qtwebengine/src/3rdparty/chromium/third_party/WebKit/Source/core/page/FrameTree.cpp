/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2006 Apple Computer, Inc.
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
 */

#include "core/page/FrameTree.h"

#include "core/dom/Document.h"
#include "core/frame/FrameClient.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/RemoteFrameView.h"
#include "core/page/Page.h"
#include "wtf/Assertions.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"

using std::swap;

namespace blink {

namespace {

const unsigned invalidChildCount = ~0U;

} // namespace

FrameTree::FrameTree(Frame* thisFrame)
    : m_thisFrame(thisFrame)
    , m_scopedChildCount(invalidChildCount)
{
}

FrameTree::~FrameTree()
{
}

void FrameTree::setName(const AtomicString& name)
{
    // This method should only be called for local frames
    // (remote frames should be updated via setPrecalculatedName).
    DCHECK(m_thisFrame->isLocalFrame());

    // When this method is called, m_uniqueName should be already initialized.
    // This assert helps ensure that early return (a few lines below) won't
    // result in an uninitialized m_uniqueName.
    DCHECK(!m_uniqueName.isNull()
        || (m_uniqueName.isNull() && m_name.isNull() && !parent()));

    // Do not recalculate m_uniqueName if there is no real change of m_name.
    // This is not just a performance optimization - other code relies on the
    // assumption that unique name shouldn't change if the assigned name didn't
    // change (i.e. code in content::FrameTreeNode::SetFrameName).
    if (m_name == name)
        return;

    m_name = name;

    // Remove our old frame name so it's not considered in calculateUniqueNameForChildFrame
    // and appendUniqueSuffix calls below.
    m_uniqueName = AtomicString();

    // Calculate a new unique name based on inputs.
    if (parent()) {
        setUniqueName(
            parent()->tree().calculateUniqueNameForChildFrame(m_thisFrame, name, nullAtom));
    } else if (name.isEmpty() || !uniqueNameExists(name)) {
        // Only main frame can have an empty unique name, so for main frames
        // emptiness guarantees uniquness.
        setUniqueName(name);
    } else {
        setUniqueName(appendUniqueSuffix(name, "<!--framePosition"));
    }
}

void FrameTree::setPrecalculatedName(const AtomicString& name, const AtomicString& uniqueName)
{
    if (!parent()) {
        DCHECK(uniqueName == name);
    } else {
        DCHECK(!uniqueName.isEmpty());
    }

    m_name = name;

    // TODO(lukasza): We would like to assert uniqueness below (i.e. by calling
    // setUniqueName), but
    // 1) uniqueness is currently violated by provisional/old frame pairs.
    // 2) there is an unresolved race between 2 OOPIFs, that can result in a
    //    non-unique |uniqueName| - see https://crbug.com/558680#c14.
    m_uniqueName = uniqueName;
}

Frame* FrameTree::parent() const
{
    if (!m_thisFrame->client())
        return nullptr;
    return m_thisFrame->client()->parent();
}

Frame* FrameTree::top() const
{
    // FIXME: top() should never return null, so here are some hacks to deal
    // with EmptyFrameLoaderClient and cases where the frame is detached
    // already...
    if (!m_thisFrame->client())
        return m_thisFrame;
    Frame* candidate = m_thisFrame->client()->top();
    return candidate ? candidate : m_thisFrame.get();
}

Frame* FrameTree::previousSibling() const
{
    if (!m_thisFrame->client())
        return nullptr;
    return m_thisFrame->client()->previousSibling();
}

Frame* FrameTree::nextSibling() const
{
    if (!m_thisFrame->client())
        return nullptr;
    return m_thisFrame->client()->nextSibling();
}

Frame* FrameTree::firstChild() const
{
    if (!m_thisFrame->client())
        return nullptr;
    return m_thisFrame->client()->firstChild();
}

Frame* FrameTree::lastChild() const
{
    if (!m_thisFrame->client())
        return nullptr;
    return m_thisFrame->client()->lastChild();
}

bool FrameTree::uniqueNameExists(const String& uniqueNameCandidate) const
{
    // This method is currently O(N), where N = number of frames in the tree.

    // Before recalculating or checking unique name, we set m_uniqueName
    // to an empty string (so the soon-to-be-removed name does not count
    // as a collision).  This means that uniqueNameExists would return
    // false positives when called with an empty |name|.
    DCHECK(!uniqueNameCandidate.isEmpty());

    for (Frame* frame = top(); frame; frame = frame->tree().traverseNext()) {
        if (frame->tree().uniqueName() == uniqueNameCandidate)
            return true;
    }
    return false;
}

AtomicString FrameTree::calculateUniqueNameForNewChildFrame(
    const AtomicString& name,
    const AtomicString& fallbackName) const
{
    AtomicString uniqueName = calculateUniqueNameForChildFrame(nullptr, name, fallbackName);

    // Caller will likely set the name via setPrecalculatedName, which
    // unfortunately cannot currently assert uniqueness of the name - let's
    // therefore assert the uniqueness here.
    DCHECK(!uniqueNameExists(uniqueName));

    return uniqueName;
}

String FrameTree::generateUniqueNameCandidate(bool existingChildFrame) const
{
    const char framePathPrefix[] = "<!--framePath ";
    const int framePathPrefixLength = 14;
    const int framePathSuffixLength = 3;

    // Find the nearest parent that has a frame with a path in it.
    HeapVector<Member<Frame>, 16> chain;
    Frame* frame;
    for (frame = m_thisFrame; frame; frame = frame->tree().parent()) {
        if (frame->tree().uniqueName().startsWith(framePathPrefix))
            break;
        chain.append(frame);
    }
    StringBuilder uniqueName;
    uniqueName.append(framePathPrefix);
    if (frame) {
        uniqueName.append(frame->tree().uniqueName().getString().substring(framePathPrefixLength,
            frame->tree().uniqueName().length() - framePathPrefixLength - framePathSuffixLength));
    }
    for (int i = chain.size() - 1; i >= 0; --i) {
        frame = chain[i];
        uniqueName.append('/');
        uniqueName.append(frame->tree().uniqueName());
    }

    uniqueName.append("/<!--frame");
    uniqueName.appendNumber(childCount() - (existingChildFrame ? 1 : 0));
    uniqueName.append("-->-->");

    // NOTE: This name might not be unique - see http://crbug.com/588800.
    return uniqueName.toAtomicString();
}

String FrameTree::generateFramePosition(Frame* child) const
{
    // This method is currently O(N), where N = number of frames in the tree.

    StringBuilder framePositionBuilder;
    framePositionBuilder.append("<!--framePosition");

    if (!child) {
        framePositionBuilder.append('-');
        framePositionBuilder.appendNumber(childCount());
        child = m_thisFrame;
    }

    while (child->tree().parent()) {
        int numberOfSiblingsBeforeChild = 0;
        Frame* sibling = child->tree().previousSibling();
        while (sibling) {
            sibling = sibling->tree().previousSibling();
            numberOfSiblingsBeforeChild++;
        }

        framePositionBuilder.append('-');
        framePositionBuilder.appendNumber(numberOfSiblingsBeforeChild);

        child = child->tree().parent();
    }

    // NOTE: The generated string is not guaranteed to be unique, but should
    // have a better chance of being unique than the string generated by
    // generateUniqueNameCandidate, because we embed extra information into the
    // string:
    // 1) we walk the full chain of ancestors, all the way to the main frame
    // 2) we use frame-position-within-parent (aka |numberOfSiblingsBeforeChild|)
    //    instead of sibling-count.
    return framePositionBuilder.toString();
}

AtomicString FrameTree::appendUniqueSuffix(
    const String& prefix,
    const String& likelyUniqueSuffix) const
{
    // Verify that we are not doing unnecessary work.
    DCHECK(uniqueNameExists(prefix));

    // We want unique name to be stable across page reloads - this is why
    // we use a deterministic |numberOfTries| rather than a random number
    // (a random number would be more likely to avoid a collision, but
    // would change after every page reload).
    int numberOfTries = 0;

    // Keep trying |prefix| + |likelyUniqueSuffix| + |numberOfTries|
    // concatenations until we get a truly unique name.
    String candidate;
    do {
        StringBuilder uniqueNameBuilder;
        uniqueNameBuilder.append(prefix);
        uniqueNameBuilder.append(likelyUniqueSuffix);
        uniqueNameBuilder.append('/');
        uniqueNameBuilder.appendNumber(numberOfTries++);
        uniqueNameBuilder.append("-->");

        candidate = uniqueNameBuilder.toString();
    } while (uniqueNameExists(candidate));
    return AtomicString(candidate);
}

AtomicString FrameTree::calculateUniqueNameForChildFrame(
    Frame* child,
    const AtomicString& assignedName,
    const AtomicString& fallbackName) const
{
    // Try to use |assignedName| (i.e. window.name or iframe.name) or |fallbackName| if possible.
    const AtomicString& requestedName = assignedName.isEmpty() ? fallbackName : assignedName;
    if (!requestedName.isEmpty() && !uniqueNameExists(requestedName) && requestedName != "_blank")
        return requestedName;

    String candidate = generateUniqueNameCandidate(child);
    if (!uniqueNameExists(candidate))
        return AtomicString(candidate);

    String likelyUniqueSuffix = generateFramePosition(child);
    return appendUniqueSuffix(candidate, likelyUniqueSuffix);

    // Description of the current unique name format
    // ---------------------------------------------
    //
    // Changing the format of unique name is undesirable, because it breaks
    // backcompatibility of session history (which stores unique names
    // generated in the past on user's disk).  This incremental,
    // backcompatibility-aware approach has resulted so far in the following
    // rather baroque format... :
    //
    //   uniqueName ::= <assignedName> | <generatedName>
    //                 (generatedName is used if assignedName is
    //                 1) non-unique / conflicts with other frame's unique name or
    //                 2) assignedName is empty for a non-main frame)
    //
    //   assignedName ::= value of iframe's name attribute
    //                 or value assigned to window.name
    //
    //   generatedName ::= oldGeneratedName newUniqueSuffix?
    //                  (newUniqueSuffix is only present if oldGeneratedName was
    //                  not unique after all)
    //
    //   oldGeneratedName ::= "<!--framePath //" ancestorChain "/<!--frame" childCount "-->-->"
    //                  (main frame is special - oldGeneratedName for main frame
    //                  is always the frame's assignedName;  oldGeneratedName is
    //                  generated by generateUniqueNameCandidate method).
    //
    //   childCount ::= current number of siblings
    //
    //   ancestorChain ::= concatenated unique names of ancestor chain,
    //                     terminated on the first ancestor (if any) starting with "<!--framePath"
    //                     each ancestor's unique name is separated by "/" character
    //   ancestorChain example1: "grandparent/parent"
    //  (ancestor's unique names :   #1--^   | #2-^ )
    //   ancestorChain example2: "<!--framePath //foo/bar/<!--frame42-->-->/blah/foobar"
    //  (ancestor's unique names :        ^--#1--^                         | #2 | #3-^ )
    //
    //   newUniqueSuffix ::= "<!--framePosition" framePosition "/" retryNumber "-->"
    //
    //   framePosition ::= "-" numberOfSiblingsBeforeChild [ framePosition-forParent? ]
    //                   | <empty string for main frame>
    //
    //   retryNumber ::= smallest non-negative integer resulting in unique name
}

void FrameTree::setUniqueName(const AtomicString& uniqueName)
{
    // Main frame is the only frame that can have an empty unique name.
    if (parent()) {
        DCHECK(!uniqueName.isEmpty() && !uniqueNameExists(uniqueName));
    } else {
        DCHECK(uniqueName.isEmpty() || !uniqueNameExists(uniqueName));
    }

    m_uniqueName = uniqueName;
}

Frame* FrameTree::scopedChild(unsigned index) const
{
    unsigned scopedIndex = 0;
    for (Frame* child = firstChild(); child; child = child->tree().nextSibling()) {
        if (child->client()->inShadowTree())
            continue;
        if (scopedIndex == index)
            return child;
        scopedIndex++;
    }

    return nullptr;
}

Frame* FrameTree::scopedChild(const AtomicString& name) const
{
    for (Frame* child = firstChild(); child; child = child->tree().nextSibling()) {
        if (child->client()->inShadowTree())
            continue;
        if (child->tree().name() == name)
            return child;
    }
    return nullptr;
}

unsigned FrameTree::scopedChildCount() const
{
    if (m_scopedChildCount == invalidChildCount) {
        unsigned scopedCount = 0;
        for (Frame* child = firstChild(); child; child = child->tree().nextSibling()) {
            if (child->client()->inShadowTree())
                continue;
            scopedCount++;
        }
        m_scopedChildCount = scopedCount;
    }
    return m_scopedChildCount;
}

void FrameTree::invalidateScopedChildCount()
{
    m_scopedChildCount = invalidChildCount;
}

unsigned FrameTree::childCount() const
{
    unsigned count = 0;
    for (Frame* result = firstChild(); result; result = result->tree().nextSibling())
        ++count;
    return count;
}

Frame* FrameTree::child(const AtomicString& name) const
{
    for (Frame* child = firstChild(); child; child = child->tree().nextSibling()) {
        if (child->tree().name() == name)
            return child;
    }
    return nullptr;
}

Frame* FrameTree::find(const AtomicString& name) const
{
    if (name == "_self" || name == "_current" || name.isEmpty())
        return m_thisFrame;

    if (name == "_top")
        return top();

    if (name == "_parent")
        return parent() ? parent() : m_thisFrame.get();

    // Since "_blank" should never be any frame's name, the following just amounts to an optimization.
    if (name == "_blank")
        return nullptr;

    // Search subtree starting with this frame first.
    for (Frame* frame = m_thisFrame; frame; frame = frame->tree().traverseNext(m_thisFrame)) {
        if (frame->tree().name() == name)
            return frame;
    }

    // Search the entire tree for this page next.
    Page* page = m_thisFrame->page();

    // The frame could have been detached from the page, so check it.
    if (!page)
        return nullptr;

    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->tree().name() == name)
            return frame;
    }

    // Search the entire tree of each of the other pages in this namespace.
    // FIXME: Is random order OK?
    for (const Page* otherPage : Page::ordinaryPages()) {
        if (otherPage == page)
            continue;
        for (Frame* frame = otherPage->mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (frame->tree().name() == name)
                return frame;
        }
    }

    return nullptr;
}

bool FrameTree::isDescendantOf(const Frame* ancestor) const
{
    if (!ancestor)
        return false;

    if (m_thisFrame->page() != ancestor->page())
        return false;

    for (Frame* frame = m_thisFrame; frame; frame = frame->tree().parent()) {
        if (frame == ancestor)
            return true;
    }
    return false;
}

Frame* FrameTree::traverseNext(const Frame* stayWithin) const
{
    Frame* child = firstChild();
    if (child) {
        ASSERT(!stayWithin || child->tree().isDescendantOf(stayWithin));
        return child;
    }

    if (m_thisFrame == stayWithin)
        return nullptr;

    Frame* sibling = nextSibling();
    if (sibling) {
        ASSERT(!stayWithin || sibling->tree().isDescendantOf(stayWithin));
        return sibling;
    }

    Frame* frame = m_thisFrame;
    while (!sibling && (!stayWithin || frame->tree().parent() != stayWithin)) {
        frame = frame->tree().parent();
        if (!frame)
            return nullptr;
        sibling = frame->tree().nextSibling();
    }

    if (frame) {
        ASSERT(!stayWithin || !sibling || sibling->tree().isDescendantOf(stayWithin));
        return sibling;
    }

    return nullptr;
}

Frame* FrameTree::traverseNextWithWrap(bool wrap) const
{
    if (Frame* result = traverseNext())
        return result;

    if (wrap)
        return m_thisFrame->page()->mainFrame();

    return nullptr;
}

Frame* FrameTree::traversePreviousWithWrap(bool wrap) const
{
    // FIXME: besides the wrap feature, this is just the traversePreviousNode algorithm

    if (Frame* prevSibling = previousSibling())
        return prevSibling->tree().deepLastChild();
    if (Frame* parentFrame = parent())
        return parentFrame;

    // no siblings, no parent, self==top
    if (wrap)
        return deepLastChild();

    // top view is always the last one in this ordering, so prev is nil without wrap
    return nullptr;
}

Frame* FrameTree::deepLastChild() const
{
    Frame* result = m_thisFrame;
    for (Frame* last = lastChild(); last; last = last->tree().lastChild())
        result = last;

    return result;
}

DEFINE_TRACE(FrameTree)
{
    visitor->trace(m_thisFrame);
}

} // namespace blink

#ifndef NDEBUG

static void printIndent(int indent)
{
    for (int i = 0; i < indent; ++i)
        printf("    ");
}

static void printFrames(const blink::Frame* frame, const blink::Frame* targetFrame, int indent)
{
    if (frame == targetFrame) {
        printf("--> ");
        printIndent(indent - 1);
    } else {
        printIndent(indent);
    }

    blink::FrameView* view = frame->isLocalFrame() ? toLocalFrame(frame)->view() : 0;
    printf("Frame %p %dx%d\n", frame, view ? view->width() : 0, view ? view->height() : 0);
    printIndent(indent);
    printf("  owner=%p\n", frame->owner());
    printIndent(indent);
    printf("  frameView=%p\n", view);
    printIndent(indent);
    printf("  document=%p\n", frame->isLocalFrame() ? toLocalFrame(frame)->document() : 0);
    printIndent(indent);
    printf("  uri=%s\n\n", frame->isLocalFrame() ? toLocalFrame(frame)->document()->url().getString().utf8().data() : 0);

    for (blink::Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling())
        printFrames(child, targetFrame, indent + 1);
}

void showFrameTree(const blink::Frame* frame)
{
    if (!frame) {
        printf("Null input frame\n");
        return;
    }

    printFrames(frame->tree().top(), frame, 0);
}

#endif
