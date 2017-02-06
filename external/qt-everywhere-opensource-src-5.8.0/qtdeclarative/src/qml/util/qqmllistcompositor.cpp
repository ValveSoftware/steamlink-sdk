/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qqmllistcompositor_p.h"

#include <QtCore/qvarlengtharray.h>

//#define QT_QML_VERIFY_MINIMAL
//#define QT_QML_VERIFY_INTEGRITY

QT_BEGIN_NAMESPACE

/*!
    \class QQmlListCompositor
    \brief The QQmlListCompositor class provides a lookup table for filtered, or re-ordered list
    indexes.
    \internal

    QQmlListCompositor is intended as an aid for developing proxy models.  It doesn't however
    directly proxy a list or model, instead a range of indexes from one or many lists can be
    inserted into the compositor and then categorized and shuffled around and it will manage the
    task of translating from an index in the combined space into an index in a particular list.

    Within a compositor indexes are categorized into groups where a group is a sub-set of the
    total indexes referenced by the compositor, each with an address space ranging from 0 to
    the number of indexes in the group - 1.  Group memberships are independent of each other with
    the one exception that items always retain the same order so if an index is moved within a
    group, its position in other groups will change as well.

    The iterator classes encapsulate information about a specific position in a compositor group.
    This includes a source list, the index of an item within that list and the groups that item
    is a member of.  The iterator for a specific position in a group can be retrieved with the
    find() function and the addition and subtraction operators of the iterators can be used to
    navigate to adjacent items in the same group.

    Items can be added to the compositor with the append() and insert() functions, group
    membership can be changed with the setFlags() and clearFlags() functions, and the position
    of items in the compositor can be changed with the move() function.  Each of these functions
    optionally returns a list of the changes made to indexes within each group which can then
    be propagated to view so that it can correctly refresh its contents; e.g. 3 items
    removed at index 6, and 5 items inserted at index 1.  The notification changes are always
    ordered from the start of the list to the end and accumulate, so if 5 items are removed at
    index 4, one is skipped and then 3 move are removed, the changes returned are 5 items removed
    at index 4, followed by 3 items removed at index 4.

    When the contents of a source list change, the mappings within the compositor can be updated
    with the listItemsInserted(), listItemsRemoved(), listItemsMoved(), and listItemsChanged()
    functions.  Like the direct manipulation functions these too return a list of group indexes
    affected by the change.  If items are removed from a source list they are also removed from
    any groups they belong to with the one exception being items belonging to the \l Cache group.
    When items belonging to this group are removed the list, index, and other group membership
    information are discarded but Cache membership is retained until explicitly removed.  This
    allows the cache index to be retained until cached resources for that item are actually
    released.

    Internally the index mapping is stored as a list of Range objects, each has a list identifier,
    a start index, a count, and a set of flags which represent group membership and some other
    properties.  The group index of a range is the sum of all preceding ranges that are members of
    that group.  To avoid the inefficiency of iterating over potentially all ranges when looking
    for a specific index, each time a lookup is done the range and its indexes are cached and the
    next lookup is done relative to this.   This works out to near constant time in most relevant
    use cases because successive index lookups are most frequently adjacent.  The total number of
    ranges is often quite small, which helps as well. If there is a need for faster random access
    then a skip list like index may be an appropriate addition.

    \sa VisualDataModel
*/

#ifdef QT_QML_VERIFY_MINIMAL
#define QT_QML_VERIFY_INTEGRITY
/*
    Diagnostic to verify there are no consecutive ranges, or that the compositor contains the
    most compact representation possible.

    Returns false and prints a warning if any range has a starting index equal to the end
    (index + count) index of the previous range, and both ranges also have the same flags and list
    property.

    If there are no consecutive ranges this will return true.
*/

static bool qt_verifyMinimal(
        const QQmlListCompositor::iterator &begin,
        const QQmlListCompositor::iterator &end)
{
    bool minimal = true;
    int index = 0;

    for (const QQmlListCompositor::Range *range = begin->next; range != *end; range = range->next, ++index) {
        if (range->previous->list == range->list
                && range->previous->flags == (range->flags & ~QQmlListCompositor::AppendFlag)
                && range->previous->end() == range->index) {
            qWarning() << index << "Consecutive ranges";
            qWarning() << *range->previous;
            qWarning() << *range;
            minimal = false;
        }
    }

    return minimal;
}

#endif

#ifdef QT_QML_VERIFY_INTEGRITY
static bool qt_printInfo(const QQmlListCompositor &compositor)
{
    qWarning() << compositor;
    return true;
}

/*
    Diagnostic to verify the integrity of a compositor.

    Per range this verifies there are no invalid range combinations, that non-append ranges have
    positive non-zero counts, and that list ranges have non-negative indexes.

    Accumulatively this verifies that the cached total group counts match the sum of counts
    of member ranges.
*/

static bool qt_verifyIntegrity(
        const QQmlListCompositor::iterator &begin,
        const QQmlListCompositor::iterator &end,
        const QQmlListCompositor::iterator &cachedIt)
{
    bool valid = true;

    int index = 0;
    QQmlListCompositor::iterator it;
    for (it = begin; *it != *end; *it = it->next) {
        if (it->count == 0 && !it->append()) {
            qWarning() << index << "Empty non-append range";
            valid = false;
        }
        if (it->count < 0) {
            qWarning() << index << "Negative count";
            valid = false;
        }
        if (it->list && it->flags != QQmlListCompositor::CacheFlag && it->index < 0) {
            qWarning() << index <<"Negative index";
            valid = false;
        }
        if (it->previous->next != it.range) {
            qWarning() << index << "broken list: it->previous->next != it.range";
            valid = false;
        }
        if (it->next->previous != it.range) {
            qWarning() << index << "broken list: it->next->previous != it.range";
            valid = false;
        }
        if (*it == *cachedIt) {
            for (int i = 0; i < end.groupCount; ++i) {
                int groupIndex = it.index[i];
                if (cachedIt->flags & (1 << i))
                    groupIndex += cachedIt.offset;
                if (groupIndex != cachedIt.index[i]) {
                    qWarning() << index
                            << "invalid cached index"
                            << QQmlListCompositor::Group(i)
                            << "Expected:"
                            << groupIndex
                            << "Actual"
                            << cachedIt.index[i]
                            << cachedIt;
                    valid = false;
                }
            }
        }
        it.incrementIndexes(it->count);
        ++index;
    }

    for (int i = 0; i < end.groupCount; ++i) {
        if (end.index[i] != it.index[i]) {
            qWarning() << "Group" << i << "count invalid. Expected:" << end.index[i] << "Actual:" << it.index[i];
            valid = false;
        }
    }
    return valid;
}
#endif

