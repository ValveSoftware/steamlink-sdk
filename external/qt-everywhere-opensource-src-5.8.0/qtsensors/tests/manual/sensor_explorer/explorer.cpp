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

#include "explorer.h"
#include <QTimer>
#include <QDebug>
#include <qsensor.h>
#include <QMetaObject>
#include <QMetaProperty>

Explorer::Explorer(QWidget *parent)
    : QMainWindow(parent)
    , m_sensor(0)
    , ignoreItemChanged(false)
{
    ui.setupUi(this);
    // Clear out example data from the .ui file
    ui.sensors->clear();
    clearSensorProperties();
    clearReading();

    // Force types to be registered
    (void)QSensor::sensorTypes();
    // Listen for changes to the registered types
    QSensor *sensor = new QSensor(QByteArray(), this);
    connect(sensor, SIGNAL(availableSensorsChanged()), this, SLOT(loadSensors()));
}

Explorer::~Explorer()
{
}

void Explorer::loadSensors()
{
    qDebug() << "Explorer::loadSensors";

    // Clear out anything that's in there now
    ui.sensors->clear();

    foreach (const QByteArray &type, QSensor::sensorTypes()) {
        qDebug() << "Found type" << type;
        foreach (const QByteArray &identifier, QSensor::sensorsForType(type)) {
            qDebug() << "Found identifier" << identifier;
            // Don't put in sensors we can't connect to
            QSensor sensor(type);
            sensor.setIdentifier(identifier);
            if (!sensor.connectToBackend()) {
                qDebug() << "Couldn't connect to" << identifier;
                continue;
            }

            qDebug() << "Adding identifier" << identifier;
            QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << QString::fromLatin1(identifier));
            item->setData(0, Qt::UserRole, QString::fromLatin1(type));
            ui.sensors->addTopLevelItem(item);
        }
    }

    if (ui.sensors->topLevelItemCount() == 0) {
        QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << tr("No Sensors Found"));
        item->setData(0, Qt::UserRole, QString());
        ui.sensors->addTopLevelItem(item);
    }

    ui.sensors->setCurrentItem(0);
    ui.scrollArea->hide();

    resizeSensors();
}

void Explorer::resizeSensors()
{
    ui.sensors->resizeColumnToContents(0);
    int length = ui.sensors->header()->length() + 4;
    ui.sensors->setFixedWidth(length);
}

void Explorer::on_sensors_currentItemChanged()
{
    qDebug() << "Explorer::sensorSelected";

    // Clear out anything that's in there now
    if (m_sensor) {
        delete m_sensor;
        m_sensor = 0;
    }
    clearSensorProperties();
    clearReading();
    ui.scrollArea->hide();

    // Check that we've selected an item
    QTreeWidgetItem *item = ui.sensors->currentItem();
    if (!item) {
        qWarning() << "Didn't select an item!";
        return;
    }

    QByteArray type = item->data(0, Qt::UserRole).toString().toLatin1();
    QByteArray identifier = item->data(0, Qt::DisplayRole).toString().toLatin1();

    if (type.isEmpty()) {
        // Uh oh, there aren't any sensors.
        // The user has clicked the dummy list entry so just ignore it.
        return;
    }

    // Connect to the sensor so we can probe it
    m_sensor = new QSensor(type, this);
    connect(m_sensor, SIGNAL(readingChanged()), this, SLOT(sensor_changed()));
    m_sensor->setIdentifier(identifier);
    if (!m_sensor->connectToBackend()) {
        delete m_sensor;
        m_sensor = 0;
        qWarning() << "Can't connect to the sensor!";
        return;
    }

    ui.scrollArea->show();
    loadSensorProperties();
    loadReading();

    adjustTableColumns(ui.sensorprops);
    adjustTableColumns(ui.reading);
    QTimer::singleShot(100, this, SLOT(adjustSizes()));
}

void Explorer::clearReading()
{
    ui.reading->setRowCount(0);
}

