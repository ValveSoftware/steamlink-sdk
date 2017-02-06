/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Purchasing module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3-COMM$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qinapppurchasebackendfactory_p.h"

#if defined(Q_OS_ANDROID)
#  include "qandroidinapppurchasebackend_p.h"
#elif defined(Q_OS_DARWIN) && !defined(Q_OS_WATCHOS)
#  include "qmacinapppurchasebackend_p.h"
#elif defined(Q_OS_WINRT)
#  include "qwinrtinapppurchasebackend_p.h"
#else
#  include "qinapppurchasebackend_p.h"
#endif

QT_BEGIN_NAMESPACE

QInAppPurchaseBackend *QInAppPurchaseBackendFactory::create()
{
#if defined(Q_OS_ANDROID)
    return new QAndroidInAppPurchaseBackend;
#elif defined(Q_OS_DARWIN) && !defined(Q_OS_WATCHOS)
    return new QMacInAppPurchaseBackend;
#elif defined (Q_OS_WINRT)
    return new QWinRTInAppPurchaseBackend;
#else
    return new QInAppPurchaseBackend;
#endif
}

QT_END_NAMESPACE
