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

#include <QUrl>
#include <QVector>
#include <QXmlNamePool>

#include "qabstractxmlnodemodel_p.h"
#include "qemptyiterator_p.h"
#include "qitemmappingiterator_p.h"
#include "qsequencemappingiterator_p.h"
#include "qsimplexmlnodemodel.h"
#include "qsingletoniterator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

class QSimpleXmlNodeModelPrivate : public QAbstractXmlNodeModelPrivate
{
public:
    QSimpleXmlNodeModelPrivate(const QXmlNamePool &np) : namePool(np)
    {
    }

    mutable QXmlNamePool namePool;
};

/*!
  \class QSimpleXmlNodeModel
  \brief The QSimpleXmlNodeModel class is an implementation of QAbstractXmlNodeModel sufficient for many common cases.
  \reentrant
  \since 4.4
  \ingroup xml-tools
  \inmodule QtXmlPatterns

  Subclassing QAbstractXmlNodeModel can be a significant task, because it
  requires implementing several, complex member functions. QSimpleXmlNodeModel
  provides default implementations of these member functions that are suitable
  for a wide range of node models.

  Subclasses of QSimpleXmlNodeModel must be thread-safe.
 */

/*!
  Constructs a QSimpleXmlNodeModel for use with with the specified
  \a namePool.
 */
QSimpleXmlNodeModel::QSimpleXmlNodeModel(const QXmlNamePool &namePool)
  : QAbstractXmlNodeModel(new QSimpleXmlNodeModelPrivate(namePool))
{
}

/*!
  Destructor.
 */
QSimpleXmlNodeModel::~QSimpleXmlNodeModel()
{
}

/*!
 If \a node is an element or attribute, typedValue() is called, and
 the return value converted to a string, as per XQuery's rules.

 If \a node is another type of node, the empty string is returned.

 If this function is overridden for comments or processing
 instructions, it is important to remember to call it (for elements
 and attributes having values not of type \c xs:string) to ensure that
 the values are formatted according to XQuery.

 */
QString QSimpleXmlNodeModel::stringValue(const QXmlNodeModelIndex &node) const
{
    const QXmlNodeModelIndex::NodeKind k= kind(node);
    if(k == QXmlNodeModelIndex::Element || k == QXmlNodeModelIndex::Attribute)
    {
        const QVariant &candidate = typedValue(node);
        if(candidate.isNull())
            return QString();
        else
            return AtomicValue::toXDM(candidate).stringValue();
    }
    else
        return QString();
}

/*!
  Returns the base URI for \a node. This is always the document
  URI.

  \sa documentUri()
 */
QUrl QSimpleXmlNodeModel::baseUri(const QXmlNodeModelIndex &node) const
{
    return documentUri(node);
}

/*!
  Returns the name pool associated with this model. The
  implementation of name() will use this name pool to create
  names.
 */
QXmlNamePool &QSimpleXmlNodeModel::namePool() const
{
    Q_D(const QSimpleXmlNodeModel);

    return d->namePool;
}

/*!
  Always returns an empty QVector. This signals that no namespace
  bindings are in scope for \a node.
 */
QVector<QXmlName> QSimpleXmlNodeModel::namespaceBindings(const QXmlNodeModelIndex &node) const
{
    Q_UNUSED(node);
    return QVector<QXmlName>();
}

/*!
  Always returns a default constructed QXmlNodeModelIndex instance,
  regardless of \a id.

  This effectively means the model has no elements that have an id.
 */
QXmlNodeModelIndex QSimpleXmlNodeModel::elementById(const QXmlName &id) const
{
    Q_UNUSED(id);
    return QXmlNodeModelIndex();
}

/*!
  Always returns an empty vector, regardless of \a idref.

  This effectively means the model has no elements or attributes of
  type \c IDREF.
 */
QVector<QXmlNodeModelIndex> QSimpleXmlNodeModel::nodesByIdref(const QXmlName &idref) const
{
    Q_UNUSED(idref);
    return QVector<QXmlNodeModelIndex>();
}

QT_END_NAMESPACE


