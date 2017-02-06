/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "qwasapiplugin.h"
#include "qwasapiaudiodeviceinfo.h"
#include "qwasapiaudioinput.h"
#include "qwasapiaudiooutput.h"
#include "qwasapiutils.h"

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcMmPlugin, "qt.multimedia.plugin")

QWasapiPlugin::QWasapiPlugin(QObject *parent)
    : QAudioSystemPlugin(parent)
{
    qCDebug(lcMmPlugin) << __FUNCTION__;
}

QByteArray QWasapiPlugin::defaultDevice(QAudio::Mode mode) const
{
    return QWasapiUtils::defaultDevice(mode);
}

QList<QByteArray> QWasapiPlugin::availableDevices(QAudio::Mode mode) const
{
    qCDebug(lcMmPlugin) << __FUNCTION__ << mode;
    return QWasapiUtils::availableDevices(mode);
}

QAbstractAudioInput *QWasapiPlugin::createInput(const QByteArray &device)
{
    qCDebug(lcMmPlugin) << __FUNCTION__ << device;
    return new QWasapiAudioInput(device);
}

QAbstractAudioOutput *QWasapiPlugin::createOutput(const QByteArray &device)
{
    qCDebug(lcMmPlugin) << __FUNCTION__ << device;
    return new QWasapiAudioOutput(device);
}

QAbstractAudioDeviceInfo *QWasapiPlugin::createDeviceInfo(const QByteArray &device, QAudio::Mode mode)
{
    qCDebug(lcMmPlugin) << __FUNCTION__ << device << mode;
    return new QWasapiAudioDeviceInfo(device, mode);
}

QT_END_NAMESPACE
