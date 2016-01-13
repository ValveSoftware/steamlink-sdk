/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
