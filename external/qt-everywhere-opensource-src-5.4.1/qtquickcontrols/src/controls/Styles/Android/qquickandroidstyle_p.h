/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#ifndef QQUICKANDROIDSTYLE_P_H
#define QQUICKANDROIDSTYLE_P_H

#include <QtCore/qobject.h>
#include <QtGui/qcolor.h>

QT_BEGIN_NAMESPACE

class QQuickAndroidStyle : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QByteArray data READ data CONSTANT FINAL)
    Q_ENUMS(Gravity)

public:
    QQuickAndroidStyle(QObject *parent = 0);

    QByteArray data() const;

    Q_INVOKABLE QColor colorValue(uint value) const;
    Q_INVOKABLE QString filePath(const QString &fileName) const;

    enum Gravity {
        NO_GRAVITY = 0x0000,
        AXIS_SPECIFIED = 0x0001,
        AXIS_PULL_BEFORE = 0x0002,
        AXIS_PULL_AFTER = 0x0004,
        AXIS_CLIP = 0x0008,
        AXIS_X_SHIFT = 0,
        AXIS_Y_SHIFT = 4,
        TOP = (AXIS_PULL_BEFORE | AXIS_SPECIFIED) << AXIS_Y_SHIFT,
        BOTTOM = (AXIS_PULL_AFTER | AXIS_SPECIFIED) << AXIS_Y_SHIFT,
        LEFT = (AXIS_PULL_BEFORE | AXIS_SPECIFIED) << AXIS_X_SHIFT,
        RIGHT = (AXIS_PULL_AFTER | AXIS_SPECIFIED) << AXIS_X_SHIFT,
        CENTER_VERTICAL = AXIS_SPECIFIED << AXIS_Y_SHIFT,
        FILL_VERTICAL = TOP | BOTTOM,
        CENTER_HORIZONTAL = AXIS_SPECIFIED << AXIS_X_SHIFT,
        FILL_HORIZONTAL = LEFT | RIGHT,
        CENTER = CENTER_VERTICAL | CENTER_HORIZONTAL,
        FILL = FILL_VERTICAL | FILL_HORIZONTAL,
        CLIP_VERTICAL = AXIS_CLIP << AXIS_Y_SHIFT,
        CLIP_HORIZONTAL = AXIS_CLIP << AXIS_X_SHIFT,
        RELATIVE_LAYOUT_DIRECTION = 0x00800000,
        HORIZONTAL_GRAVITY_MASK = (AXIS_SPECIFIED | AXIS_PULL_BEFORE | AXIS_PULL_AFTER) << AXIS_X_SHIFT,
        VERTICAL_GRAVITY_MASK = (AXIS_SPECIFIED | AXIS_PULL_BEFORE | AXIS_PULL_AFTER) << AXIS_Y_SHIFT,
        DISPLAY_CLIP_VERTICAL = 0x10000000,
        DISPLAY_CLIP_HORIZONTAL = 0x01000000,
        START = RELATIVE_LAYOUT_DIRECTION | LEFT,
        END = RELATIVE_LAYOUT_DIRECTION | RIGHT,
        RELATIVE_HORIZONTAL_GRAVITY_MASK = START | END
    };

private:
    QString m_path;
    mutable QByteArray m_data;
};

QT_END_NAMESPACE

#endif // QQUICKANDROIDSTYLE_P_H
