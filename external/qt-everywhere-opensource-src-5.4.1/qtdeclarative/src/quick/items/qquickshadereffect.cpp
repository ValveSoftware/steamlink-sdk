/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <private/qquickshadereffect_p.h>
#include <private/qquickshadereffectnode_p.h>

#include <QtQuick/qsgmaterial.h>
#include <QtQuick/private/qsgshadersourcebuilder_p.h>
#include "qquickitem_p.h"

#include <QtQuick/private/qsgcontext_p.h>
#include <QtQuick/qsgtextureprovider.h>
#include "qquickwindow.h"

#include "qquickimage_p.h"
#include "qquickshadereffectsource_p.h"

#include <QtCore/qsignalmapper.h>
#include <QtGui/qopenglframebufferobject.h>

QT_BEGIN_NAMESPACE

static const char qt_position_attribute_name[] = "qt_Vertex";
static const char qt_texcoord_attribute_name[] = "qt_MultiTexCoord0";

const char *qtPositionAttributeName()
{
    return qt_position_attribute_name;
}

const char *qtTexCoordAttributeName()
{
    return qt_texcoord_attribute_name;
}

namespace {

    enum VariableQualifier {
        AttributeQualifier,
        UniformQualifier
    };

    inline bool qt_isalpha(char c)
    {
        char ch = c | 0x20;
        return (ch >= 'a' && ch <= 'z') || c == '_';
    }

    inline bool qt_isalnum(char c)
    {
        return qt_isalpha(c) || (c >= '0' && c <= '9');
    }

    inline bool qt_isspace(char c)
    {
        return c == ' ' || (c >= 0x09 && c <= 0x0d);
    }

    // Returns -1 if not found, returns index to first character after the name if found.
    int qt_search_for_variable(const char *s, int length, int index, VariableQualifier &decl,
                               int &typeIndex, int &typeLength,
                               int &nameIndex, int &nameLength,
                               QQuickShaderEffectCommon::Key::ShaderType shaderType)
    {
        enum Identifier {
            QualifierIdentifier, // Base state
            PrecisionIdentifier,
            TypeIdentifier,
            NameIdentifier
        };
        Identifier expected = QualifierIdentifier;
        bool compilerDirectiveExpected = index == 0;

        while (index < length) {
            // Skip whitespace.
            while (qt_isspace(s[index])) {
                compilerDirectiveExpected |= s[index] == '\n';
                ++index;
            }

            if (qt_isalpha(s[index])) {
                // Read identifier.
                int idIndex = index;
                ++index;
                while (qt_isalnum(s[index]))
                    ++index;
                int idLength = index - idIndex;

                const int attrLen = sizeof("attribute") - 1;
                const int inLen = sizeof("in") - 1;
                const int uniLen = sizeof("uniform") - 1;
                const int loLen = sizeof("lowp") - 1;
                const int medLen = sizeof("mediump") - 1;
                const int hiLen = sizeof("highp") - 1;

                switch (expected) {
                case QualifierIdentifier:
                    if (idLength == attrLen && qstrncmp("attribute", s + idIndex, attrLen) == 0) {
                        decl = AttributeQualifier;
                        expected = PrecisionIdentifier;
                    } else if (shaderType == QQuickShaderEffectCommon::Key::VertexShader
                               && idLength == inLen && qstrncmp("in", s + idIndex, inLen) == 0) {
                        decl = AttributeQualifier;
                        expected = PrecisionIdentifier;
                    } else if (idLength == uniLen && qstrncmp("uniform", s + idIndex, uniLen) == 0) {
                        decl = UniformQualifier;
                        expected = PrecisionIdentifier;
                    }
                    break;
                case PrecisionIdentifier:
                    if ((idLength == loLen && qstrncmp("lowp", s + idIndex, loLen) == 0)
                            || (idLength == medLen && qstrncmp("mediump", s + idIndex, medLen) == 0)
                            || (idLength == hiLen && qstrncmp("highp", s + idIndex, hiLen) == 0))
                    {
                        expected = TypeIdentifier;
                        break;
                    }
                    // Fall through.
                case TypeIdentifier:
                    typeIndex = idIndex;
                    typeLength = idLength;
                    expected = NameIdentifier;
                    break;
                case NameIdentifier:
                    nameIndex = idIndex;
                    nameLength = idLength;
                    return index; // Attribute or uniform declaration found. Return result.
                default:
                    break;
                }
            } else if (s[index] == '#' && compilerDirectiveExpected) {
                // Skip compiler directives.
                ++index;
                while (index < length && (s[index] != '\n' || s[index - 1] == '\\'))
                    ++index;
            } else if (s[index] == '/' && s[index + 1] == '/') {
                // Skip comments.
                index += 2;
                while (index < length && s[index] != '\n')
                    ++index;
            } else if (s[index] == '/' && s[index + 1] == '*') {
                // Skip comments.
                index += 2;
                while (index < length && (s[index] != '*' || s[index + 1] != '/'))
                    ++index;
                if (index < length)
                    index += 2; // Skip star-slash.
            } else {
                expected = QualifierIdentifier;
                ++index;
            }
            compilerDirectiveExpected = false;
        }
        return -1;
    }
}



