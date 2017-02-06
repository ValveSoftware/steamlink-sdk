/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include <QList>

#include "qsensorbackend.h"

typedef QSensorBackend* (*CreateFunc) (QSensor *sensor);
class Record
{
public:
    QByteArray type;
    CreateFunc func;
};
static QList<Record> records;

static bool registerTestBackend(const char *className, CreateFunc func)
{
    Record record;
    record.type = className;
    record.func = func;
    records << record;
    return true;
}

#define REGISTER_TOO
#include "test_backends.h"
#include <QDebug>

class BackendFactory : public QSensorBackendFactory
{
    QSensorBackend *createBackend(QSensor *sensor)
    {
        foreach (const Record &record, records) {
            if (sensor->identifier() == record.type) {
                return record.func(sensor);
            }
        }
        return 0;
    };
};
static BackendFactory factory;

void register_test_backends()
{
    foreach (const Record &record, records) {
        QSensorManager::registerBackend(record.type, record.type, &factory);
    }
}

void unregister_test_backends()
{
    foreach (const Record &record, records) {
        QSensorManager::unregisterBackend(record.type, record.type);
    }
}

