/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Extras module of the Qt Toolkit.
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

#include "qquickpicture_p.h"

#include <QQmlFile>
#include <QDebug>

/*!
    \qmltype Picture
    \inherits QQuickPaintedItem
    \inqmlmodule QtQuick.Extras
    \since QtQuick.Extras 1.4
    \ingroup extras
    \ingroup extras-non-interactive
    \brief An indicator that displays a colorized QPicture icon

    Picture displays icons in a scalable vector format. It can also colorize
    the icons via the \l color property.

    The icon to display is set with the \l source property.

    For example, if you have access to the ISO 7000 icons that come with Qt
    Enterprise, you can specify the following URL:

    \code
    "qrc:/iso-icons/iso_grs_7000_4_0001.dat"
    \endcode

    Due to the
    \l {http://www.iso.org/iso/home/store/catalogue_tc/catalogue_detail.htm?csnumber=65977}
    {large selection of icons} available in this package, it is advisable to
    use Qt Creator's Qt Quick Designer tool to browse and select icons, as this
    property will then be set automatically.
*/

QQuickPicture::QQuickPicture(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    const qreal defaultFontHeight = QFontMetricsF(QFont()).height();
    setImplicitWidth(defaultFontHeight * 4);
    setImplicitHeight(defaultFontHeight * 4);
}

QQuickPicture::~QQuickPicture()
{
}

void QQuickPicture::paint(QPainter *painter)
{
    const QSize size = boundingRect().size().toSize();
    // Don't want the scale to apply to the fill.
    painter->save();
    painter->scale(qreal(size.width()) / mPicture.boundingRect().width(),
        qreal(size.height()) / mPicture.boundingRect().height());
    painter->drawPicture(0, 0, mPicture);
    painter->restore();

    if (mColor.isValid()) {
        painter->setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter->fillRect(0, 0, size.width(), size.height(), mColor);
    }
}

/*!
    \qmlproperty url Picture::source

    This property specifies the URL of the icon to use. The URL must point to a
    local file that contains \l QPicture data. For example:

    \code
    "mypicture.dat"
    \endcode
*/
QUrl QQuickPicture::source() const
{
    return mSource;
}

void QQuickPicture::setSource(const QUrl &source)
{
    if (mSource != source) {
        mSource = source;
        const QString fileName = QQmlFile::urlToLocalFileOrQrc(source);
        if (!mPicture.load(fileName)) {
            qWarning().nospace() << "Failed to load " << fileName << "; does it exist?";
            mPicture = QPicture();
        }

        setImplicitWidth(mPicture.boundingRect().width());
        setImplicitHeight(mPicture.boundingRect().height());

        update();
        emit sourceChanged();
    }
}

/*!
    \qmlproperty color Picture::color

    This property specifies the color of the indicator.

    The default value is \c "black".
*/
QColor QQuickPicture::color() const
{
    return mColor;
}

void QQuickPicture::setColor(const QColor &color)
{
    if (mColor != color) {
        mColor = color;
        update();
        emit colorChanged();
    }
}

void QQuickPicture::resetColor()
{
    setColor(QColor());
}

