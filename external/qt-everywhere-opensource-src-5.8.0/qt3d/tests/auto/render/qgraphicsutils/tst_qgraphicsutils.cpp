/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include <QtTest/QtTest>
#include <private/qgraphicsutils_p.h>

class tst_QGraphicsUtils : public QObject
{
    Q_OBJECT
private slots:
    void fillScalarInDataArray();
    void fillArray();
    void fillScalarWithOffsets();
    void fillMatrix4x4();
    void fillMatrix3x4();
    void fillMatrix4x3();
    void fillMatrixArray();
};

void tst_QGraphicsUtils::fillScalarInDataArray()
{
    Qt3DRender::Render::ShaderUniform description;

    description.m_size = 1;
    description.m_offset = 0;
    description.m_arrayStride = 10;

    QVector4D testVector(8.0f, 8.0f, 3.0f, 1.0f);
    const GLfloat *vectorData = Qt3DRender::Render::QGraphicsUtils::valueArrayFromVariant<GLfloat>(testVector, 1, 4);

    for (int i = 0; i < 4; i++) {
        if (i == 0)
            QVERIFY(vectorData[i] == testVector.x());
        else if (i == 1)
            QVERIFY(vectorData[i] == testVector.y());
        else if (i == 2)
            QVERIFY(vectorData[i] == testVector.z());
        else if (i == 3)
            QVERIFY(vectorData[i] == testVector.w());
    }

    QByteArray data(description.m_size * 4 * sizeof(GLfloat), 0);
    char *innerData = data.data();

    // Checked that we are not overflowing
    Qt3DRender::Render::QGraphicsUtils::fillDataArray(innerData, vectorData, description, 2);
    for (int i = 0; i < 4; ++i) {
        if (i < 2)
            QVERIFY(vectorData[i] == ((GLfloat*)innerData)[i]);
        else
            QVERIFY(((GLfloat*)innerData)[i] == 0.0f);
    }

    // Check that all values are copied
    Qt3DRender::Render::QGraphicsUtils::fillDataArray(innerData, vectorData, description, 4);
    for (int i = 0; i < 4; ++i)
        QVERIFY(vectorData[i] == ((GLfloat*)innerData)[i]);

    // check that offsetting works
    description.m_offset = 16;
    data = QByteArray(description.m_size * 8 * sizeof(GLfloat), 0);
    innerData = data.data();

    Qt3DRender::Render::QGraphicsUtils::fillDataArray(innerData, vectorData, description, 4);
    for (int i = 0; i < 8; ++i) {
        if (i < 4)
            QVERIFY(((GLfloat*)innerData)[i] == 0.0f);
        else
            QVERIFY(vectorData[i - 4] == ((GLfloat*)innerData)[i]);
    }
}

void tst_QGraphicsUtils::fillArray()
{
    QVector4D testVector(8.0f, 8.0f, 3.0f, 1.0f);
    QVector4D testVector2(3.0f, 5.0f, 0.0f, 7.0f);
    QVector4D testVector3(4.0f, 5.0f, 4.0f, 2.0f);

    QVariantList variantList = QVariantList() << testVector << testVector2 << testVector3;
    const GLfloat *vectorData = Qt3DRender::Render::QGraphicsUtils::valueArrayFromVariant<GLfloat>(QVariant(variantList), 3, 4);

    Qt3DRender::Render::ShaderUniform description;

    description.m_size = 3;
    description.m_offset = 16;
    description.m_arrayStride = 16;

    QByteArray data(description.m_size * (4 + description.m_arrayStride) * sizeof(GLfloat) + description.m_offset, 0);
    char *innerData = data.data();
    Qt3DRender::Render::QGraphicsUtils::fillDataArray(innerData, vectorData, description, 4);

    int offset = description.m_offset / sizeof(GLfloat);
    int stride = description.m_arrayStride / sizeof(GLfloat);

    GLfloat *innerDataFloat = (GLfloat*)innerData;

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 4; ++j) {
            int idx = i * 4 + j;
            QVERIFY(innerDataFloat[offset + j] ==  vectorData[idx]);
        }
        offset += stride;
    }
}

