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

#include "qtoken_p.h"
#include "qclucene_global_p.h"

#include <CLucene.h>
#include <CLucene/analysis/AnalysisHeader.h>

QT_BEGIN_NAMESPACE

QCLuceneTokenPrivate::QCLuceneTokenPrivate()
    : QSharedData()
{
    token = 0;
    deleteCLuceneToken = true;
}

QCLuceneTokenPrivate::QCLuceneTokenPrivate(const QCLuceneTokenPrivate &other)
    : QSharedData()
{
    token = _CL_POINTER(other.token);
    deleteCLuceneToken = other.deleteCLuceneToken;
}

QCLuceneTokenPrivate::~QCLuceneTokenPrivate()
{
    if (deleteCLuceneToken)
        _CLDECDELETE(token);
}


QCLuceneToken::QCLuceneToken()
    : d(new QCLuceneTokenPrivate())
    , tokenText(0)
    , tokenType(0)
{
    d->token = new lucene::analysis::Token();
}

QCLuceneToken::QCLuceneToken(const QString &text, qint32 startOffset,
                             qint32 endOffset, const QString &defaultTyp)
    : d(new QCLuceneTokenPrivate())
    , tokenText(QStringToTChar(text))
    , tokenType(QStringToTChar(defaultTyp))
{
    d->token = new lucene::analysis::Token(tokenText, int32_t(startOffset),
        int32_t(endOffset), tokenType);
}

QCLuceneToken::~QCLuceneToken()
{
    delete [] tokenText;
    delete [] tokenType;
}

quint32 QCLuceneToken::bufferLength() const
{
    return quint32(d->token->bufferLength());
}

void QCLuceneToken::growBuffer(quint32 size)
{
    d->token->growBuffer(size_t(size));
}

qint32 QCLuceneToken::positionIncrement() const
{
    return qint32(d->token->getPositionIncrement());
}

void QCLuceneToken::setPositionIncrement(qint32 positionIncrement)
{
    d->token->setPositionIncrement(int32_t(positionIncrement));
}

QString QCLuceneToken::termText() const
{
    return TCharToQString(d->token->termText());
}

void QCLuceneToken::setTermText(const QString &text)
{
    delete [] tokenText;
    tokenText = QStringToTChar(text);
    d->token->setText(tokenText);
}

quint32 QCLuceneToken::termTextLength() const
{
    return quint32(d->token->termTextLength());
}

void QCLuceneToken::resetTermTextLength() const
{
    d->token->resetTermTextLen();
}

qint32 QCLuceneToken::startOffset() const
{
    return quint32(d->token->startOffset());
}

void QCLuceneToken::setStartOffset(qint32 value)
{
    d->token->setStartOffset(int32_t(value));
}

qint32 QCLuceneToken::endOffset() const
{
    return quint32(d->token->endOffset());
}

void QCLuceneToken::setEndOffset(qint32 value)
{
    d->token->setEndOffset(int32_t(value));
}

QString QCLuceneToken::type() const
{
    return TCharToQString(d->token->type());
}

void QCLuceneToken::setType(const QString &type)
{
    delete [] tokenType;
    tokenType = QStringToTChar(type);
    d->token->setType(tokenType);
}

QString QCLuceneToken::toString() const
{
    return TCharToQString(d->token->toString());
}

QT_END_NAMESPACE
