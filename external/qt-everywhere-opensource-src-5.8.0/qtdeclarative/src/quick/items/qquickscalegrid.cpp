/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickscalegrid_p_p.h"

#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

/*!
    \internal
    \class QQuickScaleGrid
    \brief The QQuickScaleGrid class allows you to specify a 3x3 grid to use in scaling an image.
*/

QQuickScaleGrid::QQuickScaleGrid(QObject *parent) : QObject(parent), _left(0), _top(0), _right(0), _bottom(0)
{
}

QQuickScaleGrid::~QQuickScaleGrid()
{
}

bool QQuickScaleGrid::isNull() const
{
    return !_left && !_top && !_right && !_bottom;
}

void QQuickScaleGrid::setLeft(int pos)
{
    if (_left != pos) {
        _left = pos;
        emit borderChanged();
    }
}

void QQuickScaleGrid::setTop(int pos)
{
    if (_top != pos) {
        _top = pos;
        emit borderChanged();
    }
}

void QQuickScaleGrid::setRight(int pos)
{
    if (_right != pos) {
        _right = pos;
        emit borderChanged();
    }
}

void QQuickScaleGrid::setBottom(int pos)
{
    if (_bottom != pos) {
        _bottom = pos;
        emit borderChanged();
    }
}

QQuickGridScaledImage::QQuickGridScaledImage()
: _l(-1), _r(-1), _t(-1), _b(-1),
  _h(QQuickBorderImage::Stretch), _v(QQuickBorderImage::Stretch)
{
}

QQuickGridScaledImage::QQuickGridScaledImage(const QQuickGridScaledImage &o)
: _l(o._l), _r(o._r), _t(o._t), _b(o._b), _h(o._h), _v(o._v), _pix(o._pix)
{
}

QQuickGridScaledImage &QQuickGridScaledImage::operator=(const QQuickGridScaledImage &o)
{
    _l = o._l;
    _r = o._r;
    _t = o._t;
    _b = o._b;
    _h = o._h;
    _v = o._v;
    _pix = o._pix;
    return *this;
}

QQuickGridScaledImage::QQuickGridScaledImage(QIODevice *data)
: _l(-1), _r(-1), _t(-1), _b(-1), _h(QQuickBorderImage::Stretch), _v(QQuickBorderImage::Stretch)
{
    int l = -1;
    int r = -1;
    int t = -1;
    int b = -1;
    QString imgFile;

    QByteArray raw;
    while (raw = data->readLine(), !raw.isEmpty()) {
        QString line = QString::fromUtf8(raw.trimmed());
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        int colonId = line.indexOf(QLatin1Char(':'));
        if (colonId <= 0)
            return;

        const QStringRef property = line.leftRef(colonId).trimmed();
        QStringRef value = line.midRef(colonId + 1).trimmed();

        if (property == QLatin1String("border.left")) {
            l = value.toInt();
        } else if (property == QLatin1String("border.right")) {
            r = value.toInt();
        } else if (property == QLatin1String("border.top")) {
            t = value.toInt();
        } else if (property == QLatin1String("border.bottom")) {
            b = value.toInt();
        } else if (property == QLatin1String("source")) {
            if (value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
                value = value.mid(1, value.size() - 2); // remove leading/trailing quotes.
            imgFile = value.toString();
        } else if (property == QLatin1String("horizontalTileRule") || property == QLatin1String("horizontalTileMode")) {
            _h = stringToRule(value);
        } else if (property == QLatin1String("verticalTileRule") || property == QLatin1String("verticalTileMode")) {
            _v = stringToRule(value);
        }
    }

    if (l < 0 || r < 0 || t < 0 || b < 0 || imgFile.isEmpty())
        return;

    _l = l; _r = r; _t = t; _b = b;
    _pix = imgFile;
}

QQuickBorderImage::TileMode QQuickGridScaledImage::stringToRule(const QStringRef &s)
{
    QStringRef string = s;
    if (string.startsWith(QLatin1Char('"')) && string.endsWith(QLatin1Char('"')))
        string = string.mid(1, string.size() - 2); // remove leading/trailing quotes.

    if (string == QLatin1String("Stretch") || string == QLatin1String("BorderImage.Stretch"))
        return QQuickBorderImage::Stretch;
    if (string == QLatin1String("Repeat") || string == QLatin1String("BorderImage.Repeat"))
        return QQuickBorderImage::Repeat;
    if (string == QLatin1String("Round") || string == QLatin1String("BorderImage.Round"))
        return QQuickBorderImage::Round;

    qWarning("QQuickGridScaledImage: Invalid tile rule specified. Using Stretch.");
    return QQuickBorderImage::Stretch;
}

bool QQuickGridScaledImage::isValid() const
{
    return _l >= 0;
}

int QQuickGridScaledImage::gridLeft() const
{
    return _l;
}

int QQuickGridScaledImage::gridRight() const
{
    return _r;
}

int QQuickGridScaledImage::gridTop() const
{
    return _t;
}

int QQuickGridScaledImage::gridBottom() const
{
    return _b;
}

QString QQuickGridScaledImage::pixmapUrl() const
{
    return _pix;
}

QT_END_NAMESPACE
