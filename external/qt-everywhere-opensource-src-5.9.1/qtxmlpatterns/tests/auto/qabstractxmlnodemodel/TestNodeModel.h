/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include <QtXmlPatterns/QAbstractXmlNodeModel>

/*!
 \class TestNodeModel
 \since 4.4
 \internal
 \brief Subclass of QAbstractXmlNodeModel, used by tst_QAbstractXmlNodeModel, for testing only.
 */
class TestNodeModel : public QAbstractXmlNodeModel
{
public:
    virtual QUrl baseUri(const QXmlNodeModelIndex&) const;
    virtual QUrl documentUri(const QXmlNodeModelIndex&) const;
    virtual QXmlNodeModelIndex::NodeKind kind(const QXmlNodeModelIndex&) const;
    virtual QXmlNodeModelIndex::DocumentOrder compareOrder(const QXmlNodeModelIndex&, const QXmlNodeModelIndex&) const;
    virtual QXmlNodeModelIndex root(const QXmlNodeModelIndex&) const;
    virtual QXmlName name(const QXmlNodeModelIndex&) const;
    virtual QString stringValue(const QXmlNodeModelIndex&) const;
    virtual QVariant typedValue(const QXmlNodeModelIndex&) const;
    virtual QVector<QXmlName> namespaceBindings(const QXmlNodeModelIndex&) const;
    virtual QXmlNodeModelIndex elementById(const QXmlName &ncname) const;
    virtual QVector<QXmlNodeModelIndex> nodesByIdref(const QXmlName &ncname) const;

protected:
    virtual QXmlNodeModelIndex nextFromSimpleAxis(SimpleAxis axis,
                                                  const QXmlNodeModelIndex &origin) const;
    virtual QVector<QXmlNodeModelIndex> attributes(const QXmlNodeModelIndex&) const;

};

QUrl TestNodeModel::baseUri(const QXmlNodeModelIndex&) const
{
    return QUrl();
}

QUrl TestNodeModel::documentUri(const QXmlNodeModelIndex&) const
{
    return QUrl();
}

QXmlNodeModelIndex::NodeKind TestNodeModel::kind(const QXmlNodeModelIndex&) const
{
    return QXmlNodeModelIndex::Element;
}

QXmlNodeModelIndex::DocumentOrder TestNodeModel::compareOrder(const QXmlNodeModelIndex&, const QXmlNodeModelIndex&) const
{
    return QXmlNodeModelIndex::Precedes;
}

QXmlNodeModelIndex TestNodeModel::root(const QXmlNodeModelIndex&) const
{
    return QXmlNodeModelIndex();
}

QXmlName TestNodeModel::name(const QXmlNodeModelIndex&) const
{
    return QXmlName();
}

QString TestNodeModel::stringValue(const QXmlNodeModelIndex&) const
{
    return QString();
}

QVariant TestNodeModel::typedValue(const QXmlNodeModelIndex&) const
{
    return QVariant();
}

QVector<QXmlName> TestNodeModel::namespaceBindings(const QXmlNodeModelIndex&) const
{
    return QVector<QXmlName>();
}

QXmlNodeModelIndex TestNodeModel::elementById(const QXmlName &ncname) const
{
    Q_UNUSED(ncname);
    return QXmlNodeModelIndex();
}

QVector<QXmlNodeModelIndex> TestNodeModel::nodesByIdref(const QXmlName &ncname) const
{
    Q_UNUSED(ncname);
    return QVector<QXmlNodeModelIndex>();
}

QXmlNodeModelIndex TestNodeModel::nextFromSimpleAxis(SimpleAxis, const QXmlNodeModelIndex &) const
{
    return QXmlNodeModelIndex();
}

QVector<QXmlNodeModelIndex> TestNodeModel::attributes(const QXmlNodeModelIndex&) const
{
    return QVector<QXmlNodeModelIndex>();
}

