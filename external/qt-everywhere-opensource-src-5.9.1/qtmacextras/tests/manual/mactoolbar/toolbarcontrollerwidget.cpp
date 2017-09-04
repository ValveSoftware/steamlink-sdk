/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
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

#include "toolbarcontrollerwidget.h"

ToolBarControllerWidget::ToolBarControllerWidget()
    :QWidget(0)
{

    setWindowTitle("QMacToolBar Test");
    resize(400, 200);

    QMacToolBar *toolBar = new QMacToolBar(this);

    QIcon qtIcon(QStringLiteral(":qtlogo.png"));
    fooItem = toolBar->addItem(qtIcon, QStringLiteral("Foo"));
    fooItem->setText("foo");

    connect(fooItem, SIGNAL(activated()), this, SLOT(activated()));

    QMacToolBarItem *item5 = toolBar->addAllowedItem(qtIcon, QStringLiteral("AllowedFoo"));
    connect(item5, SIGNAL(activated()), this, SLOT(activated()));

    QLineEdit *fooItemText = new QLineEdit(this);
    fooItemText->setText("Foo");
    fooItemText->move(10, 10);
    connect(fooItemText, SIGNAL(textChanged(QString)), this, SLOT(changeItemText(QString)));

    winId();
    toolBar->attachToWindow(windowHandle());
}

void ToolBarControllerWidget::activated()
{
    qDebug() << "activated" << sender();
}

void ToolBarControllerWidget::changeItemText(const QString &text)
{
    fooItem->setText(text);
}
