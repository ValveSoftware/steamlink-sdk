/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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

