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

#include "qquickwebenginenewviewrequest_p.h"

#include "qquickwebengineview_p_p.h"
#include "web_contents_adapter.h"

/*!
    \qmltype WebEngineNewViewRequest
    \instantiates QQuickWebEngineNewViewRequest
    \inqmlmodule QtWebEngine
    \since QtWebEngine 1.1

    \brief A utility type for the WebEngineView::newViewRequested signal.

    Contains information about a request to load a page in a separate web engine view.

    \sa WebEngineView::newViewRequested
*/
QQuickWebEngineNewViewRequest::QQuickWebEngineNewViewRequest()
{
}

QQuickWebEngineNewViewRequest::~QQuickWebEngineNewViewRequest()
{
}

/*!
    \qmlproperty WebEngineView::NewViewDestination WebEngineNewViewRequest::destination
    The type of the view that is requested by the page.
 */
QQuickWebEngineView::NewViewDestination QQuickWebEngineNewViewRequest::destination() const
{
    return m_destination;
}

/*!
    \qmlproperty QUrl WebEngineNewViewRequest::requestedUrl
    The URL that is requested by the page.
    \since QtWebEngine 1.5
 */
QUrl QQuickWebEngineNewViewRequest::requestedUrl() const
{
    return m_requestedUrl;
}

/*!
    \qmlproperty bool WebEngineNewViewRequest::userInitiated
    Whether this window request was directly triggered as the result of a keyboard or mouse event.

    Use this property to block possibly unwanted \e popups.
 */
bool QQuickWebEngineNewViewRequest::isUserInitiated() const
{
    return m_isUserInitiated;
}

/*!
    \qmlmethod WebEngineNewViewRequest::openIn(WebEngineView view)

    Opens the requested page in the new web engine view \a view. State and history of the
    view and the page possibly loaded in it will be lost.

    \sa WebEngineView::newViewRequested
  */
void QQuickWebEngineNewViewRequest::openIn(QQuickWebEngineView *view)
{
    if (!m_adapter && !m_requestedUrl.isValid()) {
        qWarning("Trying to open an empty request, it was either already used or was invalidated."
            "\nYou must complete the request synchronously within the newViewRequested signal handler."
            " If a view hasn't been adopted before returning, the request will be invalidated.");
        return;
    }

    if (!view) {
        qWarning("Trying to open a WebEngineNewViewRequest in an invalid WebEngineView.");
        return;
    }
    if (m_adapter)
        view->d_func()->adoptWebContents(m_adapter.data());
    else
        view->setUrl(m_requestedUrl);
    m_adapter.reset();
}
