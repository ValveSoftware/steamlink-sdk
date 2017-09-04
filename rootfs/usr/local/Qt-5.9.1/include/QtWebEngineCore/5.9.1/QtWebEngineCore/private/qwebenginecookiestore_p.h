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

#ifndef QWEBENGINECOOKIESTORE_P_H
#define QWEBENGINECOOKIESTORE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qtwebenginecoreglobal_p.h"

#include "qwebenginecallback_p.h"
#include "qwebenginecookiestore.h"

#include <QVector>
#include <QNetworkCookie>
#include <QUrl>
#include <QtCore/private/qobject_p.h>

namespace QtWebEngineCore {
class CookieMonsterDelegateQt;
}

QT_BEGIN_NAMESPACE

class QWEBENGINE_PRIVATE_EXPORT QWebEngineCookieStorePrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QWebEngineCookieStore)
    struct CookieData {
        quint64 callbackId;
        QNetworkCookie cookie;
        QUrl origin;
    };
    friend class QTypeInfo<CookieData>;
public:
    QtWebEngineCore::CallbackDirectory callbackDirectory;
    QVector<CookieData> m_pendingUserCookies;
    quint64 m_nextCallbackId;
    bool m_deleteSessionCookiesPending;
    bool m_deleteAllCookiesPending;
    bool m_getAllCookiesPending;

    QtWebEngineCore::CookieMonsterDelegateQt *delegate;

    QWebEngineCookieStorePrivate();

    void processPendingUserCookies();
    void rejectPendingUserCookies();
    void setCookie(const QWebEngineCallback<bool> &callback, const QNetworkCookie &cookie, const QUrl &origin);
    void deleteCookie(const QNetworkCookie &cookie, const QUrl &url);
    void deleteSessionCookies();
    void deleteAllCookies();
    void getAllCookies();

    void onGetAllCallbackResult(qint64 callbackId, const QByteArray &cookieList);
    void onSetCallbackResult(qint64 callbackId, bool success);
    void onDeleteCallbackResult(qint64 callbackId, int numCookies);
    void onCookieChanged(const QNetworkCookie &cookie, bool removed);
};

Q_DECLARE_TYPEINFO(QWebEngineCookieStorePrivate::CookieData, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif // QWEBENGINECOOKIESTORE_P_H