void tst_QGraphicsUtils::fillScalarWithOffsets()
{
    // Simulates Uniform Block

    // uniform Block {
    // vec3 position; // Offset 0 - 12 bytes
    // vec3 direction; // Offset 16 - 12 bytes
    // vec4 color; // Offset 32 - 16 bytes
    // float intensity; // Offset 48 - bytes
    // } // total size 64 bytes

    QVector3D position(8.0f, 8.0f, 3.0f);
    QVector3D direction(3.0f, 5.0f, 2.0f);
    QVector4D color(4.0f, 5.0f, 4.0f, 1.0f);
    float intensity = 1.0f;

    Qt3DRender::Render::ShaderUniform posUniform;
    posUniform.m_size = 1;
    posUniform.m_arrayStride = 0;
    posUniform.m_matrixStride = 0;
    posUniform.m_offset = 0;

    Qt3DRender::Render::ShaderUniform dirUniform;
    dirUniform.m_size = 1;
    dirUniform.m_arrayStride = 0;
    dirUniform.m_matrixStride = 0;
    dirUniform.m_offset = 16;

    Qt3DRender::Render::ShaderUniform colUniform;
    colUniform.m_size = 1;
    colUniform.m_arrayStride = 0;
    colUniform.m_matrixStride = 0;
    colUniform.m_offset = 32;

    Qt3DRender::Render::ShaderUniform intUniform;
    intUniform.m_size = 1;
    intUniform.m_arrayStride = 0;
    intUniform.m_matrixStride = 0;
    intUniform.m_offset = 48;

    QVector<GLfloat> data(16);
    void *innerData = data.data();

    Qt3DRender::Render::QGraphicsUtils::fillDataArray(innerData,
                                                Qt3DRender::Render::QGraphicsUtils::valueArrayFromVariant<GLfloat>(position, 1, 3),
                                                posUniform, 3);
    Qt3DRender::Render::QGraphicsUtils::fillDataArray(innerData,
                                                Qt3DRender::Render::QGraphicsUtils::valueArrayFromVariant<GLfloat>(direction, 1, 3),
                                                dirUniform, 3);
    Qt3DRender::Render::QGraphicsUtils::fillDataArray(innerData,
                                                Qt3DRender::Render::QGraphicsUtils::valueArrayFromVariant<GLfloat>(color, 1, 4),
                                                colUniform, 4);
    Qt3DRender::Render::QGraphicsUtils::fillDataArray(innerData,
                                                Qt3DRender::Render::QGraphicsUtils::valueArrayFromVariant<GLfloat>(intensity, 1, 1),
                                                intUniform, 1);

    GLfloat *floatData = (GLfloat*)innerData;

    // Check first 16 bytes - position
    QVERIFY(floatData[0] == position.x());
    QVERIFY(floatData[1] == position.y());
    QVERIFY(floatData[2] == position.z());
    QVERIFY(floatData[3] == 0.0f);
    // Check 16 - 32 bytes - direction
    QVERIFY(floatData[4] == direction.x());
    QVERIFY(floatData[5] == direction.y());
    QVERIFY(floatData[6] == direction.z());
    QVERIFY(floatData[7] == 0.0f);
    // Check 32 - 48 bytes - color
    QVERIFY(floatData[8] == color.x());
    QVERIFY(floatData[9] == color.y());
    QVERIFY(floatData[10] == color.z());
    QVERIFY(floatData[11] == color.w());
    // Check 48 - 64 bytes - intensity
    QVERIFY(floatData[12] == intensity);
    QVERIFY(floatData[13] == 0.0f);
    QVERIFY(floatData[14] == 0.0f);
    QVERIFY(floatData[15] == 0.0f);
}

void tst_QGraphicsUtils::fillMatrix4x4()
{
    // row major
    QMatrix4x4 mat(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f);

    // column major
    const GLfloat *matData = Qt3DRender::Render::QGraphicsUtils::valueArrayFromVariant<GLfloat>(mat, 1, 16);

    Qt3DRender::Render::ShaderUniform description;

    description.m_size = 1;
    description.m_offset = 0;
    description.m_arrayStride = 0;
    description.m_matrixStride = 16;


    QByteArray data(description.m_size * 16 * sizeof(GLfloat), 0);
    char *innerData = data.data();
    Qt3DRender::Render::QGraphicsUtils::fillDataMatrixArray(innerData, matData, description, 4, 4);
    // Check for no offset/no stride
    for (int i = 0; i < 16; ++i)
        QVERIFY((((GLfloat *)innerData)[i]) == matData[i]);

    description.m_offset = 12;
    data = QByteArray((description.m_size * 16  + description.m_offset) * sizeof(GLfloat), 0);
    innerData = data.data();
    Qt3DRender::Render::QGraphicsUtils::fillDataMatrixArray(innerData, matData, description, 4, 4);
    // Check with 12 offset/no stride
    for (int i = 0; i < 16; ++i)
        QVERIFY((((GLfloat *)innerData)[3 + i]) == matData[i]);

    description.m_matrixStride = 16;
    data = QByteArray((description.m_size * 16 + 4 * description.m_matrixStride + description.m_offset) * sizeof(GLfloat), 0);
    innerData = data.data();
    Qt3DRender::Render::QGraphicsUtils::fillDataMatrixArray(innerData, matData, description, 4, 4);
    // Check with 10 offset/ 16 stride
    int offset = description.m_offset / sizeof(GLfloat);
    int matrixStride = description.m_matrixStride / sizeof(GLfloat);

    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row)
            QVERIFY((((GLfloat *)innerData)[offset + row]) == matData[col * 4 + row]);
        offset += matrixStride;
    }
}

