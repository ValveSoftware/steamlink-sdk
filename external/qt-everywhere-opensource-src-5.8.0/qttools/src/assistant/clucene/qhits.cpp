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

#include "qhits_p.h"
#include "qsearchable_p.h"

#include <CLucene.h>
#include <CLucene/search/SearchHeader.h>

QT_BEGIN_NAMESPACE

QCLuceneHitsPrivate::QCLuceneHitsPrivate()
    : QSharedData()
{
    hits = 0;
    deleteCLuceneHits = true;
}

QCLuceneHitsPrivate::QCLuceneHitsPrivate(const QCLuceneHitsPrivate &other)
    : QSharedData()
{
    hits = _CL_POINTER(other.hits);
    deleteCLuceneHits = other.deleteCLuceneHits;
}

QCLuceneHitsPrivate::~QCLuceneHitsPrivate()
{
    if (deleteCLuceneHits)
        _CLDECDELETE(hits);
}


QCLuceneHits::QCLuceneHits(const QCLuceneSearcher &searcher,
                           const QCLuceneQuery &query, const QCLuceneFilter &filter)
    : d(new QCLuceneHitsPrivate())
{
    d->hits = new lucene::search::Hits(searcher.d->searchable, query.d->query,
        filter.d->filter);
}

QCLuceneHits::QCLuceneHits(const QCLuceneSearcher &searcher, const QCLuceneQuery &query,
                         const QCLuceneFilter &filter, const QCLuceneSort &sort)
    : d(new QCLuceneHitsPrivate())
{
    d->hits = new lucene::search::Hits(searcher.d->searchable, query.d->query,
        filter.d->filter, sort.d->sort);
}

QCLuceneHits::~QCLuceneHits()
{
    // nothing todo
}

QCLuceneDocument QCLuceneHits::document(const qint32 index)
{
    // TODO: check this
    QCLuceneDocument document;
    document.d->deleteCLuceneDocument = false;
    lucene::document::Document &doc = d->hits->doc(int32_t(index));
    document.d->document = &doc;

    return document;
}

qint32 QCLuceneHits::length() const
{
    return qint32(d->hits->length());
}

qint32 QCLuceneHits::id(const qint32 index)
{
    return qint32(d->hits->id(int32_t(index)));
}

qreal QCLuceneHits::score(const qint32 index)
{
    return qreal(d->hits->score(int32_t(index)));
}

QT_END_NAMESPACE
