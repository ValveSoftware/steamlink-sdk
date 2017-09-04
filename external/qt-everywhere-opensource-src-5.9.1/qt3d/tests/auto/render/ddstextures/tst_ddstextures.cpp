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
#include <Qt3DRender/qtextureimagedata.h>
#include <Qt3DRender/private/qtexture_p.h>

class tst_DdsTextures : public QObject
{
    Q_OBJECT

private slots:
    void ddsImageData();
};

void tst_DdsTextures::ddsImageData()
{
    const struct TextureInfo {
        const char *source;
        int width;
        int height;
        int depth;
        int faces;
        int mipmapLevels;
        QOpenGLTexture::TextureFormat format;
    } textures[] = {
        { "data/16x16x1-1-lumi-nomips.dds",     16, 16, 1, 1, 1, QOpenGLTexture::R8_UNorm },
        { "data/16x16x1-1-lumi.dds",            16, 16, 1, 1, 5, QOpenGLTexture::R8_UNorm },
        { "data/16x16x1-1-rgb-nomips.dds",      16, 16, 1, 1, 1, QOpenGLTexture::RGBA8_UNorm },
        { "data/16x16x1-1-rgb.dds",             16, 16, 1, 1, 5, QOpenGLTexture::RGBA8_UNorm },
        { "data/16x16x1-1-bc1-nomips.dds",      16, 16, 1, 1, 1, QOpenGLTexture::RGBA_DXT1 },
        { "data/16x16x1-1-bc1.dds",             16, 16, 1, 1, 5, QOpenGLTexture::RGBA_DXT1 },
        { "data/16x16x1-1-bc3-nomips.dds",      16, 16, 1, 1, 1, QOpenGLTexture::RGBA_DXT5 },
        { "data/16x16x1-1-bc3.dds",             16, 16, 1, 1, 5, QOpenGLTexture::RGBA_DXT5 },
        { "data/16x16x1-1-bc1-nomips-dx10.dds", 16, 16, 1, 1, 1, QOpenGLTexture::RGBA_DXT1 },
        { "data/16x16x1-1-bc1-dx10.dds",        16, 16, 1, 1, 5, QOpenGLTexture::RGBA_DXT1 },
        { "data/16x16x1-1-bc3-nomips-dx10.dds", 16, 16, 1, 1, 1, QOpenGLTexture::RGBA_DXT5 },
        { "data/16x16x1-1-bc3-dx10.dds",        16, 16, 1, 1, 5, QOpenGLTexture::RGBA_DXT5 },
        { "data/16x16x1-6-lumi-nomips.dds",     16, 16, 1, 6, 1, QOpenGLTexture::R8_UNorm },
        { "data/16x16x1-6-lumi.dds",            16, 16, 1, 6, 5, QOpenGLTexture::R8_UNorm },
        { "data/16x16x1-6-rgb-nomips.dds",      16, 16, 1, 6, 1, QOpenGLTexture::RGBA8_UNorm },
        { "data/16x16x1-6-rgb.dds",             16, 16, 1, 6, 5, QOpenGLTexture::RGBA8_UNorm },
        { "data/16x16x1-6-bc1-nomips.dds",      16, 16, 1, 6, 1, QOpenGLTexture::RGBA_DXT1 },
        { "data/16x16x1-6-bc1.dds",             16, 16, 1, 6, 5, QOpenGLTexture::RGBA_DXT1 },
        { "data/16x16x1-6-bc3-nomips.dds",      16, 16, 1, 6, 1, QOpenGLTexture::RGBA_DXT5 },
        { "data/16x16x1-6-bc3.dds",             16, 16, 1, 6, 5, QOpenGLTexture::RGBA_DXT5 },
        { "data/16x16x1-6-bc1-nomips-dx10.dds", 16, 16, 1, 6, 1, QOpenGLTexture::RGBA_DXT1 },
        { "data/16x16x1-6-bc1-dx10.dds",        16, 16, 1, 6, 5, QOpenGLTexture::RGBA_DXT1 },
        { "data/16x16x1-6-bc3-nomips-dx10.dds", 16, 16, 1, 6, 1, QOpenGLTexture::RGBA_DXT5 },
        { "data/16x16x1-6-bc3-dx10.dds",        16, 16, 1, 6, 5, QOpenGLTexture::RGBA_DXT5 },
    };

    for (unsigned i = 0; i < sizeof(textures)/sizeof(*textures); i++) {
        const TextureInfo *texture = &textures[i];

        Qt3DRender::QTextureImageDataPtr data = Qt3DRender::TextureLoadingHelper::loadTextureData(QUrl::fromLocalFile(QFINDTESTDATA(texture->source)), true, false);

        QVERIFY(data);
        QCOMPARE(data->width(), texture->width);
        QCOMPARE(data->height(), texture->height);
        QCOMPARE(data->faces(), texture->faces);
        QCOMPARE(data->mipLevels(), texture->mipmapLevels);
        QCOMPARE(data->format(), texture->format);
    }
}

QTEST_APPLESS_MAIN(tst_DdsTextures)

#include "tst_ddstextures.moc"
