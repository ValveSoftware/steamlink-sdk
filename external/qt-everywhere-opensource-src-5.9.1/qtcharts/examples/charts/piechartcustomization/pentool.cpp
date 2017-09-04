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

#include "pentool.h"
#include <QtWidgets/QPushButton>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QColorDialog>

PenTool::PenTool(QString title, QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(title);
    setWindowFlags(Qt::Tool);

    m_colorButton = new QPushButton();

    m_widthSpinBox = new QDoubleSpinBox();

    m_styleCombo = new QComboBox();
    m_styleCombo->addItem("NoPen");
    m_styleCombo->addItem("SolidLine");
    m_styleCombo->addItem("DashLine");
    m_styleCombo->addItem("DotLine");
    m_styleCombo->addItem("DashDotLine");
    m_styleCombo->addItem("DashDotDotLine");

    m_capStyleCombo = new QComboBox();
    m_capStyleCombo->addItem("FlatCap", Qt::FlatCap);
    m_capStyleCombo->addItem("SquareCap", Qt::SquareCap);
    m_capStyleCombo->addItem("RoundCap", Qt::RoundCap);

    m_joinStyleCombo = new QComboBox();
    m_joinStyleCombo->addItem("MiterJoin", Qt::MiterJoin);
    m_joinStyleCombo->addItem("BevelJoin", Qt::BevelJoin);
    m_joinStyleCombo->addItem("RoundJoin", Qt::RoundJoin);
    m_joinStyleCombo->addItem("SvgMiterJoin", Qt::SvgMiterJoin);

    QFormLayout *layout = new QFormLayout();
    layout->addRow("Color", m_colorButton);
    layout->addRow("Width", m_widthSpinBox);
    layout->addRow("Style", m_styleCombo);
    layout->addRow("Cap style", m_capStyleCombo);
    layout->addRow("Join style", m_joinStyleCombo);
    setLayout(layout);

    connect(m_colorButton, SIGNAL(clicked()), this, SLOT(showColorDialog()));
    connect(m_widthSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateWidth(double)));
    connect(m_styleCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateStyle(int)));
    connect(m_capStyleCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateCapStyle(int)));
    connect(m_joinStyleCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateJoinStyle(int)));
}

void PenTool::setPen(const QPen &pen)
{
    m_pen = pen;
    m_colorButton->setText(m_pen.color().name());
    m_widthSpinBox->setValue(m_pen.widthF());
    m_styleCombo->setCurrentIndex(m_pen.style()); // index matches the enum
    m_capStyleCombo->setCurrentIndex(m_capStyleCombo->findData(m_pen.capStyle()));
    m_joinStyleCombo->setCurrentIndex(m_joinStyleCombo->findData(m_pen.joinStyle()));
}

QPen PenTool::pen() const
{
    return m_pen;
}

QString PenTool::name()
{
    return name(m_pen);
}

QString PenTool::name(const QPen &pen)
{
    return pen.color().name() + ":" + QString::number(pen.widthF());
}

void PenTool::showColorDialog()
{
    QColorDialog dialog(m_pen.color());
    dialog.show();
    dialog.exec();
    m_pen.setColor(dialog.selectedColor());
    m_colorButton->setText(m_pen.color().name());
    emit changed();
}

void PenTool::updateWidth(double width)
{
    if (!qFuzzyCompare((qreal) width, m_pen.widthF())) {
        m_pen.setWidthF(width);
        emit changed();
    }
}

void PenTool::updateStyle(int style)
{
    if (m_pen.style() != style) {
        m_pen.setStyle((Qt::PenStyle) style);
        emit changed();
    }
}

void PenTool::updateCapStyle(int index)
{
    Qt::PenCapStyle capStyle = (Qt::PenCapStyle) m_capStyleCombo->itemData(index).toInt();
    if (m_pen.capStyle() != capStyle) {
        m_pen.setCapStyle(capStyle);
        emit changed();
    }
}

void PenTool::updateJoinStyle(int index)
{
    Qt::PenJoinStyle joinStyle = (Qt::PenJoinStyle) m_joinStyleCombo->itemData(index).toInt();
    if (m_pen.joinStyle() != joinStyle) {
        m_pen.setJoinStyle(joinStyle);
        emit changed();
    }
}

#include "moc_pentool.cpp"
