/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include <private/qquickopenglshadereffect_p.h>

#include <QtQuick/qsgmaterial.h>
#include <QtQuick/private/qsgshadersourcebuilder_p.h>
#include "qquickitem_p.h"

#include <QtQuick/private/qsgcontext_p.h>
#include <QtQuick/qsgtextureprovider.h>
#include "qquickwindow.h"

#include "qquickimage_p.h"
#include "qquickshadereffectsource_p.h"
#include "qquickshadereffectmesh_p.h"

#include <QtQml/qqmlfile.h>
#include <QtCore/qsignalmapper.h>
#include <QtCore/qfileselector.h>

QT_BEGIN_NAMESPACE

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
                               QQuickOpenGLShaderEffectCommon::Key::ShaderType shaderType)
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
                    } else if (shaderType == QQuickOpenGLShaderEffectCommon::Key::VertexShader
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

namespace QtPrivate {
class MappedSlotObject: public QtPrivate::QSlotObjectBase
{
public:
    typedef std::function<void()> PropChangedFunc;

    explicit MappedSlotObject(PropChangedFunc func)
        : QSlotObjectBase(&impl), _signalIndex(-1), func(func)
    { ref(); }

    void setSignalIndex(int idx) { _signalIndex = idx; }
    int signalIndex() const { return _signalIndex; }

private:
    int _signalIndex;
    PropChangedFunc func;

    static void impl(int which, QSlotObjectBase *this_, QObject *, void **a, bool *ret)
    {
        auto thiz = static_cast<MappedSlotObject*>(this_);
        switch (which) {
        case Destroy:
            delete thiz;
            break;
        case Call:
            thiz->func();
            break;
        case Compare:
            *ret = thiz == reinterpret_cast<MappedSlotObject *>(a[0]);
            break;
        case NumOperations: ;
        }
    }
};
}

QQuickOpenGLShaderEffectCommon::~QQuickOpenGLShaderEffectCommon()
{
    for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType)
        clearSignalMappers(shaderType);
}

void QQuickOpenGLShaderEffectCommon::disconnectPropertySignals(QQuickItem *item, Key::ShaderType shaderType)
{
    for (int i = 0; i < uniformData[shaderType].size(); ++i) {
        if (signalMappers[shaderType].at(i) == 0)
            continue;
        const UniformData &d = uniformData[shaderType].at(i);
        auto mapper = signalMappers[shaderType].at(i);
        void *a = mapper;
        QObjectPrivate::disconnect(item, mapper->signalIndex(), &a);
        if (d.specialType == UniformData::Sampler) {
            QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(d.value));
            if (source) {
                if (item->window())
                    QQuickItemPrivate::get(source)->derefWindow();
                QObject::disconnect(source, SIGNAL(destroyed(QObject*)), host, SLOT(sourceDestroyed(QObject*)));
            }
        }
    }
}

void QQuickOpenGLShaderEffectCommon::connectPropertySignals(QQuickItem *item,
                                                            const QMetaObject *itemMetaObject,
                                                            Key::ShaderType shaderType)
{
    QQmlPropertyCache *propCache = QQmlData::ensurePropertyCache(qmlEngine(item), item);
    for (int i = 0; i < uniformData[shaderType].size(); ++i) {
        if (signalMappers[shaderType].at(i) == 0)
            continue;
        const UniformData &d = uniformData[shaderType].at(i);
        QQmlPropertyData *pd = propCache->property(QString::fromUtf8(d.name), nullptr, nullptr);
        if (pd && !pd->isFunction()) {
            if (pd->notifyIndex() == -1) {
                qWarning("QQuickOpenGLShaderEffect: property '%s' does not have notification method!", d.name.constData());
            } else {
                auto *mapper = signalMappers[shaderType].at(i);
                mapper->setSignalIndex(pd->notifyIndex());
                Q_ASSERT(item->metaObject() == itemMetaObject);
                QObjectPrivate::connectImpl(item, mapper->signalIndex(), item, nullptr, mapper,
                                            Qt::AutoConnection, nullptr, itemMetaObject);
            }
        } else {
            // If the source is set via a dynamic property, like the layer is, then we need this
            // check to disable the warning.
            if (!item->property(d.name).isValid())
                qWarning("QQuickOpenGLShaderEffect: '%s' does not have a matching property!", d.name.constData());
        }

        if (d.specialType == UniformData::Sampler) {
            QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(d.value));
            if (source) {
                if (item->window())
                    QQuickItemPrivate::get(source)->refWindow(item->window());
                QObject::connect(source, SIGNAL(destroyed(QObject*)), host, SLOT(sourceDestroyed(QObject*)));
            }
        }
    }
}

