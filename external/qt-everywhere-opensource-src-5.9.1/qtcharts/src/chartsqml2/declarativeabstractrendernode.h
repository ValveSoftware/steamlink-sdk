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

#ifndef DECLARATIVEABSTRACTRENDERNODE_H
#define DECLARATIVEABSTRACTRENDERNODE_H

#include <QtCharts/QChartGlobal>
#include <QtQuick/QSGNode>
#include <QtQuick/QQuickWindow>
#include <private/glxyseriesdata_p.h>

QT_CHARTS_BEGIN_NAMESPACE

class MouseEventResponse {
public:
    enum MouseEventType {
        None,
        Pressed,
        Released,
        Clicked,
        DoubleClicked,
        HoverEnter,
        HoverLeave
    };

    MouseEventResponse()
        : type(None),
          series(nullptr) {}
    MouseEventResponse(MouseEventType t, const QPoint &p, const QXYSeries *s)
        : type(t),
          point(p),
          series(s) {}
    MouseEventType type;
    QPoint point;
    const QXYSeries *series;
};

class DeclarativeAbstractRenderNode : public QSGRootNode
{
public:
    DeclarativeAbstractRenderNode() {}

    virtual void setTextureSize(const QSize &textureSize) = 0;
    virtual QSize textureSize() const = 0;
    virtual void setRect(const QRectF &rect) = 0;
    virtual void setSeriesData(bool mapDirty, const GLXYDataMap &dataMap) = 0;
    virtual void setAntialiasing(bool enable) = 0;
    virtual void addMouseEvents(const QVector<QMouseEvent *> &events) = 0;
    virtual void takeMouseEventResponses(QVector<MouseEventResponse> &responses) = 0;
};

QT_CHARTS_END_NAMESPACE


#endif // DECLARATIVEABSTRACTRENDERNODE_H