#if defined(QT_QML_VERIFY_MINIMAL)
#   define QT_QML_VERIFY_LISTCOMPOSITOR Q_ASSERT(!(!(qt_verifyIntegrity(iterator(m_ranges.next, 0, Default, m_groupCount), m_end, m_cacheIt) \
            && qt_verifyMinimal(iterator(m_ranges.next, 0, Default, m_groupCount), m_end)) \
            && qt_printInfo(*this)));
#elif defined(QT_QML_VERIFY_INTEGRITY)
#   define QT_QML_VERIFY_LISTCOMPOSITOR Q_ASSERT(!(!qt_verifyIntegrity(iterator(m_ranges.next, 0, Default, m_groupCount), m_end, m_cacheIt) \
            && qt_printInfo(*this)));
#else
#   define QT_QML_VERIFY_LISTCOMPOSITOR
#endif

//#define QT_QML_TRACE_LISTCOMPOSITOR(args) qDebug() << m_end.index[1] << m_end.index[0] << Q_FUNC_INFO args;
#define QT_QML_TRACE_LISTCOMPOSITOR(args)

QQmlListCompositor::iterator &QQmlListCompositor::iterator::operator +=(int difference)
{
    // Update all indexes to the start of the range.
    decrementIndexes(offset);

    // If the iterator group isn't a member of the current range ignore the current offset.
    if (!(range->flags & groupFlag))
        offset = 0;

    offset += difference;

    // Iterate backwards looking for a range with a positive offset.
    while (offset <= 0 && range->previous->flags) {
        range = range->previous;
        if (range->flags & groupFlag)
            offset += range->count;
        decrementIndexes(range->count);
    }

    // Iterate forwards looking for the first range which contains both the offset and the
    // iterator group.
    while (range->flags && (offset >= range->count || !(range->flags & groupFlag))) {
        if (range->flags & groupFlag)
            offset -= range->count;
        incrementIndexes(range->count);
        range = range->next;
    }

    // Update all the indexes to inclue the remaining offset.
    incrementIndexes(offset);

    return *this;
}

QQmlListCompositor::insert_iterator &QQmlListCompositor::insert_iterator::operator +=(int difference)
{
    iterator::operator +=(difference);

    // If the previous range contains the append flag move the iterator to the tail of the previous
    // range so that appended appear after the insert position.
    if (offset == 0 && range->previous->append()) {
        range = range->previous;
        offset = range->inGroup() ? range->count : 0;
    }

    return *this;
}


/*!
    Constructs an empty list compositor.
*/

QQmlListCompositor::QQmlListCompositor()
    : m_end(m_ranges.next, 0, Default, 2)
    , m_cacheIt(m_end)
    , m_groupCount(2)
    , m_defaultFlags(PrependFlag | DefaultFlag)
    , m_removeFlags(AppendFlag | PrependFlag | GroupMask)
    , m_moveId(0)
{
}

/*!
    Destroys a list compositor.
*/

QQmlListCompositor::~QQmlListCompositor()
{
    for (Range *next, *range = m_ranges.next; range != &m_ranges; range = next) {
        next = range->next;
        delete range;
    }
}

/*!
    Inserts a range with the given source \a list, start \a index, \a count and \a flags, in front
    of the existing range \a before.
*/

inline QQmlListCompositor::Range *QQmlListCompositor::insert(
        Range *before, void *list, int index, int count, uint flags)
{
    return new Range(before, list, index, count, flags);
}

/*!
    Erases a \a range from the compositor.

    Returns a pointer to the next range in the compositor.
*/

inline QQmlListCompositor::Range *QQmlListCompositor::erase(
        Range *range)
{
    Range *next = range->next;
    next->previous = range->previous;
    next->previous->next = range->next;
    delete range;
    return next;
}

/*!
    Sets the number (\a count) of possible groups that items may belong to in a compositor.
*/

void QQmlListCompositor::setGroupCount(int count)
{
    m_groupCount = count;
    m_end = iterator(&m_ranges, 0, Default, m_groupCount);
    m_cacheIt = m_end;
}

/*!
    Returns the number of items that belong to a \a group.
*/

int QQmlListCompositor::count(Group group) const
{
    return m_end.index[group];
}

/*!
    Returns an iterator representing the item at \a index in a \a group.

    The index must be between 0 and count(group) - 1.
*/

QQmlListCompositor::iterator QQmlListCompositor::find(Group group, int index)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< group << index)
    Q_ASSERT(index >=0 && index < count(group));
    if (m_cacheIt == m_end) {
        m_cacheIt = iterator(m_ranges.next, 0, group, m_groupCount);
        m_cacheIt += index;
    } else {
        const int offset = index - m_cacheIt.index[group];
        m_cacheIt.setGroup(group);
        m_cacheIt += offset;
    }
    Q_ASSERT(m_cacheIt.index[group] == index);
    Q_ASSERT(m_cacheIt->inGroup(group));
    QT_QML_VERIFY_LISTCOMPOSITOR
    return m_cacheIt;
}

/*!
    Returns an iterator representing the item at \a index in a \a group.

    The index must be between 0 and count(group) - 1.
*/

QQmlListCompositor::iterator QQmlListCompositor::find(Group group, int index) const
{
    return const_cast<QQmlListCompositor *>(this)->find(group, index);
}

/*!
    Returns an iterator representing an insert position in front of the item at \a index in a
    \a group.

    The iterator for an insert position can sometimes resolve to a different Range than a regular
    iterator.  This is because when items are inserted on a boundary between Ranges, if the first
    range has the Append flag set then the items should be inserted into that range to ensure
    that the append position for the existing range remains after the insert position.

    The index must be between 0 and count(group) - 1.
*/

