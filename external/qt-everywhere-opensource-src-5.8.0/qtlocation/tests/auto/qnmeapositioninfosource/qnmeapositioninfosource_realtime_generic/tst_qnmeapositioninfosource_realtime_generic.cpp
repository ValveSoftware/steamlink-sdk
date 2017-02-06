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

//TESTED_COMPONENT=src/location

#include "tst_qnmeapositioninfosource.h"

class tst_QNmeaPositionInfoSource_RealTime_Generic : public TestQGeoPositionInfoSource
{
    Q_OBJECT

public:
    tst_QNmeaPositionInfoSource_RealTime_Generic()
    {
        m_factory = new QNmeaPositionInfoSourceProxyFactory;
        /*
         * Set custom path since CI doesn't install test plugins
         */
#ifdef Q_OS_WIN
        QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() +
                                     QStringLiteral("/../../../../plugins"));
#else
        QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath()
                                         + QStringLiteral("/../../../../plugins"));
#endif
    }

    ~tst_QNmeaPositionInfoSource_RealTime_Generic()
    {
        delete m_factory;
    }

protected:
    QGeoPositionInfoSource *createTestSource()
    {
        QNmeaPositionInfoSource *source = new QNmeaPositionInfoSource(QNmeaPositionInfoSource::RealTimeMode);
        QNmeaPositionInfoSourceProxy *proxy = static_cast<QNmeaPositionInfoSourceProxy*>(m_factory->createProxy(source));
        Feeder *feeder = new Feeder(source);
        feeder->start(proxy);
        return source;
    }

private:
    QNmeaPositionInfoSourceProxyFactory *m_factory;
};

#include "tst_qnmeapositioninfosource_realtime_generic.moc"

QTEST_GUILESS_MAIN(tst_QNmeaPositionInfoSource_RealTime_Generic);
