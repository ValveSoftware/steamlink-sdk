/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 *
 */

#ifndef InlineIterator_h
#define InlineIterator_h

#include "core/layout/BidiRun.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/api/LineLayoutBlockFlow.h"
#include "core/layout/api/LineLayoutInline.h"
#include "core/layout/api/LineLayoutText.h"
#include "wtf/Allocator.h"
#include "wtf/StdLibExtras.h"

namespace blink {

struct BidiIsolatedRun {
    BidiIsolatedRun(LineLayoutItem object, unsigned position, LineLayoutItem& root, BidiRun& runToReplace, unsigned char level)
        : object(object)
        , root(root)
        , runToReplace(runToReplace)
        , position(position)
        , level(level)
    {
    }

    LineLayoutItem object;
    LineLayoutItem root;
    BidiRun& runToReplace;
    unsigned position;
    unsigned char level;
};


// This class is used to LayoutInline subtrees, stepping by character within the
// text children. InlineIterator will use bidiNext to find the next LayoutText
// optionally notifying a BidiResolver every time it steps into/out of a LayoutInline.
class InlineIterator {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    enum IncrementRule {
        FastIncrementInIsolatedLayout,
        FastIncrementInTextNode
    };

    InlineIterator()
        : m_root(nullptr)
        , m_lineLayoutItem(nullptr)
        , m_nextBreakablePosition(-1)
        , m_pos(0)
    {
    }

    InlineIterator(LineLayoutItem root, LineLayoutItem o, unsigned p)
        : m_root(root)
        , m_lineLayoutItem(o)
        , m_nextBreakablePosition(-1)
        , m_pos(p)
    {
    }

    void clear() { moveTo(0, 0); }

    void moveToStartOf(LineLayoutItem object)
    {
        moveTo(object, 0);
    }

    void moveTo(LineLayoutItem object, unsigned offset, int nextBreak = -1)
    {
        m_lineLayoutItem = object;
        m_pos = offset;
        m_nextBreakablePosition = nextBreak;
    }

    LineLayoutItem getLineLayoutItem() const { return m_lineLayoutItem; }
    void setLineLayoutItem(LineLayoutItem lineLayoutItem) { m_lineLayoutItem = lineLayoutItem; }

    int nextBreakablePosition() const { return m_nextBreakablePosition; }
    void setNextBreakablePosition(int position) { m_nextBreakablePosition = position; }

    unsigned offset() const { return m_pos; }
    void setOffset(unsigned position) { m_pos = position; }
    LineLayoutItem root() const { return m_root; }

    void fastIncrementInTextNode();
    void increment(InlineBidiResolver* = nullptr, IncrementRule = FastIncrementInTextNode);
    bool atEnd() const;

    inline bool atTextParagraphSeparator() const
    {
        return m_lineLayoutItem && m_lineLayoutItem.preservesNewline() && m_lineLayoutItem.isText() && LineLayoutText(m_lineLayoutItem).textLength()
            && !LineLayoutText(m_lineLayoutItem).isWordBreak() && LineLayoutText(m_lineLayoutItem).characterAt(m_pos) == '\n';
    }

    inline bool atParagraphSeparator() const
    {
        return (m_lineLayoutItem && m_lineLayoutItem.isBR()) || atTextParagraphSeparator();
    }

    UChar characterAt(unsigned) const;
    UChar current() const;
    UChar previousInSameNode() const;
    ALWAYS_INLINE WTF::Unicode::CharDirection direction() const;

private:
    LineLayoutItem m_root;
    LineLayoutItem m_lineLayoutItem;

