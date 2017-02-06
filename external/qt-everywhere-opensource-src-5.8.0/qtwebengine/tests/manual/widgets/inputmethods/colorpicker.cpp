/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "colorpicker.h"

#include <QColorDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

ColorPicker::ColorPicker(QWidget *parent)
    : QWidget(parent)
    , m_colorInput(new QLineEdit)
    , m_chooseButton(new QPushButton)
{
    m_chooseButton->setText(tr("Choose"));

    QHBoxLayout *layout = new QHBoxLayout;
    layout->setMargin(0);
    layout->addWidget(m_colorInput);
    layout->addWidget(m_chooseButton);
    setLayout(layout);

    connect(m_colorInput, &QLineEdit::textChanged, this, &ColorPicker::colorStringChanged);
    connect(m_chooseButton, &QPushButton::clicked, this, &ColorPicker::selectButtonClicked);
}

QColor ColorPicker::color() const
{
    return QColor(m_colorInput->text());
}

void ColorPicker::setColor(const QColor &color)
{
    if (color.isValid())
        m_colorInput->setText(color.name(QColor::HexArgb));
}

void ColorPicker::colorStringChanged(const QString &colorString)
{
    QColor color(colorString);
    QPalette palette;

    palette.setColor(QPalette::Base, color.isValid() ? color : QColor(Qt::white));
    m_colorInput->setPalette(palette);
}

void ColorPicker::selectButtonClicked()
{
    QColor selectedColor = QColorDialog::getColor(color().isValid() ? color() : Qt::white,
                                                  this,
                                                  "Select Color",
                                                  QColorDialog::ShowAlphaChannel);
    setColor(selectedColor);
}
