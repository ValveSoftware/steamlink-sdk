/****************************************************************************
**
** Copyright (C) 2003-2006 Ben van Klinken and the CLucene Team.
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtTools module of the Qt Toolkit.
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

#ifndef QFIELD_P_H
#define QFIELD_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists for the convenience
// of the help generator tools. This header file may change from version
// to version without notice, or even be removed.
//
// We mean it.
//

#include "qclucene_global_p.h"

#include <QtCore/QString>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QSharedData>

CL_NS_DEF(document)
    class Field;
CL_NS_END
CL_NS_USE(document)

QT_BEGIN_NAMESPACE

class QCLuceneReader;
class QCLuceneDocument;

class Q_CLUCENE_EXPORT QCLuceneFieldPrivate : public QSharedData
{
public:
    QCLuceneFieldPrivate();
    QCLuceneFieldPrivate(const QCLuceneFieldPrivate &other);

    ~QCLuceneFieldPrivate();

    Field *field;
    bool deleteCLuceneField;

private:
    QCLuceneFieldPrivate &operator=(const QCLuceneFieldPrivate &other);
};

class Q_CLUCENE_EXPORT QCLuceneField
{
public:
    enum Store {
        STORE_YES = 1,
        STORE_NO = 2,
        STORE_COMPRESS = 4
    };

    enum Index {
        INDEX_NO = 16,
        INDEX_TOKENIZED = 32,
        INDEX_UNTOKENIZED = 64,
        INDEX_NONORMS = 128
    };

    enum TermVector {
        TERMVECTOR_NO = 256,
        TERMVECTOR_YES = 512,
        TERMVECTOR_WITH_POSITIONS = 1024,
        TERMVECTOR_WITH_OFFSETS = 2048
    };

    QCLuceneField(const QString &name, const QString &value, int configs);
    QCLuceneField(const QString &name, QCLuceneReader *reader, int configs);
    ~QCLuceneField();

    QString name() const;
    QString stringValue() const;
    QCLuceneReader* readerValue() const;
    bool isStored() const;
    bool isIndexed() const;
    bool isTokenized() const;
    bool isCompressed() const;
    void setConfig(int termVector);
    bool isTermVectorStored() const;
    bool isStoreOffsetWithTermVector() const;
    bool isStorePositionWithTermVector() const;
    qreal getBoost() const;
    void setBoost(qreal value);
    bool isBinary() const;
    bool getOmitNorms() const;
    void setOmitNorms(bool omitNorms);
    QString toString() const;

protected:
    QCLuceneField();
    friend class QCLuceneDocument;
    QSharedDataPointer<QCLuceneFieldPrivate> d;

private:
    QCLuceneReader* reader;
};

QT_END_NAMESPACE

#endif  // QFIELD_P_H
