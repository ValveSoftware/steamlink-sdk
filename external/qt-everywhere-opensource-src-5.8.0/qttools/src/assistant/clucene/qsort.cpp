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

#include "qsort_p.h"
#include "qclucene_global_p.h"

#include <CLucene.h>
#include <CLucene/search/Sort.h>

QT_BEGIN_NAMESPACE

QCLuceneSortPrivate::QCLuceneSortPrivate()
    : QSharedData()
{
    sort = 0;
    deleteCLuceneSort = true;
}

QCLuceneSortPrivate::QCLuceneSortPrivate (const QCLuceneSortPrivate &other)
    : QSharedData()
{
    sort = _CL_POINTER(other.sort);
    deleteCLuceneSort = other.deleteCLuceneSort;
}

QCLuceneSortPrivate::~QCLuceneSortPrivate()
{
    if (deleteCLuceneSort)
        _CLDECDELETE(sort);
}


QCLuceneSort::QCLuceneSort()
    : d(new QCLuceneSortPrivate())
{
    d->sort = new lucene::search::Sort();
}

QCLuceneSort::QCLuceneSort(const QStringList &fieldNames)
    : d(new QCLuceneSortPrivate())
{
    d->sort = new lucene::search::Sort();
    setSort(fieldNames);
}

QCLuceneSort::QCLuceneSort(const QString &field, bool reverse)
    : d(new QCLuceneSortPrivate())
{
    d->sort = new lucene::search::Sort();
    setSort(field, reverse);
}

QCLuceneSort::~QCLuceneSort()
{
    // nothing todo
}

QString QCLuceneSort::toString() const
{
    return TCharToQString(d->sort->toString());
}

void QCLuceneSort::setSort(const QStringList &fieldNames)
{
    TCHAR **nameArray = new TCHAR*[fieldNames.count()];
    for (int i = 0; i < fieldNames.count(); ++i)
        nameArray[i] = QStringToTChar(fieldNames.at(i));

    d->sort->setSort((const TCHAR**)nameArray);

    for (int i = 0; i < fieldNames.count(); ++i)
        delete [] nameArray[i];
    delete [] nameArray;
}

void QCLuceneSort::setSort(const QString &field, bool reverse)
{
    TCHAR *name = QStringToTChar(field);
    d->sort->setSort(name, reverse);
    delete [] name;
}

QT_END_NAMESPACE
