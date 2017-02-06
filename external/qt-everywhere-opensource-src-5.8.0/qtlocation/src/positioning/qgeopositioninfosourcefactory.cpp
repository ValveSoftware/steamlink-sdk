/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "qgeopositioninfosourcefactory.h"

QT_BEGIN_NAMESPACE

/*!
  \class QGeoPositionInfoSourceFactory
  \inmodule QtPositioning
  \since 5.2

  \brief The QGeoPositionInfoSourceFactory class is a factory class used
  as the plugin interface for external providers of positioning data.

  The other functions must be overridden by all plugins, other than
  sourcePriority() which defaults to returning 0. Higher values of
  priority will be preferred to lower ones.
*/

/*!
  \fn QGeoPositionInfoSource *QGeoPositionInfoSourceFactory::positionInfoSource(QObject *parent)

  Returns a new QGeoPositionInfoSource associated with this plugin
  with parent \a parent. Can also return 0, in which case the plugin
  loader will use the factory with the next highest priority.
  */

/*!
  \fn QGeoSatelliteInfoSource *QGeoPositionInfoSourceFactory::satelliteInfoSource(QObject *parent)

  Returns a new QGeoSatelliteInfoSource associated with this plugin
  with parent \a parent. Can also return 0, in which case the plugin
  loader will use the factory with the next highest priority.
  */

/*!
  \fn QGeoAreaMonitorSource *QGeoPositionInfoSourceFactory::areaMonitor(QObject *parent);

  Returns a new QGeoAreaMonitorSource associated with this plugin with parent \a parent.
  Can also return 0, in which case the plugin loader will use the factory with the
  next highest priority.
  */

/*!
    Destroys the position info source factory.
*/
QGeoPositionInfoSourceFactory::~QGeoPositionInfoSourceFactory()
{}

QT_END_NAMESPACE
