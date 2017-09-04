/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>

#include <QtDataVisualization/QTouch3DInputHandler>

using namespace QtDataVisualization;

class tst_input: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void construct();

    void initialProperties();
    void initializeProperties();

private:
    QTouch3DInputHandler *m_input;
};

void tst_input::initTestCase()
{
}

void tst_input::cleanupTestCase()
{
}

void tst_input::init()
{
    m_input = new QTouch3DInputHandler();
}

void tst_input::cleanup()
{
    delete m_input;
}

void tst_input::construct()
{
    QTouch3DInputHandler *input = new QTouch3DInputHandler();
    QVERIFY(input);
    delete input;
}

void tst_input::initialProperties()
{
    QVERIFY(m_input);

    // Common (from Q3DInputHandler and QAbstract3DInputHandler)
    QCOMPARE(m_input->isRotationEnabled(), true);
    QCOMPARE(m_input->isSelectionEnabled(), true);
    QCOMPARE(m_input->isZoomAtTargetEnabled(), true);
    QCOMPARE(m_input->isZoomEnabled(), true);
    QCOMPARE(m_input->inputPosition(), QPoint(0, 0));
    QCOMPARE(m_input->inputView(), QAbstract3DInputHandler::InputViewNone);
    QVERIFY(!m_input->scene());
}

void tst_input::initializeProperties()
{
    QVERIFY(m_input);

    // Common (from Q3DInputHandler and QAbstract3DInputHandler)
    m_input->setRotationEnabled(false);
    m_input->setSelectionEnabled(false);
    m_input->setZoomAtTargetEnabled(false);
    m_input->setZoomEnabled(false);
    m_input->setInputPosition(QPoint(100, 100));
    m_input->setInputView(QAbstract3DInputHandler::InputViewOnPrimary);

    QCOMPARE(m_input->isRotationEnabled(), false);
    QCOMPARE(m_input->isSelectionEnabled(), false);
    QCOMPARE(m_input->isZoomAtTargetEnabled(), false);
    QCOMPARE(m_input->isZoomEnabled(), false);
    QCOMPARE(m_input->inputPosition(), QPoint(100, 100));
    QCOMPARE(m_input->inputView(), QAbstract3DInputHandler::InputViewOnPrimary);
}

// TODO: QTRD-3380 (mouse/touch events)

QTEST_MAIN(tst_input)
#include "tst_input.moc"
