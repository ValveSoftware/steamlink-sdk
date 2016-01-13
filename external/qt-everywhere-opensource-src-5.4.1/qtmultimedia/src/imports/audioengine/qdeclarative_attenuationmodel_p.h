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

#ifndef QDECLARATIVEATTENUATIONMODEL_P_H
#define QDECLARATIVEATTENUATIONMODEL_P_H

#include <QtQml/qqml.h>
#include <QtQml/qqmlcomponent.h>
#include <QVector3D>

QT_BEGIN_NAMESPACE

class QDeclarativeAttenuationModel : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString name READ name WRITE setName)

public:
    QDeclarativeAttenuationModel(QObject *parent = 0);
    ~QDeclarativeAttenuationModel();

    void classBegin();
    void componentComplete();

    QString name() const;
    void setName(const QString& name);

    virtual qreal calculateGain(const QVector3D &listenerPosition, const QVector3D &sourcePosition) const = 0;

protected:
    bool m_complete;
    QString m_name;

private:
    Q_DISABLE_COPY(QDeclarativeAttenuationModel);
};

class QDeclarativeAttenuationModelLinear : public QDeclarativeAttenuationModel
{
    Q_OBJECT
    Q_PROPERTY(qreal start READ startDistance WRITE setStartDistance CONSTANT)
    Q_PROPERTY(qreal end READ endDistance WRITE setEndDistance CONSTANT)

public:
    QDeclarativeAttenuationModelLinear(QObject *parent = 0);

    void componentComplete();

    qreal startDistance() const;
    void setStartDistance(qreal startDist);

    qreal endDistance() const;
    void setEndDistance(qreal endDist);

    qreal calculateGain(const QVector3D &listenerPosition, const QVector3D &sourcePosition) const;

private:
    Q_DISABLE_COPY(QDeclarativeAttenuationModelLinear);
    qreal m_start;
    qreal m_end;
};

class QDeclarativeAttenuationModelInverse : public QDeclarativeAttenuationModel
{
    Q_OBJECT
    Q_PROPERTY(qreal start READ referenceDistance WRITE setReferenceDistance CONSTANT)
    Q_PROPERTY(qreal end READ maxDistance WRITE setMaxDistance CONSTANT)
    Q_PROPERTY(qreal rolloff READ rolloffFactor WRITE setRolloffFactor CONSTANT)

public:
    QDeclarativeAttenuationModelInverse(QObject *parent = 0);

    void componentComplete();

    qreal referenceDistance() const;
    void setReferenceDistance(qreal referenceDistance);

    qreal maxDistance() const;
    void setMaxDistance(qreal maxDistance);

    qreal rolloffFactor() const;
    void setRolloffFactor(qreal rolloffFactor);

    qreal calculateGain(const QVector3D &listenerPosition, const QVector3D &sourcePosition) const;

private:
    Q_DISABLE_COPY(QDeclarativeAttenuationModelInverse);
    qreal m_ref;
    qreal m_max;
    qreal m_rolloff;
};

QT_END_NAMESPACE

#endif
