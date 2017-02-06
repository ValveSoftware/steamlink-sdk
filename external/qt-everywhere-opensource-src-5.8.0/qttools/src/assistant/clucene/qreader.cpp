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

#include "qreader_p.h"
#include "qclucene_global_p.h"

#include <CLucene.h>
#include <CLucene/util/Reader.h>

QT_BEGIN_NAMESPACE

QCLuceneReaderPrivate::QCLuceneReaderPrivate()
    : QSharedData()
{
    reader = 0;
    deleteCLuceneReader = true;
}

QCLuceneReaderPrivate::QCLuceneReaderPrivate(const QCLuceneReaderPrivate &other)
    : QSharedData()
{
    reader = _CL_POINTER(other.reader);
    deleteCLuceneReader = other.deleteCLuceneReader;
}

QCLuceneReaderPrivate::~QCLuceneReaderPrivate()
{
    if (deleteCLuceneReader)
        _CLDECDELETE(reader);
}

QCLuceneReader::QCLuceneReader()
    : d(new QCLuceneReaderPrivate())
{
    // nothing todo
}

QCLuceneReader::~QCLuceneReader()
{
    // nothing todo
}


QCLuceneStringReader::QCLuceneStringReader(const QString &value)
    : QCLuceneReader()
    , string(QStringToTChar(value))
{
    d->reader = new lucene::util::StringReader(string);
}

QCLuceneStringReader::QCLuceneStringReader(const QString &value, qint32 length)
    : QCLuceneReader()
    , string(QStringToTChar(value))
{
    d->reader = new lucene::util::StringReader(string, int32_t(length));
}

QCLuceneStringReader::QCLuceneStringReader(const QString &value, qint32 length,
                                           bool copyData)
    : QCLuceneReader()
    , string(QStringToTChar(value))
{
    d->reader = new lucene::util::StringReader(string, int32_t(length), copyData);
}

QCLuceneStringReader::~QCLuceneStringReader()
{
    delete [] string;
}


QCLuceneFileReader::QCLuceneFileReader(const QString &path, const QString &encoding,
                                       qint32 cacheLength, qint32 cacheBuffer)
    : QCLuceneReader()
{
    const QByteArray tmpPath = path.toLocal8Bit();
    const QByteArray tmpEncoding = encoding.toLatin1();
    d->reader = new lucene::util::FileReader(tmpPath.constData(),
        tmpEncoding.constData(), int32_t(cacheLength), int32_t(cacheBuffer));
}

QCLuceneFileReader::~QCLuceneFileReader()
{
    // nothing todo
}

QT_END_NAMESPACE
