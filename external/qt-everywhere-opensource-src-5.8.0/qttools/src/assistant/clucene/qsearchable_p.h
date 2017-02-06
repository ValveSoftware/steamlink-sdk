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

#ifndef QSEARCHABLE_P_H
#define QSEARCHABLE_P_H

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

#include "qhits_p.h"
#include "qsort_p.h"
#include "qquery_p.h"
#include "qfilter_p.h"
#include "qdocument_p.h"
#include "qindexreader_p.h"
#include "qclucene_global_p.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QSharedData>

CL_NS_DEF(search)
    class Searcher;
CL_NS_END
CL_NS_USE(search)

QT_BEGIN_NAMESPACE

class QCLuceneHits;
class QCLuceneSearcher;
class QCLuceneIndexSearcher;
class QCLuceneMultiSearcher;

class Q_CLUCENE_EXPORT QCLuceneSearchablePrivate : public QSharedData
{
public:
    QCLuceneSearchablePrivate();
    QCLuceneSearchablePrivate(const QCLuceneSearchablePrivate &other);

    ~QCLuceneSearchablePrivate();

    Searcher *searchable;
    bool deleteCLuceneSearchable;

private:
    QCLuceneSearchablePrivate &operator=(const QCLuceneSearchablePrivate &other);
};

class Q_CLUCENE_EXPORT QCLuceneSearchable
{
public:
    virtual ~QCLuceneSearchable();

protected:
    friend class QCLuceneSearcher;
    friend class QCLuceneIndexSearcher;
    friend class QCLuceneMultiSearcher;
    QSharedDataPointer<QCLuceneSearchablePrivate> d;

private:
    QCLuceneSearchable();
};

class Q_CLUCENE_EXPORT QCLuceneSearcher : public QCLuceneSearchable
{
public:
    QCLuceneSearcher();
    virtual ~QCLuceneSearcher();

    QCLuceneHits search(const QCLuceneQuery &query);
    QCLuceneHits search(const QCLuceneQuery &query, const QCLuceneFilter &filter);
    QCLuceneHits search(const QCLuceneQuery &query, const QCLuceneSort &sort);
    QCLuceneHits search(const QCLuceneQuery &query, const QCLuceneFilter &filter,
        const QCLuceneSort &sort);

protected:
    friend class QCLuceneHits;
};

class Q_CLUCENE_EXPORT QCLuceneIndexSearcher : public QCLuceneSearcher
{
public:
    QCLuceneIndexSearcher(const QString &path);
    QCLuceneIndexSearcher(const QCLuceneIndexReader &reader);
    ~QCLuceneIndexSearcher();

    void close();
    qint32 maxDoc() const;
    QCLuceneIndexReader getReader();
    bool doc(qint32 i, QCLuceneDocument &document);

private:
    QCLuceneIndexReader reader;
};

class Q_CLUCENE_EXPORT QCLuceneMultiSearcher : public QCLuceneSearcher
{
public:
    QCLuceneMultiSearcher(const QList<QCLuceneSearchable> searchables);
    ~QCLuceneMultiSearcher();

    void close();
    qint32 maxDoc() const;
    qint32 subDoc(qint32 index) const;
    qint32 subSearcher(qint32 index) const;
    qint32 searcherIndex(qint32 index) const;
    bool doc(qint32 i, QCLuceneDocument &document);
};

QT_END_NAMESPACE

#endif  // QSEARCHABLE_P_H
