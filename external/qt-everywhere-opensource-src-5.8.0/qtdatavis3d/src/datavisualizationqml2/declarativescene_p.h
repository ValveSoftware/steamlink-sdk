/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtDataVisualization API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#ifndef DECLARATIVESCENE_P_H
#define DECLARATIVESCENE_P_H

#include "datavisualizationglobal_p.h"
#include "q3dscene.h"

QT_BEGIN_NAMESPACE_DATAVISUALIZATION

class Declarative3DScene : public Q3DScene
{
    Q_OBJECT
    // This property is overloaded to use QPointF instead of QPoint to work around qml bug
    // where Qt.point(0, 0) can't be assigned due to error "Cannot assign QPointF to QPoint".
    Q_PROPERTY(QPointF selectionQueryPosition READ selectionQueryPosition WRITE setSelectionQueryPosition NOTIFY selectionQueryPositionChanged)
    // This is static method in parent class, overload as constant property for qml.
    Q_PROPERTY(QPoint invalidSelectionPoint READ invalidSelectionPoint CONSTANT)

public:
    Declarative3DScene(QObject *parent = 0);
    virtual ~Declarative3DScene();

    void setSelectionQueryPosition(const QPointF &point);
    QPointF selectionQueryPosition() const;
    QPoint invalidSelectionPoint() const;

Q_SIGNALS:
    void selectionQueryPositionChanged(const QPointF position);
};

QT_END_NAMESPACE_DATAVISUALIZATION

#endif
