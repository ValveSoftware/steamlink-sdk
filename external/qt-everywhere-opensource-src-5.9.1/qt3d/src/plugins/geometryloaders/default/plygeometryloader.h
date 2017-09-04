/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd and/or its subsidiary(-ies).
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

#ifndef PLYGEOMETRYLOADER_H
#define PLYGEOMETRYLOADER_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "basegeometryloader_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

#define PLYGEOMETRYLOADER_EXT QLatin1String("ply")

class PlyGeometryLoader : public BaseGeometryLoader
{
public:
    enum DataType {
        Int8,
        Uint8,
        Int16,
        Uint16,
        Int32,
        Uint32,
        Float32,
        Float64,
        TypeList,
        TypeUnknown
    };

    enum PropertyType {
        PropertyVertexIndex,
        PropertyX,
        PropertyY,
        PropertyZ,
        PropertyNormalX,
        PropertyNormalY,
        PropertyNormalZ,
        PropertyTextureU,
        PropertyTextureV,
        PropertyUnknown,
    };

    enum Format {
        FormatAscii,
        FormatBinaryLittleEndian,
        FormatBinaryBigEndian,
        FormatUnknown,
    };

    enum ElementType {
        ElementVertex,
        ElementFace,
        ElementUnknown,
    };

    struct Property {
        PropertyType type;
        DataType dataType;
        DataType listSizeType;
        DataType listElementType;
    };

    struct Element {
        ElementType type;
        int count;
        QList<Property> properties;
    };

protected:
    bool doLoad(QIODevice *ioDev, const QString &subMesh) Q_DECL_FINAL;

private:
    bool parseHeader(QIODevice *ioDev);
    bool parseMesh(QIODevice *ioDev);

    Format m_format;
    QList<Element> m_elements;

    bool m_hasNormals;
    bool m_hasTexCoords;
};

} // namespace Qt3DRender

QT_END_NAMESPACE

#endif // PLYGEOMETRYLOADER_H