    int m_nextBreakablePosition;
    unsigned m_pos;
};

inline bool operator==(const InlineIterator& it1, const InlineIterator& it2)
{
    return it1.offset() == it2.offset() && it1.getLineLayoutItem() == it2.getLineLayoutItem();
}

inline bool operator!=(const InlineIterator& it1, const InlineIterator& it2)
{
    return it1.offset() != it2.offset() || it1.getLineLayoutItem() != it2.getLineLayoutItem();
}

static inline WTF::Unicode::CharDirection embedCharFromDirection(TextDirection dir, EUnicodeBidi unicodeBidi)
{
    using namespace WTF::Unicode;
    if (unicodeBidi == Embed)
        return dir == RTL ? RightToLeftEmbedding : LeftToRightEmbedding;
    return dir == RTL ? RightToLeftOverride : LeftToRightOverride;
}

static inline bool treatAsIsolated(const ComputedStyle& style)
{
    return isIsolated(style.unicodeBidi()) && style.rtlOrdering() == LogicalOrder;
}

template <class Observer>
static inline void notifyObserverEnteredObject(Observer* observer, LineLayoutItem object)
{
    if (!observer || !object || !object.isLayoutInline())
        return;

    const ComputedStyle& style = object.styleRef();
    EUnicodeBidi unicodeBidi = style.unicodeBidi();
    if (unicodeBidi == UBNormal) {
        // http://dev.w3.org/csswg/css3-writing-modes/#unicode-bidi
        // "The element does not open an additional level of embedding with respect to the bidirectional algorithm."
        // Thus we ignore any possible dir= attribute on the span.
        return;
    }
    if (treatAsIsolated(style)) {
        // Make sure that explicit embeddings are committed before we enter the isolated content.
        observer->commitExplicitEmbedding(observer->runs());
        observer->enterIsolate();
        // Embedding/Override characters implied by dir= will be handled when
        // we process the isolated span, not when laying out the "parent" run.
        return;
    }

    if (!observer->inIsolate())
        observer->embed(embedCharFromDirection(style.direction(), unicodeBidi), FromStyleOrDOM);
}

template <class Observer>
static inline void notifyObserverWillExitObject(Observer* observer, LineLayoutItem object)
{
    if (!observer || !object || !object.isLayoutInline())
        return;

    EUnicodeBidi unicodeBidi = object.style()->unicodeBidi();
    if (unicodeBidi == UBNormal)
        return; // Nothing to do for unicode-bidi: normal
    if (treatAsIsolated(object.styleRef())) {
        observer->exitIsolate();
        return;
    }

    // Otherwise we pop any embed/override character we added when we opened this tag.
    if (!observer->inIsolate())
        observer->embed(WTF::Unicode::PopDirectionalFormat, FromStyleOrDOM);
}

static inline bool isIteratorTarget(LineLayoutItem object)
{
    ASSERT(object); // The iterator will of course return 0, but its not an expected argument to this function.
    return object.isText() || object.isFloating() || object.isOutOfFlowPositioned() || object.isAtomicInlineLevel();
}

// This enum is only used for bidiNextShared()
enum EmptyInlineBehavior {
    SkipEmptyInlines,
    IncludeEmptyInlines,
};

static bool isEmptyInline(LineLayoutItem object)
{
    if (!object.isLayoutInline())
        return false;

    for (LineLayoutItem curr = LineLayoutInline(object).firstChild(); curr; curr = curr.nextSibling()) {
        if (curr.isFloatingOrOutOfFlowPositioned())
            continue;
        if (curr.isText() && LineLayoutText(curr).isAllCollapsibleWhitespace())
            continue;

        if (!isEmptyInline(curr))
            return false;
    }
    return true;
}

// FIXME: This function is misleadingly named. It has little to do with bidi.
// This function will iterate over inlines within a block, optionally notifying
// a bidi resolver as it enters/exits inlines (so it can push/pop embedding levels).
template <class Observer>
static inline LineLayoutItem bidiNextShared(LineLayoutItem root, LineLayoutItem current, Observer* observer = 0, EmptyInlineBehavior emptyInlineBehavior = SkipEmptyInlines, bool* endOfInlinePtr = nullptr)
{
    LineLayoutItem next = nullptr;
    // oldEndOfInline denotes if when we last stopped iterating if we were at the end of an inline.
    bool oldEndOfInline = endOfInlinePtr ? *endOfInlinePtr : false;
    bool endOfInline = false;

    while (current) {
        next = 0;
        if (!oldEndOfInline && !isIteratorTarget(current)) {
            next = current.slowFirstChild();
            notifyObserverEnteredObject(observer, next);
        }

        // We hit this when either current has no children, or when current is not a layoutObject we care about.
        if (!next) {
            // If it is a layoutObject we care about, and we're doing our inline-walk, return it.
            if (emptyInlineBehavior == IncludeEmptyInlines && !oldEndOfInline && current.isLayoutInline()) {
                next = current;
                endOfInline = true;
                break;
            }

            while (current && current != root) {
                notifyObserverWillExitObject(observer, current);

                next = current.nextSibling();
                if (next) {
                    notifyObserverEnteredObject(observer, next);
                    break;
                }

                current = current.parent();
                if (emptyInlineBehavior == IncludeEmptyInlines && current && current != root && current.isLayoutInline()) {
                    next = current;
                    endOfInline = true;
                    break;
                }
            }
        }

        if (!next)
            break;

        if (isIteratorTarget(next)
            || ((emptyInlineBehavior == IncludeEmptyInlines || isEmptyInline(next)) // Always return EMPTY inlines.
                && next.isLayoutInline()))
            break;
        current = next;
    }

    if (endOfInlinePtr)
        *endOfInlinePtr = endOfInline;

    return next;
}

template <class Observer>
static inline LineLayoutItem bidiNextSkippingEmptyInlines(LineLayoutItem root, LineLayoutItem current, Observer* observer)
{
    // TODO(rhogan): Rename this caller. It's used for a detailed walk of every object in an inline flow, for example during line layout.
    // We always return empty inlines in bidiNextShared, which gives lie to the bidiNext[Skipping|Including]EmptyInlines
    // naming scheme we use to call it. bidiNextSkippingEmptyInlines is the less fussy of the two callers,
    // it will always try to advance and will return what it finds if it's a line layout object in isIteratorTarget or if
    // it's an empty LayoutInline. If the LayoutInline has content, it will advance past the start of the LayoutLine and try to return
    // one of its children.
    // The SkipEmptyInlines callers never care about endOfInlinePtr.
    return bidiNextShared(root, current, observer, SkipEmptyInlines);
}

// This makes callers cleaner as they don't have to specify a type for the observer when not providing one.
static inline LineLayoutItem bidiNextSkippingEmptyInlines(LineLayoutItem root, LineLayoutItem current)
{
    InlineBidiResolver* observer = nullptr;
    return bidiNextSkippingEmptyInlines(root, current, observer);
}

static inline LineLayoutItem bidiNextIncludingEmptyInlines(LineLayoutItem root, LineLayoutItem current, bool* endOfInlinePtr = nullptr)
{
    // TODO(rhogan): Rename this caller. It's used for quick and dirty walks of inline children by InlineWalker, which isn't
    // interested in the contents of inlines. Use cases include dirtying objects or simplified layout that leaves lineboxes intact.
    // bidiNextIncludingEmptyInlines will return if the iterator is at the start of a LayoutInline (even if it hasn't
    // advanced yet) unless it previously stopped at the start of the same LayoutInline the last time it tried to iterate.
    // If it finds itself inside a LayoutInline that doesn't have anything in isIteratorTarget it will return the enclosing
    // LayoutInline.
    InlineBidiResolver* observer = nullptr; // Callers who include empty inlines, never use an observer.
    return bidiNextShared(root, current, observer, IncludeEmptyInlines, endOfInlinePtr);
}

static inline LineLayoutItem bidiFirstSkippingEmptyInlines(LineLayoutBlockFlow root, BidiRunList<BidiRun>& runs, InlineBidiResolver* resolver = nullptr)
{
    LineLayoutItem o = root.firstChild();
    if (!o)
        return nullptr;

    if (o.isLayoutInline()) {
        notifyObserverEnteredObject(resolver, o);
        if (!isEmptyInline(o)) {
            o = bidiNextSkippingEmptyInlines(root, o, resolver);
        } else {
            // Never skip empty inlines.
            if (resolver)
                resolver->commitExplicitEmbedding(runs);
            return o;
        }
    }

    // FIXME: Unify this with the bidiNext call above.
    if (o && !isIteratorTarget(o))
        o = bidiNextSkippingEmptyInlines(root, o, resolver);

    if (resolver)
        resolver->commitExplicitEmbedding(runs);
    return o;
}

// FIXME: This method needs to be renamed when bidiNext finds a good name.
static inline LineLayoutItem bidiFirstIncludingEmptyInlines(LineLayoutBlockFlow root)
{
    LineLayoutItem o = root.firstChild();
    // If either there are no children to walk, or the first one is correct
    // then just return it.
    if (!o || o.isLayoutInline() || isIteratorTarget(o))
        return o;

    return bidiNextIncludingEmptyInlines(root, o);
}

inline void InlineIterator::fastIncrementInTextNode()
{
    ASSERT(m_lineLayoutItem);
    ASSERT(m_lineLayoutItem.isText());
    ASSERT(m_pos <= LineLayoutText(m_lineLayoutItem).textLength());
    if (m_pos < INT_MAX)
        m_pos++;
}

// FIXME: This is used by LayoutBlockFlow for simplified layout, and has nothing to do with bidi
// it shouldn't use functions called bidiFirst and bidiNext.
class InlineWalker {
    STACK_ALLOCATED();
public:
    InlineWalker(LineLayoutBlockFlow root)
        : m_root(root)
        , m_current(nullptr)
        , m_atEndOfInline(false)
    {
        // FIXME: This class should be taught how to do the SkipEmptyInlines codepath as well.
        m_current = bidiFirstIncludingEmptyInlines(m_root);
    }

