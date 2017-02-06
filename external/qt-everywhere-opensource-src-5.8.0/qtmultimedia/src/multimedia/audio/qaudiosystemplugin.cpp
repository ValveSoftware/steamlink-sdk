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


#include "qaudiosystemplugin.h"
#include "qaudiosystempluginext_p.h"

QT_BEGIN_NAMESPACE

QAudioSystemFactoryInterface::~QAudioSystemFactoryInterface()
{
}

QAudioSystemPluginExtension::~QAudioSystemPluginExtension()
{
}

/*!
    \class QAudioSystemPlugin
    \brief The QAudioSystemPlugin class provides an abstract base for audio plugins.

    \ingroup multimedia
    \ingroup multimedia_audio
    \inmodule QtMultimedia
    \internal

    Writing a audio plugin is achieved by subclassing this base class,
    reimplementing the pure virtual functions availableDevices(),
    createInput(), createOutput() and createDeviceInfo() then exporting
    the class with the Q_PLUGIN_METADATA() macro.

    The json file containing the meta data should contain a list of keys
    matching the plugin. Add "default" to your list of keys available
    to override the default audio device to be provided by your plugin.

    \code
    { "Keys": [ "default" ] }
    \endcode

    Unit tests are available to help in debugging new plugins.

    \sa QAbstractAudioDeviceInfo, QAbstractAudioOutput, QAbstractAudioInput

    Qt comes with plugins for Windows (WinMM and WASAPI), Linux (ALSA and PulseAudio), \macos / iOS
    (CoreAudio), Android (OpenSL ES) and QNX.

    If no audio plugins are available, a fallback dummy backend will be used.
    This should print out warnings if this is the case when you try and use QAudioInput
    or QAudioOutput. To fix this problem, make sure the dependencies for the Qt plugins are
    installed on the system and reconfigure Qt (e.g. alsa-devel package on Linux), or create your
    own plugin with a default key to always override the dummy fallback. The easiest way to
    determine if you have only a dummy backend is to get a list of available audio devices.

    QAudioDeviceInfo::availableDevices(QAudio::AudioOutput).size() = 0 (dummy backend)
*/

/*!
    Construct a new audio plugin with \a parent.
    This is invoked automatically by the Q_PLUGIN_METADATA() macro.
*/

QAudioSystemPlugin::QAudioSystemPlugin(QObject* parent) :
    QObject(parent)
{}

/*!
   Destroy the audio plugin

   You never have to call this explicitly. Qt destroys a plugin automatically when it is no longer used.
*/

QAudioSystemPlugin::~QAudioSystemPlugin()
{}

/*!
    \fn QList<QByteArray> QAudioSystemPlugin::availableDevices(QAudio::Mode mode) const
    Returns a list of available audio devices for \a mode
*/

/*!
    \fn QAbstractAudioInput* QAudioSystemPlugin::createInput(const QByteArray& device)
    Returns a pointer to a QAbstractAudioInput created using \a device identifier
*/

/*!
    \fn QAbstractAudioOutput* QAudioSystemPlugin::createOutput(const QByteArray& device)
    Returns a pointer to a QAbstractAudioOutput created using \a device identifier

*/

/*!
    \fn QAbstractAudioDeviceInfo* QAudioSystemPlugin::createDeviceInfo(const QByteArray& device, QAudio::Mode mode)
    Returns a pointer to a QAbstractAudioDeviceInfo created using \a device and \a mode

*/


QT_END_NAMESPACE

#include "moc_qaudiosystemplugin.cpp"
