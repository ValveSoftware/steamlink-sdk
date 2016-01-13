/****************************************************************************
**
** Copyright (C) 2014 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

class tst_QSerialPortInfo : public QObject
{
    Q_OBJECT
public:
    explicit tst_QSerialPortInfo();

private slots:
    void initTestCase();

    void constructors();
    void assignment();

private:
    QString m_senderPortName;
    QString m_receiverPortName;
    QStringList m_availablePortNames;
};

tst_QSerialPortInfo::tst_QSerialPortInfo()
{
}

void tst_QSerialPortInfo::initTestCase()
{
    m_senderPortName = QString::fromLocal8Bit(qgetenv("QTEST_SERIALPORT_SENDER"));
    m_receiverPortName = QString::fromLocal8Bit(qgetenv("QTEST_SERIALPORT_RECEIVER"));
    if (m_senderPortName.isEmpty() || m_receiverPortName.isEmpty()) {
        static const char message[] =
              "Test doesn't work because the names of serial ports aren't found in env.\n"
              "Please set environment variables:\n"
              " QTEST_SERIALPORT_SENDER to name of output serial port\n"
              " QTEST_SERIALPORT_RECEIVER to name of input serial port\n"
              "Specify short names of port"
#if defined(Q_OS_UNIX)
              ", like 'ttyS0'\n";
#elif defined(Q_OS_WIN32)
              ", like 'COM1'\n";
#else
              "\n";
#endif
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
        QSKIP(message);
#else
        QSKIP(message, SkipAll);
#endif
    } else {
        m_availablePortNames << m_senderPortName << m_receiverPortName;
    }
}

void tst_QSerialPortInfo::constructors()
{
    QSerialPortInfo empty;
    QVERIFY(empty.isNull());
    QSerialPortInfo empty2(QLatin1String("ABCD"));
    QVERIFY(empty2.isNull());
    QSerialPortInfo empty3(empty);
    QVERIFY(empty3.isNull());

    QSerialPortInfo exist(m_senderPortName);
    QVERIFY(!exist.isNull());
    QSerialPortInfo exist2(exist);
    QVERIFY(!exist2.isNull());
}

void tst_QSerialPortInfo::assignment()
{
    QSerialPortInfo empty;
    QVERIFY(empty.isNull());
    QSerialPortInfo empty2;
    empty2 = empty;
    QVERIFY(empty2.isNull());

    QSerialPortInfo exist(m_senderPortName);
    QVERIFY(!exist.isNull());
    QSerialPortInfo exist2;
    exist2 = exist;
    QVERIFY(!exist2.isNull());
}

QTEST_MAIN(tst_QSerialPortInfo)
#include "tst_qserialportinfo.moc"