    LineLayoutBlockFlow root() { return m_root; }
    LineLayoutItem current() { return m_current; }

    bool atEndOfInline() { return m_atEndOfInline; }
    bool atEnd() const { return !m_current; }

    LineLayoutItem advance()
    {
        // FIXME: Support SkipEmptyInlines and observer parameters.
        m_current = bidiNextIncludingEmptyInlines(m_root, m_current, &m_atEndOfInline);
        return m_current;
    }
private:
    LineLayoutBlockFlow m_root;
    LineLayoutItem m_current;
    bool m_atEndOfInline;
};

static inline bool endOfLineHasIsolatedObjectAncestor(const InlineIterator& isolatedIterator, const InlineIterator& ancestorItertor)
{
    if (!isolatedIterator.getLineLayoutItem() || !treatAsIsolated(isolatedIterator.getLineLayoutItem().styleRef()))
        return false;

    LineLayoutItem innerIsolatedObject = isolatedIterator.getLineLayoutItem();
    while (innerIsolatedObject && innerIsolatedObject != isolatedIterator.root()) {
        if (innerIsolatedObject == ancestorItertor.getLineLayoutItem())
            return true;
        innerIsolatedObject = innerIsolatedObject.parent();
    }
    return false;
}

inline void InlineIterator::increment(InlineBidiResolver* resolver, IncrementRule rule)
{
    if (!m_lineLayoutItem)
        return;

    if (rule == FastIncrementInIsolatedLayout
        && resolver && resolver->inIsolate()
        && !endOfLineHasIsolatedObjectAncestor(resolver->endOfLine(), resolver->position())) {
        moveTo(bidiNextSkippingEmptyInlines(m_root, m_lineLayoutItem, resolver), 0);
        return;
    }

    if (m_lineLayoutItem.isText()) {
        fastIncrementInTextNode();
        if (m_pos < LineLayoutText(m_lineLayoutItem).textLength())
            return;
    }
    // bidiNext can return 0, so use moveTo instead of moveToStartOf
    moveTo(bidiNextSkippingEmptyInlines(m_root, m_lineLayoutItem, resolver), 0);
}

inline bool InlineIterator::atEnd() const
{
    return !m_lineLayoutItem;
}

inline UChar InlineIterator::characterAt(unsigned index) const
{
    if (!m_lineLayoutItem || !m_lineLayoutItem.isText())
        return 0;

    return LineLayoutText(m_lineLayoutItem).characterAt(index);
}

inline UChar InlineIterator::current() const
{
    return characterAt(m_pos);
}

inline UChar InlineIterator::previousInSameNode() const
{
    if (!m_pos)
        return 0;

    return characterAt(m_pos - 1);
}

ALWAYS_INLINE WTF::Unicode::CharDirection InlineIterator::direction() const
{
    if (UChar c = current())
        return WTF::Unicode::direction(c);

    if (m_lineLayoutItem && m_lineLayoutItem.isListMarker())
        return m_lineLayoutItem.style()->isLeftToRightDirection() ? WTF::Unicode::LeftToRight : WTF::Unicode::RightToLeft;

    return WTF::Unicode::OtherNeutral;
}

template<>
inline void InlineBidiResolver::increment()
{
    m_current.increment(this, InlineIterator::FastIncrementInIsolatedLayout);
}

template <>
inline bool InlineBidiResolver::isEndOfLine(const InlineIterator& end)
{
    bool inEndOfLine = m_current == end || m_current.atEnd() || (inIsolate() && m_current.getLineLayoutItem() == end.getLineLayoutItem());
    if (inIsolate() && inEndOfLine) {
        m_current.moveTo(m_current.getLineLayoutItem(), end.offset(), m_current.nextBreakablePosition());
        m_last = m_current;
        updateStatusLastFromCurrentDirection(WTF::Unicode::OtherNeutral);
    }
    return inEndOfLine;
}

static inline bool isCollapsibleSpace(UChar character, LineLayoutText layoutText)
{
    if (character == ' ' || character == '\t' || character == softHyphenCharacter)
        return true;
    if (character == '\n')
        return !layoutText.style()->preserveNewline();
    return false;
}

template <typename CharacterType>
static inline int findFirstTrailingSpace(LineLayoutText lastText, const CharacterType* characters, int start, int stop)
{
    int firstSpace = stop;
    while (firstSpace > start) {
        UChar current = characters[firstSpace - 1];
        if (!isCollapsibleSpace(current, lastText))
            break;
        firstSpace--;
    }

    return firstSpace;
}

template <>
inline int InlineBidiResolver::findFirstTrailingSpaceAtRun(BidiRun* run)
{
    ASSERT(run);
    LineLayoutItem lastObject = LineLayoutItem(run->m_lineLayoutItem);
    if (!lastObject.isText())
        return run->m_stop;

    LineLayoutText lastText(lastObject);
    int firstSpace;
    if (lastText.is8Bit())
        firstSpace = findFirstTrailingSpace(lastText, lastText.characters8(), run->start(), run->stop());
    else
        firstSpace = findFirstTrailingSpace(lastText, lastText.characters16(), run->start(), run->stop());
    return firstSpace;
}

template <>
inline BidiRun* InlineBidiResolver::addTrailingRun(BidiRunList<BidiRun>& runs, int start, int stop, BidiRun* run, BidiContext* context, TextDirection direction) const
{
    BidiRun* newTrailingRun = new BidiRun(start, stop, run->m_lineLayoutItem, context, WTF::Unicode::OtherNeutral);
    if (direction == LTR)
        runs.addRun(newTrailingRun);
    else
        runs.prependRun(newTrailingRun);

    return newTrailingRun;
}

template <>
inline bool InlineBidiResolver::needsToApplyL1Rule(BidiRunList<BidiRun>& runs)
{
    if (!runs.logicallyLastRun()->m_lineLayoutItem.style()->breakOnlyAfterWhiteSpace()
        || !runs.logicallyLastRun()->m_lineLayoutItem.style()->autoWrap())
        return false;
    return true;
}

static inline bool isIsolatedInline(LineLayoutItem object)
{
    ASSERT(object);
    return object.isLayoutInline() && treatAsIsolated(object.styleRef());
}

static inline LineLayoutItem highestContainingIsolateWithinRoot(LineLayoutItem object, LineLayoutItem root)
{
    ASSERT(object);
    LineLayoutItem containingIsolateObj(nullptr);
    while (object && object != root) {
        if (isIsolatedInline(object))
            containingIsolateObj = LineLayoutItem(object);

        object = object.parent();
        ASSERT(object);
    }
    return containingIsolateObj;
}

static inline unsigned numberOfIsolateAncestors(const InlineIterator& iter)
{
    LineLayoutItem object = iter.getLineLayoutItem();
    if (!object)
        return 0;
    unsigned count = 0;
    while (object && object != iter.root()) {
        if (isIsolatedInline(object))
            count++;
        object = object.parent();
    }
    return count;
}

// FIXME: This belongs on InlineBidiResolver, except it's a template specialization
// of BidiResolver which knows nothing about LayoutObjects.
static inline BidiRun* addPlaceholderRunForIsolatedInline(InlineBidiResolver& resolver, LineLayoutItem obj, unsigned pos, LineLayoutItem root)
{
    ASSERT(obj);
    BidiRun* isolatedRun = new BidiRun(pos, pos, obj, resolver.context(), resolver.dir());
    resolver.runs().addRun(isolatedRun);
    // FIXME: isolatedRuns() could be a hash of object->run and then we could cheaply
    // ASSERT here that we didn't create multiple objects for the same inline.
    resolver.isolatedRuns().append(BidiIsolatedRun(obj, pos, root, *isolatedRun, resolver.context()->level()));
    return isolatedRun;
}

static inline BidiRun* createRun(int start, int end, LineLayoutItem obj, InlineBidiResolver& resolver)
{
    return new BidiRun(start, end, obj, resolver.context(), resolver.dir());
}

enum AppendRunBehavior {
    AppendingFakeRun,
    AppendingRunsForObject
};

class IsolateTracker {
    STACK_ALLOCATED();
public:
    explicit IsolateTracker(BidiRunList<BidiRun>& runs, unsigned nestedIsolateCount)
        : m_nestedIsolateCount(nestedIsolateCount)
        , m_haveAddedFakeRunForRootIsolate(false)
        , m_runs(runs)
    {
    }