QQuickShaderEffectCommon::~QQuickShaderEffectCommon()
{
    for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType)
        qDeleteAll(signalMappers[shaderType]);
}

void QQuickShaderEffectCommon::disconnectPropertySignals(QQuickItem *item, Key::ShaderType shaderType)
{
    for (int i = 0; i < uniformData[shaderType].size(); ++i) {
        if (signalMappers[shaderType].at(i) == 0)
            continue;
        const UniformData &d = uniformData[shaderType].at(i);
        QSignalMapper *mapper = signalMappers[shaderType].at(i);
        QObject::disconnect(item, 0, mapper, SLOT(map()));
        QObject::disconnect(mapper, SIGNAL(mapped(int)), item, SLOT(propertyChanged(int)));
        if (d.specialType == UniformData::Sampler) {
            QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(d.value));
            if (source) {
                if (item->window())
                    QQuickItemPrivate::get(source)->derefWindow();
                QObject::disconnect(source, SIGNAL(destroyed(QObject*)), item, SLOT(sourceDestroyed(QObject*)));
            }
        }
    }
}

void QQuickShaderEffectCommon::connectPropertySignals(QQuickItem *item, Key::ShaderType shaderType)
{
    for (int i = 0; i < uniformData[shaderType].size(); ++i) {
        if (signalMappers[shaderType].at(i) == 0)
            continue;
        const UniformData &d = uniformData[shaderType].at(i);
        int pi = item->metaObject()->indexOfProperty(d.name.constData());
        if (pi >= 0) {
            QMetaProperty mp = item->metaObject()->property(pi);
            if (!mp.hasNotifySignal())
                qWarning("QQuickShaderEffect: property '%s' does not have notification method!", d.name.constData());
            const QByteArray signalName = '2' + mp.notifySignal().methodSignature();
            QSignalMapper *mapper = signalMappers[shaderType].at(i);
            QObject::connect(item, signalName, mapper, SLOT(map()));
            QObject::connect(mapper, SIGNAL(mapped(int)), item, SLOT(propertyChanged(int)));
        } else {
            // If the source is set via a dynamic property, like the layer is, then we need this
            // check to disable the warning.
            if (!item->property(d.name.constData()).isValid())
                qWarning("QQuickShaderEffect: '%s' does not have a matching property!", d.name.constData());
        }

        if (d.specialType == UniformData::Sampler) {
            QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(d.value));
            if (source) {
                if (item->window())
                    QQuickItemPrivate::get(source)->refWindow(item->window());
                QObject::connect(source, SIGNAL(destroyed(QObject*)), item, SLOT(sourceDestroyed(QObject*)));
            }
        }
    }
}

void QQuickShaderEffectCommon::updateParseLog(bool ignoreAttributes)
{
    parseLog.clear();
    if (!ignoreAttributes) {
        if (!attributes.contains(qt_position_attribute_name)) {
            parseLog += QLatin1String("Warning: Missing reference to \'");
            parseLog += QLatin1String(qt_position_attribute_name);
            parseLog += QLatin1String("\'.\n");
        }
        if (!attributes.contains(qt_texcoord_attribute_name)) {
            parseLog += QLatin1String("Warning: Missing reference to \'");
            parseLog += QLatin1String(qt_texcoord_attribute_name);
            parseLog += QLatin1String("\'.\n");
        }
    }
    bool respectsMatrix = false;
    bool respectsOpacity = false;
    for (int i = 0; i < uniformData[Key::VertexShader].size(); ++i)
        respectsMatrix |= uniformData[Key::VertexShader].at(i).specialType == UniformData::Matrix;
    for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType) {
        for (int i = 0; i < uniformData[shaderType].size(); ++i)
            respectsOpacity |= uniformData[shaderType].at(i).specialType == UniformData::Opacity;
    }
    if (!respectsMatrix)
        parseLog += QLatin1String("Warning: Vertex shader is missing reference to \'qt_Matrix\'.\n");
    if (!respectsOpacity)
        parseLog += QLatin1String("Warning: Shaders are missing reference to \'qt_Opacity\'.\n");
}

