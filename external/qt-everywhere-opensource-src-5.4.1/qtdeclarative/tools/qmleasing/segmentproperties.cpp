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

#include "segmentproperties.h"
#include "splineeditor.h"

SegmentProperties::SegmentProperties(QWidget *parent) :
    QWidget(parent), m_splineEditor(0), m_blockSignals(false)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    layout->setSpacing(2);
    setLayout(layout);
    {
        QWidget *widget = new QWidget(this);
        m_ui_pane_c1.setupUi(widget);
        m_ui_pane_c1.label->setText("c1");
        m_ui_pane_c1.smooth->setVisible(false);
        layout->addWidget(widget);

        connect(m_ui_pane_c1.p1_x, SIGNAL(valueChanged(double)), this, SLOT(c1Updated()));
        connect(m_ui_pane_c1.p1_y, SIGNAL(valueChanged(double)), this, SLOT(c1Updated()));
    }
    {
        QWidget *widget = new QWidget(this);
        m_ui_pane_c2.setupUi(widget);
        m_ui_pane_c2.label->setText("c2");
        m_ui_pane_c2.smooth->setVisible(false);
        layout->addWidget(widget);

        connect(m_ui_pane_c2.p1_x, SIGNAL(valueChanged(double)), this, SLOT(c2Updated()));
        connect(m_ui_pane_c2.p1_y, SIGNAL(valueChanged(double)), this, SLOT(c2Updated()));
    }
    {
        QWidget *widget = new QWidget(this);
        m_ui_pane_p.setupUi(widget);
        m_ui_pane_p.label->setText("p1");
        layout->addWidget(widget);

        connect(m_ui_pane_p.smooth, SIGNAL(toggled(bool)), this, SLOT(pUpdated()));
        connect(m_ui_pane_p.p1_x, SIGNAL(valueChanged(double)), this, SLOT(pUpdated()));
        connect(m_ui_pane_p.p1_y, SIGNAL(valueChanged(double)), this, SLOT(pUpdated()));
    }
}

void SegmentProperties::c1Updated()
{
    if (m_splineEditor && !m_blockSignals) {
        QPointF c1(m_ui_pane_c1.p1_x->value(), m_ui_pane_c1.p1_y->value());
        m_splineEditor->setControlPoint(m_segment * 3, c1);
    }
}

void SegmentProperties::c2Updated()
{
    if (m_splineEditor && !m_blockSignals) {
        QPointF c2(m_ui_pane_c2.p1_x->value(), m_ui_pane_c2.p1_y->value());
        m_splineEditor->setControlPoint(m_segment * 3 + 1, c2);
    }
}

void SegmentProperties::pUpdated()
{
    if (m_splineEditor && !m_blockSignals) {
        QPointF p(m_ui_pane_p.p1_x->value(), m_ui_pane_p.p1_y->value());
        bool smooth = m_ui_pane_p.smooth->isChecked();
        m_splineEditor->setControlPoint(m_segment * 3 + 2, p);
        m_splineEditor->setSmooth(m_segment, smooth);
    }
}

void SegmentProperties::invalidate()
{
    m_blockSignals = true;

     m_ui_pane_p.label->setText(QLatin1String("p") + QString::number(m_segment));
     m_ui_pane_p.smooth->setChecked(m_smooth);
     m_ui_pane_p.smooth->parentWidget()->setEnabled(!m_last);

     m_ui_pane_c1.p1_x->setValue(m_points.at(0).x());
     m_ui_pane_c1.p1_y->setValue(m_points.at(0).y());

     m_ui_pane_c2.p1_x->setValue(m_points.at(1).x());
     m_ui_pane_c2.p1_y->setValue(m_points.at(1).y());

     m_ui_pane_p.p1_x->setValue(m_points.at(2).x());
     m_ui_pane_p.p1_y->setValue(m_points.at(2).y());

     m_blockSignals = false;
}
