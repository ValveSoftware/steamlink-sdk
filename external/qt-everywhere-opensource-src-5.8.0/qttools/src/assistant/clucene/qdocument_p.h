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

#ifndef QDOCUMENT_P_H
#define QDOCUMENT_P_H

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

#include "qfield_p.h"
#include "qclucene_global_p.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QSharedData>

CL_NS_DEF(document)
    class Document;
CL_NS_END
CL_NS_USE(document)

QT_BEGIN_NAMESPACE

class QCLuceneHits;
class QCLuceneIndexReader;
class QCLuceneIndexWriter;
class QCLuceneIndexSearcher;
class QCLuceneMultiSearcher;

class Q_CLUCENE_EXPORT QCLuceneDocumentPrivate : public QSharedData
{
public:
    QCLuceneDocumentPrivate();
    QCLuceneDocumentPrivate(const QCLuceneDocumentPrivate &other);

    ~QCLuceneDocumentPrivate();

    Document *document;
    bool deleteCLuceneDocument;

private:
    QCLuceneDocumentPrivate &operator=(const QCLuceneDocumentPrivate &other);
};

class Q_CLUCENE_EXPORT QCLuceneDocument
{
public:
    QCLuceneDocument();
    ~QCLuceneDocument();

    void add(QCLuceneField *field);
    QCLuceneField* getField(const QString &name) const;
    QString get(const QString &name) const;
    QString toString() const;
    void setBoost(qreal boost);
    qreal getBoost() const;
    void removeField(const QString &name);
    void removeFields(const QString &name);
    QStringList getValues(const QString &name) const;
    void clear();

protected:
    friend class QCLuceneHits;
    friend class QCLuceneIndexReader;
    friend class QCLuceneIndexWriter;
    friend class QCLuceneIndexSearcher;
    friend class QCLuceneMultiSearcher;
    QSharedDataPointer<QCLuceneDocumentPrivate> d;

private:
    mutable QList<QCLuceneField*> fieldList;
};

QT_END_NAMESPACE

#endif  // QDOCUMENT_P_H