void QQuickShaderEffectCommon::lookThroughShaderCode(QQuickItem *item, Key::ShaderType shaderType, const QByteArray &code)
{
    int index = 0;
    int typeIndex = -1;
    int typeLength = 0;
    int nameIndex = -1;
    int nameLength = 0;
    const char *s = code.constData();
    VariableQualifier decl = AttributeQualifier;
    while ((index = qt_search_for_variable(s, code.size(), index, decl, typeIndex, typeLength,
                                           nameIndex, nameLength, shaderType)) != -1)
    {
        if (decl == AttributeQualifier) {
            if (shaderType == Key::VertexShader)
                attributes.append(QByteArray(s + nameIndex, nameLength));
        } else {
            Q_ASSERT(decl == UniformQualifier);

            const int sampLen = sizeof("sampler2D") - 1;
            const int opLen = sizeof("qt_Opacity") - 1;
            const int matLen = sizeof("qt_Matrix") - 1;
            const int srLen = sizeof("qt_SubRect_") - 1;

            UniformData d;
            QSignalMapper *mapper = 0;
            d.name = QByteArray(s + nameIndex, nameLength);
            if (nameLength == opLen && qstrncmp("qt_Opacity", s + nameIndex, opLen) == 0) {
                d.specialType = UniformData::Opacity;
            } else if (nameLength == matLen && qstrncmp("qt_Matrix", s + nameIndex, matLen) == 0) {
                d.specialType = UniformData::Matrix;
            } else if (nameLength > srLen && qstrncmp("qt_SubRect_", s + nameIndex, srLen) == 0) {
                d.specialType = UniformData::SubRect;
            } else {
                mapper = new QSignalMapper;
                mapper->setMapping(item, uniformData[shaderType].size() | (shaderType << 16));
                d.value = item->property(d.name.constData());
                bool sampler = typeLength == sampLen && qstrncmp("sampler2D", s + typeIndex, sampLen) == 0;
                d.specialType = sampler ? UniformData::Sampler : UniformData::None;
            }
            uniformData[shaderType].append(d);
            signalMappers[shaderType].append(mapper);
        }
    }
}

void QQuickShaderEffectCommon::updateShader(QQuickItem *item, Key::ShaderType shaderType)
{
    disconnectPropertySignals(item, shaderType);
    qDeleteAll(signalMappers[shaderType]);
    uniformData[shaderType].clear();
    signalMappers[shaderType].clear();
    if (shaderType == Key::VertexShader)
        attributes.clear();

    const QByteArray &code = source.sourceCode[shaderType];
    if (code.isEmpty()) {
        // Optimize for default code.
        if (shaderType == Key::VertexShader) {
            attributes.append(QByteArray(qt_position_attribute_name));
            attributes.append(QByteArray(qt_texcoord_attribute_name));
            UniformData d;
            d.name = "qt_Matrix";
            d.specialType = UniformData::Matrix;
            uniformData[Key::VertexShader].append(d);
            signalMappers[Key::VertexShader].append(0);
        } else if (shaderType == Key::FragmentShader) {
            UniformData d;
            d.name = "qt_Opacity";
            d.specialType = UniformData::Opacity;
            uniformData[Key::FragmentShader].append(d);
            signalMappers[Key::FragmentShader].append(0);
            QSignalMapper *mapper = new QSignalMapper;
            mapper->setMapping(item, 1 | (Key::FragmentShader << 16));
            const char *sourceName = "source";
            d.name = sourceName;
            d.value = item->property(sourceName);
            d.specialType = UniformData::Sampler;
            uniformData[Key::FragmentShader].append(d);
            signalMappers[Key::FragmentShader].append(mapper);
        }
    } else {
        lookThroughShaderCode(item, shaderType, code);
    }

    connectPropertySignals(item, shaderType);
}

void QQuickShaderEffectCommon::updateMaterial(QQuickShaderEffectNode *node,
                                              QQuickShaderEffectMaterial *material,
                                              bool updateUniforms, bool updateUniformValues,
                                              bool updateTextureProviders)
{
    if (updateUniforms) {
        for (int i = 0; i < material->textureProviders.size(); ++i) {
            QSGTextureProvider *t = material->textureProviders.at(i);
            if (t) {
                QObject::disconnect(t, SIGNAL(textureChanged()), node, SLOT(markDirtyTexture()));
                QObject::disconnect(t, SIGNAL(destroyed(QObject*)), node, SLOT(textureProviderDestroyed(QObject*)));
            }
        }

        // First make room in the textureProviders array. Set to proper value further down.
        int textureProviderCount = 0;
        for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType) {
            for (int i = 0; i < uniformData[shaderType].size(); ++i) {
                if (uniformData[shaderType].at(i).specialType == UniformData::Sampler)
                    ++textureProviderCount;
            }
            material->uniforms[shaderType] = uniformData[shaderType];
        }
        material->textureProviders.fill(0, textureProviderCount);
        updateUniformValues = false;
        updateTextureProviders = true;
    }

    if (updateUniformValues) {
        for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType) {
            Q_ASSERT(uniformData[shaderType].size() == material->uniforms[shaderType].size());
            for (int i = 0; i < uniformData[shaderType].size(); ++i)
                material->uniforms[shaderType][i].value = uniformData[shaderType].at(i).value;
        }
    }

    if (updateTextureProviders) {
        int index = 0;
        for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType) {
            for (int i = 0; i < uniformData[shaderType].size(); ++i) {
                const UniformData &d = uniformData[shaderType].at(i);
                if (d.specialType != UniformData::Sampler)
                    continue;
                QSGTextureProvider *oldProvider = material->textureProviders.at(index);
                QSGTextureProvider *newProvider = 0;
                QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(d.value));
                if (source && source->isTextureProvider())
                    newProvider = source->textureProvider();
                if (newProvider != oldProvider) {
                    if (oldProvider) {
                        QObject::disconnect(oldProvider, SIGNAL(textureChanged()), node, SLOT(markDirtyTexture()));
                        QObject::disconnect(oldProvider, SIGNAL(destroyed(QObject*)), node, SLOT(textureProviderDestroyed(QObject*)));
                    }
                    if (newProvider) {
                        Q_ASSERT_X(newProvider->thread() == QThread::currentThread(),
                                   "QQuickShaderEffect::updatePaintNode",
                                   "Texture provider must belong to the rendering thread");
                        QObject::connect(newProvider, SIGNAL(textureChanged()), node, SLOT(markDirtyTexture()));
                        QObject::connect(newProvider, SIGNAL(destroyed(QObject*)), node, SLOT(textureProviderDestroyed(QObject*)));
                    } else {
                        const char *typeName = source ? source->metaObject()->className() : d.value.typeName();
                        qWarning("ShaderEffect: Property '%s' is not assigned a valid texture provider (%s).",
                                 d.name.constData(), typeName);
                    }
                    material->textureProviders[index] = newProvider;
                }
                ++index;
            }
        }
        Q_ASSERT(index == material->textureProviders.size());
    }
}

