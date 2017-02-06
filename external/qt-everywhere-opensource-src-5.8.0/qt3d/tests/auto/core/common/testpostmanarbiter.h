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

#include <Qt3DCore/private/qpostman_p.h>
#include <Qt3DCore/private/qchangearbiter_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {
    class QNode;
} // Qt3D

class TestArbiter;

class TestPostman : public Qt3DCore::QAbstractPostman
{
public:
    explicit TestPostman(TestArbiter *arbiter);
    void sceneChangeEvent(const Qt3DCore::QSceneChangePtr &) Q_DECL_FINAL;
    void setScene(Qt3DCore::QScene *) Q_DECL_FINAL;
    void notifyBackend(const Qt3DCore::QSceneChangePtr &e) Q_DECL_FINAL;

private:
    TestArbiter *m_arbiter;
};

class TestArbiter : public Qt3DCore::QAbstractArbiter
{
public:
    TestArbiter();
    ~TestArbiter();

    void sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e) Q_DECL_FINAL;

    void sceneChangeEventWithLock(const Qt3DCore::QSceneChangePtr &e) Q_DECL_FINAL;

    void sceneChangeEventWithLock(const Qt3DCore::QSceneChangeList &e) Q_DECL_FINAL;

    Qt3DCore::QAbstractPostman *postman() const Q_DECL_FINAL;

    QVector<Qt3DCore::QSceneChangePtr> events;

    void setArbiterOnNode(Qt3DCore::QNode *node);

private:
    TestPostman *m_postman;
};

QT_END_NAMESPACE
