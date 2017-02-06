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

#include "qindexreader_p.h"
#include "qclucene_global_p.h"

#include <CLucene.h>
#include <CLucene/index/IndexReader.h>

QT_BEGIN_NAMESPACE

QCLuceneIndexReaderPrivate::QCLuceneIndexReaderPrivate()
    : QSharedData()
{
    reader = 0;
    deleteCLuceneIndexReader = true;
}

QCLuceneIndexReaderPrivate::QCLuceneIndexReaderPrivate(const QCLuceneIndexReaderPrivate &other)
    : QSharedData()
{
    reader = _CL_POINTER(other.reader);
    deleteCLuceneIndexReader = other.deleteCLuceneIndexReader;
}

QCLuceneIndexReaderPrivate::~QCLuceneIndexReaderPrivate()
{
    if (deleteCLuceneIndexReader)
        _CLDECDELETE(reader);
}


QCLuceneIndexReader::QCLuceneIndexReader()
    : d(new QCLuceneIndexReaderPrivate())
{
    // nothing todo, private
}

QCLuceneIndexReader::~QCLuceneIndexReader()
{
    // nothing todo
}

bool QCLuceneIndexReader::isLuceneFile(const QString &filename)
{
    using namespace lucene::index;

    return IndexReader::isLuceneFile(filename);
}

bool QCLuceneIndexReader::indexExists(const QString &directory)
{
    using namespace lucene::index;
    return IndexReader::indexExists(directory);
}

QCLuceneIndexReader QCLuceneIndexReader::open(const QString &path)
{
    using namespace lucene::index;

    QCLuceneIndexReader indexReader;
    indexReader.d->reader = IndexReader::open(path);

    return indexReader;
}

void QCLuceneIndexReader::unlock(const QString &path)
{
    using namespace lucene::index;
    IndexReader::unlock(path);
}

bool QCLuceneIndexReader::isLocked(const QString &directory)
{
    using namespace lucene::index;
    return IndexReader::isLocked(directory);
}

quint64 QCLuceneIndexReader::lastModified(const QString &directory)
{
    using namespace lucene::index;
    return quint64(IndexReader::lastModified(directory));
}

qint64 QCLuceneIndexReader::getCurrentVersion(const QString &directory)
{
    using namespace lucene::index;
    return qint64(IndexReader::getCurrentVersion(directory));
}

void QCLuceneIndexReader::close()
{
    d->reader->close();
}

bool QCLuceneIndexReader::isCurrent()
{
    return d->reader->isCurrent();
}

void QCLuceneIndexReader::undeleteAll()
{
    d->reader->undeleteAll();
}

qint64 QCLuceneIndexReader::getVersion()
{
    return qint64(d->reader->getVersion());
}

void QCLuceneIndexReader::deleteDocument(qint32 docNum)
{
    d->reader->deleteDocument(int32_t(docNum));
}

bool QCLuceneIndexReader::hasNorms(const QString &field)
{
    TCHAR *fieldName = QStringToTChar(field);
    bool retValue = d->reader->hasNorms(fieldName);
    delete [] fieldName;

    return retValue;
}

qint32 QCLuceneIndexReader::deleteDocuments(const QCLuceneTerm &term)
{
    return d->reader->deleteDocuments(term.d->term);
}

bool QCLuceneIndexReader::document(qint32 index, QCLuceneDocument &document)
{
    if (!document.d->document)
        document.d->document = new lucene::document::Document();

    if (d->reader->document(int32_t(index), document.d->document))
        return true;

    return false;
}

void QCLuceneIndexReader::setNorm(qint32 doc, const QString &field, qreal value)
{
    TCHAR *fieldName = QStringToTChar(field);
    d->reader->setNorm(int32_t(doc), fieldName, qreal(value));
    delete [] fieldName;
}

void QCLuceneIndexReader::setNorm(qint32 doc, const QString &field, quint8 value)
{
    TCHAR *fieldName = QStringToTChar(field);
    d->reader->setNorm(int32_t(doc), fieldName, uint8_t(value));
    delete [] fieldName;
}

QT_END_NAMESPACE