void QQuickShaderEffectCommon::updateWindow(QQuickWindow *window)
{
    // See comment in QQuickShaderEffectCommon::propertyChanged().
    if (window) {
        for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType) {
            for (int i = 0; i < uniformData[shaderType].size(); ++i) {
                const UniformData &d = uniformData[shaderType].at(i);
                if (d.specialType == UniformData::Sampler) {
                    QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(d.value));
                    if (source)
                        QQuickItemPrivate::get(source)->refWindow(window);
                }
            }
        }
    } else {
        for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType) {
            for (int i = 0; i < uniformData[shaderType].size(); ++i) {
                const UniformData &d = uniformData[shaderType].at(i);
                if (d.specialType == UniformData::Sampler) {
                    QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(d.value));
                    if (source)
                        QQuickItemPrivate::get(source)->derefWindow();
                }
            }
        }
    }
}

void QQuickShaderEffectCommon::sourceDestroyed(QObject *object)
{
    for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType) {
        for (int i = 0; i < uniformData[shaderType].size(); ++i) {
            UniformData &d = uniformData[shaderType][i];
            if (d.specialType == UniformData::Sampler && d.value.canConvert<QObject *>()) {
                if (qvariant_cast<QObject *>(d.value) == object)
                    d.value = QVariant();
            }
        }
    }
}

static bool qquick_uniqueInUniformData(QQuickItem *source, const QVector<QQuickShaderEffectMaterial::UniformData> *uniformData, int typeToSkip, int indexToSkip)
{
    for (int s=0; s<QQuickShaderEffectMaterialKey::ShaderTypeCount; ++s) {
        for (int i=0; i<uniformData[s].size(); ++i) {
            if (s == typeToSkip && i == indexToSkip)
                continue;
            const QQuickShaderEffectMaterial::UniformData &d = uniformData[s][i];
            if (d.specialType == QQuickShaderEffectMaterial::UniformData::Sampler && qvariant_cast<QObject *>(d.value) == source)
                return false;
        }
    }
    return true;
}

void QQuickShaderEffectCommon::propertyChanged(QQuickItem *item, int mappedId,
                                               bool *textureProviderChanged)
{
    Key::ShaderType shaderType = Key::ShaderType(mappedId >> 16);
    int index = mappedId & 0xffff;
    UniformData &d = uniformData[shaderType][index];
    if (d.specialType == UniformData::Sampler) {
        QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(d.value));
        if (source) {
            if (item->window())
                QQuickItemPrivate::get(source)->derefWindow();

            // QObject::disconnect() will disconnect all matching connections. If the same
            // source has been attached to two separate samplers, then changing one of them
            // would trigger both to be disconnected. Without the connection we'll end up
            // with a dangling pointer in the uniformData.
            if (qquick_uniqueInUniformData(source, uniformData, shaderType, index))
                QObject::disconnect(source, SIGNAL(destroyed(QObject*)), item, SLOT(sourceDestroyed(QObject*)));
        }

        d.value = item->property(d.name.constData());

        source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(d.value));
        if (source) {
            // 'source' needs a window to get a scene graph node. It usually gets one through its
            // parent, but if the source item is "inline" rather than a reference -- i.e.
            // "property variant source: Image { }" instead of "property variant source: foo" -- it
            // will not get a parent. In those cases, 'source' should get the window from 'item'.
            if (item->window())
                QQuickItemPrivate::get(source)->refWindow(item->window());
            QObject::connect(source, SIGNAL(destroyed(QObject*)), item, SLOT(sourceDestroyed(QObject*)));
        }
        if (textureProviderChanged)
            *textureProviderChanged = true;
    } else {
        d.value = item->property(d.name.constData());
        if (textureProviderChanged)
            *textureProviderChanged = false;
    }
}


