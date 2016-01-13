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

#include <QtCore/qstring.h>
#include <QtCore/qdebug.h>
#include <QtCore/QDir>
#include <QtCore/QDebug>

#include "camerabinserviceplugin.h"

#include "camerabinservice.h"
#include <private/qgstutils_p.h>

QT_BEGIN_NAMESPACE

template <typename T, int N> static int lengthOf(const T(&)[N]) { return N; }

CameraBinServicePlugin::CameraBinServicePlugin()
    : m_sourceFactory(0)
{
}

CameraBinServicePlugin::~CameraBinServicePlugin()
{
    if (m_sourceFactory)
        gst_object_unref(GST_OBJECT(m_sourceFactory));
}

QMediaService* CameraBinServicePlugin::create(const QString &key)
{
    QGstUtils::initializeGst();

    if (key == QLatin1String(Q_MEDIASERVICE_CAMERA))
        return new CameraBinService(sourceFactory());

    qWarning() << "Gstreamer camerabin service plugin: unsupported key:" << key;
    return 0;
}

void CameraBinServicePlugin::release(QMediaService *service)
{
    delete service;
}

QMediaServiceProviderHint::Features CameraBinServicePlugin::supportedFeatures(
        const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_CAMERA)
        return QMediaServiceProviderHint::VideoSurface;

    return QMediaServiceProviderHint::Features();
}

QByteArray CameraBinServicePlugin::defaultDevice(const QByteArray &service) const
{
    return service == Q_MEDIASERVICE_CAMERA
            ? QGstUtils::enumerateCameras(sourceFactory()).value(0).name.toUtf8()
            : QByteArray();
}

QList<QByteArray> CameraBinServicePlugin::devices(const QByteArray &service) const
{

    return service == Q_MEDIASERVICE_CAMERA
            ? QGstUtils::cameraDevices(m_sourceFactory)
            : QList<QByteArray>();
}

QString CameraBinServicePlugin::deviceDescription(const QByteArray &service, const QByteArray &deviceName)
{
    return service == Q_MEDIASERVICE_CAMERA
            ? QGstUtils::cameraDescription(deviceName, m_sourceFactory)
            : QString();
}

QVariant CameraBinServicePlugin::deviceProperty(const QByteArray &service, const QByteArray &device, const QByteArray &property)
{
    Q_UNUSED(service);
    Q_UNUSED(device);
    Q_UNUSED(property);
    return QVariant();
}

QCamera::Position CameraBinServicePlugin::cameraPosition(const QByteArray &deviceName) const
{
    return QGstUtils::cameraPosition(deviceName, m_sourceFactory);
}

int CameraBinServicePlugin::cameraOrientation(const QByteArray &deviceName) const
{
    return QGstUtils::cameraOrientation(deviceName, m_sourceFactory);
}

GstElementFactory *CameraBinServicePlugin::sourceFactory() const
{
    if (!m_sourceFactory) {
        GstElementFactory *factory = 0;
        const QByteArray envCandidate = qgetenv("QT_GSTREAMER_CAMERABIN_SRC");
        if (!envCandidate.isEmpty())
            factory = gst_element_factory_find(envCandidate.constData());

        static const char *candidates[] = { "subdevsrc", "wrappercamerabinsrc" };
        for (int i = 0; !factory && i < lengthOf(candidates); ++i)
            factory = gst_element_factory_find(candidates[i]);

        if (factory) {
            m_sourceFactory = GST_ELEMENT_FACTORY(gst_plugin_feature_load(
                    GST_PLUGIN_FEATURE(factory)));
            gst_object_unref((GST_OBJECT(factory)));
        }
    }

    return m_sourceFactory;
}

QT_END_NAMESPACE
