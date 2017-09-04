/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include <QtTest/QTest>
#include <Qt3DCore/qentity.h>

#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/entity_p.h>

#include <Qt3DRender/private/framegraphnode_p.h>
#include <Qt3DRender/private/framegraphvisitor_p.h>

#include <Qt3DRender/qtechniquefilter.h>
#include <Qt3DRender/qnodraw.h>
#include <Qt3DRender/qfrustumculling.h>
#include <Qt3DRender/qviewport.h>

#include "testaspect.h"

namespace {



} // anonymous

using FGIdType = QPair<Qt3DCore::QNodeId, Qt3DRender::Render::FrameGraphNode::FrameGraphNodeType>;
using BranchIdsAndTypes = QVector<FGIdType>;

Q_DECLARE_METATYPE(QVector<BranchIdsAndTypes>)

class tst_FrameGraphVisitor : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void visitFGTree_data()
    {
        QTest::addColumn<Qt3DCore::QEntity *>("rootEntity");
        QTest::addColumn<Qt3DRender::QFrameGraphNode *>("fgRoot");
        QTest::addColumn<QVector<BranchIdsAndTypes>>("fgNodeIdsPerBranch");

        {
            Qt3DCore::QEntity *entity = new Qt3DCore::QEntity();
            Qt3DRender::QTechniqueFilter *techniqueFilter = new Qt3DRender::QTechniqueFilter(entity);

            QTest::newRow("singleNode") << entity
                                        << static_cast<Qt3DRender::QFrameGraphNode *>(techniqueFilter)
                                        << (QVector<BranchIdsAndTypes>()
                                            << (BranchIdsAndTypes() << FGIdType(techniqueFilter->id(), Qt3DRender::Render::FrameGraphNode::TechniqueFilter))
                                            );
        }

        {
            Qt3DCore::QEntity *entity = new Qt3DCore::QEntity();
            Qt3DRender::QTechniqueFilter *techniqueFilter = new Qt3DRender::QTechniqueFilter(entity);
            Qt3DRender::QFrustumCulling *frustumCulling = new Qt3DRender::QFrustumCulling(techniqueFilter);
            Qt3DRender::QNoDraw *noDraw = new Qt3DRender::QNoDraw(frustumCulling);

            QTest::newRow("singleBranch") << entity
                                        << static_cast<Qt3DRender::QFrameGraphNode *>(techniqueFilter)
                                        << (QVector<BranchIdsAndTypes>()
                                            << (BranchIdsAndTypes()
                                                << FGIdType(noDraw->id(), Qt3DRender::Render::FrameGraphNode::NoDraw)
                                                << FGIdType(frustumCulling->id(), Qt3DRender::Render::FrameGraphNode::FrustumCulling)
                                                << FGIdType(techniqueFilter->id(), Qt3DRender::Render::FrameGraphNode::TechniqueFilter)
                                                )
                                            );
        }
        {
            Qt3DCore::QEntity *entity = new Qt3DCore::QEntity();
            Qt3DRender::QTechniqueFilter *techniqueFilter = new Qt3DRender::QTechniqueFilter(entity);
            Qt3DRender::QFrustumCulling *frustumCulling = new Qt3DRender::QFrustumCulling(techniqueFilter);
            Qt3DRender::QNoDraw *noDraw = new Qt3DRender::QNoDraw(frustumCulling);
            Qt3DRender::QViewport *viewPort = new Qt3DRender::QViewport(frustumCulling);

            QTest::newRow("dualBranch") << entity
                                          << static_cast<Qt3DRender::QFrameGraphNode *>(techniqueFilter)
                                          << (QVector<BranchIdsAndTypes>()
                                              << (BranchIdsAndTypes()
                                                  << FGIdType(noDraw->id(), Qt3DRender::Render::FrameGraphNode::NoDraw)
                                                  << FGIdType(frustumCulling->id(), Qt3DRender::Render::FrameGraphNode::FrustumCulling)
                                                  << FGIdType(techniqueFilter->id(), Qt3DRender::Render::FrameGraphNode::TechniqueFilter)
                                                  )
                                              << (BranchIdsAndTypes()
                                                  << FGIdType(viewPort->id(), Qt3DRender::Render::FrameGraphNode::Viewport)
                                                  << FGIdType(frustumCulling->id(), Qt3DRender::Render::FrameGraphNode::FrustumCulling)
                                                  << FGIdType(techniqueFilter->id(), Qt3DRender::Render::FrameGraphNode::TechniqueFilter)
                                                  )
                                              );
        }

    }

    void visitFGTree()
    {
        // GIVEN
        QFETCH(Qt3DCore::QEntity *, rootEntity);
        QFETCH(Qt3DRender::QFrameGraphNode *, fgRoot);
        QFETCH(QVector<BranchIdsAndTypes>, fgNodeIdsPerBranch);
        QScopedPointer<Qt3DRender::TestAspect> aspect(new Qt3DRender::TestAspect(rootEntity));

        // THEN
        Qt3DRender::Render::FrameGraphManager *fgManager = aspect->nodeManagers()->frameGraphManager();
        Qt3DRender::Render::FrameGraphNode *backendFGRoot = fgManager->lookupNode(fgRoot->id());
        QVERIFY(backendFGRoot != nullptr);


        // WHEN
        Qt3DRender::Render::FrameGraphVisitor visitor(fgManager);
        const QVector<Qt3DRender::Render::FrameGraphNode *> fgNodes = visitor.traverse(backendFGRoot);

        // THEN
        QCOMPARE(fgNodeIdsPerBranch.size(), fgNodes.size());

        for (int i = 0; i < fgNodeIdsPerBranch.size(); ++i) {
            const BranchIdsAndTypes brandIdsAndTypes = fgNodeIdsPerBranch.at(i);

            Qt3DRender::Render::FrameGraphNode *fgNode = fgNodes.at(i);
            for (int j = 0; j < brandIdsAndTypes.size(); ++j) {
                const FGIdType idAndType = brandIdsAndTypes.at(j);
                QVERIFY(fgNode != nullptr);
                QCOMPARE(fgNode->peerId(), idAndType.first);
                QCOMPARE(fgNode->nodeType(), idAndType.second);
                fgNode = fgNode->parent();
            }

            QVERIFY(fgNode == nullptr);
        }

        delete rootEntity;
    }
};

QTEST_MAIN(tst_FrameGraphVisitor)

#include "tst_framegraphvisitor.moc"
