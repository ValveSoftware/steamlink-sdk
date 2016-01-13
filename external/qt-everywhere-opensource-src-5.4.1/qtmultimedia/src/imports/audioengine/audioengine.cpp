/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

QT_BEGIN_NAMESPACE

class QAudioEngineDeclarativeModule : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface/1.0")

public:
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
    }
};

QT_END_NAMESPACE

#include "audioengine.moc"