void tst_QGraphicsUtils::fillMatrix3x4()
{
    QMatrix3x4 mat;

    mat.fill(6.0f);
    const GLfloat *matData = Qt3DRender::Render::QGraphicsUtils::valueArrayFromVariant<GLfloat>(QVariant::fromValue(mat), 1, 12);

    Qt3DRender::Render::ShaderUniform description;

    description.m_size = 1;
    description.m_offset = 16;
    description.m_arrayStride = 0;
    description.m_matrixStride = 12;

    QByteArray data((description.m_size * 12 + 3 * description.m_matrixStride + description.m_offset) * sizeof(GLfloat), 0);
    char *innerData = data.data();
    Qt3DRender::Render::QGraphicsUtils::fillDataMatrixArray(innerData, matData, description, 3, 4);
    // Check with 16 offset/ 12 stride
    int offset = description.m_offset / sizeof(GLfloat);
    int matrixStride = description.m_matrixStride / sizeof(GLfloat);

    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 4; ++row)
            QVERIFY((((GLfloat *)innerData)[offset + row]) == matData[col * 4 + row]);
        offset += matrixStride;
    }
}

void tst_QGraphicsUtils::fillMatrix4x3()
{
    QMatrix4x3 mat;

    mat.fill(6.0f);
    const GLfloat *matData = Qt3DRender::Render::QGraphicsUtils::valueArrayFromVariant<GLfloat>(QVariant::fromValue(mat), 1, 12);

    Qt3DRender::Render::ShaderUniform description;

    description.m_size = 1;
    description.m_offset = 16;
    description.m_arrayStride = 0;
    description.m_matrixStride = 16;

    QByteArray data((description.m_size * 12 + 4 * description.m_matrixStride + description.m_offset) * sizeof(GLfloat), 0);
    char *innerData = data.data();
    Qt3DRender::Render::QGraphicsUtils::fillDataMatrixArray(innerData, matData, description, 4, 3);
    // Check with 16 offset/ 16 stride
    int offset = description.m_offset / sizeof(GLfloat);
    int matrixStride = description.m_matrixStride / sizeof(GLfloat);

    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 3; ++row)
            QVERIFY((((GLfloat *)innerData)[offset + row]) == matData[col * 3 + row]);
        offset += matrixStride;
    }
}

void tst_QGraphicsUtils::fillMatrixArray()
{
    QMatrix4x3 mat1;
    QMatrix4x3 mat2;
    QMatrix4x3 mat3;
    mat1.fill(6.0f);
    mat2.fill(2.0f);
    mat3.fill(7.0f);

    QVariantList matrices = QVariantList() << QVariant::fromValue(mat1) << QVariant::fromValue(mat2) << QVariant::fromValue(mat3);

    const GLfloat *matData = Qt3DRender::Render::QGraphicsUtils::valueArrayFromVariant<GLfloat>(QVariant::fromValue(matrices), 3, 12);

    Qt3DRender::Render::ShaderUniform description;

    description.m_size = 3;
    description.m_offset = 12;
    description.m_arrayStride = 4;
    description.m_matrixStride = 16;

    QByteArray data((description.m_size * (12 + 4 * description.m_matrixStride + description.m_arrayStride) + description.m_offset) * sizeof(GLfloat), 0);
    char *innerData = data.data();
    Qt3DRender::Render::QGraphicsUtils::fillDataMatrixArray(innerData, matData, description, 4, 3);
    // Check with 12 offset/ 4 array stride / 16 matrix stride
    int offset = description.m_offset / sizeof(GLfloat);
    int matrixStride = description.m_matrixStride / sizeof(GLfloat);
    int arrayStride = description.m_arrayStride / sizeof(GLfloat);

    for (int i = 0; i < 3; ++i) {
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 3; ++row) {
                int idx = i * 4 * 3 + col * 3 + row;
                QVERIFY((((GLfloat *)innerData)[offset + row]) == matData[idx]);
            }
            offset += matrixStride;
        }
        offset += arrayStride;
    }
}

QTEST_APPLESS_MAIN(tst_QGraphicsUtils)

#include "tst_qgraphicsutils.moc"