QQmlListCompositor::insert_iterator QQmlListCompositor::findInsertPosition(Group group, int index)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< group << index)
    Q_ASSERT(index >=0 && index <= count(group));
    insert_iterator it;
    if (m_cacheIt == m_end) {
        it = iterator(m_ranges.next, 0, group, m_groupCount);
        it += index;
    } else {
        const int offset = index - m_cacheIt.index[group];
        it = m_cacheIt;
        it.setGroup(group);
        it += offset;
    }
    Q_ASSERT(it.index[group] == index);
    return it;
}

/*!
    Appends a range of \a count indexes starting at \a index from a \a list into a compositor
    with the given \a flags.

    If supplied the \a inserts list will be populated with the positions of the inserted items
    in each group.
*/

void QQmlListCompositor::append(
        void *list, int index, int count, uint flags, QVector<Insert> *inserts)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< list << index << count << flags)
    insert(m_end, list, index, count, flags, inserts);
}

/*!
    Inserts a range of \a count indexes starting at \a index from a \a list with the given \a flags
    into a \a group at index \a before.

    If supplied the \a inserts list will be populated with the positions of items inserted into
    each group.
*/

void QQmlListCompositor::insert(
        Group group, int before, void *list, int index, int count, uint flags, QVector<Insert> *inserts)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< group << before << list << index << count << flags)
    insert(findInsertPosition(group, before), list, index, count, flags, inserts);
}

/*!
    Inserts a range of \a count indexes starting at \a index from a \a list with the given \a flags
    into a compositor at position \a before.

    If supplied the \a inserts list will be populated with the positions of items inserted into
    each group.
*/

QQmlListCompositor::iterator QQmlListCompositor::insert(
        iterator before, void *list, int index, int count, uint flags, QVector<Insert> *inserts)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< before << list << index << count << flags)
    if (inserts) {
        inserts->append(Insert(before, count, flags & GroupMask));
    }
    if (before.offset > 0) {
        // Inserting into the middle of a range.  Split it two and update the iterator so it's
        // positioned at the start of the second half.
        *before = insert(
                *before, before->list, before->index, before.offset, before->flags & ~AppendFlag)->next;
        before->index += before.offset;
        before->count -= before.offset;
        before.offset = 0;
    }


    if (!(flags & AppendFlag) && *before != m_ranges.next
            && before->previous->list == list
            && before->previous->flags == flags
            && (!list || before->previous->end() == index)) {
        // The insert arguments represent a continuation of the previous range so increment
        // its count instead of inserting a new range.
        before->previous->count += count;
        before.incrementIndexes(count, flags);
    } else {
        *before = insert(*before, list, index, count, flags);
        before.offset = 0;
    }

    if (!(flags & AppendFlag) && before->next != &m_ranges
            && before->list == before->next->list
            && before->flags == before->next->flags
            && (!list || before->end() == before->next->index)) {
        // The current range and the next are continuous so add their counts and delete one.
        before->next->index = before->index;
        before->next->count += before->count;
        *before = erase(*before);
    }

    m_end.incrementIndexes(count, flags);
    m_cacheIt = before;
    QT_QML_VERIFY_LISTCOMPOSITOR
    return before;
}

/*!
    Sets the given flags \a flags on \a count items belonging to \a group starting at the position
    identified by \a fromGroup and the index \a from.

    If supplied the \a inserts list will be populated with insert notifications for affected groups.
*/

void QQmlListCompositor::setFlags(
        Group fromGroup, int from, int count, Group group, int flags, QVector<Insert> *inserts)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< fromGroup << from << count << group << flags)
    setFlags(find(fromGroup, from), count, group, flags, inserts);
}

/*!
    Sets the given flags \a flags on \a count items belonging to \a group starting at the position
    \a from.

    If supplied the \a inserts list will be populated with insert notifications for affected groups.
*/

void QQmlListCompositor::setFlags(
        iterator from, int count, Group group, uint flags, QVector<Insert> *inserts)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< from << count << flags)
    if (!flags || !count)
        return;

    if (from != group) {
        // Skip to the next full range if the start one is not a member of the target group.
        from.incrementIndexes(from->count - from.offset);
        from.offset = 0;
        *from = from->next;
    } else if (from.offset > 0) {
        // If the start position is mid range split off the portion unaffected.
        *from = insert(*from, from->list, from->index, from.offset, from->flags & ~AppendFlag)->next;
        from->index += from.offset;
        from->count -= from.offset;
        from.offset = 0;
    }

    for (; count > 0; *from = from->next) {
        if (from != from.group) {
            // Skip ranges that are not members of the target group.
            from.incrementIndexes(from->count);
            continue;
        }
        // Find the number of items affected in the current range.
        const int difference = qMin(count, from->count);
        count -= difference;

        // Determine the actual changes made to the range and increment counts accordingly.
        const uint insertFlags = ~from->flags & flags;
        const uint setFlags = (from->flags | flags) & ~AppendFlag;
        if (insertFlags && inserts)
            inserts->append(Insert(from, difference, insertFlags | (from->flags & CacheFlag)));
        m_end.incrementIndexes(difference, insertFlags);
        from.incrementIndexes(difference, setFlags);

        if (from->previous != &m_ranges
                && from->previous->list == from->list
                && (!from->list || from->previous->end() == from->index)
                && from->previous->flags == setFlags) {
            // If the additional flags make the current range a continuation of the previous
            // then move the affected items over to the previous range.
            from->previous->count += difference;
            from->index += difference;
            from->count -= difference;
            if (from->count == 0) {
                // Delete the current range if it is now empty, preserving the append flag
                // in the previous range.
                if (from->append())
                    from->previous->flags |= AppendFlag;
                *from = erase(*from)->previous;
                continue;
            } else {
                break;
            }
        } else if (!insertFlags) {
            // No new flags, so roll onto the next range.
            from.incrementIndexes(from->count - difference);
            continue;
        } else if (difference < from->count) {
            // Create a new range with the updated flags, and remove the affected items
            // from the current range.
            *from = insert(*from, from->list, from->index, difference, setFlags)->next;
            from->index += difference;
            from->count -= difference;
        } else {
            // The whole range is affected so simply update the flags.
            from->flags |= flags;
            continue;
        }
        from.incrementIndexes(from->count);
    }

    if (from->previous != &m_ranges
            && from->previous->list == from->list
            && (!from->list || from->previous->end() == from->index)
            && from->previous->flags == (from->flags & ~AppendFlag)) {
        // If the following range is now a continuation, merge it with its previous range.
        from.offset = from->previous->count;
        from->previous->count += from->count;
        from->previous->flags = from->flags;
        *from = erase(*from)->previous;
    }
    m_cacheIt = from;
    QT_QML_VERIFY_LISTCOMPOSITOR
}

