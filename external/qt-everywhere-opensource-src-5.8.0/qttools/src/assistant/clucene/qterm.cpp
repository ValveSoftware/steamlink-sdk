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

#include "qterm_p.h"
#include "qclucene_global_p.h"

#include <CLucene.h>
#include <CLucene/index/IndexReader.h>

QT_BEGIN_NAMESPACE

QCLuceneTermPrivate::QCLuceneTermPrivate()
    : QSharedData()
{
    term = 0;
    deleteCLuceneTerm = true;
}

QCLuceneTermPrivate::QCLuceneTermPrivate(const QCLuceneTermPrivate &other)
    : QSharedData()
{
    term = _CL_POINTER(other.term);
    deleteCLuceneTerm = other.deleteCLuceneTerm;
}

QCLuceneTermPrivate::~QCLuceneTermPrivate()
{
    if (deleteCLuceneTerm)
        _CLDECDELETE(term);
}


QCLuceneTerm::QCLuceneTerm()
    : d(new QCLuceneTermPrivate())
{
    d->term = new lucene::index::Term();
}

QCLuceneTerm::QCLuceneTerm(const QString &field, const QString &text)
    : d(new QCLuceneTermPrivate())
{
    TCHAR *fieldName = QStringToTChar(field);
    TCHAR *termText = QStringToTChar(text);

    d->term = new lucene::index::Term(fieldName, termText);

    delete [] fieldName;
    delete [] termText;
}

QCLuceneTerm::QCLuceneTerm(const QCLuceneTerm &fieldTerm, const QString &text)
    : d(new QCLuceneTermPrivate())
{
    TCHAR *termText = QStringToTChar(text);
    d->term = new lucene::index::Term(fieldTerm.d->term, termText);
    delete [] termText;
}

QCLuceneTerm::~QCLuceneTerm()
{
    // nothing todo
}

QString QCLuceneTerm::field() const
{
    return TCharToQString(d->term->field());
}

QString QCLuceneTerm::text() const
{
    return TCharToQString(d->term->text());
}

void QCLuceneTerm::set(const QString &field, const QString &text)
{
    set(field, text, true);
}

void QCLuceneTerm::set(const QCLuceneTerm &fieldTerm, const QString &text)
{
    set(fieldTerm.field(), text, false);
}

void QCLuceneTerm::set(const QString &field, const QString &text, bool internField)
{
    TCHAR *fieldName = QStringToTChar(field);
    TCHAR *termText = QStringToTChar(text);

    d->term->set(fieldName, termText, internField);

    delete [] fieldName;
    delete [] termText;
}

bool QCLuceneTerm::equals(const QCLuceneTerm &other) const
{
    return d->term->equals(other.d->term);
}

qint32 QCLuceneTerm::compareTo(const QCLuceneTerm &other) const
{
    return quint32(d->term->compareTo(other.d->term));
}

QString QCLuceneTerm::toString() const
{
    return TCharToQString(d->term->toString());
}

quint32 QCLuceneTerm::hashCode() const
{
    return quint32(d->term->hashCode());
}

quint32 QCLuceneTerm::textLength() const
{
    return quint32(d->term->textLength());
}

QT_END_NAMESPACE
