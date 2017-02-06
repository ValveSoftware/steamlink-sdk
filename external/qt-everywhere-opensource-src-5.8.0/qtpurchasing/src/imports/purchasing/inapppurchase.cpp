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

#include "qinappstoreqmltype_p.h"

#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>

#include <QtPurchasing/qinappproduct.h>
#include <QtPurchasing/qinapptransaction.h>

QT_BEGIN_NAMESPACE

class QInAppPurchaseModule : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtPurchasing"));

        qmlRegisterType<QInAppStoreQmlType>(uri,
                                            1, 0,
                                            "Store");
        qmlRegisterType<QInAppProductQmlType>(uri,
                                              1, 0,
                                              "Product");
        qmlRegisterUncreatableType<QInAppTransaction>(uri,
                                                      1, 0,
                                                      "Transaction",
                                                      trUtf8("Transaction is provided by InAppStore"));
    }

    void initializeEngine(QQmlEngine *engine, const char *uri)
    {
        Q_UNUSED(uri);
        Q_UNUSED(engine);
    }
};

QT_END_NAMESPACE

#include "inapppurchase.moc"



