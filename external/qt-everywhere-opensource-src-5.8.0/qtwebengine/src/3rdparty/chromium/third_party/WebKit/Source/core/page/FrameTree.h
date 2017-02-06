/*
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

#ifndef FrameTree_h
#define FrameTree_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class Frame;
class TreeScope;

class CORE_EXPORT FrameTree final {
    WTF_MAKE_NONCOPYABLE(FrameTree);
    DISALLOW_NEW();
public:
    explicit FrameTree(Frame* thisFrame);
    ~FrameTree();

    const AtomicString& name() const { return m_name; }
    void setName(const AtomicString&);

    // Unique name of a frame (unique per page).  Mainly used to identify the
    // frame for session history purposes, but also used in expected results
    // of layout tests.
    //
    // The value should be treated as an unstructured, opaque string.
    const AtomicString& uniqueName() const { return m_uniqueName; }

    // Directly assigns both the name and uniqueName.  Can be used when
    // |uniqueName| is already known (i.e. when it has been precalculated by
    // calculateUniqueNameForNewChildFrame OR when replicating the name between
    // LocalFrames and RemoteFrames for the same logical frame).
    void setPrecalculatedName(const AtomicString& name, const AtomicString& uniqueName);

    Frame* parent() const;
    Frame* top() const;
    Frame* previousSibling() const;
    Frame* nextSibling() const;
    Frame* firstChild() const;
    Frame* lastChild() const;

    bool isDescendantOf(const Frame* ancestor) const;
    Frame* traversePreviousWithWrap(bool) const;
    Frame* traverseNext(const Frame* stayWithin = nullptr) const;
    Frame* traverseNextWithWrap(bool) const;

    Frame* child(const AtomicString& name) const;
    Frame* find(const AtomicString& name) const;
    unsigned childCount() const;

    Frame* scopedChild(unsigned index) const;
    Frame* scopedChild(const AtomicString& name) const;
    unsigned scopedChildCount() const;
    void invalidateScopedChildCount();

    DECLARE_TRACE();

    AtomicString calculateUniqueNameForNewChildFrame(
        const AtomicString& name,
        const AtomicString& fallbackName = nullAtom) const;

private:
    Frame* deepLastChild() const;

    // Returns true if one of frames in the tree already has unique name equal
    // to |uniqueNameCandidate|.
    bool uniqueNameExists(const String& uniqueNameCandidate) const;

    // Generates a hopefully-but-not-necessarily unique name based on frame's
    // relative position in the tree and on unique names of ancestors.
    String generateUniqueNameCandidate(bool existingChildFrame) const;

    // Generates a hopefully-but-not-necessarily unique suffix based on |child|
    // absolute position in the tree.  If |child| is nullptr, calculations are
    // made for a position that a new child of |this| would have.
    String generateFramePosition(Frame* child) const;

    // Concatenates |prefix|, |likelyUniqueSuffix| (and additional, internally
    // generated suffix) until the result is a unique name, that doesn't exist
    // elsewhere in the frame tree.  Returns the unique name built in this way.
    AtomicString appendUniqueSuffix(
        const String& prefix,
        const String& likelyUniqueSuffix) const;

    // Calculates a unique name for |child| frame (which might be nullptr if the
    // child has not yet been created - i.e. when we need unique name for a new
    // frame).  Tries to use the |assignedName| or |fallbackName| if possible,
    // otherwise falls back to generating a deterministic,
    // stable-across-page-reloads string based on |child| position in the tree.
    AtomicString calculateUniqueNameForChildFrame(
        Frame* child,
        const AtomicString& assignedName,
        const AtomicString& fallbackName = nullAtom) const;

    // Sets |m_uniqueName| and asserts its uniqueness.
    void setUniqueName(const AtomicString&);

    Member<Frame> m_thisFrame;

    AtomicString m_name; // The actual frame name (may be empty).
    AtomicString m_uniqueName;

    mutable unsigned m_scopedChildCount;
};

} // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showFrameTree(const blink::Frame*);
#endif

#endif // FrameTree_h