void Explorer::loadReading()
{
    // Probe the reading using Qt's meta-object facilities
    QSensorReading *reading = m_sensor->reading();
    const QMetaObject *mo = reading->metaObject();
    int firstProperty = QSensorReading::staticMetaObject.propertyOffset();

    ui.reading->setRowCount(mo->propertyCount() - firstProperty);

    for (int i = firstProperty; i < mo->propertyCount(); ++i) {
        int row = i - firstProperty;
        QTableWidgetItem *index;
        if (row == 0)
            // timestamp is not available via index
            index = new QTableWidgetItem(QLatin1String("N/A"));
        else
            index = new QTableWidgetItem(QVariant(row - 1).toString());
        QTableWidgetItem *prop = new QTableWidgetItem(mo->property(i).name());
        QString typeName = QLatin1String(mo->property(i).typeName());
        int crap = typeName.lastIndexOf("::");
        if (crap != -1)
            typeName = typeName.mid(crap + 2);
        QTableWidgetItem *type = new QTableWidgetItem(typeName);
        QTableWidgetItem *value = new QTableWidgetItem();

        index->setFlags(value->flags() ^ Qt::ItemIsEditable);
        prop->setFlags(value->flags() ^ Qt::ItemIsEditable);
        type->setFlags(value->flags() ^ Qt::ItemIsEditable);
        value->setFlags(value->flags() ^ Qt::ItemIsEditable);

        ui.reading->setItem(row, 0, index);
        ui.reading->setItem(row, 1, prop);
        ui.reading->setItem(row, 2, type);
        ui.reading->setItem(row, 3, value);
    }
}

void Explorer::clearSensorProperties()
{
    ui.sensorprops->setRowCount(0);
}

void Explorer::loadSensorProperties()
{
    ignoreItemChanged = true;

    // Probe the sensor using Qt's meta-object facilities
    const QMetaObject *mo = m_sensor->metaObject();
    int firstProperty = QSensor::staticMetaObject.propertyOffset();

    int rows = mo->propertyCount() - firstProperty;
    ui.sensorprops->setRowCount(rows);

    int offset = 0;
    for (int i = firstProperty; i < mo->propertyCount(); ++i) {
        int row = i - firstProperty - offset;
        QLatin1String name(mo->property(i).name());
        if (name == "identifier" ||
            //name == "type" ||
            name == "reading" ||
            name == "connected" ||
            name == "running" ||
            name == "supportsPolling") {
            ++offset;
            continue;
        }
        QTableWidgetItem *prop = new QTableWidgetItem(name);
        QString typeName = QLatin1String(mo->property(i).typeName());
        int crap = typeName.lastIndexOf("::");
        if (crap != -1)
            typeName = typeName.mid(crap + 2);
        QTableWidgetItem *type = new QTableWidgetItem(typeName);
        QVariant v = mo->property(i).read(m_sensor);
        QString val;
        if (typeName == "qrangelist") {
            qrangelist rl = v.value<qrangelist>();
            QStringList out;
            foreach (const qrange &r, rl) {
                if (r.first == r.second)
                    out << QString("%1 Hz").arg(r.first);
                else
                    out << QString("%1-%2 Hz").arg(r.first).arg(r.second);
            }
            val = out.join(", ");
        } else if (typeName == "qoutputrangelist") {
            qoutputrangelist rl = v.value<qoutputrangelist>();
            QStringList out;
            foreach (const qoutputrange &r, rl) {
                out << QString("(%1, %2) += %3").arg(r.minimum).arg(r.maximum).arg(r.accuracy);
            }
            val = out.join(", ");
        } else {
            val = v.toString();
        }
        QTableWidgetItem *value = new QTableWidgetItem(val);

        prop->setFlags(value->flags() ^ Qt::ItemIsEditable);
        type->setFlags(value->flags() ^ Qt::ItemIsEditable);
        if (!mo->property(i).isWritable()) {
            // clear the editable flag
            value->setFlags(value->flags() ^ Qt::ItemIsEditable);
        }

        ui.sensorprops->setItem(row, 0, prop);
        ui.sensorprops->setItem(row, 1, type);
        ui.sensorprops->setItem(row, 2, value);
    }

    // We don't add all properties
    ui.sensorprops->setRowCount(rows - offset);

    ignoreItemChanged = false;
}

void Explorer::showEvent(QShowEvent *event)
{
    // Once we're visible, load the sensors
    // (don't delay showing the UI while we load plugins, connect to backends, etc.)
    QTimer::singleShot(0, this, SLOT(loadSensors()));

    QMainWindow::showEvent(event);
}

