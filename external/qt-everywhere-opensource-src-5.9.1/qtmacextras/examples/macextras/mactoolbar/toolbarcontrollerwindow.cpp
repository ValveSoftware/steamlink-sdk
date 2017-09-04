/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "toolbarcontrollerwindow.h"

ToolBarControllerWindow::ToolBarControllerWindow()
    :RasterWindow(0)
{
    setTitle("QMacToolBar Example");
    RasterWindow::setText("QMacToolBar Example");
    resize(400, 200);

    QMacToolBar *toolBar = new QMacToolBar(this);

    QIcon qtIcon(QStringLiteral(":qtlogo.png"));

    // add Items
    QMacToolBarItem *item1 = toolBar->addItem(qtIcon, QStringLiteral("Foo 1"));
    connect(item1, SIGNAL(activated()), this, SLOT(activated()));
    QMacToolBarItem *item2 = toolBar->addItem(qtIcon, QStringLiteral("Bar 1"));
    connect(item2, SIGNAL(activated()), this, SLOT(activated()));

    toolBar->addSeparator();

    QMacToolBarItem *item3 = toolBar->addItem(qtIcon, QStringLiteral("Foo 2"));
    connect(item3, SIGNAL(activated()), this, SLOT(activated()));
    QMacToolBarItem *item4 = toolBar->addItem(qtIcon, QStringLiteral("Bar 2"));
    connect(item4, SIGNAL(activated()), this, SLOT(activated()));

    // add allowed items for the customization menu.
    QMacToolBarItem *item5 = toolBar->addAllowedItem(qtIcon, QStringLiteral("AllowedFoo"));
    connect(item5, SIGNAL(activated()), this, SLOT(activated()));
    QMacToolBarItem *item6 = toolBar->addAllowedItem(qtIcon, QStringLiteral("AllowedBar"));
    connect(item6, SIGNAL(activated()), this, SLOT(activated()));

    // Attach to the window
    toolBar->attachToWindow(this);
}

void ToolBarControllerWindow::activated()
{
    QMacToolBarItem *item = static_cast<QMacToolBarItem *>(sender());
    setText(QStringLiteral("Activated ") + item->text());
}