/*!
    Clears the given flags \a flags on \a count items belonging to \a group starting at the position
    \a from.

    If supplied the \a removes list will be populated with remove notifications for affected groups.
*/

void QQmlListCompositor::clearFlags(
        Group fromGroup, int from, int count, Group group, uint flags, QVector<Remove> *removes)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< fromGroup << from << count << group << flags)
    clearFlags(find(fromGroup, from), count, group, flags, removes);
}

/*!
    Clears the given flags \a flags on \a count items belonging to \a group starting at the position
    identified by \a fromGroup and the index \a from.

    If supplied the \a removes list will be populated with remove notifications for affected groups.
*/

void QQmlListCompositor::clearFlags(
        iterator from, int count, Group group, uint flags, QVector<Remove> *removes)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< from << count << flags)
    if (!flags || !count)
        return;

    const bool clearCache = flags & CacheFlag;

    if (from != group) {
        // Skip to the next full range if the start one is not a member of the target group.
        from.incrementIndexes(from->count - from.offset);
        from.offset = 0;
        *from = from->next;
    } else if (from.offset > 0) {
        // If the start position is mid range split off the portion unaffected.
        *from = insert(*from, from->list, from->index, from.offset, from->flags & ~AppendFlag)->next;
        from->index += from.offset;
        from->count -= from.offset;
        from.offset = 0;
    }

    for (; count > 0; *from = from->next) {
        if (from != group) {
            // Skip ranges that are not members of the target group.
            from.incrementIndexes(from->count);
            continue;
        }
        // Find the number of items affected in the current range.
        const int difference = qMin(count, from->count);
        count -= difference;


        // Determine the actual changes made to the range and decrement counts accordingly.
        const uint removeFlags = from->flags & flags & ~(AppendFlag | PrependFlag);
        const uint clearedFlags = from->flags & ~(flags | AppendFlag | UnresolvedFlag);
        if (removeFlags && removes) {
            const int maskedFlags = clearCache
                    ? (removeFlags & ~CacheFlag)
                    : (removeFlags | (from->flags & CacheFlag));
            if (maskedFlags)
                removes->append(Remove(from, difference, maskedFlags));
        }
        m_end.decrementIndexes(difference, removeFlags);
        from.incrementIndexes(difference, clearedFlags);

        if (from->previous != &m_ranges
                && from->previous->list == from->list
                && (!from->list || clearedFlags == CacheFlag || from->previous->end() == from->index)
                && from->previous->flags == clearedFlags) {
            // If the removed flags make the current range a continuation of the previous
            // then move the affected items over to the previous range.
            from->previous->count += difference;
            from->index += difference;
            from->count -= difference;
            if (from->count == 0) {
                // Delete the current range if it is now empty, preserving the append flag
                if (from->append())
                    from->previous->flags |= AppendFlag;
                *from = erase(*from)->previous;
            } else {
                from.incrementIndexes(from->count);
            }
        } else if (difference < from->count) {
            // Create a new range with the reduced flags, and remove the affected items from
            // the current range.
            if (clearedFlags)
                *from = insert(*from, from->list, from->index, difference, clearedFlags)->next;
            from->index += difference;
            from->count -= difference;
            from.incrementIndexes(from->count);
        } else if (clearedFlags) {
            // The whole range is affected so simply update the flags.
            from->flags &= ~flags;
        } else {
            // All flags have been removed from the range so remove it.
            *from = erase(*from)->previous;
        }
    }

    if (*from != &m_ranges && from->previous != &m_ranges
            && from->previous->list == from->list
            && (!from->list || from->previous->end() == from->index)
            && from->previous->flags == (from->flags & ~AppendFlag)) {
        // If the following range is now a continuation, merge it with its previous range.
        from.offset = from->previous->count;
        from->previous->count += from->count;
        from->previous->flags = from->flags;
        *from = erase(*from)->previous;
    }
    m_cacheIt = from;
    QT_QML_VERIFY_LISTCOMPOSITOR
}

bool QQmlListCompositor::verifyMoveTo(
        Group fromGroup, int from, Group toGroup, int to, int count, Group group) const
{
    if (group != toGroup) {
        // determine how many items from the destination group intersect with the source group.
        iterator fromIt = find(fromGroup, from);

        int intersectingCount = 0;

        for (; count > 0; *fromIt = fromIt->next) {
            if (*fromIt == &m_ranges)
                return false;
            if (!fromIt->inGroup(group))
                continue;
            if (fromIt->inGroup(toGroup))
                intersectingCount += qMin(count, fromIt->count - fromIt.offset);
            count -= fromIt->count - fromIt.offset;
            fromIt.offset = 0;
        }
        count = intersectingCount;
    }

    return to >= 0 && to + count <= m_end.index[toGroup];
}

/*!
    \internal

    Moves \a count items belonging to \a moveGroup from the index \a from in \a fromGroup
    to the index \a to in \a toGroup.

    If \a removes and \a inserts are not null they will be populated with per group notifications
    of the items moved.
 */

