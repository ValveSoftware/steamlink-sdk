/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlstringconverters_p.h"
#include <private/qqmlglobal_p.h>

#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qvariant.h>
#include <QtCore/qdatetime.h>

QT_BEGIN_NAMESPACE

QVariant QQmlStringConverters::variantFromString(const QString &s)
{
    if (s.isEmpty())
        return QVariant(s);

    bool ok = false;
    QRectF r = rectFFromString(s, &ok);
    if (ok) return QVariant(r);
    QPointF p = pointFFromString(s, &ok);
    if (ok) return QVariant(p);
    QSizeF sz = sizeFFromString(s, &ok);
    if (ok) return QVariant(sz);

    return QQml_valueTypeProvider()->createVariantFromString(s);
}

QVariant QQmlStringConverters::variantFromString(const QString &s, int preferredType, bool *ok)
{
    switch (preferredType) {
    case QMetaType::Int:
        return QVariant(int(qRound(s.toDouble(ok))));
    case QMetaType::UInt:
        return QVariant(uint(qRound(s.toDouble(ok))));
#ifndef QT_NO_DATESTRING
    case QMetaType::QDate:
        return QVariant::fromValue(dateFromString(s, ok));
    case QMetaType::QTime:
        return QVariant::fromValue(timeFromString(s, ok));
    case QMetaType::QDateTime:
        return QVariant::fromValue(dateTimeFromString(s, ok));
#endif // QT_NO_DATESTRING
    case QMetaType::QPointF:
        return QVariant::fromValue(pointFFromString(s, ok));
    case QMetaType::QPoint:
        return QVariant::fromValue(pointFFromString(s, ok).toPoint());
    case QMetaType::QSizeF:
        return QVariant::fromValue(sizeFFromString(s, ok));
    case QMetaType::QSize:
        return QVariant::fromValue(sizeFFromString(s, ok).toSize());
    case QMetaType::QRectF:
        return QVariant::fromValue(rectFFromString(s, ok));
    case QMetaType::QRect:
        return QVariant::fromValue(rectFFromString(s, ok).toRect());
    default:
        return QQml_valueTypeProvider()->createVariantFromString(preferredType, s, ok);
    }
}

QVariant QQmlStringConverters::colorFromString(const QString &s, bool *ok)
{
    return QQml_colorProvider()->colorFromString(s, ok);
}

unsigned QQmlStringConverters::rgbaFromString(const QString &s, bool *ok)
{
    return QQml_colorProvider()->rgbaFromString(s, ok);
}

#ifndef QT_NO_DATESTRING
QDate QQmlStringConverters::dateFromString(const QString &s, bool *ok)
{
    QDate d = QDate::fromString(s, Qt::ISODate);
    if (ok) *ok =  d.isValid();
    return d;
}

QTime QQmlStringConverters::timeFromString(const QString &s, bool *ok)
{
    QTime t = QTime::fromString(s, Qt::ISODate);
    if (ok) *ok = t.isValid();
    return t;
}

QDateTime QQmlStringConverters::dateTimeFromString(const QString &s, bool *ok)
{
    QDateTime d = QDateTime::fromString(s, Qt::ISODate);
    if (ok) *ok =  d.isValid();
    // V8 never parses a date string as local time.  For consistency do the same here.
    if (d.timeSpec() == Qt::LocalTime)
        d.setTimeSpec(Qt::UTC);
    return d;
}
#endif // QT_NO_DATESTRING

//expects input of "x,y"
QPointF QQmlStringConverters::pointFFromString(const QString &s, bool *ok)
{
    if (s.count(QLatin1Char(',')) != 1) {
        if (ok)
            *ok = false;
        return QPointF();
    }

    bool xGood, yGood;
    int index = s.indexOf(QLatin1Char(','));
    qreal xCoord = s.left(index).toDouble(&xGood);
    qreal yCoord = s.mid(index+1).toDouble(&yGood);
    if (!xGood || !yGood) {
        if (ok)
            *ok = false;
        return QPointF();
    }

    if (ok)
        *ok = true;
    return QPointF(xCoord, yCoord);
}

//expects input of "widthxheight"
QSizeF QQmlStringConverters::sizeFFromString(const QString &s, bool *ok)
{
    if (s.count(QLatin1Char('x')) != 1) {
        if (ok)
            *ok = false;
        return QSizeF();
    }

    bool wGood, hGood;
    int index = s.indexOf(QLatin1Char('x'));
    qreal width = s.left(index).toDouble(&wGood);
    qreal height = s.mid(index+1).toDouble(&hGood);
    if (!wGood || !hGood) {
        if (ok)
            *ok = false;
        return QSizeF();
    }

    if (ok)
        *ok = true;
    return QSizeF(width, height);
}

