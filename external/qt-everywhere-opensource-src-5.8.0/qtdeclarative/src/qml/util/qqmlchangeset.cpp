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

#include "qqmlchangeset_p.h"

QT_BEGIN_NAMESPACE


/*!
    \class QQmlChangeSet
    \brief The QQmlChangeSet class stores an ordered list of notifications about
    changes to a linear data set.
    \internal

    QQmlChangeSet can be used to record a series of notifications about items in an indexed list
    being inserted, removed, moved, and changed.  Notifications in the set are re-ordered so that
    all notifications of a single type are grouped together and sorted in order of ascending index,
    with remove notifications preceding all others, followed by insert notification, and then
    change notifications.

    Moves in a change set are represented by a remove notification paired with an insert
    notification by way of a shared unique moveId.  Re-ordering may result in one or both of the
    paired notifications being divided, when this happens the offset member of the notification
    will indicate the relative offset of the divided notification from the beginning of the
    original.
*/

/*!
    Constructs an empty change set.
*/

QQmlChangeSet::QQmlChangeSet()
    : m_difference(0)
{
}

/*!
    Constructs a copy of a \a changeSet.
*/

QQmlChangeSet::QQmlChangeSet(const QQmlChangeSet &changeSet)
    : m_removes(changeSet.m_removes)
    , m_inserts(changeSet.m_inserts)
    , m_changes(changeSet.m_changes)
    , m_difference(changeSet.m_difference)
{
}

/*!
    Destroys a change set.
*/

QQmlChangeSet::~QQmlChangeSet()
{
}

/*!
    Assigns the value of a \a changeSet to another.
*/

QQmlChangeSet &QQmlChangeSet::operator =(const QQmlChangeSet &changeSet)
{
    m_removes = changeSet.m_removes;
    m_inserts = changeSet.m_inserts;
    m_changes = changeSet.m_changes;
    m_difference = changeSet.m_difference;
    return *this;
}

/*!
    Appends a notification that \a count items were inserted at \a index.
*/

void QQmlChangeSet::insert(int index, int count)
{
    insert(QVector<Change>() << Change(index, count));
}

/*!
    Appends a notification that \a count items were removed at \a index.
*/

void QQmlChangeSet::remove(int index, int count)
{
    QVector<Change> removes;
    removes.append(Change(index, count));
    remove(&removes, 0);
}

/*!
    Appends a notification that \a count items were moved \a from one index \a to another.

    The \a moveId must be unique across the lifetime of the change set and any related
    change sets.
*/

void QQmlChangeSet::move(int from, int to, int count, int moveId)
{
    QVector<Change> removes;
    removes.append(Change(from, count, moveId));
    QVector<Change> inserts;
    inserts.append(Change(to, count, moveId));
    remove(&removes, &inserts);
    insert(inserts);
}

/*!
    Appends a notification that \a count items were changed at \a index.
*/

void QQmlChangeSet::change(int index, int count)
{
    QVector<Change> changes;
    changes.append(Change(index, count));
    change(changes);
}

/*!
    Applies the changes in a \a changeSet to another.
*/

void QQmlChangeSet::apply(const QQmlChangeSet &changeSet)
{
    QVector<Change> r = changeSet.m_removes;
    QVector<Change> i = changeSet.m_inserts;
    QVector<Change> c = changeSet.m_changes;
    remove(&r, &i);
    insert(i);
    change(c);
}

/*!
    Applies a list of \a removes to a change set.

    If a remove contains a moveId then any intersecting insert in the set will replace the
    corresponding intersection in the optional \a inserts list.
*/

void QQmlChangeSet::remove(const QVector<Change> &removes, QVector<Change> *inserts)
{
    QVector<Change> r = removes;
    remove(&r, inserts);
}

