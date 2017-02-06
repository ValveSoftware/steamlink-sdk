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

#include "qquicktextmetrics_p.h"

#include <QFont>

QT_BEGIN_NAMESPACE

/*!
    \qmltype TextMetrics
    \instantiates QQuickTextMetrics
    \inqmlmodule QtQuick
    \since 5.4
    \ingroup qtquick-text-utility
    \brief Provides metrics for a given font and text

    TextMetrics calculates various properties of a given string of text for a
    particular font.

    It provides a declarative API for the functions in \l QFontMetricsF which
    take arguments.

    \code
    TextMetrics {
        id: textMetrics
        font.family: "Arial"
        elide: Text.ElideMiddle
        elideWidth: 100
        text: "Hello World"
    }

    MyItem {
        text: textMetrics.elidedText
    }
    \endcode

    \sa QFontMetricsF, FontMetrics
*/
QQuickTextMetrics::QQuickTextMetrics(QObject *parent) :
    QObject(parent),
    m_metrics(m_font),
    m_elide(Qt::ElideNone),
    m_elideWidth(0)
{
}

QQuickTextMetrics::~QQuickTextMetrics()
{
}

/*!
    \qmlproperty font QtQuick::TextMetrics::font

    This property holds the font used for the metrics calculations.
*/
QFont QQuickTextMetrics::font() const
{
    return m_font;
}

void QQuickTextMetrics::setFont(const QFont &font)
{
    if (m_font != font) {
        m_font = font;
        m_metrics = QFontMetricsF(m_font);
        emit fontChanged();
        emit metricsChanged();
    }
}

/*!
    \qmlproperty string QtQuick::TextMetrics::text

    This property holds the text used for the metrics calculations.
*/
QString QQuickTextMetrics::text() const
{
    return m_text;
}

void QQuickTextMetrics::setText(const QString &text)
{
    if (m_text != text) {
        m_text = text;
        emit textChanged();
        emit metricsChanged();
    }
}

/*!
    \qmlproperty enumeration QtQuick::TextMetrics::elide

    This property holds the elide mode of the text. This determines the
    position in which the string is elided. The possible values are:

    \list
        \li \c Qt::ElideNone - No eliding; this is the default value.
        \li \c Qt::ElideLeft - For example: "...World"
        \li \c Qt::ElideMiddle - For example: "He...ld"
        \li \c Qt::ElideRight - For example: "Hello..."
    \endlist

    \sa elideWidth, QFontMetrics::elidedText
*/
Qt::TextElideMode QQuickTextMetrics::elide() const
{
    return m_elide;
}

void QQuickTextMetrics::setElide(Qt::TextElideMode elide)
{
    if (m_elide != elide) {
        m_elide = elide;
        emit elideChanged();
        emit metricsChanged();
    }
}

/*!
    \qmlproperty real QtQuick::TextMetrics::elideWidth

    This property holds the largest width the text can have (in pixels) before
    eliding will occur.

    \sa elide, QFontMetrics::elidedText
*/
qreal QQuickTextMetrics::elideWidth() const
{
    return m_elideWidth;
}

void QQuickTextMetrics::setElideWidth(qreal elideWidth)
{
    if (m_elideWidth != elideWidth) {
        m_elideWidth = elideWidth;
        emit elideWidthChanged();
        emit metricsChanged();
    }
}

/*!
    \qmlproperty real QtQuick::TextMetrics::advanceWidth

    This property holds the advance in pixels of the characters in \l text.
    This is the distance from the position of the string to where the next
    string should be drawn.

    \sa {QFontMetricsF::width()}
*/
qreal QQuickTextMetrics::advanceWidth() const
{
    return m_metrics.width(m_text);
}

/*!
    \qmlproperty rect QtQuick::TextMetrics::boundingRect

    This property holds the bounding rectangle of the characters in the string
    specified by \l text.

    \sa {QFontMetricsF::boundingRect()}, tightBoundingRect
*/
QRectF QQuickTextMetrics::boundingRect() const
{
    return m_metrics.boundingRect(m_text);
}

/*!
    \qmlproperty real QtQuick::TextMetrics::width

    This property holds the width of the bounding rectangle of the characters
    in the string specified by \l text. It is equivalent to:

    \code
    textMetrics.boundingRect.width
    \endcode

    \sa boundingRect
*/
qreal QQuickTextMetrics::width() const
{
    return boundingRect().width();
}

/*!
    \qmlproperty real QtQuick::TextMetrics::height

    This property holds the height of the bounding rectangle of the characters
    in the string specified by \l text. It is equivalent to:

    \code
    textMetrics.boundingRect.height
    \endcode

    \sa boundingRect
*/
qreal QQuickTextMetrics::height() const
{
    return boundingRect().height();
}

/*!
    \qmlproperty rect QtQuick::TextMetrics::tightBoundingRect

    This property holds a tight bounding rectangle around the characters in the
    string specified by \l text.

    \sa {QFontMetricsF::tightBoundingRect()}, boundingRect
*/
QRectF QQuickTextMetrics::tightBoundingRect() const
{
    return m_metrics.tightBoundingRect(m_text);
}

/*!
    \qmlproperty string QtQuick::TextMetrics::elidedText

    This property holds an elided version of the string (i.e., a string with
    "..." in it) if the string \l text is wider than \l elideWidth. If the
    text is not wider than \l elideWidth, or \l elide is set to
    \c Qt::ElideNone, this property will be equal to the original string.

    \sa {QFontMetricsF::elidedText()}
*/
QString QQuickTextMetrics::elidedText() const
{
    return m_metrics.elidedText(m_text, m_elide, m_elideWidth);
}

QT_END_NAMESPACE
