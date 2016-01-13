/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "satellitemodel.h"
#include <QTimer>
#include <QDebug>

SatelliteModel::SatelliteModel(QObject *parent) :
    QAbstractListModel(parent), source(0), m_componentCompleted(false), m_running(false),
    m_runningRequested(false), demo(false), isSingle(false), singleRequestServed(false)
{
    source = QGeoSatelliteInfoSource::createDefaultSource(this);
    if (!demo && !source) {
        qWarning() << "No satellite data source found. Changing to demo mode.";
        demo = true;
    }
    if (!demo) {
        source->setUpdateInterval(3000);
        connect(source, SIGNAL(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)),
                this, SLOT(satellitesInViewUpdated(QList<QGeoSatelliteInfo>)));
        connect(source, SIGNAL(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)),
                this, SLOT(satellitesInUseUpdated(QList<QGeoSatelliteInfo>)));
        connect(source, SIGNAL(error(QGeoSatelliteInfoSource::Error)),
                this, SLOT(error(QGeoSatelliteInfoSource::Error)));
    }

    if (demo) {
        timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(updateDemoData()));
        timer->start(3000);
    }
}

int SatelliteModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (!source && !demo)
        return 0;

    return knownSatellites.count();
}

QVariant SatelliteModel::data(const QModelIndex &index, int role) const
{
    if (!demo && !source)
        return QVariant();

    if (!index.isValid() || index.row() < 0)
        return QVariant();

    if (index.row() >= knownSatellites.count()) {
        qWarning() << "SatelliteModel: Index out of bound";
        return QVariant();
    }

    const QGeoSatelliteInfo &info = knownSatellites.at(index.row());
    switch (role) {
    case IdentifierRole:
        return info.satelliteIdentifier();
    case InUseRole:
        return satellitesInUse.contains(info.satelliteIdentifier());
    case SignalStrengthRole:
        return info.signalStrength();
    case ElevationRole:
        if (!info.hasAttribute(QGeoSatelliteInfo::Elevation))
            return QVariant();
        return info.attribute(QGeoSatelliteInfo::Elevation);
    case AzimuthRole:
        if (!info.hasAttribute(QGeoSatelliteInfo::Azimuth))
            return QVariant();
        return info.attribute(QGeoSatelliteInfo::Azimuth);
    default:
        break;

    }

    return QVariant();
}

QHash<int, QByteArray> SatelliteModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames.insert(IdentifierRole, "satelliteIdentifier");
    roleNames.insert(InUseRole, "isInUse");
    roleNames.insert(SignalStrengthRole, "signalStrength");
    roleNames.insert(ElevationRole, "elevation");
    roleNames.insert(AzimuthRole, "azimuth");
    return roleNames;
}

void SatelliteModel::componentComplete()
{
    m_componentCompleted = true;
    if (m_runningRequested)
        setRunning(true);
}

bool SatelliteModel::running() const
{
    return m_running;
}

bool SatelliteModel::isSingleRequest() const
{
    return isSingle;
}

void SatelliteModel::setSingleRequest(bool single)
{
    if (running()) {
        qWarning() << "Cannot change single request mode while running";
        return;
    }

    if (single != isSingle) { //flag changed
        isSingle = single;
        emit singleRequestChanged();
    }
}

void SatelliteModel::setRunning(bool isActive)
{
    if (!source && !demo)
        return;

    if (!m_componentCompleted) {
        m_runningRequested = isActive;
        return;
    }

    if (m_running == isActive)
        return;

    m_running = isActive;

    if (m_running) {
        clearModel();
        if (demo)
            timer->start(2000);
        else if (isSingleRequest())
            source->requestUpdate(10000);
        else
            source->startUpdates();

        if (demo)
            singleRequestServed = false;
    } else {
        if (demo)
            timer->stop();
        else if (!isSingleRequest())
            source->stopUpdates();
    }


    Q_EMIT runningChanged();
}

int SatelliteModel::entryCount() const
{
    return knownSatellites.count();
}

bool SatelliteModel::canProvideSatelliteInfo() const
{
    return !demo;
}

void SatelliteModel::clearModel()
{
    beginResetModel();
    knownSatelliteIds.clear();
    knownSatellites.clear();
    satellitesInUse.clear();
    endResetModel();
}

void SatelliteModel::updateDemoData()
{
    static bool flag = true;
    QList<QGeoSatelliteInfo> satellites;
    if (flag) {
        for (int i = 0; i<5; i++) {
            QGeoSatelliteInfo info;
            info.setSatelliteIdentifier(i);
            info.setSignalStrength(20 + 20*i);
            satellites.append(info);
        }
    } else {
        for (int i = 0; i<9; i++) {
            QGeoSatelliteInfo info;
            info.setSatelliteIdentifier(i*2);
            info.setSignalStrength(20 + 10*i);
            satellites.append(info);
        }
    }


    satellitesInViewUpdated(satellites);
    flag ? satellitesInUseUpdated(QList<QGeoSatelliteInfo>() << satellites.at(2))
         : satellitesInUseUpdated(QList<QGeoSatelliteInfo>() << satellites.at(3));
    flag = !flag;

    emit errorFound(flag);

    if (isSingleRequest() && !singleRequestServed) {
        singleRequestServed = true;
        setRunning(false);
    }
}

void SatelliteModel::error(QGeoSatelliteInfoSource::Error error)
{
    emit errorFound((int)error);
}

QT_BEGIN_NAMESPACE
inline bool operator<(const QGeoSatelliteInfo& a, const QGeoSatelliteInfo& b)
{
    return a.satelliteIdentifier() < b.satelliteIdentifier();
}
QT_END_NAMESPACE

void SatelliteModel::satellitesInViewUpdated(const QList<QGeoSatelliteInfo> &infos)
{
    if (!running())
        return;

    int oldEntryCount = knownSatellites.count();


    QSet<int> satelliteIdsInUpdate;
    foreach (const QGeoSatelliteInfo &info, infos)
        satelliteIdsInUpdate.insert(info.satelliteIdentifier());

    QSet<int> toBeRemoved = knownSatelliteIds - satelliteIdsInUpdate;

    //We reset the model as in reality just about all entry values change
    //and there are generally a lot of inserts and removals each time
    //Hence we don't bother with complex model update logic beyond resetModel()
    beginResetModel();

    knownSatellites = infos;

    //sort them for presentation purposes
    std::sort(knownSatellites.begin(), knownSatellites.end());

    //remove old "InUse" data
    //new satellites are by default not in "InUse"
    //existing satellites keep their "inUse" state
    satellitesInUse -= toBeRemoved;

    knownSatelliteIds = satelliteIdsInUpdate;
    endResetModel();

    if (oldEntryCount != knownSatellites.count())
        emit entryCountChanged();
}

void SatelliteModel::satellitesInUseUpdated(const QList<QGeoSatelliteInfo> &infos)
{
    if (!running())
        return;

    beginResetModel();

    satellitesInUse.clear();
    foreach (const QGeoSatelliteInfo &info, infos)
        satellitesInUse.insert(info.satelliteIdentifier());

    endResetModel();
}

