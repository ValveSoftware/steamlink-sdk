/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
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

#ifndef SEGMENTPROPERTIES_H
#define SEGMENTPROPERTIES_H

#include <QWidget>
#include <ui_pane.h>

class SplineEditor;

class SegmentProperties : public QWidget
{
    Q_OBJECT
public:
    explicit SegmentProperties(QWidget *parent = 0);
    void setSplineEditor(SplineEditor *splineEditor)
    {
        m_splineEditor = splineEditor;
    }

    void setSegment(int segment, QVector<QPointF> points, bool smooth, bool last)
    {
        m_segment = segment;
        m_points = points;
        m_smooth = smooth;
        m_last = last;
        invalidate();
    }

signals:

public slots:

private slots:
    void c1Updated();
    void c2Updated();
    void pUpdated();

private:
    void invalidate();

    Ui_Pane m_ui_pane_c1;
    Ui_Pane m_ui_pane_c2;
    Ui_Pane m_ui_pane_p;

    SplineEditor *m_splineEditor;
    QVector<QPointF> m_points;
    int m_segment;
    bool m_smooth;
    bool m_last;

    bool m_blockSignals;
};

#endif // SEGMENTPROPERTIES_H
