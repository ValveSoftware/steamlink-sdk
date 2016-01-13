/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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

#include "AddressBar.h"

#include <QtCore>
#include <QtWidgets>

AddressBar::AddressBar(QWidget *parent)
    : QWidget(parent)
{
    m_lineEdit = new QLineEdit(parent);
    m_lineEdit->setPlaceholderText("Enter address or search terms");
    connect(m_lineEdit, SIGNAL(returnPressed()), SLOT(processAddress()));
    m_toolButton = new QToolButton(parent);
    m_toolButton->setText("Go");
    connect(m_toolButton, SIGNAL(clicked()), SLOT(processAddress()));
}

QSize AddressBar::sizeHint() const
{
    return m_lineEdit->sizeHint();
}

void AddressBar::processAddress()
{
    if (!m_lineEdit->text().isEmpty())
        emit addressEntered(m_lineEdit->text());
}

void AddressBar::resizeEvent(QResizeEvent *event)
{
    int x, y, w, h;

    m_toolButton->adjustSize();
    x = width() - m_toolButton->width();
    y = 0;
    w = m_toolButton->width();
    h = height() - 1;
    m_toolButton->setGeometry(x, y, w, h);
    m_toolButton->show();

    x = 0;
    y = 0;
    w = width() - m_toolButton->width();
    h = height() - 1;
    m_lineEdit->setGeometry(x, y, w, h);
    m_lineEdit->show();
}

void AddressBar::focusInEvent(QFocusEvent *event)
{
    m_lineEdit->setFocus();
    QWidget::focusInEvent(event);
}
