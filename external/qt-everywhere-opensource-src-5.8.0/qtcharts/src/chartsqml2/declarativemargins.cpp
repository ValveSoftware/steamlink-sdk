/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "declarativemargins.h"
#include <QtCore/QDataStream>
#include <QtCore/QDebug>

QT_CHARTS_BEGIN_NAMESPACE

/*!
    \qmltype Margins
    \inqmlmodule QtCharts

    \brief Type is used to define margins.

    Uncreatable type that is used to define top, bottom, left and right margins.
*/

/*!
    \qmlproperty int Margins::top
    The top margin.
*/

/*!
    \qmlproperty int Margins::bottom
    The bottom margin.
*/

/*!
    \qmlproperty int Margins::left
    The left margin.
*/

/*!
    \qmlproperty int Margins::right
    The right margin.
*/

DeclarativeMargins::DeclarativeMargins(QObject *parent) :
    QObject(parent)
{
    QMargins::setTop(0);
    QMargins::setBottom(0);
    QMargins::setLeft(0);
    QMargins::setRight(0);
}

void DeclarativeMargins::setTop(int top)
{
    if (top < 0) {
        qWarning() << "Cannot set top margin to a negative value:" << top;
    } else {
        if (top != QMargins::top()) {
            QMargins::setTop(top);
            emit topChanged(QMargins::top(), QMargins::bottom(), QMargins::left(), QMargins::right());
        }
    }
}

void DeclarativeMargins::setBottom(int bottom)
{
    if (bottom < 0) {
        qWarning() << "Cannot set bottom margin to a negative value:" << bottom;
    } else {
        if (bottom != QMargins::bottom()) {
            QMargins::setBottom(bottom);
            emit bottomChanged(QMargins::top(), QMargins::bottom(), QMargins::left(), QMargins::right());
        }
    }
}

void DeclarativeMargins::setLeft(int left)
{
    if (left < 0) {
        qWarning() << "Cannot set left margin to a negative value:" << left;
    } else {
        if (left != QMargins::left()) {
            QMargins::setLeft(left);
            emit leftChanged(QMargins::top(), QMargins::bottom(), QMargins::left(), QMargins::right());
        }
    }
}

void DeclarativeMargins::setRight(int right)
{
    if (right < 0) {
        qWarning() << "Cannot set left margin to a negative value:" << right;
    } else {
        if (right != QMargins::right()) {
            QMargins::setRight(right);
            emit rightChanged(QMargins::top(), QMargins::bottom(), QMargins::left(), QMargins::right());
        }
    }
}

#include "moc_declarativemargins.cpp"

QT_CHARTS_END_NAMESPACE
