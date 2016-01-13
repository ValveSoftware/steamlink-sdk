/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef Patternist_QObjectNodeModel_H
#define Patternist_QObjectNodeModel_H

#include <QSimpleXmlNodeModel>

QT_BEGIN_NAMESPACE

class QObject;
class PropertyToAtomicValue;

/**
 * @short Delegates QtCore's QObject into Patternist's QAbstractXmlNodeModel.
 * known as pre/post numbering.
 *
 * QObjectXmlModel sets the toggle on QXmlNodeModelIndex to @c true, if it
 * represents a property of the QObject. That is, if the QXmlNodeModelIndex is
 * an attribute.
 *
 * @author Frans Englich <frans.englich@nokia.com>
 */
class QObjectXmlModel : public QSimpleXmlNodeModel
{
  public:
    QObjectXmlModel(QObject *const object, const QXmlNamePool &np);

    QXmlNodeModelIndex root() const;

//! [0]
    virtual QXmlNodeModelIndex::DocumentOrder compareOrder(const QXmlNodeModelIndex &n1, const QXmlNodeModelIndex &n2) const;
    virtual QXmlName name(const QXmlNodeModelIndex &n) const;
    virtual QUrl documentUri(const QXmlNodeModelIndex &n) const;
    virtual QXmlNodeModelIndex::NodeKind kind(const QXmlNodeModelIndex &n) const;
    virtual QXmlNodeModelIndex root(const QXmlNodeModelIndex &n) const;
    virtual QVariant typedValue(const QXmlNodeModelIndex &n) const;
    virtual QVector<QXmlNodeModelIndex> attributes(const QXmlNodeModelIndex&) const;
    virtual QXmlNodeModelIndex nextFromSimpleAxis(SimpleAxis, const QXmlNodeModelIndex&) const;
//! [0]

  private:
    /**
     * The highest three bits are used to signify whether the node index
     * is an artificial node.
     *
     * @short if QXmlNodeModelIndex::additionalData() has the
     * QObjectPropery flag set, then the QXmlNodeModelIndex is an
     * attribute of the QObject element, and the remaining bits form
     * an offset to the QObject property that the QXmlNodeModelIndex
     * refers to.
     *
     */
//! [3]
    enum QObjectNodeType
    {
        IsQObject               = 0,
        QObjectProperty         = 1 << 26,
        MetaObjects             = 2 << 26,
        MetaObject              = 3 << 26,
        MetaObjectClassName     = 4 << 26,
        MetaObjectSuperClass    = 5 << 26,
        QObjectClassName        = 6 << 26
    };
//! [3]

//! [1]
    typedef QVector<const QMetaObject *> AllMetaObjects;
//! [1]
    AllMetaObjects allMetaObjects() const;

    static QObjectNodeType toNodeType(const QXmlNodeModelIndex &n);
    static bool isTypeSupported(QVariant::Type type);
    static inline QObject *asQObject(const QXmlNodeModelIndex &n);
    static inline bool isProperty(const QXmlNodeModelIndex n);
    static inline QMetaProperty toMetaProperty(const QXmlNodeModelIndex &n);
    /**
     * Returns the ancestors of @p n. Does therefore not include
     * @p n.
     */
    inline QXmlNodeModelIndex::List ancestors(const QXmlNodeModelIndex n) const;
    QXmlNodeModelIndex qObjectSibling(const int pos,
                                      const QXmlNodeModelIndex &n) const;
    QXmlNodeModelIndex metaObjectSibling(const int pos,
                                         const QXmlNodeModelIndex &n) const;

//! [2]
    const QUrl              m_baseURI;
    QObject *const          m_root;
//! [4]
    const AllMetaObjects    m_allMetaObjects;
//! [4]
//! [2]
};

QT_END_NAMESPACE

#endif
