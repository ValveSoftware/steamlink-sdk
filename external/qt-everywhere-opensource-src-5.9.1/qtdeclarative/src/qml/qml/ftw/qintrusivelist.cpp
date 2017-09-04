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

#include "qintrusivelist_p.h"

/*!
\class QIntrusiveList
\brief The QIntrusiveList class is a template class that provides a list of objects using static storage.
\internal

QIntrusiveList creates a linked list of objects.  Adding and removing objects from the
QIntrusiveList is a constant time operation and is very quick.  The list performs no memory
allocations, but does require the objects being added to the list to contain a QIntrusiveListNode
instance for the list's use.  Even so, for small lists QIntrusiveList uses less memory than Qt's
other list classes.

As QIntrusiveList uses storage inside the objects in the list, each object can only be in one
list at a time.  Objects are inserted by the insert() method.  If the object is already
in a list (including the one it is being inserted into) it is first removed, and then inserted
at the head of the list.  QIntrusiveList is a last-in-first-out list.  That is, following an
insert() the inserted object becomes the list's first() object.

\code
struct MyObject {
    MyObject(int value) : value(value) {}

    int value;
    QIntrusiveListNode node;
};
typedef QIntrusiveList<MyObject, &MyObject::node> MyObjectList;

void foo() {
    MyObjectList list;

    MyObject m0(0);
    MyObject m1(1);
    MyObject m2(2);

    list.insert(&m0);
    list.insert(&m1);
    list.insert(&m2);

    // QIntrusiveList is LIFO, so will print: 2... 1... 0...
    for (MyObjectList::iterator iter = list.begin(); iter != list.end(); ++iter) {
        qWarning() << iter->value;
    }
}
\endcode
*/


/*!
\fn QIntrusiveList::QIntrusiveList();

Construct an empty list.
*/

/*!
\fn QIntrusiveList::~QIntrusiveList();

Destroy the list.  All entries are removed.
*/

/*!
\fn void QIntrusiveList::insert(N *object);

Insert \a object into the list.  If \a object is a member of this, or another list, it will be
removed and inserted at the head of this list.
*/

/*!
\fn void QIntrusiveList::remove(N *object);

Remove \a object from the list.  \a object must not be null.
*/

/*!
\fn bool QIntrusiveList::contains(N *object) const

Returns true if the list contains \a object; otherwise returns false.
*/

/*!
\fn N *QIntrusiveList::first() const

Returns the first entry in this list, or null if the list is empty.
*/

/*!
\fn N *QIntrusiveList::next(N *current)

Returns the next object after \a current, or null if \a current is the last object.  \a current cannot be null.
*/

/*!
\fn iterator QIntrusiveList::begin()

Returns an STL-style interator pointing to the first item in the list.

\sa end()
*/

/*!
\fn iterator QIntrusiveList::end()

Returns an STL-style iterator pointing to the imaginary item after the last item in the list.

\sa begin()
*/

/*!
\fn iterator &QIntrusiveList::iterator::erase()

Remove the current object from the list, and return an iterator to the next element.
*/


/*!
\fn QIntrusiveListNode::QIntrusiveListNode()

Create a QIntrusiveListNode.
*/

/*!
\fn QIntrusiveListNode::~QIntrusiveListNode()

Destroy the QIntrusiveListNode.  If the node is in a list, it is removed.
*/

/*!
\fn void QIntrusiveListNode::remove()

If in a list, remove this node otherwise do nothing.
*/

/*!
\fn bool QIntrusiveListNode::isInList() const

Returns true if this node is in a list, false otherwise.
*/