void QQmlChangeSet::remove(QVector<Change> *removes, QVector<Change> *inserts)
{
    int removeCount = 0;
    int insertCount = 0;
    QVector<Change>::iterator insert = m_inserts.begin();
    QVector<Change>::iterator change = m_changes.begin();
    QVector<Change>::iterator rit = removes->begin();
    for (; rit != removes->end(); ++rit) {
        int index = rit->index + removeCount;
        int count = rit->count;

        // Decrement the accumulated remove count from the indexes of any changes prior to the
        // current remove.
        for (; change != m_changes.end() && change->end() < rit->index; ++change)
            change->index -= removeCount;
        // Remove any portion of a change notification that intersects the current remove.
        for (; change != m_changes.end() && change->index > rit->end(); ++change) {
            change->count -= qMin(change->end(), rit->end()) - qMax(change->index, rit->index);
            if (change->count == 0) {
                change = m_changes.erase(change);
            } else if (rit->index < change->index) {
                change->index = rit->index;
            }
        }

        // Decrement the accumulated remove count from the indexes of any inserts prior to the
        // current remove.
        for (; insert != m_inserts.end() && insert->end() <= index; ++insert) {
            insertCount += insert->count;
            insert->index -= removeCount;
        }

        rit->index -= insertCount;

        // Remove any portion of a insert notification that intersects the current remove.
        while (insert != m_inserts.end() && insert->index < index + count) {
            int offset =  index - insert->index;
            const int difference = qMin(insert->end(), index + count) - qMax(insert->index, index);

            // If part of the remove or insert that precedes the intersection has a moveId create
            // a new delta for that portion and subtract the size of that delta from the current
            // one.
            if (offset < 0 && rit->moveId != -1) {
                rit = removes->insert(rit, Change(
                        rit->index, -offset, rit->moveId, rit->offset));
                ++rit;
                rit->count -= -offset;
                rit->offset += -offset;
                index += -offset;
                count -= -offset;
                removeCount += -offset;
                offset = 0;
            } else if (offset > 0 && insert->moveId != -1) {
                insert = m_inserts.insert(insert, Change(
                        insert->index - removeCount, offset, insert->moveId, insert->offset));
                ++insert;
                insert->index += offset;
                insert->count -= offset;
                insert->offset += offset;
                rit->index -= offset;
                insertCount += offset;
            }

            // If the current remove has a move id, find any inserts with the same move id and
            // replace the corresponding sections with the insert removed from the change set.
            if (rit->moveId != -1 && difference > 0 && inserts) {
                for (QVector<Change>::iterator iit = inserts->begin(); iit != inserts->end(); ++iit) {
                    if (iit->moveId != rit->moveId
                            || rit->offset > iit->offset + iit->count
                            || iit->offset > rit->offset + difference) {
                        continue;
                    }
                    // If the intersecting insert starts before the replacement one create
                    // a new insert for the portion prior to the replacement insert.
                    const int overlapOffset = rit->offset - iit->offset;
                    if (overlapOffset > 0) {
                        iit = inserts->insert(iit, Change(
                                iit->index, overlapOffset, iit->moveId, iit->offset));
                        ++iit;
                        iit->index += overlapOffset;
                        iit->count -= overlapOffset;
                        iit->offset += overlapOffset;
                    }
                    if (iit->offset >= rit->offset
                            && iit->offset + iit->count <= rit->offset + difference) {
                        // If the replacement insert completely encapsulates the existing
                        // one just change the moveId.
                        iit->moveId = insert->moveId;
                        iit->offset = insert->offset + qMax(0, -overlapOffset);
                    } else {
                        // Create a new insertion before the intersecting one with the number of intersecting
                        // items and remove that number from that insert.
                        const int count
                                = qMin(iit->offset + iit->count, rit->offset + difference)
                                - qMax(iit->offset, rit->offset);
                        iit = inserts->insert(iit, Change(
                                iit->index,
                                count,
                                insert->moveId,
                                insert->offset + qMax(0, -overlapOffset)));
                        ++iit;
                        iit->index += count;
                        iit->count -= count;
                        iit->offset += count;
                    }
                }
            }

            // Subtract the number of intersecting items from the current remove and insert.
            insert->count -= difference;
            insert->offset += difference;
            rit->count -= difference;
            rit->offset += difference;

            index += difference;
            count -= difference;
            removeCount += difference;

            if (insert->count == 0) {
                insert = m_inserts.erase(insert);
            } else if (rit->count == -offset || rit->count == 0) {
                insert->index += difference;
                break;
            } else {
                insert->index -= removeCount - difference;
                rit->index -= insert->count;
                insertCount += insert->count;
                ++insert;
            }
        }
        removeCount += rit->count;
    }
    for (; insert != m_inserts.end(); ++insert)
        insert->index -= removeCount;

    removeCount = 0;
    QVector<Change>::iterator remove = m_removes.begin();
    for (rit = removes->begin(); rit != removes->end(); ++rit) {
        if (rit->count == 0)
            continue;
        // Accumulate consecutive removes into a single delta before attempting to apply.
        for (QVector<Change>::iterator next = rit + 1; next != removes->end()
                && next->index == rit->index
                && next->moveId == -1
                && rit->moveId == -1; ++next) {
            next->count += rit->count;
            rit = next;
        }
        int index = rit->index + removeCount;
        // Decrement the accumulated remove count from the indexes of any inserts prior to the
        // current remove.
        for (; remove != m_removes.end() && index > remove->index; ++remove)
            remove->index -= removeCount;
        while (remove != m_removes.end() && index + rit->count >= remove->index) {
            int count = 0;
            const int offset = remove->index - index;
            QVector<Change>::iterator rend = remove;
            for (; rend != m_removes.end()
                    && rit->moveId == -1
                    && rend->moveId == -1
                    && index + rit->count >= rend->index; ++rend) {
                count += rend->count;
            }
            if (remove != rend) {
                // Accumulate all existing non-move removes that are encapsulated by or immediately
                // follow the current remove into it.
                int difference = 0;
                if (rend == m_removes.end()) {
                    difference = rit->count;
                } else if (rit->index + rit->count < rend->index - removeCount) {
                    difference = rit->count;
                } else if (rend->moveId != -1) {
                    difference = rend->index - removeCount - rit->index;
                    index += difference;
                }
                count += difference;

                rit->count -= difference;
                removeCount += difference;
                remove->index = rit->index;
                remove->count = count;
                remove = m_removes.erase(++remove, rend);
            } else {
                // Insert a remove for the portion of the unmergable current remove prior to the
                // point of intersection.
                if (offset > 0) {
                    remove = m_removes.insert(remove, Change(
                            rit->index, offset, rit->moveId, rit->offset));
                    ++remove;
                    rit->count -= offset;
                    rit->offset += offset;
                    removeCount += offset;
                    index += offset;
                }
                remove->index = rit->index;

                ++remove;
            }
        }

        if (rit->count > 0) {
            remove = m_removes.insert(remove, *rit);
            ++remove;
        }
        removeCount += rit->count;
    }
    for (; remove != m_removes.end(); ++remove)
        remove->index -= removeCount;
    m_difference -= removeCount;
}

