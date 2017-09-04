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


#include <QSimpleXmlNodeModel>
#include <QVector>

class LoadingModel : public QSimpleXmlNodeModel
{
public:
    virtual ~LoadingModel();
    static QAbstractXmlNodeModel::Ptr create(const QXmlNamePool &np);

    virtual QUrl documentUri(const QXmlNodeModelIndex &) const;
    virtual QXmlNodeModelIndex::NodeKind kind(const QXmlNodeModelIndex &) const;
    virtual QXmlNodeModelIndex::DocumentOrder compareOrder(const QXmlNodeModelIndex &, const QXmlNodeModelIndex&) const;
    virtual QXmlNodeModelIndex root(const QXmlNodeModelIndex &) const;
    virtual QXmlName name(const QXmlNodeModelIndex &) const;
    virtual QVariant typedValue(const QXmlNodeModelIndex &) const;
    virtual QString stringValue(const QXmlNodeModelIndex &) const;
    virtual QXmlNodeModelIndex nextFromSimpleAxis(QAbstractXmlNodeModel::SimpleAxis, const QXmlNodeModelIndex &) const;
    virtual QVector<QXmlNodeModelIndex> attributes(const QXmlNodeModelIndex &) const;

private:
    friend class Loader;
    class Node
    {
    public:
        inline Node(const QXmlNodeModelIndex::NodeKind k,
                    const Node *const p,
                    const QString &v = QString(),
                    const QXmlName &n = QXmlName()) : kind(k)
                                                    , value(v)
                                                    , parent(p)
                                                    , precedingSibling(0)
                                                    , followingSibling(0)
                                                    , firstChild(0)
                                                    , name(n)
        {
        }

        typedef QVector<const Node *> Vector;
        QXmlNodeModelIndex::NodeKind    kind;
        QString                         value;
        const Node *                    parent;
        const Node *                    precedingSibling;
        const Node *                    followingSibling;
        const Node *                    firstChild;
        Node::Vector                    attributes;
        QXmlName                        name;
    };

    inline const Node *toInternal(const QXmlNodeModelIndex &ni) const;
    inline QXmlNodeModelIndex createIndex(const Node *const internal) const;

    LoadingModel(const Node::Vector &content,
                 const QXmlNamePool &np);

    Node::Vector m_nodes;
};

