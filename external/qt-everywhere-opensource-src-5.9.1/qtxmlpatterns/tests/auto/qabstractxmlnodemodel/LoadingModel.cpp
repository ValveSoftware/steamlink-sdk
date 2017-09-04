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

#include <QFile>
#include <QStack>

#include <QXmlNamePool>
#include <QXmlStreamReader>
#include <QtDebug>
#include <QTest>

#include "LoadingModel.h"
LoadingModel::LoadingModel(const Node::Vector &content,
                           const QXmlNamePool &np) : QSimpleXmlNodeModel(np)
                                                   , m_nodes(content)
{
    /*
    foreach(const Node *n, content)
        qDebug() << "this:" << n
                 << "kind:" << n->kind
                 << "parent: " << n->parent
                 << "preceding: " << n->precedingSibling
                 << "following: " << n->followingSibling
                 << "firstChild: " << n->firstChild
                 << "value: " << n->value;
                 */
}

LoadingModel::~LoadingModel()
{
     qDeleteAll(m_nodes);
}

const LoadingModel::Node *LoadingModel::toInternal(const QXmlNodeModelIndex &ni) const
{
    return static_cast<const Node *>(ni.internalPointer());
}

QXmlNodeModelIndex LoadingModel::createIndex(const Node *const internal) const
{
    if (!internal)
        qFatal("%s: cannot construct a model index from a null pointer", Q_FUNC_INFO);
    return QAbstractXmlNodeModel::createIndex(const_cast<Node *>(internal));
}

QUrl LoadingModel::documentUri(const QXmlNodeModelIndex &) const
{
    qFatal("%s: This method should not be called during the test", Q_FUNC_INFO);
    return QUrl();
}

QXmlNodeModelIndex::NodeKind LoadingModel::kind(const QXmlNodeModelIndex &ni) const
{
    if (ni.isNull())
        qFatal("%s: node model index should not be null", Q_FUNC_INFO);
    return toInternal(ni)->kind;
}

QXmlNodeModelIndex::DocumentOrder LoadingModel::compareOrder(const QXmlNodeModelIndex &n1, const QXmlNodeModelIndex &n2) const
{
    const Node *const in1 = toInternal(n1);
    const Node *const in2 = toInternal(n2);
    if (m_nodes.indexOf(in1) == -1)
        qFatal("%s: node n1 is not in internal node list", Q_FUNC_INFO);
    if (m_nodes.indexOf(in2) == -1)
        qFatal("%s: node n2 is not in internal node list", Q_FUNC_INFO);

    if(in1 == in2)
        return QXmlNodeModelIndex::Is;
    else if(m_nodes.indexOf(in1) < m_nodes.indexOf(in2))
        return QXmlNodeModelIndex::Precedes;
    else
        return QXmlNodeModelIndex::Follows;
}

QXmlNodeModelIndex LoadingModel::root(const QXmlNodeModelIndex &) const
{
    if (kind(createIndex(m_nodes.first())) != QXmlNodeModelIndex::Document) {
        qWarning("%s: first node must be a Document node", Q_FUNC_INFO);
        return QXmlNodeModelIndex();
    }
    return createIndex(m_nodes.first());
}

QXmlName LoadingModel::name(const QXmlNodeModelIndex &ni) const
{
    return toInternal(ni)->name;
}

QVariant LoadingModel::typedValue(const QXmlNodeModelIndex &ni) const
{
    const Node *const internal = toInternal(ni);

    if (internal->kind != QXmlNodeModelIndex::Attribute
        && internal->kind != QXmlNodeModelIndex::Element) {
        qWarning("%s: node must be an attribute or element", Q_FUNC_INFO);
        return QVariant();
    }

    return internal->value;
}

QString LoadingModel::stringValue(const QXmlNodeModelIndex &ni) const
{
    const Node *const internal = toInternal(ni);

    switch(internal->kind)
    {
        case QXmlNodeModelIndex::Text:
        /* Fallthrough. */
        case QXmlNodeModelIndex::ProcessingInstruction:
        /* Fallthrough. */
        case QXmlNodeModelIndex::Comment:
        /* Fallthrough. */
        case QXmlNodeModelIndex::Attribute:
            return internal->value;
        default:
            return QString();
    }
}

QXmlNodeModelIndex LoadingModel::nextFromSimpleAxis(QAbstractXmlNodeModel::SimpleAxis axis,
                                                    const QXmlNodeModelIndex &ni) const
{
    const Node *const internal = toInternal(ni);

    /* Note that a QXmlNodeModelIndex containing a null pointer is not a null node. */
    switch(axis)
    {
        case Parent:
            return internal->parent ? createIndex(internal->parent) : QXmlNodeModelIndex();
        case FirstChild:
            return internal->firstChild ? createIndex(internal->firstChild) : QXmlNodeModelIndex();
        case PreviousSibling:
            return internal->precedingSibling ? createIndex(internal->precedingSibling) : QXmlNodeModelIndex();
        case NextSibling:
            return internal->followingSibling ? createIndex(internal->followingSibling) : QXmlNodeModelIndex();
        default:
            qWarning("%s: unknown axis enum value %d", Q_FUNC_INFO, static_cast<int>(axis));
            return QXmlNodeModelIndex();
    }
}

QVector<QXmlNodeModelIndex> LoadingModel::attributes(const QXmlNodeModelIndex &ni) const
{
    QVector<QXmlNodeModelIndex> retval;
    for (const Node *n : toInternal(ni)->attributes)
        retval.append(createIndex(n));

    return retval;
}