/*!
    Applies a list of \a inserts to a change set.
*/

void QQmlChangeSet::insert(const QVector<Change> &inserts)
{
    int insertCount = 0;
    QVector<Change>::iterator insert = m_inserts.begin();
    QVector<Change>::iterator change = m_changes.begin();
    for (QVector<Change>::const_iterator iit = inserts.begin(); iit != inserts.end(); ++iit) {
        if (iit->count == 0)
            continue;
        int index = iit->index - insertCount;

        Change current = *iit;
        // Accumulate consecutive inserts into a single delta before attempting to insert.
        for (QVector<Change>::const_iterator next = iit + 1; next != inserts.end()
                && next->index == iit->index + iit->count
                && next->moveId == -1
                && iit->moveId == -1; ++next) {
            current.count += next->count;
            iit = next;
        }

        // Increment the index of any changes before the current insert by the accumlated insert
        // count.
        for (; change != m_changes.end() && change->index >= index; ++change)
            change->index += insertCount;
        // If the current insert index is in the middle of a change split it in two at that
        // point and increment the index of the latter half.
        if (change != m_changes.end() && change->index < index + iit->count) {
                int offset = index - change->index;
                change = m_changes.insert(change, Change(change->index + insertCount, offset));
                ++change;
                change->index += iit->count + offset;
                change->count -= offset;
        }

        // Increment the index of any inserts before the current insert by the accumlated insert
        // count.
        for (; insert != m_inserts.end() && index > insert->index + insert->count; ++insert)
            insert->index += insertCount;
        if (insert == m_inserts.end()) {
            insert = m_inserts.insert(insert, current);
            ++insert;
        } else {
            const int offset = index - insert->index;

            if (offset < 0) {
                // If the current insert is before an existing insert and not adjacent just insert
                // it into the list.
                insert = m_inserts.insert(insert, current);
                ++insert;
            } else if (iit->moveId == -1 && insert->moveId == -1) {
                // If neither the current nor existing insert has a moveId add the current insert
                // to the existing one.
                if (offset < insert->count) {
                    insert->index -= current.count;
                    insert->count += current.count;
                } else {
                    insert->index += insertCount;
                    insert->count += current.count;
                    ++insert;
                }
            } else if (offset < insert->count) {
                // If either insert has a moveId then split the existing insert and insert the
                // current one in the middle.
                if (offset > 0) {
                    insert = m_inserts.insert(insert, Change(
                            insert->index + insertCount, offset, insert->moveId, insert->offset));
                    ++insert;
                    insert->index += offset;
                    insert->count -= offset;
                    insert->offset += offset;
                }
                insert = m_inserts.insert(insert, current);
                ++insert;
            } else {
                insert->index += insertCount;
                ++insert;
                insert = m_inserts.insert(insert, current);
                ++insert;
            }
        }
        insertCount += current.count;
    }
    for (; insert != m_inserts.end(); ++insert)
        insert->index += insertCount;
    m_difference += insertCount;
}