// Resize columns to fit the space.
// This shouldn't be so hard!
void Explorer::adjustTableColumns(QTableWidget *table)
{
    if (table->rowCount() == 0) {
        table->setFixedHeight(0);
        return;
    }

    // At least this is easy to do
    table->resizeColumnsToContents();
    int length = table->verticalHeader()->length();
    length += (length / static_cast<qreal>(table->verticalHeader()->count())); // Add 1 more (the header itself)
    table->setFixedHeight(length);

    int columns = table->columnCount();
    QList<int> width;
    int suggestedWidth = 0;
    for (int i = 0; i < columns; ++i) {
        int cwidth = table->columnWidth(i);
        width << cwidth;
        suggestedWidth += cwidth;
    }

    int actualWidth = table->size().width();
    //qDebug() << "suggestedWidth" << suggestedWidth << "actualWidth" << actualWidth;

    // We only scale the columns up, we don't scale down
    if (actualWidth <= suggestedWidth)
        return;

    qreal multiplier = actualWidth / static_cast<qreal>(suggestedWidth);
    int currentSpace = 4;
    for (int i = 0; i < columns; ++i) {
        width[i] = multiplier * width[i];
        currentSpace += width[i];
    }

    // It ends up too big due to cell decorations or something.
    // Make things smaller one pixel at a time in round robin fashion until we're good.
    int i = 0;
    while (currentSpace > actualWidth) {
        --width[i];
        --currentSpace;
        i = (i + 1) % columns;
    }

    for (int i = 0; i < columns; ++i) {
        table->setColumnWidth(i, width[i]);
    }

    table->setMinimumWidth(suggestedWidth);
}

void Explorer::adjustSizes()
{
    adjustTableColumns(ui.reading);
    adjustTableColumns(ui.sensorprops);
}

void Explorer::resizeEvent(QResizeEvent *event)
{
    resizeSensors();
    adjustSizes();

    QMainWindow::resizeEvent(event);
}

void Explorer::on_start_clicked()
{
    m_sensor->start();
    QTimer::singleShot(0, this, SLOT(loadSensorProperties()));
}

void Explorer::on_stop_clicked()
{
    m_sensor->stop();
    QTimer::singleShot(0, this, SLOT(loadSensorProperties()));
}

void Explorer::sensor_changed()
{
    QSensorReading *reading = m_sensor->reading();
    filter(reading);
}

bool Explorer::filter(QSensorReading *reading)
{
    const QMetaObject *mo = reading->metaObject();
    int firstProperty = QSensorReading::staticMetaObject.propertyOffset();

    for (int i = firstProperty; i < mo->propertyCount(); ++i) {
        int row = i - firstProperty;
        QString typeName = QLatin1String(mo->property(i).typeName());
        int crap = typeName.lastIndexOf("::");
        if (crap != -1)
            typeName = typeName.mid(crap + 2);
        QLatin1String name(mo->property(i).name());
        QTableWidgetItem *value = ui.reading->item(row, 3);
        QVariant val = mo->property(i).read(reading);
        if (typeName == "LightLevel") {
            QString text;
            switch (val.toInt()) {
            case 1:
                text = "Dark";
                break;
            case 2:
                text = "Twilight";
                break;
            case 3:
                text = "Light";
                break;
            case 4:
                text = "Bright";
                break;
            case 5:
                text = "Sunny";
                break;
            default:
                text = "Undefined";
                break;
            }
            value->setText(text);
        } else {
            value->setText(val.toString());
        }
    }

    adjustTableColumns(ui.reading);
    //QTimer::singleShot(0, this, SLOT(adjustSizes()));

    return false;
}

void Explorer::on_sensorprops_itemChanged(QTableWidgetItem *item)
{
    if (ignoreItemChanged)
        return;
    if (!(item->flags() & Qt::ItemIsEditable))
        return;

    int row = item->row();
    QString name = ui.sensorprops->item(row, 0)->text();
    QVariant value = item->text();

    qDebug() << "setProperty" << name << value;
    m_sensor->setProperty(name.toLatin1().constData(), QVariant(value));

    QTimer::singleShot(0, this, SLOT(loadSensorProperties()));
}