    void setMidpointStateForRootIsolate(const LineMidpointState& midpointState)
    {
        m_midpointStateForRootIsolate = midpointState;
    }

    void enterIsolate() { m_nestedIsolateCount++; }
    void exitIsolate()
    {
        ASSERT(m_nestedIsolateCount >= 1);
        m_nestedIsolateCount--;
        if (!inIsolate())
            m_haveAddedFakeRunForRootIsolate = false;
    }
    bool inIsolate() const { return m_nestedIsolateCount; }

    // We don't care if we encounter bidi directional overrides.
    void embed(WTF::Unicode::CharDirection, BidiEmbeddingSource) {}
    void commitExplicitEmbedding(BidiRunList<BidiRun>&) { }
    BidiRunList<BidiRun>& runs() { return m_runs; }

    void addFakeRunIfNecessary(LineLayoutItem obj, unsigned pos, unsigned end, LineLayoutItem root, InlineBidiResolver& resolver)
    {
        // We only need to add a fake run for a given isolated span once during each call to createBidiRunsForLine.
        // We'll be called for every span inside the isolated span so we just ignore subsequent calls.
        // We also avoid creating a fake run until we hit a child that warrants one, e.g. we skip floats.
        if (LayoutBlockFlow::shouldSkipCreatingRunsForObject(obj))
            return;
        if (!m_haveAddedFakeRunForRootIsolate) {
            BidiRun* run = addPlaceholderRunForIsolatedInline(resolver, obj, pos, root);
            resolver.setMidpointStateForIsolatedRun(*run, m_midpointStateForRootIsolate);
            m_haveAddedFakeRunForRootIsolate = true;
        }
        // obj and pos together denote a single position in the inline, from which the parsing of the isolate will start.
        // We don't need to mark the end of the run because this is implicit: it is either endOfLine or the end of the
        // isolate, when we call createBidiRunsForLine it will stop at whichever comes first.
    }

private:
    unsigned m_nestedIsolateCount;
    bool m_haveAddedFakeRunForRootIsolate;
    LineMidpointState m_midpointStateForRootIsolate;
    BidiRunList<BidiRun>& m_runs;
};

static void inline appendRunObjectIfNecessary(LineLayoutItem obj, unsigned start, unsigned end, LineLayoutItem root, InlineBidiResolver& resolver, AppendRunBehavior behavior, IsolateTracker& tracker)
{
    // Trailing space code creates empty BidiRun objects, start == end, so
    // that case needs to be handled specifically.
    bool addEmptyRun = (end == start);

    // Append BidiRun objects, at most 64K chars at a time, until all
    // text between |start| and |end| is represented.
    while (end > start || addEmptyRun) {
        addEmptyRun = false;
        const int limit = USHRT_MAX; // InlineTextBox stores text length as unsigned short.
        unsigned limitedEnd = end;
        if (end - start > limit)
            limitedEnd = start + limit;
        if (behavior == AppendingFakeRun)
            tracker.addFakeRunIfNecessary(obj, start, limitedEnd, root, resolver);
        else
            resolver.runs().addRun(createRun(start, limitedEnd, obj, resolver));
        start = limitedEnd;
    }
}

static void adjustMidpointsAndAppendRunsForObjectIfNeeded(LineLayoutItem obj, unsigned start, unsigned end, LineLayoutItem root, InlineBidiResolver& resolver, AppendRunBehavior behavior, IsolateTracker& tracker)
{
    if (start > end || LayoutBlockFlow::shouldSkipCreatingRunsForObject(obj))
        return;

    LineMidpointState& lineMidpointState = resolver.midpointState();
    bool haveNextMidpoint = (lineMidpointState.currentMidpoint() < lineMidpointState.numMidpoints());
    InlineIterator nextMidpoint;
    if (haveNextMidpoint)
        nextMidpoint = lineMidpointState.midpoints()[lineMidpointState.currentMidpoint()];
    if (lineMidpointState.betweenMidpoints()) {
        if (!(haveNextMidpoint && nextMidpoint.getLineLayoutItem() == obj))
            return;
        // This is a new start point. Stop ignoring objects and
        // adjust our start.
        lineMidpointState.setBetweenMidpoints(false);
        start = nextMidpoint.offset();
        lineMidpointState.incrementCurrentMidpoint();
        if (start < end)
            return adjustMidpointsAndAppendRunsForObjectIfNeeded(obj, start, end, root, resolver, behavior, tracker);
    } else {
        if (!haveNextMidpoint || (obj != nextMidpoint.getLineLayoutItem())) {
            appendRunObjectIfNecessary(obj, start, end, root, resolver, behavior, tracker);
            return;
        }

        // An end midpoint has been encountered within our object. We
        // need to go ahead and append a run with our endpoint.
        if (nextMidpoint.offset() + 1 <= end) {
            lineMidpointState.setBetweenMidpoints(true);
            lineMidpointState.incrementCurrentMidpoint();
            if (nextMidpoint.offset() != UINT_MAX) { // UINT_MAX means stop at the object and don't nclude any of it.
                if (nextMidpoint.offset() + 1 > start)
                    appendRunObjectIfNecessary(obj, start, nextMidpoint.offset() + 1, root, resolver, behavior, tracker);
                return adjustMidpointsAndAppendRunsForObjectIfNeeded(obj, nextMidpoint.offset() + 1, end, root, resolver, behavior, tracker);
            }
        } else {
            appendRunObjectIfNecessary(obj, start, end, root, resolver, behavior, tracker);
        }
    }
}

static inline void addFakeRunIfNecessary(LineLayoutItem obj, unsigned start, unsigned end, LineLayoutItem root, InlineBidiResolver& resolver, IsolateTracker& tracker)
{
    tracker.setMidpointStateForRootIsolate(resolver.midpointState());
    adjustMidpointsAndAppendRunsForObjectIfNeeded(obj, start, obj.length(), root, resolver, AppendingFakeRun, tracker);
}

template <>
inline void InlineBidiResolver::appendRun(BidiRunList<BidiRun>& runs)
{
    if (!m_emptyRun && !m_eor.atEnd() && !m_reachedEndOfLine) {
        // Keep track of when we enter/leave "unicode-bidi: isolate" inlines.
        // Initialize our state depending on if we're starting in the middle of such an inline.
        // FIXME: Could this initialize from this->inIsolate() instead of walking up the layout tree?
        IsolateTracker isolateTracker(runs, numberOfIsolateAncestors(m_sor));
        int start = m_sor.offset();
        LineLayoutItem obj = m_sor.getLineLayoutItem();
        while (obj && obj != m_eor.getLineLayoutItem() && obj != m_endOfRunAtEndOfLine.getLineLayoutItem()) {
            if (isolateTracker.inIsolate())
                addFakeRunIfNecessary(obj, start, obj.length(), m_sor.root(), *this, isolateTracker);
            else
                adjustMidpointsAndAppendRunsForObjectIfNeeded(obj, start, obj.length(), m_sor.root(), *this, AppendingRunsForObject, isolateTracker);
            // FIXME: start/obj should be an InlineIterator instead of two separate variables.
            start = 0;
            obj = bidiNextSkippingEmptyInlines(m_sor.root(), obj, &isolateTracker);
        }
        bool isEndOfLine = obj == m_endOfLine.getLineLayoutItem() && !m_endOfLine.offset();
        if (obj && !isEndOfLine) {
            unsigned pos = obj == m_eor.getLineLayoutItem() ? m_eor.offset() : INT_MAX;
            if (obj == m_endOfRunAtEndOfLine.getLineLayoutItem() && m_endOfRunAtEndOfLine.offset() <= pos) {
                m_reachedEndOfLine = true;
                pos = m_endOfRunAtEndOfLine.offset();
            }
            // It's OK to add runs for zero-length LayoutObjects, just don't make the run larger than it should be
            int end = obj.length() ? pos + 1 : 0;
            if (isolateTracker.inIsolate())
                addFakeRunIfNecessary(obj, start, end, m_sor.root(), *this, isolateTracker);
            else
                adjustMidpointsAndAppendRunsForObjectIfNeeded(obj, start, end, m_sor.root(), *this, AppendingRunsForObject, isolateTracker);
        }

        if (isEndOfLine)
            m_reachedEndOfLine = true;
        // If isolateTrack is inIsolate, the next |start of run| can not be the current isolated layoutObject.
        if (isolateTracker.inIsolate())
            m_eor.moveTo(bidiNextSkippingEmptyInlines(m_eor.root(), m_eor.getLineLayoutItem()), 0);
        else
            m_eor.increment();
        m_sor = m_eor;
    }

    m_direction = WTF::Unicode::OtherNeutral;
    m_status.eor = WTF::Unicode::OtherNeutral;
}

} // namespace blink

#endif // InlineIterator_h
