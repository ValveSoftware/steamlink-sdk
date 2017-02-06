/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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


#include <QtTest/QTest>
#include <Qt3DExtras/qforwardrenderer.h>
#include <Qt3DCore/qentity.h>
#include <QObject>
#include <QSignalSpy>

class tst_QForwardRenderer : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        qRegisterMetaType<Qt3DCore::QEntity*>();
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DExtras::QForwardRenderer forwardRenderer;

        // THEN
        QVERIFY(forwardRenderer.surface() == nullptr);
        QCOMPARE(forwardRenderer.viewportRect(), QRectF(0.0f, 0.0f, 1.0f, 1.0f));
        QCOMPARE(forwardRenderer.clearColor(), QColor(Qt::white));
        QVERIFY(forwardRenderer.camera() == nullptr);
        QCOMPARE(forwardRenderer.externalRenderTargetSize(), QSize());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DExtras::QForwardRenderer forwardRenderer;

        {
            // WHEN
            QSignalSpy spy(&forwardRenderer, SIGNAL(surfaceChanged(QObject *)));
            QWindow newValue;
            forwardRenderer.setSurface(&newValue);

            // THEN
            QCOMPARE(forwardRenderer.surface(), &newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            forwardRenderer.setSurface(&newValue);

            // THEN
            QCOMPARE(forwardRenderer.surface(), &newValue);
            QCOMPARE(spy.count(), 0);

            // WHEN
            forwardRenderer.setSurface(nullptr);

            // THEN
            QVERIFY(forwardRenderer.surface() == nullptr);
            QCOMPARE(spy.count(), 1);
        }
        {
            // WHEN
            QSignalSpy spy(&forwardRenderer, SIGNAL(viewportRectChanged(QRectF)));
            const QRectF newValue = QRectF(0.5, 0.5, 0.5, 0.5);
            forwardRenderer.setViewportRect(newValue);

            // THEN
            QCOMPARE(forwardRenderer.viewportRect(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            forwardRenderer.setViewportRect(newValue);

            // THEN
            QCOMPARE(forwardRenderer.viewportRect(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&forwardRenderer, SIGNAL(clearColorChanged(QColor)));
            const QColor newValue = QColor(Qt::red);
            forwardRenderer.setClearColor(newValue);

            // THEN
            QCOMPARE(forwardRenderer.clearColor(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            forwardRenderer.setClearColor(newValue);

            // THEN
            QCOMPARE(forwardRenderer.clearColor(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&forwardRenderer, SIGNAL(cameraChanged(Qt3DCore::QEntity *)));
            Qt3DCore::QEntity newValue;
            forwardRenderer.setCamera(&newValue);

            // THEN
            QCOMPARE(forwardRenderer.camera(), &newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            forwardRenderer.setCamera(&newValue);

            // THEN
            QCOMPARE(forwardRenderer.camera(), &newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&forwardRenderer, SIGNAL(externalRenderTargetSizeChanged(QSize)));
            const QSize newValue = QSize(454, 427);
            forwardRenderer.setExternalRenderTargetSize(newValue);

            // THEN
            QCOMPARE(forwardRenderer.externalRenderTargetSize(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            forwardRenderer.setExternalRenderTargetSize(newValue);

            // THEN
            QCOMPARE(forwardRenderer.externalRenderTargetSize(), newValue);
            QCOMPARE(spy.count(), 0);

        }
    }

};

QTEST_MAIN(tst_QForwardRenderer)

#include "tst_qforwardrenderer.moc"
