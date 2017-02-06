/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Templates 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKDRAWER_P_P_H
#define QQUICKDRAWER_P_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qquickdrawer_p.h"
#include "qquickpopup_p_p.h"
#include "qquickvelocitycalculator_p_p.h"

QT_BEGIN_NAMESPACE

class QQuickDrawerPrivate : public QQuickPopupPrivate
{
    Q_DECLARE_PUBLIC(QQuickDrawer)

public:
    QQuickDrawerPrivate();

    static QQuickDrawerPrivate *get(QQuickDrawer *drawer)
    {
        return drawer->d_func();
    }

    qreal positionAt(const QPointF &point) const;
    void reposition() override;
    void resizeOverlay() override;

    bool startDrag(QQuickWindow *window, QMouseEvent *event);
    bool grabMouse(QMouseEvent *event);
    bool ungrabMouse(QMouseEvent *event);

    bool handleMousePressEvent(QQuickItem *item, QMouseEvent *event);
    bool handleMouseMoveEvent(QQuickItem *item, QMouseEvent *event);
    bool handleMouseReleaseEvent(QQuickItem *item, QMouseEvent *event);

    bool prepareEnterTransition() override;
    bool prepareExitTransition() override;

    void setEdge(Qt::Edge edge);

    Qt::Edge edge;
    qreal offset;
    qreal position;
    qreal dragMargin;
    QPointF pressPoint;
    QQuickVelocityCalculator velocityCalculator;
};

QT_END_NAMESPACE

#endif // QQUICKDRAWER_P_P_H
