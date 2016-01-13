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

#include "qgstreameraudioinputselector_p.h"

#include <QtCore/QDir>
#include <QtCore/QDebug>

#include <gst/gst.h>

#ifdef HAVE_ALSA
#include <alsa/asoundlib.h>
#endif

QGstreamerAudioInputSelector::QGstreamerAudioInputSelector(QObject *parent)
    :QAudioInputSelectorControl(parent)
{
    update();
}

QGstreamerAudioInputSelector::~QGstreamerAudioInputSelector()
{
}

QList<QString> QGstreamerAudioInputSelector::availableInputs() const
{
    return m_names;
}

QString QGstreamerAudioInputSelector::inputDescription(const QString& name) const
{
    QString desc;

    for (int i = 0; i < m_names.size(); i++) {
        if (m_names.at(i).compare(name) == 0) {
            desc = m_descriptions.at(i);
            break;
        }
    }
    return desc;
}

QString QGstreamerAudioInputSelector::defaultInput() const
{
    if (m_names.size() > 0)
        return m_names.at(0);

    return QString();
}

QString QGstreamerAudioInputSelector::activeInput() const
{
    return m_audioInput;
}

void QGstreamerAudioInputSelector::setActiveInput(const QString& name)
{
    if (m_audioInput.compare(name) != 0) {
        m_audioInput = name;
        emit activeInputChanged(name);
    }
}

void QGstreamerAudioInputSelector::update()
{
    m_names.clear();
    m_descriptions.clear();

    //use autoaudiosrc as the first default device
    m_names.append("default:");
    m_descriptions.append(tr("System default device"));

    updatePulseDevices();
    updateAlsaDevices();
    updateOssDevices();
    if (m_names.size() > 0)
        m_audioInput = m_names.at(0);
}

void QGstreamerAudioInputSelector::updateAlsaDevices()
{
#ifdef HAVE_ALSA
    void **hints, **n;
    int card = -1;

    while (snd_card_next(&card) == 0 && card >= 0) {
        if (snd_device_name_hint(card, "pcm", &hints) < 0)
            continue;

        n = hints;
        while (*n != NULL) {
            char *name = snd_device_name_get_hint(*n, "NAME");
            char *descr = snd_device_name_get_hint(*n, "DESC");
            char *io = snd_device_name_get_hint(*n, "IOID");

            if ((name != NULL) && (descr != NULL)) {
                if ( io == NULL || qstrcmp(io,"Input") == 0 ) {
                    m_names.append(QLatin1String("alsa:")+QString::fromUtf8(name));
                    m_descriptions.append(QString::fromUtf8(descr));
                }
            }

            if (name != NULL)
                free(name);
            if (descr != NULL)
                free(descr);
            if (io != NULL)
                free(io);
            ++n;
        }

        snd_device_name_free_hint(hints);
    }
#endif
}

void QGstreamerAudioInputSelector::updateOssDevices()
{
    QDir devDir("/dev");
    devDir.setFilter(QDir::System);
    QFileInfoList entries = devDir.entryInfoList(QStringList() << "dsp*");
    foreach(const QFileInfo& entryInfo, entries) {
        m_names.append(QLatin1String("oss:")+entryInfo.filePath());
        m_descriptions.append(QString("OSS device %1").arg(entryInfo.fileName()));
    }
}

void QGstreamerAudioInputSelector::updatePulseDevices()
{
    GstElementFactory *factory = gst_element_factory_find("pulsesrc");
    if (factory) {
        m_names.append("pulseaudio:");
        m_descriptions.append("PulseAudio device.");
        gst_object_unref(GST_OBJECT(factory));
    }
}
