/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#ifndef EXPLORER_H
#define EXPLORER_H

#include <QMainWindow>
#include <ui_explorer.h>
#include <qsensor.h>


class Explorer : public QMainWindow, public QSensorFilter
{
    Q_OBJECT
public:
    Explorer(QWidget *parent = 0);
    ~Explorer();

    bool filter(QSensorReading *reading);

private slots:
    void loadSensors();
    void on_sensors_currentItemChanged();
    void on_sensorprops_itemChanged(QTableWidgetItem *item);
    void on_start_clicked();
    void on_stop_clicked();
    void sensor_changed();
    void adjustSizes();
    void loadSensorProperties();

private:
    void showEvent(QShowEvent *event);
    void resizeEvent(QResizeEvent *event);

    void clearReading();
    void loadReading();
    void clearSensorProperties();
    void adjustTableColumns(QTableWidget *table);
    void resizeSensors();

    Ui::Explorer ui;
    QSensor *m_sensor;
    bool ignoreItemChanged;
};

#endif

