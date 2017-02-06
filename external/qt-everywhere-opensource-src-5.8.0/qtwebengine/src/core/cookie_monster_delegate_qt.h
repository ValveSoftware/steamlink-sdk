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

#ifndef COOKIE_MONSTER_DELEGATE_QT_H
#define COOKIE_MONSTER_DELEGATE_QT_H

#include "qtwebenginecoreglobal.h"

QT_WARNING_PUSH
// For some reason adding -Wno-unused-parameter to QMAKE_CXXFLAGS has no
// effect with clang, so use a pragma for these dirty chromium headers
QT_WARNING_DISABLE_CLANG("-Wunused-parameter")
#include "base/memory/ref_counted.h"
#include "net/cookies/cookie_monster.h"
QT_WARNING_POP

#include <QNetworkCookie>
#include <QPointer>

QT_FORWARD_DECLARE_CLASS(QWebEngineCookieStore)

namespace QtWebEngineCore {

// Extends net::CookieMonster::kDefaultCookieableSchemes with qrc, without enabling
// cookies for the file:// scheme, which is disabled by default in Chromium.
// Since qrc:// is similar to file:// and there are some unknowns about how
// to correctly handle file:// cookies, qrc:// should only be used for testing.
static const char* const kCookieableSchemes[] =
    { "http", "https", "qrc", "ws", "wss" };

class QWEBENGINE_EXPORT CookieMonsterDelegateQt: public net::CookieMonsterDelegate {
    QPointer<QWebEngineCookieStore> m_client;
    net::CookieMonster *m_cookieMonster;
public:
    CookieMonsterDelegateQt();
    ~CookieMonsterDelegateQt();

    bool hasCookieMonster();

    void setCookie(quint64 callbackId, const QNetworkCookie &cookie, const QUrl &origin);
    void deleteCookie(const QNetworkCookie &cookie, const QUrl &origin);
    void getAllCookies(quint64 callbackId);
    void deleteSessionCookies(quint64 callbackId);
    void deleteAllCookies(quint64 callbackId);

    void setCookieMonster(net::CookieMonster* monster);
    void setClient(QWebEngineCookieStore *client);

    bool canSetCookie(const QUrl &firstPartyUrl, const QByteArray &cookieLine, const QUrl &url);
    void OnCookieChanged(const net::CanonicalCookie& cookie, bool removed, ChangeCause cause) override;

private:
    void GetAllCookiesOnIOThread(const net::CookieMonster::GetCookieListCallback& callback);
    void SetCookieOnIOThread(const GURL& url, const std::string& cookie_line, const net::CookieMonster::SetCookiesCallback& callback);
    void DeleteCookieOnIOThread(const GURL& url, const std::string& cookie_name);
    void DeleteSessionCookiesOnIOThread(const net::CookieMonster::DeleteCallback& callback);
    void DeleteAllOnIOThread(const net::CookieMonster::DeleteCallback& callback);
};

}

#endif // COOKIE_MONSTER_DELEGATE_QT_H
