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
#include "brushtool.h"
#include <QtWidgets/QPushButton>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QColorDialog>

BrushTool::BrushTool(QString title, QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(title);
    setWindowFlags(Qt::Tool);

    m_colorButton = new QPushButton();
    m_styleCombo = new QComboBox();
    m_styleCombo->addItem("Nobrush", (int) Qt::NoBrush);
    m_styleCombo->addItem("Solidpattern", (int) Qt::SolidPattern);
    m_styleCombo->addItem("Dense1pattern", (int) Qt::Dense1Pattern);
    m_styleCombo->addItem("Dense2attern", (int) Qt::Dense2Pattern);
    m_styleCombo->addItem("Dense3Pattern", (int) Qt::Dense3Pattern);
    m_styleCombo->addItem("Dense4Pattern", (int) Qt::Dense4Pattern);
    m_styleCombo->addItem("Dense5Pattern", (int) Qt::Dense5Pattern);
    m_styleCombo->addItem("Dense6Pattern", (int) Qt::Dense6Pattern);
    m_styleCombo->addItem("Dense7Pattern", (int) Qt::Dense7Pattern);
    m_styleCombo->addItem("HorPattern", (int) Qt::HorPattern);
    m_styleCombo->addItem("VerPattern", (int) Qt::VerPattern);
    m_styleCombo->addItem("CrossPattern", (int) Qt::CrossPattern);
    m_styleCombo->addItem("BDiagPattern", (int) Qt::BDiagPattern);
    m_styleCombo->addItem("FDiagPattern", (int) Qt::FDiagPattern);
    m_styleCombo->addItem("DiagCrossPattern", (int) Qt::DiagCrossPattern);

    QFormLayout *layout = new QFormLayout();
    layout->addRow("Color", m_colorButton);
    layout->addRow("Style", m_styleCombo);
    setLayout(layout);

    connect(m_colorButton, SIGNAL(clicked()), this, SLOT(showColorDialog()));
    connect(m_styleCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateStyle()));
}

void BrushTool::setBrush(QBrush brush)
{
    m_brush = brush;
    m_colorButton->setText(m_brush.color().name());
    m_styleCombo->setCurrentIndex(m_brush.style()); // index matches the enum
}

QBrush BrushTool::brush() const
{
    return m_brush;
}

QString BrushTool::name()
{
    return name(m_brush);
}

QString BrushTool::name(const QBrush &brush)
{
    return brush.color().name();
}

void BrushTool::showColorDialog()
{
    QColorDialog dialog(m_brush.color());
    dialog.show();
    dialog.exec();
    m_brush.setColor(dialog.selectedColor());
    m_colorButton->setText(m_brush.color().name());
    emit changed();
}

void BrushTool::updateStyle()
{
    Qt::BrushStyle style = (Qt::BrushStyle) m_styleCombo->itemData(m_styleCombo->currentIndex()).toInt();
    if (m_brush.style() != style) {
        m_brush.setStyle(style);
        emit changed();
    }
}

#include "moc_brushtool.cpp"
