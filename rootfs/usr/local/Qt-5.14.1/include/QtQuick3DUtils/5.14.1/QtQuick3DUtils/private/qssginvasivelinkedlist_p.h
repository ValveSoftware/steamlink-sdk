/****************************************************************************
**
** Copyright (C) 2008-2012 NVIDIA Corporation.
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Quick 3D.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QSSGINVASIVELINKEDLIST_H
#define QSSGINVASIVELINKEDLIST_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtQuick3DUtils/private/qtquick3dutilsglobal_p.h>

QT_BEGIN_NAMESPACE

// Base linked list without an included head or tail member.
template<typename TObjType, typename TObjHeadOp, typename TObjTailOp>
struct QSSGInvasiveLinkListBase
{
    TObjType *tail(TObjType *inObj)
    {
        if (inObj)
            return TObjTailOp().get(inObj);
        return nullptr;
    }

    TObjType *head(TObjType *inObj)
    {
        if (inObj)
            return TObjHeadOp().get(inObj);
        return nullptr;
    }

    const TObjType *tail(const TObjType *inObj)
    {
        if (inObj)
            return TObjTailOp().get(inObj);
        return nullptr;
    }

    const TObjType *head(const TObjType *inObj)
    {
        if (inObj)
            return TObjHeadOp().get(inObj);
        return nullptr;
    }

    void remove(TObjType &inObj)
    {
        TObjHeadOp theHeadOp;
        TObjTailOp theTailOp;
        TObjType *theHead = theHeadOp.get(inObj);
        TObjType *theTail = theTailOp.get(inObj);
        if (theHead)
            theTailOp.set(*theHead, theTail);
        if (theTail)
            theHeadOp.set(*theTail, theHead);
        theHeadOp.set(inObj, nullptr);
        theTailOp.set(inObj, nullptr);
    }

    void insert_after(TObjType &inPosition, TObjType &inObj)
    {
        TObjTailOp theTailOp;
        TObjType *theHead = &inPosition;
        TObjType *theTail = theTailOp.get(inPosition);
        insert(theHead, theTail, inObj);
    }

    void insert_before(TObjType &inPosition, TObjType &inObj)
    {
        TObjHeadOp theHeadOp;
        TObjType *theHead = theHeadOp.get(inPosition);
        TObjType *theTail = &inPosition;
        insert(theHead, theTail, inObj);
    }

    void insert(TObjType *inHead, TObjType *inTail, TObjType &inObj)
    {
        TObjHeadOp theHeadOp;
        TObjTailOp theTailOp;
        if (inHead)
            theTailOp.set(*inHead, &inObj);
        if (inTail)
            theHeadOp.set(*inTail, &inObj);
        theHeadOp.set(inObj, inHead);
        theTailOp.set(inObj, inTail);
    }
};

template<typename TObjType, typename TObjTailOp>
struct QSSGLinkedListIterator
{
    typedef QSSGLinkedListIterator<TObjType, TObjTailOp> TMyType;
    TObjType *m_obj;
    QSSGLinkedListIterator(TObjType *inObj = nullptr) : m_obj(inObj) {}

    bool operator!=(const TMyType &inIter) const { return m_obj != inIter.m_obj; }
    bool operator==(const TMyType &inIter) const { return m_obj == inIter.m_obj; }

    TMyType &operator++()
    {
        if (m_obj)
            m_obj = TObjTailOp().get(*m_obj);
        return *this;
    }

    TMyType &operator++(int)
    {
        TMyType retval(*this);
        ++(*this);
        return retval;
    }

    TObjType &operator*() { return *m_obj; }
    TObjType *operator->() { return m_obj; }
};

// Used for singly linked list where
// items have either no head or tail ptr.
template<typename TObjType>
struct QSSGNullOp
{
    void set(TObjType &, TObjType *) {}
    TObjType *get(const TObjType &) { return nullptr; }
};

template<typename TObjType, typename TObjTailOp>
struct QSSGInvasiveSingleLinkedList : public QSSGInvasiveLinkListBase<TObjType, QSSGNullOp<TObjType>, TObjTailOp>
{
    typedef QSSGInvasiveSingleLinkedList<TObjType, TObjTailOp> TMyType;
    typedef QSSGInvasiveLinkListBase<TObjType, QSSGNullOp<TObjType>, TObjTailOp> TBaseType;
    typedef QSSGLinkedListIterator<TObjType, TObjTailOp> iterator;
    typedef iterator const_iterator;
    TObjType *m_head = nullptr;
    QSSGInvasiveSingleLinkedList() = default;
    QSSGInvasiveSingleLinkedList(const TMyType &inOther) : m_head(inOther.m_head) {}
    TMyType &operator=(const TMyType &inOther)
    {
        m_head = inOther.m_head;
        return *this;
    }

    TObjType &front() const { return *m_head; }

    void push_front(TObjType &inObj)
    {
        if (m_head != nullptr)
            TBaseType::insert_before(*m_head, inObj);
        m_head = &inObj;
    }

    void push_back(TObjType &inObj)
    {
        if (m_head == nullptr)
            m_head = &inObj;
        else {
            TObjType *lastObj = nullptr;
            for (iterator iter = begin(), endIter = end(); iter != endIter; ++iter)
                lastObj = &(*iter);

            Q_ASSERT(lastObj);
            if (lastObj)
                TObjTailOp().set(*lastObj, &inObj);
        }
    }

    void remove(TObjType &inObj)
    {
        if (m_head == &inObj)
            m_head = TObjTailOp().get(inObj);
        TBaseType::remove(inObj);
    }

    bool empty() const { return m_head == nullptr; }

    iterator begin() { return iterator(m_head); }
    iterator end() { return iterator(nullptr); }

    const_iterator begin() const { return iterator(m_head); }
    const_iterator end() const { return iterator(nullptr); }
};

template<typename TObjType, typename TObjHeadOp, typename TObjTailOp>
struct QSSGInvasiveLinkedList : public QSSGInvasiveLinkListBase<TObjType, TObjHeadOp, TObjTailOp>
{
    typedef QSSGInvasiveLinkedList<TObjType, TObjHeadOp, TObjTailOp> TMyType;
    typedef QSSGInvasiveLinkListBase<TObjType, TObjHeadOp, TObjTailOp> TBaseType;
    typedef QSSGLinkedListIterator<TObjType, TObjTailOp> iterator;
    typedef iterator const_iterator;
    typedef QSSGLinkedListIterator<TObjType, TObjHeadOp> reverse_iterator;
    typedef reverse_iterator const_reverse_iterator;

    TObjType *m_head = nullptr;
    TObjType *m_tail = nullptr;

    QSSGInvasiveLinkedList() = default;
    QSSGInvasiveLinkedList(const TMyType &inOther) : m_head(inOther.m_head), m_tail(inOther.m_tail) {}
    TMyType &operator=(const TMyType &inOther)
    {
        m_head = inOther.m_head;
        m_tail = inOther.m_tail;
        return *this;
    }

    TObjType &front() const
    {
        Q_ASSERT(m_head);
        return *m_head;
    }
    TObjType &back() const
    {
        Q_ASSERT(m_tail);
        return *m_tail;
    }

    TObjType *front_ptr() const { return m_head; }
    TObjType *back_ptr() const { return m_tail; }

    void push_front(TObjType &inObj)
    {
        if (m_head != nullptr)
            TBaseType::insert_before(*m_head, inObj);
        m_head = &inObj;

        if (m_tail == nullptr)
            m_tail = &inObj;
    }

    void push_back(TObjType &inObj)
    {
        if (m_tail != nullptr)
            TBaseType::insert_after(*m_tail, inObj);
        m_tail = &inObj;

        if (m_head == nullptr)
            m_head = &inObj;
    }

    void remove(TObjType &inObj)
    {
        if (m_head == &inObj)
            m_head = TObjTailOp().get(inObj);
        if (m_tail == &inObj)
            m_tail = TObjHeadOp().get(inObj);

        TBaseType::remove(inObj);
    }

    bool empty() const { return m_head == nullptr; }

    iterator begin() { return iterator(m_head); }
    iterator end() { return iterator(nullptr); }

    const_iterator begin() const { return iterator(m_head); }
    const_iterator end() const { return iterator(nullptr); }

    reverse_iterator rbegin() { return reverse_iterator(m_tail); }
    reverse_iterator rend() { return reverse_iterator(nullptr); }

    const_reverse_iterator rbegin() const { return reverse_iterator(m_tail); }
    const_reverse_iterator rend() const { return reverse_iterator(nullptr); }
};

// Macros to speed up the definitely of invasive linked lists.
#define DEFINE_INVASIVE_SINGLE_LIST(type)                                                                              \
    struct type;                                                                                                       \
    struct type##NextOp                                                                                                \
    {                                                                                                                  \
        type *get(type &s);                                                                                            \
        const type *get(const type &s) const;                                                                          \
        void set(type &inItem, type *inNext);                                                                          \
    };                                                                                                                 \
    typedef QSSGInvasiveSingleLinkedList<type, type##NextOp> type##List;

#define DEFINE_INVASIVE_LIST(type)                                                                                     \
    struct type;                                                                                                       \
    struct type##NextOp                                                                                                \
    {                                                                                                                  \
        type *get(type &s);                                                                                            \
        const type *get(const type &s) const;                                                                          \
        void set(type &inItem, type *inNext);                                                                          \
    };                                                                                                                 \
    struct type##PreviousOp                                                                                            \
    {                                                                                                                  \
        type *get(type &s);                                                                                            \
        const type *get(const type &s) const;                                                                          \
        void set(type &inItem, type *inNext);                                                                          \
    };                                                                                                                 \
    typedef QSSGInvasiveLinkedList<type, type##PreviousOp, type##NextOp> type##List;

#define IMPLEMENT_INVASIVE_LIST(type, prevMember, nextMember)                                                          \
    inline type *type##NextOp::get(type &s) { return s.nextMember; }                                                   \
    inline const type *type##NextOp::get(const type &s) const { return s.nextMember; }                                 \
    inline void type##NextOp::set(type &inItem, type *inNext) { inItem.nextMember = inNext; }                          \
    inline type *type##PreviousOp::get(type &s) { return s.prevMember; }                                               \
    inline const type *type##PreviousOp::get(const type &s) const { return s.prevMember; }                             \
    inline void type##PreviousOp::set(type &inItem, type *inNext) { inItem.prevMember = inNext; }

#define IMPLEMENT_INVASIVE_SINGLE_LIST(type, nextMember)                                                               \
    inline type *type##NextOp::get(type &s) { return s.nextMember; }                                                   \
    inline const type *type##NextOp::get(const type &s) const { return s.nextMember; }                                 \
    inline void type##NextOp::set(type &inItem, type *inNext) { inItem.nextMember = inNext; }

QT_END_NAMESPACE

#endif // QSSGINVASIVELINKEDLIST_H
