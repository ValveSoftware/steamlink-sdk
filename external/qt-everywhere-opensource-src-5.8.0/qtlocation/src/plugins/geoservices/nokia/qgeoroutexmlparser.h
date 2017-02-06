/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QROUTEXMLPARSER_H
#define QROUTEXMLPARSER_H

#include <QtCore/QObject>
#include <QtCore/QRunnable>
#include <QtCore/QString>
#include <QtCore/QList>

#include <QtLocation/QGeoRouteRequest>
#include <QtLocation/QGeoRouteSegment>
#include <QtLocation/QGeoManeuver>

QT_BEGIN_NAMESPACE

class QXmlStreamReader;
class QGeoRoute;
class QGeoCoordinate;
class QGeoRectangle;

class QGeoManeuverContainer
{
public:
    QGeoManeuver maneuver;
    QString id;
    QString toId;
};

class QGeoRouteSegmentContainer
{
public:
    QGeoRouteSegment segment;
    QString id;
    QString maneuverId;
};

class QGeoDynamicSpeedInfoContainer
{
public:
    QGeoDynamicSpeedInfoContainer();

public:
    double trafficSpeed;
    double baseSpeed;
    int trafficTime;
    int baseTime;
};

class QGeoRouteXmlParser : public QObject, public QRunnable
{
    Q_OBJECT

public:
    QGeoRouteXmlParser(const QGeoRouteRequest &request);
    ~QGeoRouteXmlParser();

    void parse(const QByteArray &data);
    void run();

signals:
    void results(const QList<QGeoRoute> &routes);
    void error(const QString &errorString);

private:
    bool parseRootElement();
    bool parseRoute(QGeoRoute *route);
    //bool parseWaypoint(QGeoRoute *route);
    bool parseCoordinates(QGeoCoordinate &coord);
    bool parseMode(QGeoRoute *route);
    bool parseSummary(QGeoRoute *route);
    bool parseGeoPoints(const QString &strPoints, QList<QGeoCoordinate> *geoPoints, const QString &elementName);
    bool parseLeg();
    bool parseManeuver();
    bool parseLink();
    bool postProcessRoute(QGeoRoute *route);

    bool parseBoundingBox(QGeoRectangle &bounds);
    bool parseDynamicSpeedInfo(QGeoDynamicSpeedInfoContainer &speedInfo);

    QGeoRouteRequest m_request;
    QByteArray m_data;
    QXmlStreamReader *m_reader;

    QList<QGeoRoute> m_results;
    QList<QGeoManeuverContainer> m_maneuvers;
    QList<QGeoRouteSegmentContainer> m_segments;
};

QT_END_NAMESPACE

#endif
