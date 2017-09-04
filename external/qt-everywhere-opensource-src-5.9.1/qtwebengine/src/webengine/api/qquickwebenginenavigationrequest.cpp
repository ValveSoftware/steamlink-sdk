/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
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

/*!
    \qmltype WebEngineNavigationRequest
    \instantiates QQuickWebEngineNavigationRequest
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.0

    \brief Represents a request for navigating to a web page as part of
    \l{WebEngineView::navigationRequested()}.

    To accept or reject a request, set \l action to
    \c WebEngineNavigationRequest.AcceptRequest or
    \c WebEngineNavigationRequest.IgnoreRequest.
*/

QQuickWebEngineNavigationRequest::QQuickWebEngineNavigationRequest(const QUrl& url, QQuickWebEngineView::NavigationType navigationType, bool mainFrame, QObject* parent)
    : QObject(parent)
    , d_ptr(new QQuickWebEngineNavigationRequestPrivate(url, navigationType, mainFrame))
{
}

QQuickWebEngineNavigationRequest::~QQuickWebEngineNavigationRequest()
{
}

/*!
    \qmlproperty enumeration WebEngineNavigationRequest::action

    Whether to accept or ignore the navigation request.

    \value  WebEngineNavigationRequest.AcceptRequest
            Accepts a navigation request.
    \value  WebEngineNavigationRequest.IgnoreRequest
            Ignores a navigation request.
*/

void QQuickWebEngineNavigationRequest::setAction(QQuickWebEngineView::NavigationRequestAction action)
{
    Q_D(QQuickWebEngineNavigationRequest);
    if (d->action == action)
        return;

    d->action = action;
    emit actionChanged();
}

/*!
    \qmlproperty url WebEngineNavigationRequest::url
    \readonly

    The URL of the web page to go to.
*/

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

/*!
    \qmlproperty enumeration WebEngineNavigationRequest::navigationType
    \readonly

    The method used to navigate to a web page.

    \value  WebEngineNavigationRequest.LinkClickedNavigation
            Clicking a link.
    \value  WebEngineNavigationRequest.TypedNavigation
            Entering an URL on the address bar.
    \value  WebEngineNavigationRequest.FormSubmittedNavigation
            Submitting a form.
    \value  WebEngineNavigationRequest.BackForwardNavigation
            Using navigation history to go to the previous or next page.
    \value  WebEngineNavigationRequest.ReloadNavigation
            Reloading the page.
    \value  WebEngineNavigationRequest.OtherNavigation
            Using some other method to go to a page.
*/

QQuickWebEngineView::NavigationType QQuickWebEngineNavigationRequest::navigationType() const
{
    Q_D(const QQuickWebEngineNavigationRequest);
    return d->navigationType;
}

/*!
    \qmlproperty bool WebEngineNavigationRequest::isMainFrame
    \readonly

    Whether the navigation issue is requested for a top level page.
*/

bool QQuickWebEngineNavigationRequest::isMainFrame() const
{
    Q_D(const QQuickWebEngineNavigationRequest);
    return d->isMainFrame;
}

QT_END_NAMESPACE
