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

#include "qquickpathinterpolator_p.h"

#include "qquickpath_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype PathInterpolator
    \instantiates QQuickPathInterpolator
    \inqmlmodule QtQuick
    \ingroup qtquick-animation-control
    \brief Specifies how to manually animate along a path

    PathInterpolator provides \c x, \c y, and \c angle information for a particular \c progress
    along a path.

    In the following example, we animate a green rectangle along a bezier path.

    \snippet qml/pathinterpolator.qml 0
*/

QQuickPathInterpolator::QQuickPathInterpolator(QObject *parent) :
    QObject(parent), _path(0), _x(0), _y(0), _angle(0), _progress(0)
{
}

/*!
    \qmlproperty Path QtQuick::PathInterpolator::path
    This property holds the path to interpolate.

    For more information on defining a path see the \l Path documentation.
*/
QQuickPath *QQuickPathInterpolator::path() const
{
    return _path;
}

void QQuickPathInterpolator::setPath(QQuickPath *path)
{
    if (_path == path)
        return;
    if (_path)
        disconnect(_path, SIGNAL(changed()), this, SLOT(_q_pathUpdated()));
    _path = path;
    connect(_path, SIGNAL(changed()), this, SLOT(_q_pathUpdated()));
    emit pathChanged();
}

/*!
    \qmlproperty real QtQuick::PathInterpolator::progress
    This property holds the current progress along the path.

    Typical usage of PathInterpolator is to set the progress
    (often via a NumberAnimation), and read the corresponding
    values for x, y, and angle (often via bindings to these values).

    Progress ranges from 0.0 to 1.0.
*/
qreal QQuickPathInterpolator::progress() const
{
    return _progress;
}

void QQuickPathInterpolator::setProgress(qreal progress)
{
    progress = qMin(qMax(progress, (qreal)0.0), (qreal)1.0);

    if (progress == _progress)
        return;
    _progress = progress;
    emit progressChanged();
    _q_pathUpdated();
}

/*!
    \qmlproperty real QtQuick::PathInterpolator::x
    \qmlproperty real QtQuick::PathInterpolator::y

    These properties hold the position of the path at \l progress.
*/
qreal QQuickPathInterpolator::x() const
{
    return _x;
}

qreal QQuickPathInterpolator::y() const
{
    return _y;
}

/*!
    \qmlproperty real QtQuick::PathInterpolator::angle

    This property holds the angle of the path tangent at \l progress.

    Angles are reported clockwise, with zero degrees at the 3 o'clock position.
*/
qreal QQuickPathInterpolator::angle() const
{
    return _angle;
}

void QQuickPathInterpolator::_q_pathUpdated()
{
    if (! _path)
        return;

    qreal angle = 0;
    const QPointF pt = _path->sequentialPointAt(_progress, &angle);

    if (_x != pt.x()) {
        _x = pt.x();
        emit xChanged();
    }

    if (_y != pt.y()) {
        _y = pt.y();
        emit yChanged();
    }

    //convert to clockwise
    angle = qreal(360) - angle;
    if (qFuzzyCompare(angle, qreal(360)))
        angle = qreal(0);

    if (angle != _angle) {
        _angle = angle;
        emit angleChanged();
    }
}

QT_END_NAMESPACE
