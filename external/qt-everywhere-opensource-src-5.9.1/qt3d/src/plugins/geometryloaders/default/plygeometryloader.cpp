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

#include "plygeometryloader.h"

#include <QtCore/QDataStream>
#include <QtCore/QLoggingCategory>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

Q_LOGGING_CATEGORY(PlyGeometryLoaderLog, "Qt3D.PlyGeometryLoader", QtWarningMsg)

namespace {

class PlyDataReader
{
public:
    virtual ~PlyDataReader() {}

    virtual int readIntValue(PlyGeometryLoader::DataType type) = 0;
    virtual float readFloatValue(PlyGeometryLoader::DataType type) = 0;
};

class AsciiPlyDataReader : public PlyDataReader
{
public:
    AsciiPlyDataReader(QIODevice *ioDev)
        : m_stream(ioDev)
    { }

    int readIntValue(PlyGeometryLoader::DataType) Q_DECL_OVERRIDE
    {
        int value;
        m_stream >> value;
        return value;
    }

    float readFloatValue(PlyGeometryLoader::DataType) Q_DECL_OVERRIDE
    {
        float value;
        m_stream >> value;
        return value;
    }

private:
    QTextStream m_stream;
};

class BinaryPlyDataReader : public PlyDataReader
{
public:
    BinaryPlyDataReader(QIODevice *ioDev, QDataStream::ByteOrder byteOrder)
        : m_stream(ioDev)
    {
        ioDev->setTextModeEnabled(false);
        m_stream.setByteOrder(byteOrder);
    }

    int readIntValue(PlyGeometryLoader::DataType type) Q_DECL_OVERRIDE
    {
        return readValue<int>(type);
    }

    float readFloatValue(PlyGeometryLoader::DataType type) Q_DECL_OVERRIDE
    {
        return readValue<float>(type);
    }

private:
    template <typename T>
    T readValue(PlyGeometryLoader::DataType type)
    {
        switch (type) {
        case PlyGeometryLoader::Int8:
        {
            qint8 value;
            m_stream >> value;
            return value;
        }

        case PlyGeometryLoader::Uint8:
        {
            quint8 value;
            m_stream >> value;
            return value;
        }

        case PlyGeometryLoader::Int16:
        {
            qint16 value;
            m_stream >> value;
            return value;
        }

        case PlyGeometryLoader::Uint16:
        {
            quint16 value;
            m_stream >> value;
            return value;
        }

        case PlyGeometryLoader::Int32:
        {
            qint32 value;
            m_stream >> value;
            return value;
        }

        case PlyGeometryLoader::Uint32:
        {
            quint32 value;
            m_stream >> value;
            return value;
        }

        case PlyGeometryLoader::Float32:
        {
            m_stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
            float value;
            m_stream >> value;
            return value;
        }

        case PlyGeometryLoader::Float64:
        {
            m_stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
            double value;
            m_stream >> value;
            return value;
        }

        default:
            break;
        }

        return 0;
    }

    QDataStream m_stream;
};

}

static PlyGeometryLoader::DataType toPlyDataType(const QString &typeName)
{
    if (typeName == QStringLiteral("int8") || typeName == QStringLiteral("char")) {
        return PlyGeometryLoader::Int8;
    } else if (typeName == QStringLiteral("uint8") || typeName == QStringLiteral("uchar")) {
        return PlyGeometryLoader::Uint8;
    } else if (typeName == QStringLiteral("int16") || typeName == QStringLiteral("short")) {
        return PlyGeometryLoader::Int16;
    } else if (typeName == QStringLiteral("uint16") || typeName == QStringLiteral("ushort")) {
        return PlyGeometryLoader::Uint16;
    } else if (typeName == QStringLiteral("int32") || typeName == QStringLiteral("int")) {
        return PlyGeometryLoader::Int32;
    } else if (typeName == QStringLiteral("uint32") || typeName == QStringLiteral("uint")) {
        return PlyGeometryLoader::Uint32;
    } else if (typeName == QStringLiteral("float32") || typeName == QStringLiteral("float")) {
        return PlyGeometryLoader::Float32;
    } else if (typeName == QStringLiteral("float64") || typeName == QStringLiteral("double")) {
        return PlyGeometryLoader::Float64;
    } else if (typeName == QStringLiteral("list")) {
        return PlyGeometryLoader::TypeList;
    } else {
        return PlyGeometryLoader::TypeUnknown;
    }
}

bool PlyGeometryLoader::doLoad(QIODevice *ioDev, const QString &subMesh)
{
    Q_UNUSED(subMesh);

    if (!parseHeader(ioDev))
        return false;

    if (!parseMesh(ioDev))
        return false;

    return true;
}

