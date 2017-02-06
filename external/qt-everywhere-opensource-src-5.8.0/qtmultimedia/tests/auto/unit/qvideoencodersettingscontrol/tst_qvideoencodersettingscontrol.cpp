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

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>
#include "qvideoencodersettingscontrol.h"
class MyVideEncoderControl: public QVideoEncoderSettingsControl
{
    Q_OBJECT

public:
    MyVideEncoderControl(QObject *parent = 0 ):QVideoEncoderSettingsControl(parent)
    {

    }

    ~MyVideEncoderControl()
    {

    }

    QList<QSize> supportedResolutions(const QVideoEncoderSettings &settings,bool *continuous = 0) const
    {
        Q_UNUSED(settings);
        Q_UNUSED(continuous);

        return (QList<QSize>());
    }

    QList<qreal> supportedFrameRates(const QVideoEncoderSettings &settings, bool *continuous = 0) const
    {
        Q_UNUSED(settings);
        Q_UNUSED(continuous);

        return (QList<qreal>());

    }

    QStringList supportedVideoCodecs() const
    {
        return QStringList();

    }

    QString videoCodecDescription(const QString &codecName) const
    {
        Q_UNUSED(codecName)
        return QString();

    }

    QVideoEncoderSettings videoSettings() const
    {
        return QVideoEncoderSettings();
    }

    void setVideoSettings(const QVideoEncoderSettings &settings)
    {
        Q_UNUSED(settings);
    }
};

class tst_QVideoEncoderSettingsControl: public QObject
{
    Q_OBJECT
private slots:
    void constructor();
};

void tst_QVideoEncoderSettingsControl::constructor()
{
    QObject parent;
    MyVideEncoderControl control(&parent);
}

QTEST_MAIN(tst_QVideoEncoderSettingsControl)
#include "tst_qvideoencodersettingscontrol.moc"


