/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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
#include <Qt3DQuick/private/quick3dnode_p.h>
#include <QObject>
#include <qmlscenereader.h>

class tst_Quick3DNode : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkPropertyTrackingOverrides_V9()
    {
        // GIVEN
        QmlSceneReader sceneReader(QUrl("qrc:/quick3dnodev9.qml"));

        // THEN
        QVERIFY(sceneReader.root() != nullptr);
        Qt3DCore::QNode *node = static_cast<Qt3DCore::QNode *>(sceneReader.root());
        QCOMPARE(node->propertyTracking(QLatin1String("enabled")), Qt3DCore::QNode::DontTrackValues);
        QCOMPARE(node->propertyTracking(QLatin1String("displacement")), Qt3DCore::QNode::TrackFinalValues);
    }
};

QTEST_MAIN(tst_Quick3DNode)

#include "tst_quick3dnode.moc"
