/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
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

#include <QtQml/QQmlExtensionPlugin>
#include <QtQml/QQmlEngine>
#include <QtQml/qqml.h>
#include <QDebug>
#include <QDateTime>
#include <QBasicTimer>
#include <QCoreApplication>
#include <QRemoteObjectReplica>
#include <QRemoteObjectNode>
#include "rep_timemodel_replica.h"

// Implements a "TimeModel" class with hour and minute properties
// that change on-the-minute yet efficiently sleep the rest
// of the time.

QRemoteObjectNode m_client;

class TimeModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int hour READ hour NOTIFY timeChanged)
    Q_PROPERTY(int minute READ minute NOTIFY timeChanged)
    Q_PROPERTY(bool isValid READ isValid NOTIFY isValidChanged)

public:
    TimeModel(QObject *parent = nullptr) : QObject(parent), d_ptr(nullptr)
    {
        d_ptr.reset(m_client.acquire< MinuteTimerReplica >());
        connect(d_ptr.data(), SIGNAL(hourChanged()), this, SIGNAL(timeChanged()));
        connect(d_ptr.data(), SIGNAL(minuteChanged()), this, SIGNAL(timeChanged()));
        connect(d_ptr.data(), SIGNAL(timeChanged()), this, SIGNAL(timeChanged()));
        connect(d_ptr.data(), SIGNAL(timeChanged2(QTime)), this, SLOT(test(QTime)));
        connect(d_ptr.data(), SIGNAL(sendCustom(PresetInfo)), this, SLOT(testCustom(PresetInfo)));
    }

    ~TimeModel()
    {
    }

    int minute() const { return d_ptr->minute(); }
    int hour() const { return d_ptr->hour(); }
    bool isValid() const { return d_ptr->state() == QRemoteObjectReplica::Valid; }

public slots:
    //Test a signal with parameters
    void test(QTime t)
    {
        qDebug()<<"Test"<<t;
        d_ptr->SetTimeZone(t.minute());
    }
    //Test a signal with a custom type
    void testCustom(PresetInfo info)
    {
        qDebug()<<"testCustom"<<info.presetNumber()<<info.frequency()<<info.stationName();
    }

signals:
    void timeChanged();
    void timeChanged2(QTime t);
    void sendCustom(PresetInfo info);
    void isValidChanged();

private:
    QScopedPointer<MinuteTimerReplica> d_ptr;
};

class QExampleQmlPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

public:
    void initializeEngine(QQmlEngine *engine, const char *uri)
    {
        Q_UNUSED(engine);
        Q_UNUSED(uri);
        Q_ASSERT(uri == QLatin1String("TimeExample"));
        engine->addImportPath(QStringLiteral("qrc:/qml"));
        m_client.setRegistryUrl(QUrl(QStringLiteral("local:registry")));
    }
    void registerTypes(const char *uri)
    {
        Q_ASSERT(uri == QLatin1String("TimeExample"));
        qmlRegisterType<TimeModel>(uri, 1, 0, "Time");
    }

};

#include "plugin.moc"