class Loader
{
public:
    inline Loader(const QXmlNamePool &namePool) : m_namePool(namePool)
                                                , m_currentNode(0)
    {
        m_parentStack.push(0);
    }

private:
    inline void adjustSiblings(LoadingModel::Node *const justBorn);
    friend class LoadingModel;
    Q_DISABLE_COPY(Loader);

    void load();

    QXmlNamePool                        m_namePool;
    QXmlStreamReader                    m_reader;
    LoadingModel::Node::Vector          m_result;
    LoadingModel::Node *                m_currentNode;
    QStack<LoadingModel::Node *>        m_parentStack;
};

inline void Loader::adjustSiblings(LoadingModel::Node *const justBorn)
{
    if(m_currentNode)
    {
        if(m_currentNode->parent == justBorn->parent)
            justBorn->precedingSibling = m_currentNode;

        m_currentNode->followingSibling = justBorn;
    }

    m_currentNode = justBorn;

    /* Otherwise we're the first child, and our precedingSibling should remain null. */

    if(m_parentStack.top() && !m_parentStack.top()->firstChild)
        m_parentStack.top()->firstChild = justBorn;
}

void Loader::load()
{
    QFile in(QLatin1String("tree.xml"));

    /* LoadingModel::m_result will be null, signalling failure. */
    if(!in.open(QIODevice::ReadOnly))
        return;

    QXmlStreamReader reader(&in);
    while(!reader.atEnd())
    {
        reader.readNext();

        switch(reader.tokenType())
        {
            case QXmlStreamReader::StartDocument:
            /* Fallthrough. */
            case QXmlStreamReader::StartElement:
            {
                QXmlName name;
                if(reader.tokenType() == QXmlStreamReader::StartElement)
                {
                    name = QXmlName(m_namePool,
                                    reader.name().toString(),
                                    reader.namespaceUri().toString(),
                                    reader.prefix().toString());
                }
                /* Else, the name is null. */

                LoadingModel::Node *const tmp = new LoadingModel::Node(reader.tokenType() == QXmlStreamReader::StartElement
                                                                       ? QXmlNodeModelIndex::Element
                                                                       : QXmlNodeModelIndex::Document,
                                                                       m_parentStack.top(),
                                                                       QString(),
                                                                       name);
                m_result.append(tmp);

                if(m_currentNode)
                {
                    if(m_currentNode->parent == m_parentStack.top())
                        m_currentNode->followingSibling = tmp;
                }

                const QXmlStreamAttributes attributes(reader.attributes());
                const int len = attributes.count();

                for(int i = 0; i < len; ++i)
                {
                    const QXmlStreamAttribute &attr = attributes.at(i);
                    const LoadingModel::Node *const a = new LoadingModel::Node(QXmlNodeModelIndex::Attribute,
                                                                               m_parentStack.top(),
                                                                               attr.value().toString(),
                                                                               QXmlName(m_namePool,
                                                                                       attr.name().toString(),
                                                                                       attr.namespaceUri().toString(),
                                                                                       attr.prefix().toString()));
                    /* We add it also to m_result such that compareOrder() is correct
                     * for attributes. m_result owns a. */
                    tmp->attributes.append(a);
                    m_result.append(a);
                }

                adjustSiblings(tmp);
                m_parentStack.push(m_currentNode);
                break;
            }
            case QXmlStreamReader::EndDocument:
            /* Fallthrough. */
            case QXmlStreamReader::EndElement:
            {
                m_currentNode->followingSibling = 0;
                m_currentNode = m_parentStack.pop();

                if(reader.tokenType() == QXmlStreamReader::EndDocument)
                    const_cast<LoadingModel::Node *>(m_result.first())->followingSibling = 0;

                break;
            }
            case QXmlStreamReader::Characters:
            {
                LoadingModel::Node *const tmp = new LoadingModel::Node(QXmlNodeModelIndex::Text, m_parentStack.top(), reader.text().toString());
                m_result.append(tmp);
                adjustSiblings(tmp);
                break;
            }
            case QXmlStreamReader::ProcessingInstruction:
            {
                LoadingModel::Node *const tmp = new LoadingModel::Node(QXmlNodeModelIndex::ProcessingInstruction,
                                                                       m_parentStack.top(),
                                                                       reader.processingInstructionData().toString(),
                                                                       QXmlName(m_namePool, reader.processingInstructionTarget().toString()));
                m_result.append(tmp);
                adjustSiblings(tmp);
                break;
            }
            case QXmlStreamReader::Comment:
            {
                LoadingModel::Node *const tmp = new LoadingModel::Node(QXmlNodeModelIndex::Comment, m_parentStack.top(), reader.text().toString());
                m_result.append(tmp);
                adjustSiblings(tmp);
                break;
            }
            case QXmlStreamReader::DTD:
                qFatal("%s: QXmlStreamReader::DTD token is not supported", Q_FUNC_INFO);
                break;
            case QXmlStreamReader::EntityReference:
                qFatal("%s: QXmlStreamReader::EntityReference token is not supported", Q_FUNC_INFO);
                break;
            case QXmlStreamReader::NoToken:
            /* Fallthrough. */
            case QXmlStreamReader::Invalid:
            {
                qWarning("%s", qPrintable(reader.errorString()));
                m_result.clear();
                return;
            }
        }
    }

    if(reader.hasError())
    {
        qWarning("%s", qPrintable(reader.errorString()));
        m_result.clear();
    }
}

QAbstractXmlNodeModel::Ptr LoadingModel::create(const QXmlNamePool &np)
{
    Loader loader(np);
    loader.load();
    if (loader.m_result.isEmpty()) {
        qWarning("%s: attempt to create model with no content", Q_FUNC_INFO);
        return Ptr(0);
    }

    return Ptr(new LoadingModel(loader.m_result, np));
}
