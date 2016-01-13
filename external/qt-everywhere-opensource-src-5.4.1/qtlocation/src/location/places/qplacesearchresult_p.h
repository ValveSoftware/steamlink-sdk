/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QPLACESEARCHRESULT_P_H
#define QPLACESEARCHRESULT_P_H

#include "qplacesearchresult.h"
#include "qplacesearchrequest.h"

#include <QSharedData>
#include <QtLocation/QPlaceIcon>

QT_BEGIN_NAMESPACE

#define Q_IMPLEMENT_SEARCHRESULT_D_FUNC(Class) \
    Class##Private *Class::d_func() { return reinterpret_cast<Class##Private *>(d_ptr.data()); } \
    const Class##Private *Class::d_func() const { return reinterpret_cast<const Class##Private *>(d_ptr.constData()); } \

#define Q_IMPLEMENT_SEARCHRESULT_COPY_CTOR(Class) \
    Class::Class(const QPlaceSearchResult &other) : QPlaceSearchResult() { Class##Private::copyIfPossible(d_ptr, other); }

#define Q_DEFINE_SEARCHRESULT_PRIVATE_HELPER(Class, ResultType) \
    virtual QPlaceSearchResultPrivate *clone() const { return new Class##Private(*this); } \
    virtual QPlaceSearchResult::SearchResultType type() const {return ResultType;} \
    static void copyIfPossible(QSharedDataPointer<QPlaceSearchResultPrivate> &d_ptr, const QPlaceSearchResult &other) \
    { \
        if (other.type() == ResultType) \
            d_ptr = extract_d(other); \
        else \
            d_ptr = new Class##Private; \
    }

class QPlaceSearchResultPrivate : public QSharedData
{
public:
    QPlaceSearchResultPrivate() {}
    virtual ~QPlaceSearchResultPrivate() {}

    virtual bool compare(const QPlaceSearchResultPrivate *other) const;

    static const QSharedDataPointer<QPlaceSearchResultPrivate>
            &extract_d(const QPlaceSearchResult &other) { return other.d_ptr; }

    Q_DEFINE_SEARCHRESULT_PRIVATE_HELPER(QPlaceSearchResult, QPlaceSearchResult::UnknownSearchResult)

    QString title;
    QPlaceIcon icon;
};

template<> QPlaceSearchResultPrivate *QSharedDataPointer<QPlaceSearchResultPrivate>::clone();

QT_END_NAMESPACE

#endif // QPLACESEARCHRESULT_P_H