//expects input of "x,y,widthxheight" //### use space instead of second comma?
QRectF QQmlStringConverters::rectFFromString(const QString &s, bool *ok)
{
    if (s.count(QLatin1Char(',')) != 2 || s.count(QLatin1Char('x')) != 1) {
        if (ok)
            *ok = false;
        return QRectF();
    }

    bool xGood, yGood, wGood, hGood;
    int index = s.indexOf(QLatin1Char(','));
    qreal x = s.left(index).toDouble(&xGood);
    int index2 = s.indexOf(QLatin1Char(','), index+1);
    qreal y = s.mid(index+1, index2-index-1).toDouble(&yGood);
    index = s.indexOf(QLatin1Char('x'), index2+1);
    qreal width = s.mid(index2+1, index-index2-1).toDouble(&wGood);
    qreal height = s.mid(index+1).toDouble(&hGood);
    if (!xGood || !yGood || !wGood || !hGood) {
        if (ok)
            *ok = false;
        return QRectF();
    }

    if (ok)
        *ok = true;
    return QRectF(x, y, width, height);
}

bool QQmlStringConverters::createFromString(int type, const QString &s, void *data, size_t n)
{
    Q_ASSERT(data);

    bool ok = false;

    switch (type) {
    case QMetaType::Int:
        {
        Q_ASSERT(n >= sizeof(int));
        int *p = reinterpret_cast<int *>(data);
        *p = int(qRound(s.toDouble(&ok)));
        return ok;
        }
    case QMetaType::UInt:
        {
        Q_ASSERT(n >= sizeof(uint));
        uint *p = reinterpret_cast<uint *>(data);
        *p = uint(qRound(s.toDouble(&ok)));
        return ok;
        }
#ifndef QT_NO_DATESTRING
    case QMetaType::QDate:
        {
        Q_ASSERT(n >= sizeof(QDate));
        QDate *p = reinterpret_cast<QDate *>(data);
        *p = dateFromString(s, &ok);
        return ok;
        }
    case QMetaType::QTime:
        {
        Q_ASSERT(n >= sizeof(QTime));
        QTime *p = reinterpret_cast<QTime *>(data);
        *p = timeFromString(s, &ok);
        return ok;
        }
    case QMetaType::QDateTime:
        {
        Q_ASSERT(n >= sizeof(QDateTime));
        QDateTime *p = reinterpret_cast<QDateTime *>(data);
        *p = dateTimeFromString(s, &ok);
        return ok;
        }
#endif // QT_NO_DATESTRING
    case QMetaType::QPointF:
        {
        Q_ASSERT(n >= sizeof(QPointF));
        QPointF *p = reinterpret_cast<QPointF *>(data);
        *p = pointFFromString(s, &ok);
        return ok;
        }
    case QMetaType::QPoint:
        {
        Q_ASSERT(n >= sizeof(QPoint));
        QPoint *p = reinterpret_cast<QPoint *>(data);
        *p = pointFFromString(s, &ok).toPoint();
        return ok;
        }
    case QMetaType::QSizeF:
        {
        Q_ASSERT(n >= sizeof(QSizeF));
        QSizeF *p = reinterpret_cast<QSizeF *>(data);
        *p = sizeFFromString(s, &ok);
        return ok;
        }
    case QMetaType::QSize:
        {
        Q_ASSERT(n >= sizeof(QSize));
        QSize *p = reinterpret_cast<QSize *>(data);
        *p = sizeFFromString(s, &ok).toSize();
        return ok;
        }
    case QMetaType::QRectF:
        {
        Q_ASSERT(n >= sizeof(QRectF));
        QRectF *p = reinterpret_cast<QRectF *>(data);
        *p = rectFFromString(s, &ok);
        return ok;
        }
    case QMetaType::QRect:
        {
        Q_ASSERT(n >= sizeof(QRect));
        QRect *p = reinterpret_cast<QRect *>(data);
        *p = rectFFromString(s, &ok).toRect();
        return ok;
        }
    default:
        return QQml_valueTypeProvider()->createValueFromString(type, s, data, n);
    }
}

QT_END_NAMESPACE
