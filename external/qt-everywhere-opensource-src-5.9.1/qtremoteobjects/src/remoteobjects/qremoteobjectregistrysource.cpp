/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#include "qremoteobjectregistrysource_p.h"
#include <QDataStream>

QT_BEGIN_NAMESPACE

QRegistrySource::QRegistrySource(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaTypeStreamOperators<QRemoteObjectSourceLocation>();
    qRegisterMetaTypeStreamOperators<QRemoteObjectSourceLocations>();
}

QRegistrySource::~QRegistrySource()
{
}

QRemoteObjectSourceLocations QRegistrySource::sourceLocations() const
{
    qCDebug(QT_REMOTEOBJECT) << "sourceLocations property requested on RegistrySource" << m_sourceLocations;
    return m_sourceLocations;
}

void QRegistrySource::removeServer(const QUrl &url)
{
    QVector<QString> results;
    typedef QRemoteObjectSourceLocations::const_iterator CustomIterator;
    const CustomIterator end = m_sourceLocations.constEnd();
    for (CustomIterator it = m_sourceLocations.constBegin(); it != end; ++it)
        if (it.value().hostUrl == url)
            results.push_back(it.key());
    Q_FOREACH (const QString &res, results)
        m_sourceLocations.remove(res);
}

void QRegistrySource::addSource(const QRemoteObjectSourceLocation &entry)
{
    qCDebug(QT_REMOTEOBJECT) << "An entry was added to the RegistrySource" << entry;
    if (m_sourceLocations.contains(entry.first)) {
        if (m_sourceLocations[entry.first].hostUrl == entry.second.hostUrl)
            qCWarning(QT_REMOTEOBJECT) << "Node warning: Ignoring Source" << entry.first
                                       << "as this Node already has a Source by that name.";
        else
            qCWarning(QT_REMOTEOBJECT) << "Node warning: Ignoring Source" << entry.first
                                       << "as another source (" << m_sourceLocations[entry.first]
                                       << ") has already registered that name.";
        return;
    }
    m_sourceLocations[entry.first] = entry.second;
    emit remoteObjectAdded(entry);
}

void QRegistrySource::removeSource(const QRemoteObjectSourceLocation &entry)
{
    if (m_sourceLocations.contains(entry.first) && m_sourceLocations[entry.first].hostUrl == entry.second.hostUrl) {
        m_sourceLocations.remove(entry.first);
        emit remoteObjectRemoved(entry);
    }
}

QT_END_NAMESPACE