bool PlyGeometryLoader::parseHeader(QIODevice *ioDev)
{
    Format format = FormatUnknown;

    m_hasNormals = m_hasTexCoords = false;

    while (!ioDev->atEnd()) {
        QByteArray lineBuffer = ioDev->readLine();
        QTextStream textStream(lineBuffer, QIODevice::ReadOnly);

        QString token;
        textStream >> token;

        if (token == QStringLiteral("end_header")) {
            break;
        } else if (token == QStringLiteral("format")) {
            QString formatName;
            textStream >> formatName;

            if (formatName == QStringLiteral("ascii")) {
                m_format = FormatAscii;
            } else if (formatName == QStringLiteral("binary_little_endian")) {
                m_format = FormatBinaryLittleEndian;
            } else if (formatName == QStringLiteral("binary_big_endian")) {
                m_format = FormatBinaryBigEndian;
            } else {
                qCDebug(PlyGeometryLoaderLog) << "Unrecognized PLY file format" << format;
                return false;
            }
        } else if (token == QStringLiteral("element")) {
            Element element;

            QString elementName;
            textStream >> elementName;

            if (elementName == QStringLiteral("vertex")) {
                element.type = ElementVertex;
            } else if (elementName == QStringLiteral("face")) {
                element.type = ElementFace;
            } else {
                element.type = ElementUnknown;
            }

            textStream >> element.count;

            m_elements.append(element);
        } else if (token == QStringLiteral("property")) {
            if (m_elements.isEmpty()) {
                qCDebug(PlyGeometryLoaderLog) << "Misplaced property in header";
                return false;
            }

            Property property;

            QString dataTypeName;
            textStream >> dataTypeName;

            property.dataType = toPlyDataType(dataTypeName);

            if (property.dataType == TypeList) {
                QString listSizeTypeName;
                textStream >> listSizeTypeName;

                property.listSizeType = toPlyDataType(listSizeTypeName);

                QString listElementTypeName;
                textStream >> listElementTypeName;

                property.listElementType = toPlyDataType(listElementTypeName);
            }

            QString propertyName;
            textStream >> propertyName;

            if (propertyName == QStringLiteral("vertex_index")) {
                property.type = PropertyVertexIndex;
            } else if (propertyName == QStringLiteral("x")) {
                property.type = PropertyX;
            } else if (propertyName == QStringLiteral("y")) {
                property.type = PropertyY;
            } else if (propertyName == QStringLiteral("z")) {
                property.type = PropertyZ;
            } else if (propertyName == QStringLiteral("nx")) {
                property.type = PropertyNormalX;
                m_hasNormals = true;
            } else if (propertyName == QStringLiteral("ny")) {
                property.type = PropertyNormalY;
                m_hasNormals = true;
            } else if (propertyName == QStringLiteral("nz")) {
                property.type = PropertyNormalZ;
                m_hasNormals = true;
            } else if (propertyName == QStringLiteral("s")) {
                property.type = PropertyTextureU;
                m_hasTexCoords = true;
            } else if (propertyName == QStringLiteral("t")) {
                property.type = PropertyTextureV;
                m_hasTexCoords = true;
            } else {
                property.type = PropertyUnknown;
            }

            Element &element = m_elements.last();
            element.properties.append(property);
        }
    }

    if (m_format == FormatUnknown) {
        qCDebug(PlyGeometryLoaderLog) << "Missing PLY file format";
        return false;
    }

    return true;
}

bool PlyGeometryLoader::parseMesh(QIODevice *ioDev)
{
    QScopedPointer<PlyDataReader> dataReader;

    switch (m_format) {
    case FormatAscii:
        dataReader.reset(new AsciiPlyDataReader(ioDev));
        break;

    case FormatBinaryLittleEndian:
        dataReader.reset(new BinaryPlyDataReader(ioDev, QDataStream::LittleEndian));
        break;

    default:
        dataReader.reset(new BinaryPlyDataReader(ioDev, QDataStream::BigEndian));
        break;
    }

    for (auto &element : qAsConst(m_elements)) {
        if (element.type == ElementVertex) {
            m_points.reserve(element.count);

            if (m_hasNormals)
                m_normals.reserve(element.count);

            if (m_hasTexCoords)
                m_texCoords.reserve(element.count);
        }

        for (int i = 0; i < element.count; ++i) {
            QVector3D point;
            QVector3D normal;
            QVector2D texCoord;

            QVector<unsigned int> faceIndices;

            for (auto &property : element.properties) {
                if (property.dataType == TypeList) {
                    const int listSize = dataReader->readIntValue(property.listSizeType);

                    if (element.type == ElementFace)
                        faceIndices.reserve(listSize);

                    for (int j = 0; j < listSize; ++j) {
                        const unsigned int value = dataReader->readIntValue(property.listElementType);

                        if (element.type == ElementFace)
                            faceIndices.append(value);
                    }
                } else {
                    float value = dataReader->readFloatValue(property.dataType);

                    if (element.type == ElementVertex) {
                        switch (property.type) {
                        case PropertyX: point.setX(value); break;
                        case PropertyY: point.setY(value); break;
                        case PropertyZ: point.setZ(value); break;
                        case PropertyNormalX: normal.setX(value); break;
                        case PropertyNormalY: normal.setY(value); break;
                        case PropertyNormalZ: normal.setZ(value); break;
                        case PropertyTextureU: texCoord.setX(value); break;
                        case PropertyTextureV: texCoord.setY(value); break;
                        default: break;
                        }
                    }
                }
            }

            if (element.type == ElementVertex) {
                m_points.append(point);

                if (m_hasNormals)
                    m_normals.append(normal);

                if (m_hasTexCoords)
                    m_texCoords.append(texCoord);
            } else if (element.type == ElementFace) {
                if (faceIndices.size() >= 3) {
                    // decompose face into triangle fan

                    for (int j = 1; j < faceIndices.size() - 1; ++j) {
                        m_indices.append(faceIndices[0]);
                        m_indices.append(faceIndices[j]);
                        m_indices.append(faceIndices[j + 1]);
                    }
                }
            }
        }
    }

    return true;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