void QQmlListCompositor::move(
        Group fromGroup,
        int from,
        Group toGroup,
        int to,
        int count,
        Group moveGroup,
        QVector<Remove> *removes,
        QVector<Insert> *inserts)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< fromGroup << from << toGroup << to << count)
    Q_ASSERT(count > 0);
    Q_ASSERT(from >=0);
    Q_ASSERT(verifyMoveTo(fromGroup, from, toGroup, to, count, moveGroup));

    // Find the position of the first item to move.
    iterator fromIt = find(fromGroup, from);

    if (fromIt != moveGroup) {
        // If the range at the from index doesn't contain items from the move group; skip
        // to the next range.
        fromIt.incrementIndexes(fromIt->count - fromIt.offset);
        fromIt.offset = 0;
        *fromIt = fromIt->next;
    } else if (fromIt.offset > 0) {
        // If the range at the from index contains items from the move group and the index isn't
        // at the start of the range; split the range at the index and move the iterator to start
        // of the second range.
        *fromIt = insert(
                *fromIt, fromIt->list, fromIt->index, fromIt.offset, fromIt->flags & ~AppendFlag)->next;
        fromIt->index += fromIt.offset;
        fromIt->count -= fromIt.offset;
        fromIt.offset = 0;
    }

    // Remove count items belonging to the move group from the list.
    Range movedFlags;
    for (int moveId = m_moveId; count > 0;) {
        if (fromIt != moveGroup) {
            // Skip ranges not containing items from the move group.
            fromIt.incrementIndexes(fromIt->count);
            *fromIt = fromIt->next;
            continue;
        }
        int difference = qMin(count, fromIt->count);

        // Create a new static range containing the moved items from an existing range.
        new Range(
                &movedFlags,
                fromIt->list,
                fromIt->index,
                difference,
                fromIt->flags & ~(PrependFlag | AppendFlag));
        // Remove moved items from the count, the existing range, and a remove notification.
        if (removes)
            removes->append(Remove(fromIt, difference, fromIt->flags, ++moveId));
        count -= difference;
        fromIt->count -= difference;

        // If the existing range contains the prepend flag replace the removed items with
        // a placeholder range for new items inserted into the source model.
        int removeIndex = fromIt->index;
        if (fromIt->prepend()
                && fromIt->previous != &m_ranges
                && fromIt->previous->flags == PrependFlag
                && fromIt->previous->list == fromIt->list
                && fromIt->previous->end() == fromIt->index) {
            // Grow the previous range instead of creating a new one if possible.
            fromIt->previous->count += difference;
        } else if (fromIt->prepend()) {
            *fromIt = insert(*fromIt, fromIt->list, removeIndex, difference, PrependFlag)->next;
        }
        fromIt->index += difference;

        if (fromIt->count == 0) {
            // If the existing range has no items remaining; remove it from the list.
            if (fromIt->append())
                fromIt->previous->flags |= AppendFlag;
            *fromIt = erase(*fromIt);

            // If the ranges before and after the removed range can be joined, do so.
            if (*fromIt != m_ranges.next && fromIt->flags == PrependFlag
                    && fromIt->previous != &m_ranges
                    && fromIt->previous->flags == PrependFlag
                    && fromIt->previous->list == fromIt->list
                    && fromIt->previous->end() == fromIt->index) {
                fromIt.incrementIndexes(fromIt->count);
                fromIt->previous->count += fromIt->count;
                *fromIt = erase(*fromIt);
            }
        } else if (count > 0) {
            *fromIt = fromIt->next;
        }
    }

    // Try and join the range following the removed items to the range preceding it.
    if (*fromIt != m_ranges.next
            && *fromIt != &m_ranges
            && fromIt->previous->list == fromIt->list
            && (!fromIt->list || fromIt->previous->end() == fromIt->index)
            && fromIt->previous->flags == (fromIt->flags & ~AppendFlag)) {
        if (fromIt == fromIt.group)
            fromIt.offset = fromIt->previous->count;
        fromIt.offset = fromIt->previous->count;
        fromIt->previous->count += fromIt->count;
        fromIt->previous->flags = fromIt->flags;
        *fromIt = erase(*fromIt)->previous;
    }

    // Find the destination position of the move.
    insert_iterator toIt = fromIt;
    toIt.setGroup(toGroup);

    const int difference = to - toIt.index[toGroup];
    toIt += difference;

    // If the insert position is part way through a range; split it and move the iterator to the
    // start of the second range.
    if (toIt.offset > 0) {
        *toIt = insert(*toIt, toIt->list, toIt->index, toIt.offset, toIt->flags & ~AppendFlag)->next;
        toIt->index += toIt.offset;
        toIt->count -= toIt.offset;
        toIt.offset = 0;
    }

    // Insert the moved ranges before the insert iterator, growing the previous range if that
    // is an option.
    for (Range *range = movedFlags.previous; range != &movedFlags; range = range->previous) {
        if (*toIt != &m_ranges
                && range->list == toIt->list
                && (!range->list || range->end() == toIt->index)
                && range->flags == (toIt->flags & ~AppendFlag)) {
            toIt->index -= range->count;
            toIt->count += range->count;
        } else {
            *toIt = insert(*toIt, range->list, range->index, range->count, range->flags);
        }
    }

    // Try and join the range after the inserted ranges to the last range inserted.
    if (*toIt != m_ranges.next
            && toIt->previous->list == toIt->list
            && (!toIt->list || (toIt->previous->end() == toIt->index && toIt->previous->flags == (toIt->flags & ~AppendFlag)))) {
        toIt.offset = toIt->previous->count;
        toIt->previous->count += toIt->count;
        toIt->previous->flags = toIt->flags;
        *toIt = erase(*toIt)->previous;
    }
    // Create insert notification for the ranges moved.
    Insert insert(toIt, 0, 0, 0);
    for (Range *next, *range = movedFlags.next; range != &movedFlags; range = next) {
        insert.count = range->count;
        insert.flags = range->flags;
        if (inserts) {
            insert.moveId = ++m_moveId;
            inserts->append(insert);
        }
        for (int i = 0; i < m_groupCount; ++i) {
            if (insert.inGroup(i))
                insert.index[i] += range->count;
        }

        next = range->next;
        delete range;
    }

    m_cacheIt = toIt;

    QT_QML_VERIFY_LISTCOMPOSITOR
}

/*!
    Clears the contents of a compositor.
*/

void QQmlListCompositor::clear()
{
    QT_QML_TRACE_LISTCOMPOSITOR("")
    for (Range *range = m_ranges.next; range != &m_ranges; range = erase(range)) {}
    m_end = iterator(m_ranges.next, 0, Default, m_groupCount);
    m_cacheIt = m_end;
}