/*!
    \qmltype ShaderEffect
    \instantiates QQuickShaderEffect
    \inqmlmodule QtQuick
    \inherits Item
    \ingroup qtquick-effects
    \brief Applies custom shaders to a rectangle

    The ShaderEffect type applies a custom OpenGL
    \l{vertexShader}{vertex} and \l{fragmentShader}{fragment} shader to a
    rectangle. It allows you to write effects such as drop shadow, blur,
    colorize and page curl directly in QML.

    There are two types of input to the \l vertexShader:
    uniform variables and attributes. Some are predefined:
    \list
    \li uniform mat4 qt_Matrix - combined transformation
       matrix, the product of the matrices from the root item to this
       ShaderEffect, and an orthogonal projection.
    \li uniform float qt_Opacity - combined opacity, the product of the
       opacities from the root item to this ShaderEffect.
    \li attribute vec4 qt_Vertex - vertex position, the top-left vertex has
       position (0, 0), the bottom-right (\l{Item::width}{width},
       \l{Item::height}{height}).
    \li attribute vec2 qt_MultiTexCoord0 - texture coordinate, the top-left
       coordinate is (0, 0), the bottom-right (1, 1). If \l supportsAtlasTextures
       is true, coordinates will be based on position in the atlas instead.
    \endlist

    In addition, any property that can be mapped to an OpenGL Shading Language
    (GLSL) type is available as a uniform variable. The following list shows
    how properties are mapped to GLSL uniform variables:
    \list
    \li bool, int, qreal -> bool, int, float - If the type in the shader is not
       the same as in QML, the value is converted automatically.
    \li QColor -> vec4 - When colors are passed to the shader, they are first
       premultiplied. Thus Qt.rgba(0.2, 0.6, 1.0, 0.5) becomes
       vec4(0.1, 0.3, 0.5, 0.5) in the shader, for example.
    \li QRect, QRectF -> vec4 - Qt.rect(x, y, w, h) becomes vec4(x, y, w, h) in
       the shader.
    \li QPoint, QPointF, QSize, QSizeF -> vec2
    \li QVector3D -> vec3
    \li QVector4D -> vec4
    \li QTransform -> mat3
    \li QMatrix4x4 -> mat4
    \li QQuaternion -> vec4, scalar value is \c w.
    \li \l Image, \l ShaderEffectSource -> sampler2D - Origin is in the top-left
       corner, and the color values are premultiplied.
    \endlist

    The QML scene graph back-end may choose to allocate textures in texture
    atlases. If a texture allocated in an atlas is passed to a ShaderEffect,
    it is by default copied from the texture atlas into a stand-alone texture
    so that the texture coordinates span from 0 to 1, and you get the expected
    wrap modes. However, this will increase the memory usage. To avoid the
    texture copy, set \l supportsAtlasTextures for simple shaders using
    qt_MultiTexCoord0, or for each "uniform sampler2D <name>" declare a
    "uniform vec4 qt_SubRect_<name>" which will be assigned the texture's
    normalized source rectangle. For stand-alone textures, the source rectangle
    is [0, 1]x[0, 1]. For textures in an atlas, the source rectangle corresponds
    to the part of the texture atlas where the texture is stored.
    The correct way to calculate the texture coordinate for a texture called
    "source" within a texture atlas is
    "qt_SubRect_source.xy + qt_SubRect_source.zw * qt_MultiTexCoord0".

    The output from the \l fragmentShader should be premultiplied. If
    \l blending is enabled, source-over blending is used. However, additive
    blending can be achieved by outputting zero in the alpha channel.

    \table
    \row
    \li \image declarative-shadereffectitem.png
    \li \qml
        import QtQuick 2.0

        Rectangle {
            width: 200; height: 100
            Row {
                Image { id: img; sourceSize { width: 100; height: 100 } source: "qt-logo.png" }
                ShaderEffect {
                    width: 100; height: 100
                    property variant src: img
                    vertexShader: "
                        uniform highp mat4 qt_Matrix;
                        attribute highp vec4 qt_Vertex;
                        attribute highp vec2 qt_MultiTexCoord0;
                        varying highp vec2 coord;
                        void main() {
                            coord = qt_MultiTexCoord0;
                            gl_Position = qt_Matrix * qt_Vertex;
                        }"
                    fragmentShader: "
                        varying highp vec2 coord;
                        uniform sampler2D src;
                        uniform lowp float qt_Opacity;
                        void main() {
                            lowp vec4 tex = texture2D(src, coord);
                            gl_FragColor = vec4(vec3(dot(tex.rgb, vec3(0.344, 0.5, 0.156))), tex.a) * qt_Opacity;
                        }"
                }
            }
        }
        \endqml
    \endtable

    By default, the ShaderEffect consists of four vertices, one for each
    corner. For non-linear vertex transformations, like page curl, you can
    specify a fine grid of vertices by specifying a \l mesh resolution.

    \section1 ShaderEffect and Item Layers

    The ShaderEffect type can be combined with \l {Item Layers} {layered items}.

    \table
    \row
      \li \b {Layer with effect disabled} \inlineimage qml-shadereffect-nolayereffect.png
      \li \b {Layer with effect enabled} \inlineimage qml-shadereffect-layereffect.png
      \li \snippet qml/layerwitheffect.qml 1
    \endtable

    It is also possible to combine multiple layered items:

    \table
    \row
      \li \inlineimage qml-shadereffect-opacitymask.png
      \li \snippet qml/opacitymask.qml 1
    \endtable

    The \l {Qt Graphical Effects} module contains several ready-made effects
    for using with Qt Quick applications.

    \note Scene Graph textures have origin in the top-left corner rather than
    bottom-left which is common in OpenGL.

    For information about the GLSL version being used, see \l QtQuick::OpenGLInfo.

    \sa {Item Layers}
*/

