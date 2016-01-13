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

#ifndef QDECLARATIVEPLAYVARIATION_P_H
#define QDECLARATIVEPLAYVARIATION_P_H

#include <QtQml/qqml.h>
#include <QtQml/qqmlcomponent.h>

QT_BEGIN_NAMESPACE

class QDeclarativeAudioSample;
class QSoundInstance;

class QDeclarativePlayVariation : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString sample READ sample WRITE setSample)
    Q_PROPERTY(bool looping READ isLooping WRITE setLooping)
    Q_PROPERTY(qreal maxGain READ maxGain WRITE setMaxGain)
    Q_PROPERTY(qreal minGain READ minGain WRITE setMinGain)
    Q_PROPERTY(qreal maxPitch READ maxPitch WRITE setMaxPitch)
    Q_PROPERTY(qreal minPitch READ minPitch WRITE setMinPitch)

public:
    QDeclarativePlayVariation(QObject *parent = 0);
    ~QDeclarativePlayVariation();

    void classBegin();
    void componentComplete();

    QString sample() const;
    void setSample(const QString& sample);

    bool isLooping() const;
    void setLooping(bool looping);

    qreal maxGain() const;
    void setMaxGain(qreal maxGain);
    qreal minGain() const;
    void setMinGain(qreal minGain);

    qreal maxPitch() const;
    void setMaxPitch(qreal maxPitch);
    qreal minPitch() const;
    void setMinPitch(qreal minPitch);

    //called by QDeclarativeAudioEngine
    void setSampleObject(QDeclarativeAudioSample *sampleObject);
    QDeclarativeAudioSample* sampleObject() const;

    void applyParameters(QSoundInstance *soundInstance);

private:
    Q_DISABLE_COPY(QDeclarativePlayVariation);
    bool m_complete;
    QString m_sample;
    bool m_looping;
    qreal m_maxGain;
    qreal m_minGain;
    qreal m_maxPitch;
    qreal m_minPitch;
    QDeclarativeAudioSample *m_sampleObject;
};

QT_END_NAMESPACE

#endif
