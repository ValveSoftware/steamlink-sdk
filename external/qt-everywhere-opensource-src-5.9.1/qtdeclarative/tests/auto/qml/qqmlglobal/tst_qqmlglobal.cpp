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

#include <qtest.h>
#include <qqml.h>

#include <private/qqmlglobal_p.h>

class tst_qqmlglobal : public QObject
{
    Q_OBJECT
public:
    tst_qqmlglobal() {}

private slots:
    void initTestCase();

    void colorProviderWarning();
    void noGuiProviderWarning();
};

void tst_qqmlglobal::initTestCase()
{
}

void tst_qqmlglobal::colorProviderWarning()
{
    const QLatin1String expected("Warning: QQml_colorProvider: no color provider has been set!");
    QTest::ignoreMessage(QtWarningMsg, expected.data());
    QQml_colorProvider();
}

void tst_qqmlglobal::noGuiProviderWarning()
{
    QVERIFY(QQml_guiProvider()); //No GUI provider, so a default non-zero application instance is returned.
}

QTEST_MAIN(tst_qqmlglobal)

#include "tst_qqmlglobal.moc"
