/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

        QStringList list;
        list.append(line.left(colonId).trimmed());
        list.append(line.mid(colonId+1).trimmed());

        if (list[0] == QLatin1String("border.left"))
            l = list[1].toInt();
        else if (list[0] == QLatin1String("border.right"))
            r = list[1].toInt();
        else if (list[0] == QLatin1String("border.top"))
            t = list[1].toInt();
        else if (list[0] == QLatin1String("border.bottom"))
            b = list[1].toInt();
        else if (list[0] == QLatin1String("source"))
            imgFile = list[1];
        else if (list[0] == QLatin1String("horizontalTileRule") || list[0] == QLatin1String("horizontalTileMode"))
            _h = stringToRule(list[1]);
        else if (list[0] == QLatin1String("verticalTileRule") || list[0] == QLatin1String("verticalTileMode"))
            _v = stringToRule(list[1]);
    }

    if (l < 0 || r < 0 || t < 0 || b < 0 || imgFile.isEmpty())
        return;

    _l = l; _r = r; _t = t; _b = b;

    _pix = imgFile;
    if (_pix.startsWith(QLatin1Char('"')) && _pix.endsWith(QLatin1Char('"')))
        _pix = _pix.mid(1, _pix.size() - 2); // remove leading/trailing quotes.
}

QQuickBorderImage::TileMode QQuickGridScaledImage::stringToRule(const QString &s)
{
    QString string = s;
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
