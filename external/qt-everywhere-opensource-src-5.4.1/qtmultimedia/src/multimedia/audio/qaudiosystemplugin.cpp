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


#include "qaudiosystemplugin.h"

QT_BEGIN_NAMESPACE

QAudioSystemFactoryInterface::~QAudioSystemFactoryInterface()
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

    Qt supports win32, linux(alsa) and Mac OS X standard (builtin to the
    QtMultimedia library at compile time).

    You can support other backends other than these predefined ones by
    creating a plugin subclassing QAudioSystemPlugin, QAbstractAudioDeviceInfo,
    QAbstractAudioOutput and QAbstractAudioInput.


    -audio-backend configure option will force compiling in of the builtin backend
    into the QtMultimedia library at compile time. This is automatic by default
    and will only be compiled into the library if the dependencies are installed.
    eg. alsa-devel package installed for linux.

    If the builtin backend is not compiled into the QtMultimedia library and
    no audio plugins are available a fallback dummy backend will be used.
    This should print out warnings if this is the case when you try and use QAudioInput or QAudioOutput. To fix this problem
    reconfigure Qt using -audio-backend or create your own plugin with a default
    key to always override the dummy fallback. The easiest way to determine
    if you have only a dummy backend is to get a list of available audio devices.

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