void QQmlListCompositor::listItemsInserted(
        QVector<Insert> *translatedInsertions,
        void *list,
        const QVector<QQmlChangeSet::Change> &insertions,
        const QVector<MovedFlags> *movedFlags)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< list << insertions)
    for (iterator it(m_ranges.next, 0, Default, m_groupCount); *it != &m_ranges; *it = it->next) {
        if (it->list != list || it->flags == CacheFlag) {
            // Skip ranges that don't reference list.
            it.incrementIndexes(it->count);
            continue;
        } else if (it->flags & MovedFlag) {
            // Skip ranges that were already moved in listItemsRemoved.
            it->flags &= ~MovedFlag;
            it.incrementIndexes(it->count);
            continue;
        }
        foreach (const QQmlChangeSet::Change &insertion, insertions) {
            int offset = insertion.index - it->index;
            if ((offset > 0 && offset < it->count)
                    || (offset == 0 && it->prepend())
                    || (offset == it->count && it->append())) {
                // The insert index is within the current range.
                if (it->prepend()) {
                    // The range has the prepend flag set so we insert new items into the range.
                    uint flags = m_defaultFlags;
                    if (insertion.isMove()) {
                        // If the insert was part of a move replace the default flags with
                        // the flags from the source range.
                        for (QVector<MovedFlags>::const_iterator move = movedFlags->begin();
                                move != movedFlags->end();
                                ++move) {
                            if (move->moveId == insertion.moveId) {
                                flags = move->flags;
                                break;
                            }
                        }
                    }
                    if (flags & ~(AppendFlag | PrependFlag)) {
                        // If any items are added to groups append an insert notification.
                        Insert translatedInsert(it, insertion.count, flags, insertion.moveId);
                        for (int i = 0; i < m_groupCount; ++i) {
                            if (it->inGroup(i))
                                translatedInsert.index[i] += offset;
                        }
                        translatedInsertions->append(translatedInsert);
                    }
                    if ((it->flags & ~AppendFlag) == flags) {
                        // Accumulate items on the current range it its flags are the same as
                        // the insert flags.
                        it->count += insertion.count;
                    } else if (offset == 0
                            && it->previous != &m_ranges
                            && it->previous->list == list
                            && it->previous->end() == insertion.index
                            && it->previous->flags == flags) {
                        // Attempt to append to the previous range if the insert position is at
                        // the start of the current range.
                        it->previous->count += insertion.count;
                        it->index += insertion.count;
                        it.incrementIndexes(insertion.count);
                    } else {
                        if (offset > 0) {
                            // Divide the current range at the insert position.
                            it.incrementIndexes(offset);
                            *it = insert(*it, it->list, it->index, offset, it->flags & ~AppendFlag)->next;
                        }
                        // Insert a new range, and increment the start index of the current range.
                        *it = insert(*it, it->list, insertion.index, insertion.count, flags)->next;
                        it.incrementIndexes(insertion.count, flags);
                        it->index += offset + insertion.count;
                        it->count -= offset;
                    }
                    m_end.incrementIndexes(insertion.count, flags);
                } else {
                    // The range doesn't have the prepend flag set so split the range into parts;
                    // one before the insert position and one after the last inserted item.
                    if (offset > 0) {
                        *it = insert(*it, it->list, it->index, offset, it->flags)->next;
                        it->index += offset;
                        it->count -= offset;
                    }
                    it->index += insertion.count;
                }
            } else if (offset <= 0) {
                // The insert position was before the current range so increment the start index.
                it->index += insertion.count;
            }
        }
        it.incrementIndexes(it->count);
    }
    m_cacheIt = m_end;
    QT_QML_VERIFY_LISTCOMPOSITOR
}

/*!
    Updates the contents of a compositor when \a count items are inserted into a \a list at
    \a index.

    This corrects the indexes of each range for that list in the compositor, splitting the range
    in two if the insert index is in the middle of that range.  If a range at the insert position
    has the Prepend flag set then a new range will be inserted at that position with the groups
    specified in defaultGroups().  If the insert index corresponds to the end of a range with
    the Append flag set a new range will be inserted before the end of the append range.

    The \a translatedInsertions list is populated with insert notifications for affected
    groups.
*/

void QQmlListCompositor::listItemsInserted(
        void *list, int index, int count, QVector<Insert> *translatedInsertions)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< list << index << count)
    Q_ASSERT(count > 0);

    QVector<QQmlChangeSet::Change> insertions;
    insertions.append(QQmlChangeSet::Change(index, count));

    listItemsInserted(translatedInsertions, list, insertions);
}

