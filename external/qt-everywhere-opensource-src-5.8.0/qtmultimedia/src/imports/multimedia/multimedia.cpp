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

#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include "qsoundeffect.h"

#include <private/qdeclarativevideooutput_p.h>
#include "qabstractvideofilter.h"

#include "qdeclarativemultimediaglobal_p.h"
#include "qdeclarativemediametadata_p.h"
#include "qdeclarativeaudio_p.h"
#include "qdeclarativeradio_p.h"
#include "qdeclarativeradiodata_p.h"
#include "qdeclarativeplaylist_p.h"
#include "qdeclarativecamera_p.h"
#include "qdeclarativecamerapreviewprovider_p.h"
#include "qdeclarativecameraexposure_p.h"
#include "qdeclarativecameraflash_p.h"
#include "qdeclarativecamerafocus_p.h"
#include "qdeclarativecameraimageprocessing_p.h"
#include "qdeclarativecameraviewfinder_p.h"
#include "qdeclarativetorch_p.h"

QML_DECLARE_TYPE(QSoundEffect)

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtMultimedia);
#endif
}

QT_BEGIN_NAMESPACE

static QObject *multimedia_global_object(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    return new QDeclarativeMultimediaGlobal(jsEngine);
}

class QMultimediaDeclarativeModule : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    QMultimediaDeclarativeModule(QObject *parent = 0) : QQmlExtensionPlugin(parent) { initResources(); }
    virtual void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtMultimedia"));

        // 5.0 types
        qmlRegisterType<QSoundEffect>(uri, 5, 0, "SoundEffect");
        qmlRegisterType<QDeclarativeAudio>(uri, 5, 0, "Audio");
        qmlRegisterType<QDeclarativeAudio>(uri, 5, 0, "MediaPlayer");
        qmlRegisterType<QDeclarativeVideoOutput>(uri, 5, 0, "VideoOutput");
        qmlRegisterType<QDeclarativeRadio>(uri, 5, 0, "Radio");
        qmlRegisterType<QDeclarativeRadioData>(uri, 5, 0, "RadioData");
        qmlRegisterType<QDeclarativeCamera>(uri, 5, 0, "Camera");
        qmlRegisterType<QDeclarativeTorch>(uri, 5, 0, "Torch");
        qmlRegisterUncreatableType<QDeclarativeCameraCapture>(uri, 5, 0, "CameraCapture",
                                trUtf8("CameraCapture is provided by Camera"));
        qmlRegisterUncreatableType<QDeclarativeCameraRecorder>(uri, 5, 0, "CameraRecorder",
                                trUtf8("CameraRecorder is provided by Camera"));
        qmlRegisterUncreatableType<QDeclarativeCameraExposure>(uri, 5, 0, "CameraExposure",
                                trUtf8("CameraExposure is provided by Camera"));
        qmlRegisterUncreatableType<QDeclarativeCameraFocus>(uri, 5, 0, "CameraFocus",
                                trUtf8("CameraFocus is provided by Camera"));
        qmlRegisterUncreatableType<QDeclarativeCameraFlash>(uri, 5, 0, "CameraFlash",
                                trUtf8("CameraFlash is provided by Camera"));
        qmlRegisterUncreatableType<QDeclarativeCameraImageProcessing>(uri, 5, 0, "CameraImageProcessing",
                                trUtf8("CameraImageProcessing is provided by Camera"));

        // 5.2 types
        qmlRegisterType<QDeclarativeVideoOutput, 2>(uri, 5, 2, "VideoOutput");

        // 5.3 types
        // Nothing changed, but adding "import QtMultimedia 5.3" in QML will fail unless at
        // least one type is registered for that version.
        qmlRegisterType<QSoundEffect>(uri, 5, 3, "SoundEffect");

        // 5.4 types
        qmlRegisterSingletonType<QDeclarativeMultimediaGlobal>(uri, 5, 4, "QtMultimedia", multimedia_global_object);
        qmlRegisterType<QDeclarativeCamera, 1>(uri, 5, 4, "Camera");
        qmlRegisterUncreatableType<QDeclarativeCameraViewfinder>(uri, 5, 4, "CameraViewfinder",
                                trUtf8("CameraViewfinder is provided by Camera"));

        // 5.5 types
        qmlRegisterUncreatableType<QDeclarativeCameraImageProcessing, 1>(uri, 5, 5, "CameraImageProcessing", trUtf8("CameraImageProcessing is provided by Camera"));
        qmlRegisterType<QDeclarativeCamera, 2>(uri, 5, 5, "Camera");

        // 5.6 types
        qmlRegisterType<QDeclarativeAudio, 1>(uri, 5, 6, "Audio");
        qmlRegisterType<QDeclarativeAudio, 1>(uri, 5, 6, "MediaPlayer");
        qmlRegisterType<QDeclarativePlaylist>(uri, 5, 6, "Playlist");
        qmlRegisterType<QDeclarativePlaylistItem>(uri, 5, 6, "PlaylistItem");

        // 5.7 types
        qmlRegisterType<QDeclarativePlaylist, 1>(uri, 5, 7, "Playlist");
        qmlRegisterUncreatableType<QDeclarativeCameraImageProcessing, 2>(uri, 5, 7, "CameraImageProcessing",
                                trUtf8("CameraImageProcessing is provided by Camera"));

        // 5.8 types (nothing new, re-register one of the types)
        qmlRegisterType<QSoundEffect>(uri, 5, 8, "SoundEffect");

        qmlRegisterType<QDeclarativeMediaMetaData>();
        qmlRegisterType<QAbstractVideoFilter>();
    }

    void initializeEngine(QQmlEngine *engine, const char *uri)
    {
        Q_UNUSED(uri);
        engine->addImageProvider("camera", new QDeclarativeCameraPreviewProvider);
    }
};

QT_END_NAMESPACE

#include "multimedia.moc"

