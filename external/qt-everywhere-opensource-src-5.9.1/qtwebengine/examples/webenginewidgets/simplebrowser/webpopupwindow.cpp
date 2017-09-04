/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
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

#include "urllineedit.h"
#include "webpage.h"
#include "webpopupwindow.h"
#include "webview.h"
#include <QIcon>
#include <QVBoxLayout>

WebPopupWindow::WebPopupWindow(QWebEngineProfile *profile)
    : m_addressBar(new UrlLineEdit(this))
    , m_view(new WebView(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin(0);
    setLayout(layout);
    layout->addWidget(m_addressBar);
    layout->addWidget(m_view);

    m_view->setPage(new WebPage(profile, m_view));
    m_view->setFocus();
    m_addressBar->setReadOnly(true);
    m_addressBar->setFavIcon(QIcon(QStringLiteral(":defaulticon.png")));

    connect(m_view, &WebView::titleChanged, this, &QWidget::setWindowTitle);
    connect(m_view, &WebView::urlChanged, this, &WebPopupWindow::setUrl);
    connect(m_view->page(), &WebPage::iconChanged, this, &WebPopupWindow::handleIconChanged);
    connect(m_view->page(), &WebPage::geometryChangeRequested, this, &WebPopupWindow::handleGeometryChangeRequested);
    connect(m_view->page(), &WebPage::windowCloseRequested, this, &QWidget::close);
}

QWebEngineView *WebPopupWindow::view() const
{
    return m_view;
}

void WebPopupWindow::setUrl(const QUrl &url)
{
    m_addressBar->setUrl(url);
}

void WebPopupWindow::handleGeometryChangeRequested(const QRect &newGeometry)
{
    m_view->setMinimumSize(newGeometry.width(), newGeometry.height());
    move(newGeometry.topLeft() - m_view->pos());
    // let the layout do the magic
    resize(0, 0);
    show();
}

void WebPopupWindow::handleIconChanged(const QIcon &icon)
{
    if (icon.isNull())
        m_addressBar->setFavIcon(QIcon(QStringLiteral(":defaulticon.png")));
    else
        m_addressBar->setFavIcon(icon);
}
