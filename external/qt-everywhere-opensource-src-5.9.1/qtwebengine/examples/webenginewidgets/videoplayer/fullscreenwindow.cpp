/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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
#include "fullscreenwindow.h"

#include "fullscreennotification.h"

#include <QAction>
#include <QLabel>
#include <QWebEngineView>

FullScreenWindow::FullScreenWindow(QWebEngineView *oldView, QWidget *parent)
    : QWidget(parent)
    , m_view(new QWebEngineView(this))
    , m_notification(new FullScreenNotification(this))
    , m_oldView(oldView)
    , m_oldGeometry(oldView->window()->geometry())
{
    m_view->stackUnder(m_notification);

    auto exitAction = new QAction(this);
    exitAction->setShortcut(Qt::Key_Escape);
    connect(exitAction, &QAction::triggered, [this]() {
        m_view->triggerPageAction(QWebEnginePage::ExitFullScreen);
    });
    addAction(exitAction);

    m_view->setPage(m_oldView->page());
    setGeometry(m_oldGeometry);
    showFullScreen();
    m_oldView->window()->hide();
}

FullScreenWindow::~FullScreenWindow()
{
    m_oldView->setPage(m_view->page());
    m_oldView->window()->setGeometry(m_oldGeometry);
    m_oldView->window()->show();
    hide();
}

void FullScreenWindow::resizeEvent(QResizeEvent *event)
{
    QRect viewGeometry(QPoint(0, 0), size());
    m_view->setGeometry(viewGeometry);

    QRect notificationGeometry(QPoint(0, 0), m_notification->sizeHint());
    notificationGeometry.moveCenter(viewGeometry.center());
    m_notification->setGeometry(notificationGeometry);

    QWidget::resizeEvent(event);
}
