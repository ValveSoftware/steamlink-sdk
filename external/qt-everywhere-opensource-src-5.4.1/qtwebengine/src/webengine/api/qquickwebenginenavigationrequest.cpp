/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickwebenginenavigationrequest_p.h"

#include "qquickwebengineview_p.h"

QT_BEGIN_NAMESPACE

class QQuickWebEngineNavigationRequestPrivate {
public:
    QQuickWebEngineNavigationRequestPrivate(const QUrl& url, QQuickWebEngineView::NavigationType navigationType, bool mainFrame)
        : url(url)
        , action(QQuickWebEngineView::AcceptRequest)
        , navigationType(navigationType)
        , isMainFrame(mainFrame)
    {
    }

    ~QQuickWebEngineNavigationRequestPrivate()
    {
    }

    QUrl url;
    QQuickWebEngineView::NavigationRequestAction action;
    QQuickWebEngineView::NavigationType navigationType;
    bool isMainFrame;
};

QQuickWebEngineNavigationRequest::QQuickWebEngineNavigationRequest(const QUrl& url, QQuickWebEngineView::NavigationType navigationType, bool mainFrame, QObject* parent)
    : QObject(parent)
    , d_ptr(new QQuickWebEngineNavigationRequestPrivate(url, navigationType, mainFrame))
{
}

QQuickWebEngineNavigationRequest::~QQuickWebEngineNavigationRequest()
{
}

void QQuickWebEngineNavigationRequest::setAction(QQuickWebEngineView::NavigationRequestAction action)
{
    Q_D(QQuickWebEngineNavigationRequest);
    if (d->action == action)
        return;

    d->action = action;
    emit actionChanged();
}

QUrl QQuickWebEngineNavigationRequest::url() const
{
    Q_D(const QQuickWebEngineNavigationRequest);
    return d->url;
}

QQuickWebEngineView::NavigationRequestAction QQuickWebEngineNavigationRequest::action() const
{
    Q_D(const QQuickWebEngineNavigationRequest);
    return d->action;
}

QQuickWebEngineView::NavigationType QQuickWebEngineNavigationRequest::navigationType() const
{
    Q_D(const QQuickWebEngineNavigationRequest);
    return d->navigationType;
}

bool QQuickWebEngineNavigationRequest::isMainFrame() const
{
    Q_D(const QQuickWebEngineNavigationRequest);
    return d->isMainFrame;
}

QT_END_NAMESPACE