QQuickShaderEffect::QQuickShaderEffect(QQuickItem *parent)
    : QQuickItem(parent)
    , m_meshResolution(1, 1)
    , m_mesh(0)
    , m_cullMode(NoCulling)
    , m_status(Uncompiled)
    , m_blending(true)
    , m_dirtyUniforms(true)
    , m_dirtyUniformValues(true)
    , m_dirtyTextureProviders(true)
    , m_dirtyProgram(true)
    , m_dirtyParseLog(true)
    , m_dirtyMesh(true)
    , m_dirtyGeometry(true)
    , m_customVertexShader(false)
    , m_supportsAtlasTextures(false)
{
    setFlag(QQuickItem::ItemHasContents);
}

QQuickShaderEffect::~QQuickShaderEffect()
{
    for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType)
        m_common.disconnectPropertySignals(this, Key::ShaderType(shaderType));
}

/*!
    \qmlproperty string QtQuick::ShaderEffect::fragmentShader

    This property holds the fragment shader's GLSL source code.
    The default shader passes the texture coordinate along to the fragment
    shader as "varying highp vec2 qt_TexCoord0".
*/

void QQuickShaderEffect::setFragmentShader(const QByteArray &code)
{
    if (m_common.source.sourceCode[Key::FragmentShader].constData() == code.constData())
        return;
    m_common.source.sourceCode[Key::FragmentShader] = code;
    m_dirtyProgram = true;
    m_dirtyParseLog = true;

    if (isComponentComplete())
        m_common.updateShader(this, Key::FragmentShader);

    update();
    if (m_status != Uncompiled) {
        m_status = Uncompiled;
        emit statusChanged();
    }
    emit fragmentShaderChanged();
}

/*!
    \qmlproperty string QtQuick::ShaderEffect::vertexShader

    This property holds the vertex shader's GLSL source code.
    The default shader expects the texture coordinate to be passed from the
    vertex shader as "varying highp vec2 qt_TexCoord0", and it samples from a
    sampler2D named "source".
*/

void QQuickShaderEffect::setVertexShader(const QByteArray &code)
{
    if (m_common.source.sourceCode[Key::VertexShader].constData() == code.constData())
        return;
    m_common.source.sourceCode[Key::VertexShader] = code;
    m_dirtyProgram = true;
    m_dirtyParseLog = true;
    m_customVertexShader = true;

    if (isComponentComplete())
        m_common.updateShader(this, Key::VertexShader);

    update();
    if (m_status != Uncompiled) {
        m_status = Uncompiled;
        emit statusChanged();
    }
    emit vertexShaderChanged();
}

/*!
    \qmlproperty bool QtQuick::ShaderEffect::blending

    If this property is true, the output from the \l fragmentShader is blended
    with the background using source-over blend mode. If false, the background
    is disregarded. Blending decreases the performance, so you should set this
    property to false when blending is not needed. The default value is true.
*/

void QQuickShaderEffect::setBlending(bool enable)
{
    if (blending() == enable)
        return;

    m_blending = enable;
    update();

    emit blendingChanged();
}

/*!
    \qmlproperty variant QtQuick::ShaderEffect::mesh

    This property defines the mesh used to draw the ShaderEffect. It can hold
    any \l GridMesh object.
    If a size value is assigned to this property, the ShaderEffect implicitly
    uses a \l GridMesh with the value as
    \l{GridMesh::resolution}{mesh resolution}. By default, this property is
    the size 1x1.

    \sa GridMesh
*/

QVariant QQuickShaderEffect::mesh() const
{
    return m_mesh ? qVariantFromValue(static_cast<QObject *>(m_mesh))
                  : qVariantFromValue(m_meshResolution);
}