void QQuickOpenGLShaderEffectCommon::updateParseLog(bool ignoreAttributes)
{
    parseLog.clear();
    if (!ignoreAttributes) {
        if (!attributes.contains(qtPositionAttributeName())) {
            parseLog += QLatin1String("Warning: Missing reference to \'");
            parseLog += QLatin1String(qtPositionAttributeName());
            parseLog += QLatin1String("\'.\n");
        }
        if (!attributes.contains(qtTexCoordAttributeName())) {
            parseLog += QLatin1String("Warning: Missing reference to \'");
            parseLog += QLatin1String(qtTexCoordAttributeName());
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

void QQuickOpenGLShaderEffectCommon::lookThroughShaderCode(QQuickItem *item,
                                                           const QMetaObject *itemMetaObject,
                                                           Key::ShaderType shaderType,
                                                           const QByteArray &code)
{
    QQmlPropertyCache *propCache = QQmlData::ensurePropertyCache(qmlEngine(item), item);
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
            QtPrivate::MappedSlotObject *mapper = nullptr;
            d.name = QByteArray(s + nameIndex, nameLength);
            if (nameLength == opLen && qstrncmp("qt_Opacity", s + nameIndex, opLen) == 0) {
                d.specialType = UniformData::Opacity;
            } else if (nameLength == matLen && qstrncmp("qt_Matrix", s + nameIndex, matLen) == 0) {
                d.specialType = UniformData::Matrix;
            } else if (nameLength > srLen && qstrncmp("qt_SubRect_", s + nameIndex, srLen) == 0) {
                d.specialType = UniformData::SubRect;
            } else {
                if (QQmlPropertyData *pd = propCache->property(QString::fromUtf8(d.name), nullptr, nullptr)) {
                    if (!pd->isFunction())
                        d.propertyIndex = pd->coreIndex();
                }
                const int mappedId = uniformData[shaderType].size() | (shaderType << 16);
                mapper = new QtPrivate::MappedSlotObject([this, mappedId](){
                    this->mappedPropertyChanged(mappedId);
                });
                bool sampler = typeLength == sampLen && qstrncmp("sampler2D", s + typeIndex, sampLen) == 0;
                d.specialType = sampler ? UniformData::Sampler : UniformData::None;
                d.setValueFromProperty(item, itemMetaObject);
            }
            uniformData[shaderType].append(d);
            signalMappers[shaderType].append(mapper);
        }
    }
}

void QQuickOpenGLShaderEffectCommon::updateShader(QQuickItem *item,
                                                  const QMetaObject *itemMetaObject,
                                                  Key::ShaderType shaderType)
{
    disconnectPropertySignals(item, shaderType);
    uniformData[shaderType].clear();
    clearSignalMappers(shaderType);
    if (shaderType == Key::VertexShader)
        attributes.clear();

    // A qrc or file URL means the shader source is to be read from the specified file.
    QUrl srcUrl(QString::fromUtf8(source.sourceCode[shaderType]));
    if (!srcUrl.scheme().compare(QLatin1String("qrc"), Qt::CaseInsensitive) || srcUrl.isLocalFile()) {
        if (!fileSelector) {
            fileSelector = new QFileSelector(item);
            // There may not be an OpenGL context accessible here. So rely on
            // the window's requestedFormat().
            if (item->window()
                    && item->window()->requestedFormat().profile() == QSurfaceFormat::CoreProfile) {
                fileSelector->setExtraSelectors(QStringList() << QStringLiteral("glslcore"));
            }
        }
        const QString fn = fileSelector->select(QQmlFile::urlToLocalFileOrQrc(srcUrl));
        QFile f(fn);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            source.sourceCode[shaderType] = f.readAll();
            f.close();
        } else {
            qWarning("ShaderEffect: Failed to read %s", qPrintable(fn));
            source.sourceCode[shaderType] = QByteArray();
        }
    }

    const QByteArray &code = source.sourceCode[shaderType];
    if (code.isEmpty()) {
        // Optimize for default code.
        if (shaderType == Key::VertexShader) {
            attributes.append(QByteArray(qtPositionAttributeName()));
            attributes.append(QByteArray(qtTexCoordAttributeName()));
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
            const int mappedId = 1 | (Key::FragmentShader << 16);
            auto mapper = new QtPrivate::MappedSlotObject([this, mappedId](){mappedPropertyChanged(mappedId);});
            const char *sourceName = "source";
            d.name = sourceName;
            d.setValueFromProperty(item, itemMetaObject);
            d.specialType = UniformData::Sampler;
            uniformData[Key::FragmentShader].append(d);
            signalMappers[Key::FragmentShader].append(mapper);
        }
    } else {
        lookThroughShaderCode(item, itemMetaObject, shaderType, code);
    }

    connectPropertySignals(item, itemMetaObject, shaderType);
}