void QQmlListCompositor::listItemsRemoved(
        QVector<Remove> *translatedRemovals,
        void *list,
        QVector<QQmlChangeSet::Change> *removals,
        QVector<QQmlChangeSet::Change> *insertions,
        QVector<MovedFlags> *movedFlags)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< list << *removals)

    for (iterator it(m_ranges.next, 0, Default, m_groupCount); *it != &m_ranges; *it = it->next) {
        if (it->list != list || it->flags == CacheFlag) {
            // Skip ranges that don't reference list.
            it.incrementIndexes(it->count);
            continue;
        }
        bool removed = false;
        for (QVector<QQmlChangeSet::Change>::iterator removal = removals->begin();
                !removed && removal != removals->end();
                ++removal) {
            int relativeIndex = removal->index - it->index;
            int itemsRemoved = removal->count;
            if (relativeIndex + removal->count > 0 && relativeIndex < it->count) {
                // If the current range intersects the remove; remove the intersecting items.
                const int offset = qMax(0, relativeIndex);
                int removeCount = qMin(it->count, relativeIndex + removal->count) - offset;
                it->count -= removeCount;
                int removeFlags = it->flags & m_removeFlags;
                Remove translatedRemoval(it, removeCount, it->flags);
                for (int i = 0; i < m_groupCount; ++i) {
                    if (it->inGroup(i))
                        translatedRemoval.index[i] += offset;
                }
                if (removal->isMove()) {
                    // If the removal was part of a move find the corresponding insert.
                    QVector<QQmlChangeSet::Change>::iterator insertion = insertions->begin();
                    for (; insertion != insertions->end() && insertion->moveId != removal->moveId;
                            ++insertion) {}
                    Q_ASSERT(insertion != insertions->end());
                    Q_ASSERT(insertion->count == removal->count);

                    if (relativeIndex < 0) {
                        // If the remove started before the current range, split it and the
                        // corresponding insert so we're only working with intersecting part.
                        int splitMoveId = ++m_moveId;
                        removal = removals->insert(removal, QQmlChangeSet::Change(
                                removal->index, -relativeIndex, splitMoveId));
                        ++removal;
                        removal->count -= -relativeIndex;
                        insertion = insertions->insert(insertion, QQmlChangeSet::Change(
                                insertion->index, -relativeIndex, splitMoveId));
                        ++insertion;
                        insertion->index += -relativeIndex;
                        insertion->count -= -relativeIndex;
                    }

                    if (it->prepend()) {
                        // If the current range has the prepend flag preserve its flags to transfer
                        // to its new location.
                        removeFlags |= it->flags & CacheFlag;
                        translatedRemoval.moveId = ++m_moveId;
                        movedFlags->append(MovedFlags(m_moveId, it->flags & ~AppendFlag));

                        if (removeCount < removal->count) {
                            // If the remove doesn't encompass all of the current range,
                            // split it and the corresponding insert.
                            removal = removals->insert(removal, QQmlChangeSet::Change(
                                    removal->index, removeCount, translatedRemoval.moveId));
                            ++removal;
                            insertion = insertions->insert(insertion, QQmlChangeSet::Change(
                                    insertion->index, removeCount, translatedRemoval.moveId));
                            ++insertion;

                            removal->count -= removeCount;
                            insertion->index += removeCount;
                            insertion->count -= removeCount;
                        } else {
                            // If there's no need to split the move simply replace its moveId
                            // with that of the translated move.
                            removal->moveId = translatedRemoval.moveId;
                            insertion->moveId = translatedRemoval.moveId;
                        }
                    } else {
                        // If the current range doesn't have prepend flags then insert a new range
                        // with list indexes from the corresponding insert location.  The MoveFlag
                        // is to notify listItemsInserted that it can skip evaluation of that range.
                        if (offset > 0) {
                            *it = insert(*it, it->list, it->index, offset, it->flags & ~AppendFlag)->next;
                            it->index += offset;
                            it->count -= offset;
                            it.incrementIndexes(offset);
                        }
                        if (it->previous != &m_ranges
                                && it->previous->list == it->list
                                && it->end() == insertion->index
                                && it->previous->flags == (it->flags | MovedFlag)) {
                            it->previous->count += removeCount;
                        } else {
                            *it = insert(*it, it->list, insertion->index, removeCount, it->flags | MovedFlag)->next;
                        }
                        // Clear the changed flags as the item hasn't been removed.
                        translatedRemoval.flags = 0;
                        removeFlags = 0;
                    }
                } else if (it->inCache()) {
                    // If not moving and the current range has the cache flag, insert a new range
                    // with just the cache flag set to retain caching information for the removed
                    // range.
                    if (offset > 0) {
                        *it = insert(*it, it->list, it->index, offset, it->flags & ~AppendFlag)->next;
                        it->index += offset;
                        it->count -= offset;
                        it.incrementIndexes(offset);
                    }
                    if (it->previous != &m_ranges
                            && it->previous->list == it->list
                            && it->previous->flags == CacheFlag) {
                        it->previous->count += removeCount;
                    } else {
                        *it = insert(*it, it->list, -1, removeCount, CacheFlag)->next;
                    }
                    it.index[Cache] += removeCount;
                }
                if (removeFlags & GroupMask)
                    translatedRemovals->append(translatedRemoval);
                m_end.decrementIndexes(removeCount, removeFlags);
                if (it->count == 0 && !it->append()) {
                    // Erase empty non-append ranges.
                    *it = erase(*it)->previous;
                    removed = true;
                } else if (relativeIndex <= 0) {
                    // If the remove started before the current range move the start index of
                    // the range to the remove index.
                    it->index = removal->index;
                }
            } else if (relativeIndex < 0) {
                // If the remove was before the current range decrement the start index by the
                // number of items removed.
                it->index -= itemsRemoved;

                if (it->previous != &m_ranges
                        && it->previous->list == it->list
                        && it->previous->end() == it->index
                        && it->previous->flags == (it->flags & ~AppendFlag)) {
                    // Compress ranges made continuous by the removal of separating ranges.
                    it.decrementIndexes(it->previous->count);
                    it->previous->count += it->count;
                    it->previous->flags = it->flags;
                    *it = erase(*it)->previous;
                }
            }
        }
        if (it->flags == CacheFlag && it->next->flags == CacheFlag && it->next->list == it->list) {
            // Compress consecutive cache only ranges.
            it.index[Cache] += it->next->count;
            it->count += it->next->count;
            erase(it->next);
        } else if (!removed) {
            it.incrementIndexes(it->count);
        }
    }
    m_cacheIt = m_end;
    QT_QML_VERIFY_LISTCOMPOSITOR
}


/*!
    Updates the contents of a compositor when \a count items are removed from a \a list at
    \a index.

    Ranges that intersect the specified range are removed from the compositor and the indexes of
    ranges after index + count are updated.

    The \a translatedRemovals list is populated with remove notifications for the affected
    groups.
*/


void QQmlListCompositor::listItemsRemoved(
        void *list, int index, int count, QVector<Remove> *translatedRemovals)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< list << index << count)
    Q_ASSERT(count >= 0);

    QVector<QQmlChangeSet::Change> removals;
    removals.append(QQmlChangeSet::Change(index, count));
    listItemsRemoved(translatedRemovals, list, &removals, 0, 0);
}

/*!
    Updates the contents of a compositor when \a count items are removed from a \a list at
    \a index.

    Ranges that intersect the specified range are removed from the compositor and the indexes of
    ranges after index + count are updated.

    The \a translatedRemovals and translatedInserts lists are populated with move
    notifications for the affected groups.
*/

void QQmlListCompositor::listItemsMoved(
        void *list,
        int from,
        int to,
        int count,
        QVector<Remove> *translatedRemovals,
        QVector<Insert> *translatedInsertions)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< list << from << to << count)
    Q_ASSERT(count >= 0);

    QVector<QQmlChangeSet::Change> removals;
    QVector<QQmlChangeSet::Change> insertions;
    QVector<MovedFlags> movedFlags;
    removals.append(QQmlChangeSet::Change(from, count, 0));
    insertions.append(QQmlChangeSet::Change(to, count, 0));

    listItemsRemoved(translatedRemovals, list, &removals, &insertions, &movedFlags);
    listItemsInserted(translatedInsertions, list, insertions, &movedFlags);
}