void QQuickShaderEffect::setMesh(const QVariant &mesh)
{
    QQuickShaderEffectMesh *newMesh = qobject_cast<QQuickShaderEffectMesh *>(qvariant_cast<QObject *>(mesh));
    if (newMesh && newMesh == m_mesh)
        return;
    if (m_mesh)
        disconnect(m_mesh, SIGNAL(geometryChanged()), this, 0);
    m_mesh = newMesh;
    if (m_mesh) {
        connect(m_mesh, SIGNAL(geometryChanged()), this, SLOT(updateGeometry()));
    } else {
        if (mesh.canConvert<QSize>()) {
            m_meshResolution = mesh.toSize();
        } else {
            QList<QByteArray> res = mesh.toByteArray().split('x');
            bool ok = res.size() == 2;
            if (ok) {
                int w = res.at(0).toInt(&ok);
                if (ok) {
                    int h = res.at(1).toInt(&ok);
                    if (ok)
                        m_meshResolution = QSize(w, h);
                }
            }
            if (!ok)
                qWarning("ShaderEffect: mesh property must be size or object deriving from QQuickShaderEffectMesh.");
        }
        m_defaultMesh.setResolution(m_meshResolution);
    }

    m_dirtyMesh = true;
    m_dirtyParseLog = true;
    update();
    emit meshChanged();
}

/*!
    \qmlproperty enumeration QtQuick::ShaderEffect::cullMode

    This property defines which sides of the item should be visible.

    \list
    \li ShaderEffect.NoCulling - Both sides are visible
    \li ShaderEffect.BackFaceCulling - only front side is visible
    \li ShaderEffect.FrontFaceCulling - only back side is visible
    \endlist

    The default is NoCulling.
*/

void QQuickShaderEffect::setCullMode(CullMode face)
{
    if (face == m_cullMode)
        return;
    m_cullMode = face;
    update();
    emit cullModeChanged();
}

/*!
    \qmlproperty bool QtQuick::ShaderEffect::supportsAtlasTextures

    Set this property true to indicate that the ShaderEffect is able to
    use the default source texture without first removing it from an atlas.
    In this case the range of qt_MultiTexCoord0 will based on the position of
    the texture within the atlas, rather than (0,0) to (1,1).

    Setting this to true may enable some optimizations.

    The default value is false.

    \since 5.4
    \since QtQuick 2.4
*/

void QQuickShaderEffect::setSupportsAtlasTextures(bool supports)
{
    if (supports == m_supportsAtlasTextures)
        return;
    m_supportsAtlasTextures = supports;
    updateGeometry();
    emit supportsAtlasTexturesChanged();
}

QString QQuickShaderEffect::parseLog()
{
    if (m_dirtyParseLog) {
        m_common.updateParseLog(m_mesh != 0);
        m_dirtyParseLog = false;
    }
    return m_common.parseLog;
}

bool QQuickShaderEffect::event(QEvent *event)
{
    if (event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *e = static_cast<QDynamicPropertyChangeEvent *>(event);
        for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType) {
            for (int i = 0; i < m_common.uniformData[shaderType].size(); ++i) {
                if (m_common.uniformData[shaderType].at(i).name == e->propertyName()) {
                    bool textureProviderChanged;
                    m_common.propertyChanged(this, (shaderType << 16) | i, &textureProviderChanged);
                    m_dirtyTextureProviders |= textureProviderChanged;
                    m_dirtyUniformValues = true;
                    update();
                }
            }
        }
    }
    return QQuickItem::event(event);
}

/*!
    \qmlproperty enumeration QtQuick::ShaderEffect::status

    This property tells the current status of the OpenGL shader program.

    \list
    \li ShaderEffect.Compiled - the shader program was successfully compiled and linked.
    \li ShaderEffect.Uncompiled - the shader program has not yet been compiled.
    \li ShaderEffect.Error - the shader program failed to compile or link.
    \endlist

    When setting the fragment or vertex shader source code, the status will become Uncompiled.
    The first time the ShaderEffect is rendered with new shader source code, the shaders are
    compiled and linked, and the status is updated to Compiled or Error.

    \sa log
*/

/*!
    \qmlproperty string QtQuick::ShaderEffect::log

    This property holds a log of warnings and errors from the latest attempt at compiling and
    linking the OpenGL shader program. It is updated at the same time \l status is set to Compiled
    or Error.

    \sa status
*/

void QQuickShaderEffect::updateGeometry()
{
    m_dirtyGeometry = true;
    update();
}

void QQuickShaderEffect::updateLogAndStatus(const QString &log, int status)
{
    m_log = parseLog() + log;
    m_status = Status(status);
    emit logChanged();
    emit statusChanged();
}

void QQuickShaderEffect::sourceDestroyed(QObject *object)
{
    m_common.sourceDestroyed(object);
}


void QQuickShaderEffect::propertyChanged(int mappedId)
{
    bool textureProviderChanged;
    m_common.propertyChanged(this, mappedId, &textureProviderChanged);
    m_dirtyTextureProviders |= textureProviderChanged;
    m_dirtyUniformValues = true;
    update();
}

void QQuickShaderEffect::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    m_dirtyGeometry = true;
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}

