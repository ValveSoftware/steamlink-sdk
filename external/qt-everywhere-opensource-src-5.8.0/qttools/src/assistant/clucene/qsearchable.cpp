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

#include "qsearchable_p.h"

#include <CLucene.h>
#include <CLucene/search/SearchHeader.h>

QT_BEGIN_NAMESPACE

QCLuceneSearchablePrivate::QCLuceneSearchablePrivate()
    : QSharedData()
{
    searchable = 0;
    deleteCLuceneSearchable = true;
}

QCLuceneSearchablePrivate::QCLuceneSearchablePrivate(const QCLuceneSearchablePrivate &other)
    : QSharedData()
{
    searchable = _CL_POINTER(other.searchable);
    deleteCLuceneSearchable = other.deleteCLuceneSearchable;
}

QCLuceneSearchablePrivate::~QCLuceneSearchablePrivate()
{
    if (deleteCLuceneSearchable)
        _CLDECDELETE(searchable);
}


QCLuceneSearchable::QCLuceneSearchable()
    : d(new QCLuceneSearchablePrivate())
{
    // nothing todo
}

QCLuceneSearchable::~QCLuceneSearchable()
{
    // nothing todo
}


QCLuceneSearcher::QCLuceneSearcher()
    : QCLuceneSearchable()
{
    // nothing todo
}

QCLuceneSearcher::~QCLuceneSearcher()
{
    // nothing todo;
}

QCLuceneHits QCLuceneSearcher::search(const QCLuceneQuery &query)
{
    return search(query, QCLuceneFilter());
}

QCLuceneHits QCLuceneSearcher::search(const QCLuceneQuery &query,
                                      const QCLuceneFilter &filter)
{
    return QCLuceneHits(*this, query, filter);
}

QCLuceneHits QCLuceneSearcher::search(const QCLuceneQuery &query,
                                      const QCLuceneSort &sort)
{
    return QCLuceneHits(*this, query, QCLuceneFilter(), sort);
}

QCLuceneHits QCLuceneSearcher::search(const QCLuceneQuery &query,
                                      const QCLuceneFilter &filter,
                                      const QCLuceneSort &sort)
{
    return QCLuceneHits(*this, query, filter, sort);
}


QCLuceneIndexSearcher::QCLuceneIndexSearcher(const QString &path)
    : QCLuceneSearcher()
{
    lucene::search::IndexSearcher *searcher =
        new lucene::search::IndexSearcher(path);

    reader.d->reader = searcher->getReader();
    reader.d->deleteCLuceneIndexReader = false;

    d->searchable = searcher;
}

QCLuceneIndexSearcher::QCLuceneIndexSearcher(const QCLuceneIndexReader &reader)
    : QCLuceneSearcher()
    , reader(reader)
{
    d->searchable = new lucene::search::IndexSearcher(reader.d->reader);
}

QCLuceneIndexSearcher::~QCLuceneIndexSearcher()
{
    // nothing todo
}

void QCLuceneIndexSearcher::close()
{
    d->searchable->close();
}

qint32 QCLuceneIndexSearcher::maxDoc() const
{
    return qint32(d->searchable->maxDoc());
}

QCLuceneIndexReader QCLuceneIndexSearcher::getReader()
{
    return reader;
}

bool QCLuceneIndexSearcher::doc(qint32 i, QCLuceneDocument &document)
{
    return d->searchable->doc(int32_t(i), document.d->document);
}


QCLuceneMultiSearcher::QCLuceneMultiSearcher(const QList<QCLuceneSearchable> searchables)
: QCLuceneSearcher()
{
    lucene::search::Searchable** list=
        _CL_NEWARRAY(lucene::search::Searchable*, searchables.count());

    d->searchable = new lucene::search::MultiSearcher(list);

    _CLDELETE_ARRAY(list);
}

QCLuceneMultiSearcher::~QCLuceneMultiSearcher()
{
    // nothing todo
}

void QCLuceneMultiSearcher::close()
{
    d->searchable->close();
}

qint32 QCLuceneMultiSearcher::maxDoc() const
{
    return qint32(d->searchable->maxDoc());
}

qint32 QCLuceneMultiSearcher::subDoc(qint32 index) const
{
    lucene::search::MultiSearcher *searcher =
        static_cast<lucene::search::MultiSearcher*> (d->searchable);

    if (searcher == 0)
        return 0;

    return qint32(searcher->subDoc(int32_t(index)));
}

qint32 QCLuceneMultiSearcher::subSearcher(qint32 index) const
{
    lucene::search::MultiSearcher *searcher =
        static_cast<lucene::search::MultiSearcher*> (d->searchable);

    if (searcher == 0)
        return 0;

    return qint32(searcher->subSearcher(int32_t(index)));
}

qint32 QCLuceneMultiSearcher::searcherIndex(qint32 index) const
{
    lucene::search::MultiSearcher *searcher =
        static_cast<lucene::search::MultiSearcher*> (d->searchable);

    if (searcher == 0)
        return 0;

    return qint32(searcher->searcherIndex(int32_t(index)));
}

bool QCLuceneMultiSearcher::doc(qint32 i, QCLuceneDocument &document)
{
    return d->searchable->doc(int32_t(i), document.d->document);
}

QT_END_NAMESPACE
