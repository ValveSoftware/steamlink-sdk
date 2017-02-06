/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the QtSerialBus module.
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

#ifndef CANBUSUTIL_H
#define CANBUSUTIL_H

#include <QObject>
#include <QTextStream>
#include <QCoreApplication>
#include <QScopedPointer>

#include "readtask.h"

QT_BEGIN_NAMESPACE

class QCanBusFrame;

QT_END_NAMESPACE

class CanBusUtil : public QObject
{
    Q_OBJECT
public:
    explicit CanBusUtil(QTextStream &output, QCoreApplication &app, QObject *parent = nullptr);

    void setShowTimeStamp(bool showTimeStamp);
    bool start(const QString &pluginName, const QString &deviceName, const QString &data = QString());
    void printPlugins();

private:
    bool parseDataField(qint32 &id, QString &payload);
    bool setFrameFromPayload(QString payload, QCanBusFrame *frame);
    bool connectCanDevice();
    bool startListeningOnCanDevice();
    bool sendData();

private:
    QCanBus *m_canBus;
    QTextStream &m_output;
    QCoreApplication &m_app;
    bool m_listening;
    QString m_pluginName;
    QString m_deviceName;
    QString m_data;
    QScopedPointer<QCanBusDevice> m_canDevice;
    ReadTask *m_readTask;
};

#endif // CANBUSUTIL_H
