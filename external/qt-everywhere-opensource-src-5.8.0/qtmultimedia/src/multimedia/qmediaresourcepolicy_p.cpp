/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include "qmediaresourcepolicy_p.h"
#include "qmediapluginloader_p.h"
#include "qmediaresourcepolicyplugin_p.h"
#include "qmediaresourceset_p.h"

namespace {
    class QDummyMediaPlayerResourceSet : public QMediaPlayerResourceSetInterface
    {
    public:
        QDummyMediaPlayerResourceSet(QObject *parent)
            : QMediaPlayerResourceSetInterface(parent)
        {
        }

        bool isVideoEnabled() const
        {
            return true;
        }

        bool isGranted() const
        {
            return true;
        }

        bool isAvailable() const
        {
            return true;
        }

        void acquire() {}
        void release() {}
        void setVideoEnabled(bool) {}
    };
}

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC_WITH_ARGS(QMediaPluginLoader, resourcePolicyLoader,
        (QMediaResourceSetFactoryInterface_iid, QLatin1String("resourcepolicy"), Qt::CaseInsensitive))

Q_GLOBAL_STATIC(QObject, dummyRoot)

QObject* QMediaResourcePolicy::createResourceSet(const QString& interfaceId)
{
    QMediaResourceSetFactoryInterface *factory =
            qobject_cast<QMediaResourceSetFactoryInterface*>(resourcePolicyLoader()
                                                             ->instance(QLatin1String("default")));
    QObject* obj = 0;
    if (factory)
        obj = factory->create(interfaceId);

    if (!obj) {
        if (interfaceId == QLatin1String(QMediaPlayerResourceSetInterface_iid)) {
            obj = new QDummyMediaPlayerResourceSet(dummyRoot());
        }
    }
    Q_ASSERT(obj);
    return obj;
}

void QMediaResourcePolicy::destroyResourceSet(QObject* resourceSet)
{
    if (resourceSet->parent() == dummyRoot()) {
        delete resourceSet;
        return;
    }
    QMediaResourceSetFactoryInterface *factory =
            qobject_cast<QMediaResourceSetFactoryInterface*>(resourcePolicyLoader()
                                                             ->instance(QLatin1String("default")));
    Q_ASSERT(factory);
    if (!factory)
        return;
    return factory->destroy(resourceSet);
}
QT_END_NAMESPACE
