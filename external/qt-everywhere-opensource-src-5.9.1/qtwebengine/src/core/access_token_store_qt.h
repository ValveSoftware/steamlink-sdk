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

#ifndef ACCESS_TOKEN_STORE_QT_H
#define ACCESS_TOKEN_STORE_QT_H

#include "base/memory/ref_counted.h"
#include "device/geolocation/access_token_store.h"

#include <QtCore/QFile>
#include <QtCore/QScopedPointer>

namespace net {
class URLRequestContextGetter;
}

namespace QtWebEngineCore {

class AccessTokenStoreQt : public device::AccessTokenStore {
public:
    AccessTokenStoreQt();
    ~AccessTokenStoreQt();

    void LoadAccessTokens(const LoadAccessTokensCallback& request) override;
    void SaveAccessToken(const GURL& serverUrl, const base::string16& accessToken) override;

private:
    void performWorkOnUIThread();
    void respondOnOriginatingThread(const LoadAccessTokensCallback& callback);


    net::URLRequestContextGetter *m_systemRequestContext;
    AccessTokenMap m_accessTokenSet;

    DISALLOW_COPY_AND_ASSIGN(AccessTokenStoreQt);
};

} // namespace QtWebEngineCore

#endif  // ACCESS_TOKEN_STORE_QT_H
