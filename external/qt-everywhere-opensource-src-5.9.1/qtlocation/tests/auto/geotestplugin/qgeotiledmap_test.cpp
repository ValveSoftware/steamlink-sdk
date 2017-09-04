/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qgeotiledmap_test.h"
#include <QtLocation/private/qgeotiledmap_p_p.h>
#include <QtLocation/private/qgeomapparameter_p.h>

QT_USE_NAMESPACE

class QGeoTiledMapTestPrivate: public QGeoTiledMapPrivate
{
    Q_DECLARE_PUBLIC(QGeoTiledMapTest)
public:
    QGeoTiledMapTestPrivate(QGeoTiledMappingManagerEngine *engine)
        : QGeoTiledMapPrivate(engine)
    {

    }

    ~QGeoTiledMapTestPrivate()
    {

    }

    void addParameter(QGeoMapParameter *param) override
    {
        Q_Q(QGeoTiledMapTest);
        if (param->type() == QStringLiteral("cameraCenter_test")) {
            // We assume that cameraCenter_test parameters have a QGeoCoordinate property named "center"
            // Handle the parameter
            QGeoCameraData cameraData = m_cameraData;
            QGeoCoordinate newCenter = param->property("center").value<QGeoCoordinate>();
            cameraData.setCenter(newCenter);
            q->setCameraData(cameraData);
            // Connect for further changes handling
            q->connect(param, SIGNAL(propertyUpdated(QGeoMapParameter *, const char *)),
                       q, SLOT(onCameraCenter_testChanged(QGeoMapParameter*, const char*)));

        }
    }
    void removeParameter(QGeoMapParameter *param) override
    {
        Q_Q(QGeoTiledMapTest);
        param->disconnect(q);
    }
};

QGeoTiledMapTest::QGeoTiledMapTest(QGeoTiledMappingManagerEngine *engine, QObject *parent)
:   QGeoTiledMap(*new QGeoTiledMapTestPrivate(engine), engine, parent), m_engine(engine)
{
}

void QGeoTiledMapTest::onCameraCenter_testChanged(QGeoMapParameter *param, const char *propertyName)
{
    Q_D(QGeoTiledMapTest);
    if (strcmp(propertyName, "center") == 0) {
        QGeoCameraData cameraData = d->m_cameraData;
        // Not testing for propertyName as this param has only one allowed property
        QGeoCoordinate newCenter = param->property(propertyName).value<QGeoCoordinate>();
        cameraData.setCenter(newCenter);
        setCameraData(cameraData);
    }
}
