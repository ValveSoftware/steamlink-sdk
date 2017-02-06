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

#include <Qt3DRender/private/qtextureimagedata_p.h>
#include <Qt3DRender/qtextureimagedata.h>

#include "testpostmanarbiter.h"

// We need to call QNode::clone which is protected
// So we sublcass QNode instead of QObject
class tst_QTextureImageData : public QObject
{
    Q_OBJECT
public:
    ~tst_QTextureImageData()
    {
    }

private Q_SLOTS:

    void checkSaneDefaults()
    {
        Qt3DRender::QTextureImageData *tid = new Qt3DRender::QTextureImageData();

        QCOMPARE(tid->width(), -1);
        QCOMPARE(tid->height(), -1);
        QCOMPARE(tid->depth(), -1);
        QCOMPARE(tid->layers(), -1);
        QCOMPARE(tid->mipLevels(), -1);
        QCOMPARE(tid->target(), QOpenGLTexture::Target2D);
        QCOMPARE(tid->format(), QOpenGLTexture::RGBA8_UNorm);
        QCOMPARE(tid->pixelFormat(), QOpenGLTexture::RGBA);
        QCOMPARE(tid->pixelType(), QOpenGLTexture::UInt8);
        QCOMPARE(tid->isCompressed(), false);
    }

    void checkCloning_data()
    {

    }

    void checkCloning()
    {
    }

    void checkPropertyUpdates()
    {
    }
/*
protected:

    Qt3DCore::QNode *doClone() const Q_DECL_OVERRIDE
    {
        return Q_NULLPTR;
    }
    */

};

QTEST_MAIN(tst_QTextureImageData)

#include "tst_qtextureimagedata.moc"