void QQuickOpenGLShaderEffectCommon::updateMaterial(QQuickOpenGLShaderEffectNode *node,
                                              QQuickOpenGLShaderEffectMaterial *material,
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
                                   "QQuickOpenGLShaderEffect::updatePaintNode",
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

void QQuickOpenGLShaderEffectCommon::updateWindow(QQuickWindow *window)
{
    // See comment in QQuickOpenGLShaderEffectCommon::propertyChanged().
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

void QQuickOpenGLShaderEffectCommon::sourceDestroyed(QObject *object)
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

static bool qquick_uniqueInUniformData(QQuickItem *source, const QVector<QQuickOpenGLShaderEffectMaterial::UniformData> *uniformData, int typeToSkip, int indexToSkip)
{
    for (int s=0; s<QQuickOpenGLShaderEffectMaterialKey::ShaderTypeCount; ++s) {
        for (int i=0; i<uniformData[s].size(); ++i) {
            if (s == typeToSkip && i == indexToSkip)
                continue;
            const QQuickOpenGLShaderEffectMaterial::UniformData &d = uniformData[s][i];
            if (d.specialType == QQuickOpenGLShaderEffectMaterial::UniformData::Sampler && qvariant_cast<QObject *>(d.value) == source)
                return false;
        }
    }
    return true;
}

void QQuickOpenGLShaderEffectCommon::propertyChanged(QQuickItem *item,
                                                     const QMetaObject *itemMetaObject,
                                                     int mappedId, bool *textureProviderChanged)
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
                QObject::disconnect(source, SIGNAL(destroyed(QObject*)), host, SLOT(sourceDestroyed(QObject*)));
        }

        d.setValueFromProperty(item, itemMetaObject);

        source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(d.value));
        if (source) {
            // 'source' needs a window to get a scene graph node. It usually gets one through its
            // parent, but if the source item is "inline" rather than a reference -- i.e.
            // "property variant source: Image { }" instead of "property variant source: foo" -- it
            // will not get a parent. In those cases, 'source' should get the window from 'item'.
            if (item->window())
                QQuickItemPrivate::get(source)->refWindow(item->window());
            QObject::connect(source, SIGNAL(destroyed(QObject*)), host, SLOT(sourceDestroyed(QObject*)));
        }
        if (textureProviderChanged)
            *textureProviderChanged = true;
    } else {
        d.setValueFromProperty(item, itemMetaObject);
        if (textureProviderChanged)
            *textureProviderChanged = false;
    }
}

void QQuickOpenGLShaderEffectCommon::clearSignalMappers(int shader)
{
    for (auto mapper : qAsConst(signalMappers[shader])) {
        if (mapper)
            mapper->destroyIfLastRef();
    }
    signalMappers[shader].clear();
}

QQuickOpenGLShaderEffect::QQuickOpenGLShaderEffect(QQuickShaderEffect *item, QObject *parent)
    : QObject(parent)
    , m_item(item)
    , m_itemMetaObject(nullptr)
    , m_meshResolution(1, 1)
    , m_mesh(0)
    , m_cullMode(QQuickShaderEffect::NoCulling)
    , m_status(QQuickShaderEffect::Uncompiled)
    , m_common(this, [this](int mappedId){this->propertyChanged(mappedId);})
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
    , m_vertNeedsUpdate(true)
    , m_fragNeedsUpdate(true)
{
}

QQuickOpenGLShaderEffect::~QQuickOpenGLShaderEffect()
{
    for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType)
        m_common.disconnectPropertySignals(m_item, Key::ShaderType(shaderType));
}

void QQuickOpenGLShaderEffect::setFragmentShader(const QByteArray &code)
{
    if (m_common.source.sourceCode[Key::FragmentShader].constData() == code.constData())
        return;
    m_common.source.sourceCode[Key::FragmentShader] = code;
    m_dirtyProgram = true;
    m_dirtyParseLog = true;

    m_fragNeedsUpdate = true;
    if (m_item->isComponentComplete())
        maybeUpdateShaders();

    m_item->update();
    if (m_status != QQuickShaderEffect::Uncompiled) {
        m_status = QQuickShaderEffect::Uncompiled;
        emit m_item->statusChanged();
    }
    emit m_item->fragmentShaderChanged();
}