void QQmlListCompositor::listItemsChanged(
        QVector<Change> *translatedChanges,
        void *list,
        const QVector<QQmlChangeSet::Change> &changes)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< list << changes)
    for (iterator it(m_ranges.next, 0, Default, m_groupCount); *it != &m_ranges; *it = it->next) {
        if (it->list != list || it->flags == CacheFlag) {
            it.incrementIndexes(it->count);
            continue;
        } else if (!it->inGroup()) {
            continue;
        }
        foreach (const QQmlChangeSet::Change &change, changes) {
            const int offset = change.index - it->index;
            if (offset + change.count > 0 && offset < it->count) {
                const int changeOffset = qMax(0, offset);
                const int changeCount = qMin(it->count, offset + change.count) - changeOffset;

                Change translatedChange(it, changeCount, it->flags);
                for (int i = 0; i < m_groupCount; ++i) {
                    if (it->inGroup(i))
                        translatedChange.index[i] += changeOffset;
                }
                translatedChanges->append(translatedChange);
            }
        }
        it.incrementIndexes(it->count);
    }
}


/*!
    Translates the positions of \a count changed items at \a index in a \a list.

    The \a translatedChanges list is populated with change notifications for the
    affected groups.
*/

void QQmlListCompositor::listItemsChanged(
        void *list, int index, int count, QVector<Change> *translatedChanges)
{
    QT_QML_TRACE_LISTCOMPOSITOR(<< list << index << count)
    Q_ASSERT(count >= 0);
    QVector<QQmlChangeSet::Change> changes;
    changes.append(QQmlChangeSet::Change(index, count));
    listItemsChanged(translatedChanges, list, changes);
}

void QQmlListCompositor::transition(
        Group from,
        Group to,
        QVector<QQmlChangeSet::Change> *removes,
        QVector<QQmlChangeSet::Change> *inserts)
{
    int removeCount = 0;
    for (iterator it(m_ranges.next, 0, Default, m_groupCount); *it != &m_ranges; *it = it->next) {
        if (it == from && it != to) {
            removes->append(QQmlChangeSet::Change(it.index[from]- removeCount, it->count));
            removeCount += it->count;
        } else if (it != from && it == to) {
            inserts->append(QQmlChangeSet::Change(it.index[to], it->count));
        }
        it.incrementIndexes(it->count);
    }
}

/*!
    \internal
    Writes the name of \a group to \a debug.
*/

QDebug operator <<(QDebug debug, const QQmlListCompositor::Group &group)
{
    switch (group) {
    case QQmlListCompositor::Cache:   return debug << "Cache";
    case QQmlListCompositor::Default: return debug << "Default";
    default: return (debug.nospace() << "Group" << int(group)).space();
    }

}

/*!
    \internal
    Writes the contents of \a range to \a debug.
*/

QDebug operator <<(QDebug debug, const QQmlListCompositor::Range &range)
{
    (debug.nospace()
            << "Range("
            << range.list) << ' '
            << range.index << ' '
            << range.count << ' '
            << (range.isUnresolved() ? 'U' : '0')
            << (range.append() ? 'A' : '0')
            << (range.prepend() ? 'P' : '0');
    for (int i = QQmlListCompositor::MaximumGroupCount - 1; i >= 2; --i)
        debug << (range.inGroup(i) ? '1' : '0');
    return (debug
            << (range.inGroup(QQmlListCompositor::Default) ? 'D' : '0')
            << (range.inGroup(QQmlListCompositor::Cache) ? 'C' : '0'));
}

static void qt_print_indexes(QDebug &debug, int count, const int *indexes)
{
    for (int i = count - 1; i >= 0; --i)
        debug << indexes[i];
}

/*!
    \internal
    Writes the contents of \a it to \a debug.
*/

QDebug operator <<(QDebug debug, const QQmlListCompositor::iterator &it)
{
    (debug.nospace() << "iterator(" << it.group).space() << "offset:" << it.offset;
    qt_print_indexes(debug, it.groupCount, it.index);
    return ((debug << **it).nospace() << ')').space();
}

static QDebug qt_print_change(QDebug debug, const char *name, const QQmlListCompositor::Change &change)
{
    debug.nospace() << name << '(' << change.moveId << ' ' << change.count << ' ';
    for (int i = QQmlListCompositor::MaximumGroupCount - 1; i >= 2; --i)
        debug << (change.inGroup(i) ? '1' : '0');
    debug << (change.inGroup(QQmlListCompositor::Default) ? 'D' : '0')
            << (change.inGroup(QQmlListCompositor::Cache) ? 'C' : '0');
    int i = QQmlListCompositor::MaximumGroupCount - 1;
    for (; i >= 0 && !change.inGroup(i); --i) {}
    for (; i >= 0; --i)
        debug << ' ' << change.index[i];
    return (debug << ')').maybeSpace();
}

/*!
    \internal
    Writes the contents of \a change to \a debug.
*/

QDebug operator <<(QDebug debug, const QQmlListCompositor::Change &change)
{
    return qt_print_change(debug, "Change", change);
}

/*!
    \internal
    Writes the contents of \a remove to \a debug.
*/

QDebug operator <<(QDebug debug, const QQmlListCompositor::Remove &remove)
{
    return qt_print_change(debug, "Remove", remove);
}

/*!
    \internal
    Writes the contents of \a insert to \a debug.
*/

QDebug operator <<(QDebug debug, const QQmlListCompositor::Insert &insert)
{
    return qt_print_change(debug, "Insert", insert);
}

/*!
    \internal
    Writes the contents of \a list to \a debug.
*/

QDebug operator <<(QDebug debug, const QQmlListCompositor &list)
{
    int indexes[QQmlListCompositor::MaximumGroupCount];
    for (int i = 0; i < QQmlListCompositor::MaximumGroupCount; ++i)
        indexes[i] = 0;
    debug.nospace() << "QQmlListCompositor(";
    qt_print_indexes(debug, list.m_groupCount, list.m_end.index);
    for (QQmlListCompositor::Range *range = list.m_ranges.next; range != &list.m_ranges; range = range->next) {
        (debug << '\n').space();
        qt_print_indexes(debug, list.m_groupCount, indexes);
        debug << ' ' << *range;

        for (int i = 0; i < list.m_groupCount; ++i) {
            if (range->inGroup(i))
                indexes[i] += range->count;
        }
    }
    return (debug << ')').maybeSpace();
}

QT_END_NAMESPACE
