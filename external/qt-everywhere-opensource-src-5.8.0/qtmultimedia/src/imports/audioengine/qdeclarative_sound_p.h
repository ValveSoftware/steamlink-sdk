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

#ifndef QDECLARATIVESOUND_P_H
#define QDECLARATIVESOUND_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtQml/qqml.h>
#include <QtCore/qlist.h>
#include "qdeclarative_playvariation_p.h"

QT_BEGIN_NAMESPACE

class QDeclarativeAudioCategory;
class QDeclarativeAttenuationModel;
class QDeclarativeSoundInstance;
class QDeclarativeAudioEngine;

class QDeclarativeSoundCone : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal innerAngle READ innerAngle WRITE setInnerAngle)
    Q_PROPERTY(qreal outerAngle READ outerAngle WRITE setOuterAngle)
    Q_PROPERTY(qreal outerGain READ outerGain WRITE setOuterGain)
public:
    QDeclarativeSoundCone(QObject *parent = 0);

    //by degree
    qreal innerAngle() const;
    void setInnerAngle(qreal innerAngle);

    //by degree
    qreal outerAngle() const;
    void setOuterAngle(qreal outerAngle);

    qreal outerGain() const;
    void setOuterGain(qreal outerGain);

    void setEngine(QDeclarativeAudioEngine *engine);

private:
    Q_DISABLE_COPY(QDeclarativeSoundCone)
    qreal m_innerAngle;
    qreal m_outerAngle;
    qreal m_outerGain;
    QDeclarativeAudioEngine *m_engine;
};

class QDeclarativeSound : public QObject
{
    friend class QDeclarativeSoundCone;

    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(PlayType playType READ playType WRITE setPlayType)
    Q_PROPERTY(QString category READ category WRITE setCategory)
    Q_PROPERTY(QDeclarativeSoundCone* cone READ cone CONSTANT)
    Q_PROPERTY(QString attenuationModel READ attenuationModel WRITE setAttenuationModel)
    Q_PROPERTY(QQmlListProperty<QDeclarativePlayVariation> playVariationlist READ playVariationlist CONSTANT)
    Q_CLASSINFO("DefaultProperty", "playVariationlist")

    Q_ENUMS(PlayType)
public:
    enum PlayType
    {
        Random,
        Sequential
    };

    QDeclarativeSound(QObject *parent = 0);
    ~QDeclarativeSound();

    PlayType playType() const;
    void setPlayType(PlayType playType);

    QString category() const;
    void setCategory(const QString& category);

    QString name() const;
    void setName(const QString& name);

    QString attenuationModel() const;
    void setAttenuationModel(const QString &attenuationModel);

    QDeclarativeAudioEngine *engine() const;
    void setEngine(QDeclarativeAudioEngine *);

    QDeclarativeSoundCone* cone() const;

    QDeclarativeAttenuationModel* attenuationModelObject() const;
    void setAttenuationModelObject(QDeclarativeAttenuationModel *attenuationModelObject);
    QDeclarativeAudioCategory* categoryObject() const;
    void setCategoryObject(QDeclarativeAudioCategory *categoryObject);

    int genVariationIndex(int oldVariationIndex);
    QDeclarativePlayVariation* getVariation(int index);

    //This is used for tracking new PlayVariation declared inside Sound
    QQmlListProperty<QDeclarativePlayVariation> playVariationlist();
    QList<QDeclarativePlayVariation*>& playlist();

    Q_INVOKABLE Q_REVISION(1) void addPlayVariation(QDeclarativePlayVariation*);

public Q_SLOTS:
    void play();
    void play(qreal gain);
    void play(qreal gain, qreal pitch);
    void play(const QVector3D& position);
    void play(const QVector3D& position, const QVector3D& velocity);
    void play(const QVector3D& position, const QVector3D& velocity, const QVector3D& direction);
    void play(const QVector3D& position, qreal gain);
    void play(const QVector3D& position, const QVector3D& velocity, qreal gain);
    void play(const QVector3D& position, const QVector3D& velocity, const QVector3D& direction, qreal gain);
    void play(const QVector3D& position, qreal gain, qreal pitch);
    void play(const QVector3D& position, const QVector3D& velocity, qreal gain, qreal pitch);
    void play(const QVector3D& position, const QVector3D& velocity, const QVector3D& direction, qreal gain, qreal pitch);
    QDeclarativeSoundInstance* newInstance();

private:
    Q_DISABLE_COPY(QDeclarativeSound)
    QDeclarativeSoundInstance* newInstance(bool managed);
    static void appendFunction(QQmlListProperty<QDeclarativePlayVariation> *property, QDeclarativePlayVariation *value);
    PlayType m_playType;
    QString m_name;
    QString m_category;
    QString m_attenuationModel;
    QList<QDeclarativePlayVariation*> m_playlist;
    QDeclarativeSoundCone *m_cone;

    QDeclarativeAttenuationModel *m_attenuationModelObject;
    QDeclarativeAudioCategory *m_categoryObject;
    QDeclarativeAudioEngine *m_engine;
};

QT_END_NAMESPACE

#endif