void QQuickOpenGLShaderEffect::setVertexShader(const QByteArray &code)
{
    if (m_common.source.sourceCode[Key::VertexShader].constData() == code.constData())
        return;
    m_common.source.sourceCode[Key::VertexShader] = code;
    m_dirtyProgram = true;
    m_dirtyParseLog = true;
    m_customVertexShader = true;

    m_vertNeedsUpdate = true;
    if (m_item->isComponentComplete())
        maybeUpdateShaders();

    m_item->update();
    if (m_status != QQuickShaderEffect::Uncompiled) {
        m_status = QQuickShaderEffect::Uncompiled;
        emit m_item->statusChanged();
    }
    emit m_item->vertexShaderChanged();
}

void QQuickOpenGLShaderEffect::setBlending(bool enable)
{
    if (blending() == enable)
        return;

    m_blending = enable;
    m_item->update();

    emit m_item->blendingChanged();
}

QVariant QQuickOpenGLShaderEffect::mesh() const
{
    return m_mesh ? qVariantFromValue(static_cast<QObject *>(m_mesh))
                  : qVariantFromValue(m_meshResolution);
}

void QQuickOpenGLShaderEffect::setMesh(const QVariant &mesh)
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
    m_item->update();
    emit m_item->meshChanged();
}

void QQuickOpenGLShaderEffect::setCullMode(QQuickShaderEffect::CullMode face)
{
    if (face == m_cullMode)
        return;
    m_cullMode = face;
    m_item->update();
    emit m_item->cullModeChanged();
}

void QQuickOpenGLShaderEffect::setSupportsAtlasTextures(bool supports)
{
    if (supports == m_supportsAtlasTextures)
        return;
    m_supportsAtlasTextures = supports;
    updateGeometry();
    emit m_item->supportsAtlasTexturesChanged();
}

QString QQuickOpenGLShaderEffect::parseLog()
{
    maybeUpdateShaders(true);

    if (m_dirtyParseLog) {
        m_common.updateParseLog(m_mesh != 0);
        m_dirtyParseLog = false;
    }
    return m_common.parseLog;
}

void QQuickOpenGLShaderEffect::handleEvent(QEvent *event)
{
    if (event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *e = static_cast<QDynamicPropertyChangeEvent *>(event);
        for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType) {
            for (int i = 0; i < m_common.uniformData[shaderType].size(); ++i) {
                if (m_common.uniformData[shaderType].at(i).name == e->propertyName()) {
                    bool textureProviderChanged;
                    m_common.propertyChanged(m_item, m_itemMetaObject,
                                             (shaderType << 16) | i, &textureProviderChanged);
                    m_dirtyTextureProviders |= textureProviderChanged;
                    m_dirtyUniformValues = true;
                    m_item->update();
                }
            }
        }
    }
}

void QQuickOpenGLShaderEffect::updateGeometry()
{
    m_dirtyGeometry = true;
    m_item->update();
}

void QQuickOpenGLShaderEffect::updateGeometryIfAtlased()
{
    if (m_supportsAtlasTextures)
        updateGeometry();
}

void QQuickOpenGLShaderEffect::updateLogAndStatus(const QString &log, int status)
{
    m_log = parseLog() + log;
    m_status = QQuickShaderEffect::Status(status);
    emit m_item->logChanged();
    emit m_item->statusChanged();
}

void QQuickOpenGLShaderEffect::sourceDestroyed(QObject *object)
{
    m_common.sourceDestroyed(object);
}

void QQuickOpenGLShaderEffect::propertyChanged(int mappedId)
{
    bool textureProviderChanged;
    m_common.propertyChanged(m_item, m_itemMetaObject, mappedId, &textureProviderChanged);
    m_dirtyTextureProviders |= textureProviderChanged;
    m_dirtyUniformValues = true;
    m_item->update();
}

void QQuickOpenGLShaderEffect::handleGeometryChanged(const QRectF &, const QRectF &)
{
    m_dirtyGeometry = true;
}

