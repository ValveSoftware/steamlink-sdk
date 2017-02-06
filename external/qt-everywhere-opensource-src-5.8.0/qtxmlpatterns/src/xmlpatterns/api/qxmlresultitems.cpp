/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include "qxmlresultitems.h"
#include "qxmlresultitems_p.h"
#include "qitem_p.h"

QT_BEGIN_NAMESPACE

/*!
  \class QXmlResultItems
  \brief The QXmlResultItems class iterates through the results of evaluating an XQuery in QXmlQuery.
  \reentrant
  \since 4.4
  \ingroup xml-tools
  \inmodule QtXmlPatterns

  QXmlResultItems presents the evaluation of an associated query as a
  sequence of \l{QXmlItem}{QXmlItems}. The sequence is traversed by
  repeatedly calling next(), which actually produces the sequence by
  lazy evaluation of the query.

  \snippet code/src_xmlpatterns_api_qxmlresultitems.cpp 0

  An effect of letting next() produce the sequence by lazy evaluation
  is that a query error can occur on any call to next(). If an error
  occurs, both next() and current() will return the null QXmlItem, and
  hasError() will return true.

  QXmlResultItems can be thought of as an "iterator" that traverses
  the sequence of query results once, in the forward direction. Each
  call to next() advances the iterator to the next QXmlItem in the
  sequence and returns it, and current() always returns the QXmlItem
  that next() returned the last time it was called.

  \note When using the QXmlResultItems overload of QXmlQuery::evaluateTo()
  to execute a query, it is advisable to create a new instance of this
  class for each new set of results rather than reusing an old instance.

  \sa QXmlItem::isNode(), QXmlItem::isAtomicValue(), QXmlNodeModelIndex
 */

/*!
  Constructs an instance of QXmlResultItems.
 */
QXmlResultItems::QXmlResultItems() : d_ptr(new QXmlResultItemsPrivate())
{
}

/*!
  Destroys this instance of QXmlResultItems.
 */
QXmlResultItems::~QXmlResultItems()
{
}

/*!
  Returns the next result in the sequence produced by lazy evaluation
  of the associated query. When the returned QXmlItem is null, either
  the evaluation terminated normally without producing another result,
  or an error occurred. Call hasError() to determine whether the null
  item was caused by normal termination or by an error.

  Returns a null QXmlItem if there is no associated QXmlQuery.
 */
QXmlItem QXmlResultItems::next()
{
    Q_D(QXmlResultItems);
    if(d->hasError)
        return QXmlItem();

    try
    {
        d->current = QPatternist::Item::toPublic(d->iterator->next());
        return d->current;
    }
    catch(const QPatternist::Exception)
    {
        d->current = QXmlItem();
        d->hasError = true;
        return QXmlItem();
    }
}

/*!
  Returns the current item. The current item is the last item
  that was produced and returned by next().

  Returns a null QXmlItem if there is no associated QXmlQuery.
 */
QXmlItem QXmlResultItems::current() const
{
    Q_D(const QXmlResultItems);

    if(d->hasError)
        return QXmlItem();
    else
        return d->current;
}

/*!

  If an error occurred during evaluation of the query, true is
  returned.

  Returns false if query evaluation has been done.
 */
bool QXmlResultItems::hasError() const
{
    Q_D(const QXmlResultItems);
    return d->hasError;
}

QT_END_NAMESPACE

