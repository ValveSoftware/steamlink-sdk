/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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
#include <private/renderview_p.h>
#include <private/qframeallocator_p.h>
#include <private/qframeallocator_p_p.h>

class tst_RenderViews : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void checkRenderViewSizeFitsWithAllocator()
    {
        QSKIP("Allocated Disabled");
        QVERIFY(sizeof(Qt3DRender::Render::RenderView) <= 192);
        QVERIFY(sizeof(Qt3DRender::Render::RenderView::InnerData) <= 192);
    }

    void testSort()
    {

    }

    void checkRenderViewDoesNotLeak()
    {
        QSKIP("Allocated Disabled");
        // GIVEN
        Qt3DCore::QFrameAllocator allocator(192, 16, 128);
        Qt3DRender::Render::RenderView *rv = allocator.allocate<Qt3DRender::Render::RenderView>();

        // THEN
        QVERIFY(!allocator.isEmpty());

        // WHEN
        delete rv;

        // THEN
        QVERIFY(allocator.isEmpty());
    }

private:
};


QTEST_APPLESS_MAIN(tst_RenderViews)

#include "tst_renderviews.moc"