/*!
    Applies a combined list of \a removes and \a inserts to a change set.  This is equivalent
    calling \l remove() followed by \l insert() with the same lists.
*/

void QQmlChangeSet::move(const QVector<Change> &removes, const QVector<Change> &inserts)
{
    QVector<Change> r = removes;
    QVector<Change> i = inserts;
    remove(&r, &i);
    insert(i);
}

/*!
    Applies a list of \a changes to a change set.
*/

void QQmlChangeSet::change(const QVector<Change> &changes)
{
    QVector<Change> c = changes;
    change(&c);
}

void QQmlChangeSet::change(QVector<Change> *changes)
{
    QVector<Change>::iterator insert = m_inserts.begin();
    QVector<Change>::iterator change = m_changes.begin();
    for (QVector<Change>::iterator cit = changes->begin(); cit != changes->end(); ++cit) {
        for (; insert != m_inserts.end() && insert->end() < cit->index; ++insert) {}
        for (; insert != m_inserts.end() && insert->index < cit->end(); ++insert) {
            const int offset = insert->index - cit->index;
            const int count = cit->count + cit->index - insert->index - insert->count;
            if (offset == 0) {
                cit->index = insert->index + insert->count;
                cit->count = count;
            } else {
                cit = changes->insert(++cit, Change(insert->index + insert->count, count));
                --cit;
                cit->count = offset;
            }
        }

        for (; change != m_changes.end() && change->index + change->count < cit->index; ++change) {}
        if (change == m_changes.end() || change->index > cit->index + cit->count) {
            if (cit->count > 0) {
                change = m_changes.insert(change, *cit);
                ++change;
            }
        } else {
            if (cit->index < change->index) {
                change->count += change->index - cit->index;
                change->index = cit->index;
            }

            if (cit->index + cit->count > change->index + change->count) {
                change->count = cit->index + cit->count - change->index;
                QVector<Change>::iterator cbegin = change;
                QVector<Change>::iterator cend = ++cbegin;
                for (; cend != m_changes.end() && cend->index <= change->index + change->count; ++cend) {
                    if (cend->index + cend->count > change->index + change->count)
                        change->count = cend->index + cend->count - change->index;
                }
                if (cbegin != cend) {
                    change = m_changes.erase(cbegin, cend);
                    --change;
                }
            }
        }
    }
}

/*!
    Prints the contents of a change \a set to the \a debug stream.
*/

QDebug operator <<(QDebug debug, const QQmlChangeSet &set)
{
    debug.nospace() << "QQmlChangeSet(";
    foreach (const QQmlChangeSet::Change &remove, set.removes()) debug << remove;
    foreach (const QQmlChangeSet::Change &insert, set.inserts()) debug << insert;
    foreach (const QQmlChangeSet::Change &change, set.changes()) debug << change;
    return debug.nospace() << ')';
}

/*!
    Prints a \a change to the \a debug stream.
*/

QDebug operator <<(QDebug debug, const QQmlChangeSet::Change &change)
{
    return (debug.nospace() << "Change(" << change.index << ',' << change.count << ')').space();
}

QT_END_NAMESPACE