QSGNode *QQuickShaderEffect::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    QQuickShaderEffectNode *node = static_cast<QQuickShaderEffectNode *>(oldNode);

    // In the case of zero-size or a bad vertex shader, don't try to create a node...
    if (m_common.attributes.isEmpty() || width() <= 0 || height() <= 0) {
        if (node)
            delete node;
        return 0;
    }

    if (!node) {
        node = new QQuickShaderEffectNode;
        node->setMaterial(new QQuickShaderEffectMaterial(node));
        node->setFlag(QSGNode::OwnsMaterial, true);
        m_dirtyProgram = true;
        m_dirtyUniforms = true;
        m_dirtyGeometry = true;
        connect(node, SIGNAL(logAndStatusChanged(QString,int)), this, SLOT(updateLogAndStatus(QString,int)));
    }

    QQuickShaderEffectMaterial *material = static_cast<QQuickShaderEffectMaterial *>(node->material());

    // Update blending
    if (bool(material->flags() & QSGMaterial::Blending) != m_blending) {
        material->setFlag(QSGMaterial::Blending, m_blending);
        node->markDirty(QSGNode::DirtyMaterial);
    }

    if (int(material->cullMode) != int(m_cullMode)) {
        material->cullMode = QQuickShaderEffectMaterial::CullMode(m_cullMode);
        node->markDirty(QSGNode::DirtyMaterial);
    }

    if (m_dirtyProgram) {
        Key s = m_common.source;
        QSGShaderSourceBuilder builder;
        if (s.sourceCode[Key::FragmentShader].isEmpty()) {
            builder.appendSourceFile(QStringLiteral(":/items/shaders/shadereffect.frag"));
            s.sourceCode[Key::FragmentShader] = builder.source();
            builder.clear();
        }
        if (s.sourceCode[Key::VertexShader].isEmpty()) {
            builder.appendSourceFile(QStringLiteral(":/items/shaders/shadereffect.vert"));
            s.sourceCode[Key::VertexShader] = builder.source();
        }
        s.className = metaObject()->className();

        material->setProgramSource(s);
        material->attributes = m_common.attributes;
        node->markDirty(QSGNode::DirtyMaterial);
        m_dirtyProgram = false;
        m_dirtyUniforms = true;
    }

    if (m_dirtyUniforms || m_dirtyUniformValues || m_dirtyTextureProviders) {
        m_common.updateMaterial(node, material, m_dirtyUniforms, m_dirtyUniformValues,
                                m_dirtyTextureProviders);
        node->markDirty(QSGNode::DirtyMaterial);
        m_dirtyUniforms = m_dirtyUniformValues = m_dirtyTextureProviders = false;
    }

    int textureCount = material->textureProviders.size();
    bool preventBatching = m_customVertexShader || textureCount > 1 || (textureCount > 0 && !m_supportsAtlasTextures);

    QRectF srcRect(0, 0, 1, 1);
    if (m_supportsAtlasTextures && textureCount != 0) {
        if (QSGTextureProvider *provider = material->textureProviders.at(0)) {
            if (provider->texture())
                srcRect = provider->texture()->normalizedTextureSubRect();
        }
    }

    if (bool(material->flags() & QSGMaterial::RequiresFullMatrix) != preventBatching) {
        material->setFlag(QSGMaterial::RequiresFullMatrix, preventBatching);
        node->markDirty(QSGNode::DirtyMaterial);
    }

    if (material->supportsAtlasTextures != m_supportsAtlasTextures) {
        material->supportsAtlasTextures = m_supportsAtlasTextures;
        node->markDirty(QSGNode::DirtyMaterial);
    }

    if (m_dirtyMesh) {
        node->setGeometry(0);
        m_dirtyMesh = false;
        m_dirtyGeometry = true;
    }

    if (m_dirtyGeometry) {
        node->setFlag(QSGNode::OwnsGeometry, false);
        QSGGeometry *geometry = node->geometry();
        QRectF rect(0, 0, width(), height());
        QQuickShaderEffectMesh *mesh = m_mesh ? m_mesh : &m_defaultMesh;

        geometry = mesh->updateGeometry(geometry, m_common.attributes, srcRect, rect);
        if (!geometry) {
            QString log = mesh->log();
            if (!log.isNull()) {
                m_log = parseLog();
                m_log += QLatin1String("*** Mesh ***\n");
                m_log += log;
                m_status = Error;
                emit logChanged();
                emit statusChanged();
            }
            delete node;
            return 0;
        }

        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry, true);

        m_dirtyGeometry = false;
    }

    return node;
}

void QQuickShaderEffect::componentComplete()
{
    m_common.updateShader(this, Key::VertexShader);
    m_common.updateShader(this, Key::FragmentShader);
    QQuickItem::componentComplete();
}

void QQuickShaderEffect::itemChange(ItemChange change, const ItemChangeData &value)
{
    if (change == QQuickItem::ItemSceneChange)
        m_common.updateWindow(value.window);
    QQuickItem::itemChange(change, value);
}

QT_END_NAMESPACE
