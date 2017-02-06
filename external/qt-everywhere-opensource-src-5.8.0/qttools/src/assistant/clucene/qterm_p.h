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

#ifndef QTERM_P_H
#define QTERM_P_H

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

#include <QtCore/QSharedData>
#include <QtCore/QString>
#include <QtCore/QSharedDataPointer>

CL_NS_DEF(index)
    class Term;
CL_NS_END
CL_NS_USE(index)

QT_BEGIN_NAMESPACE

class QCLuceneTermQuery;
class QCLuceneRangeQuery;
class QCLucenePrefixQuery;
class QCLuceneIndexReader;
class QCLucenePhraseQuery;

class Q_CLUCENE_EXPORT QCLuceneTermPrivate : public QSharedData
{
public:
    QCLuceneTermPrivate();
    QCLuceneTermPrivate(const QCLuceneTermPrivate &other);

    ~QCLuceneTermPrivate();

    Term *term;
    bool deleteCLuceneTerm;

private:
    QCLuceneTermPrivate &operator=(const QCLuceneTermPrivate &other);
};

class Q_CLUCENE_EXPORT QCLuceneTerm
{
public:
    QCLuceneTerm();
    QCLuceneTerm(const QString &field, const QString &text);
    QCLuceneTerm(const QCLuceneTerm &fieldTerm, const QString &text);

    virtual ~QCLuceneTerm();

    QString field() const;
    QString text() const;

    void set(const QString &field, const QString &text);
    void set(const QCLuceneTerm &fieldTerm, const QString &text);
    void set(const QString &field, const QString &text, bool internField);

    bool equals(const QCLuceneTerm &other) const;
    qint32 compareTo(const QCLuceneTerm &other) const;

    QString toString() const;
    quint32 hashCode() const;
    quint32 textLength() const;

protected:
    friend class QCLuceneTermQuery;
    friend class QCLuceneRangeQuery;
    friend class QCLucenePrefixQuery;
    friend class QCLuceneIndexReader;
    friend class QCLucenePhraseQuery;
    QSharedDataPointer<QCLuceneTermPrivate> d;
};

QT_END_NAMESPACE

#endif  // QTERM_P_H