QSGNode *QQuickOpenGLShaderEffect::handleUpdatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    QQuickOpenGLShaderEffectNode *node = static_cast<QQuickOpenGLShaderEffectNode *>(oldNode);

    // In the case of zero-size or a bad vertex shader, don't try to create a node...
    if (m_common.attributes.isEmpty() || m_item->width() <= 0 || m_item->height() <= 0) {
        if (node)
            delete node;
        return 0;
    }

    if (!node) {
        node = new QQuickOpenGLShaderEffectNode;
        node->setMaterial(new QQuickOpenGLShaderEffectMaterial(node));
        node->setFlag(QSGNode::OwnsMaterial, true);
        m_dirtyProgram = true;
        m_dirtyUniforms = true;
        m_dirtyGeometry = true;
        connect(node, SIGNAL(logAndStatusChanged(QString,int)), this, SLOT(updateLogAndStatus(QString,int)));
        connect(node, &QQuickOpenGLShaderEffectNode::dirtyTexture,
                this, &QQuickOpenGLShaderEffect::updateGeometryIfAtlased);
    }

    QQuickOpenGLShaderEffectMaterial *material = static_cast<QQuickOpenGLShaderEffectMaterial *>(node->material());

    // Update blending
    if (bool(material->flags() & QSGMaterial::Blending) != m_blending) {
        material->setFlag(QSGMaterial::Blending, m_blending);
        node->markDirty(QSGNode::DirtyMaterial);
    }

    if (int(material->cullMode) != int(m_cullMode)) {
        material->cullMode = QQuickShaderEffect::CullMode(m_cullMode);
        node->markDirty(QSGNode::DirtyMaterial);
    }

    if (m_dirtyProgram) {
        Key s = m_common.source;
        QSGShaderSourceBuilder builder;
        if (s.sourceCode[Key::FragmentShader].isEmpty()) {
            builder.appendSourceFile(QStringLiteral(":/qt-project.org/items/shaders/shadereffect.frag"));
            s.sourceCode[Key::FragmentShader] = builder.source();
            builder.clear();
        }
        if (s.sourceCode[Key::VertexShader].isEmpty()) {
            builder.appendSourceFile(QStringLiteral(":/qt-project.org/items/shaders/shadereffect.vert"));
            s.sourceCode[Key::VertexShader] = builder.source();
        }

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

    QRectF srcRect(0, 0, 1, 1);
    bool geometryUsesTextureSubRect = false;
    if (m_supportsAtlasTextures && material->textureProviders.size() == 1) {
        QSGTextureProvider *provider = material->textureProviders.at(0);
        if (provider->texture()) {
            srcRect = provider->texture()->normalizedTextureSubRect();
            geometryUsesTextureSubRect = true;
        }
    }

    if (bool(material->flags() & QSGMaterial::RequiresFullMatrix) != m_customVertexShader) {
        material->setFlag(QSGMaterial::RequiresFullMatrix, m_customVertexShader);
        node->markDirty(QSGNode::DirtyMaterial);
    }

    if (material->geometryUsesTextureSubRect != geometryUsesTextureSubRect) {
        material->geometryUsesTextureSubRect = geometryUsesTextureSubRect;
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
        QRectF rect(0, 0, m_item->width(), m_item->height());
        QQuickShaderEffectMesh *mesh = m_mesh ? m_mesh : &m_defaultMesh;

        int posIndex = 0;
        if (!mesh->validateAttributes(m_common.attributes, &posIndex)) {
            QString log = mesh->log();
            if (!log.isNull()) {
                m_log = parseLog();
                m_log += QLatin1String("*** Mesh ***\n");
                m_log += log;
                m_status = QQuickShaderEffect::Error;
                emit m_item->logChanged();
                emit m_item->statusChanged();
            }
            delete node;
            return 0;
        }

        geometry = mesh->updateGeometry(geometry, m_common.attributes.count(), posIndex, srcRect, rect);

        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry, true);

        m_dirtyGeometry = false;
    }

    return node;
}

void QQuickOpenGLShaderEffect::maybeUpdateShaders(bool force)
{
    if (!m_itemMetaObject)
        m_itemMetaObject = m_item->metaObject();

    // Defer processing if a window is not yet associated with the item. This
    // is because the actual scenegraph backend is not known so conditions
    // based on GraphicsInfo.shaderType and similar evaluate to wrong results.
    if (!m_item->window() && !force) {
        m_item->polish();
        return;
    }

    if (m_vertNeedsUpdate) {
        m_vertNeedsUpdate = false;
        m_common.updateShader(m_item, m_itemMetaObject, Key::VertexShader);
    }

    if (m_fragNeedsUpdate) {
        m_fragNeedsUpdate = false;
        m_common.updateShader(m_item, m_itemMetaObject, Key::FragmentShader);
    }
}

void QQuickOpenGLShaderEffect::handleItemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value)
{
    if (change == QQuickItem::ItemSceneChange)
        m_common.updateWindow(value.window);
}

QT_END_NAMESPACE
