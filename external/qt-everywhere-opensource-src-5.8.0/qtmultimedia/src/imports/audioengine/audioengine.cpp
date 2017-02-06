/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qdeclarative_audioengine_p.h"
#include "qdeclarative_soundinstance_p.h"
#include "qdeclarative_sound_p.h"
#include "qdeclarative_playvariation_p.h"
#include "qdeclarative_audiocategory_p.h"
#include "qdeclarative_audiolistener_p.h"
#include "qdeclarative_audiosample_p.h"
#include "qdeclarative_attenuationmodel_p.h"

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtAudioEngine);
#endif
}

QT_BEGIN_NAMESPACE

class QAudioEngineDeclarativeModule : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    QAudioEngineDeclarativeModule(QObject *parent = 0) : QQmlExtensionPlugin(parent) { initResources(); }
    virtual void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtAudioEngine"));

        qmlRegisterType<QDeclarativeAudioEngine>(uri, 1, 0, "AudioEngine");
        qmlRegisterType<QDeclarativeAudioSample>(uri, 1, 0, "AudioSample");
        qmlRegisterType<QDeclarativeAudioCategory>(uri, 1, 0, "AudioCategory");
        qmlRegisterType<QDeclarativeSoundCone>(uri, 1, 0, "");
        qmlRegisterType<QDeclarativeSound>(uri, 1, 0, "Sound");
        qmlRegisterType<QDeclarativePlayVariation>(uri, 1, 0, "PlayVariation");
        qmlRegisterType<QDeclarativeAudioListener>(uri, 1, 0, "AudioListener");
        qmlRegisterType<QDeclarativeSoundInstance>(uri, 1, 0, "SoundInstance");

        qmlRegisterType<QDeclarativeAttenuationModelLinear>(uri, 1, 0, "AttenuationModelLinear");
        qmlRegisterType<QDeclarativeAttenuationModelInverse>(uri, 1, 0, "AttenuationModelInverse");

        // Dynamically adding audio engine related objects is only supported through revision 1
        qmlRegisterType<QDeclarativeAudioEngine, 1>(uri, 1, 1, "AudioEngine");
        qmlRegisterType<QDeclarativeSound, 1>(uri, 1, 1, "Sound");
    }
};

QT_END_NAMESPACE

#include "audioengine.moc"

