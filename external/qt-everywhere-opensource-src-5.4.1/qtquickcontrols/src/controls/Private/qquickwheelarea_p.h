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

#ifndef QQUICKWHEELAREA_P_H
#define QQUICKWHEELAREA_P_H

#include <QtGui/qevent.h>
#include <QtQuick/qquickitem.h>

QT_BEGIN_NAMESPACE

class QQuickWheelArea : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(qreal verticalDelta READ verticalDelta WRITE setVerticalDelta NOTIFY verticalWheelMoved)
    Q_PROPERTY(qreal horizontalDelta READ horizontalDelta WRITE setHorizontalDelta NOTIFY horizontalWheelMoved)
    Q_PROPERTY(qreal horizontalMinimumValue READ horizontalMinimumValue WRITE setHorizontalMinimumValue)
    Q_PROPERTY(qreal horizontalMaximumValue READ horizontalMaximumValue WRITE setHorizontalMaximumValue)
    Q_PROPERTY(qreal verticalMinimumValue READ verticalMinimumValue WRITE setVerticalMinimumValue)
    Q_PROPERTY(qreal verticalMaximumValue READ verticalMaximumValue WRITE setVerticalMaximumValue)
    Q_PROPERTY(qreal horizontalValue READ horizontalValue WRITE setHorizontalValue)
    Q_PROPERTY(qreal verticalValue READ verticalValue WRITE setVerticalValue)
    Q_PROPERTY(qreal scrollSpeed READ scrollSpeed WRITE setScrollSpeed NOTIFY scrollSpeedChanged)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)

public:
    QQuickWheelArea(QQuickItem *parent = 0);
    virtual ~QQuickWheelArea();

    void setHorizontalMinimumValue(qreal value);
    qreal horizontalMinimumValue() const;

    void setHorizontalMaximumValue(qreal value);
    qreal horizontalMaximumValue() const;

    void setVerticalMinimumValue(qreal value);
    qreal verticalMinimumValue() const;

    void setVerticalMaximumValue(qreal value);
    qreal verticalMaximumValue() const;

    void setHorizontalValue(qreal value);
    qreal horizontalValue() const;

    void setVerticalValue(qreal value);
    qreal verticalValue() const;

    void setVerticalDelta(qreal value);
    qreal verticalDelta() const;

    void setHorizontalDelta(qreal value);
    qreal horizontalDelta() const;

    void setScrollSpeed(qreal value);
    qreal scrollSpeed() const;

    bool isActive() const;
    void setActive(bool active);

    void wheelEvent(QWheelEvent *event);

    bool isAtXEnd() const;
    bool isAtXBeginning() const;
    bool isAtYEnd() const;
    bool isAtYBeginning() const;

Q_SIGNALS:
    void verticalValueChanged();
    void horizontalValueChanged();
    void verticalWheelMoved();
    void horizontalWheelMoved();
    void scrollSpeedChanged();
    void activeChanged();

private:
    qreal m_horizontalMinimumValue;
    qreal m_horizontalMaximumValue;
    qreal m_verticalMinimumValue;
    qreal m_verticalMaximumValue;
    qreal m_horizontalValue;
    qreal m_verticalValue;
    qreal m_verticalDelta;
    qreal m_horizontalDelta;
    qreal m_scrollSpeed;
    bool m_active;

    Q_DISABLE_COPY(QQuickWheelArea)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickWheelArea)

#endif // QQUICKWHEELAREA_P_H
