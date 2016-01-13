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

#include <private/qquickshadereffectnode_p.h>

#include "qquickshadereffect_p.h"
#include <QtQuick/qsgtextureprovider.h>
#include <QtQuick/private/qsgrenderer_p.h>
#include <QtQuick/private/qsgshadersourcebuilder_p.h>
#include <QtQuick/private/qsgtexture_p.h>

QT_BEGIN_NAMESPACE

class QQuickCustomMaterialShader : public QSGMaterialShader
{
public:
    QQuickCustomMaterialShader(const QQuickShaderEffectMaterialKey &key, const QVector<QByteArray> &attributes);
    virtual void deactivate();
    virtual void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect);
    virtual char const *const *attributeNames() const;

protected:
    friend class QQuickShaderEffectNode;

    virtual void compile();
    virtual const char *vertexShader() const;
    virtual const char *fragmentShader() const;

    const QQuickShaderEffectMaterialKey m_key;
    QVector<QByteArray> m_attributes;
    QVector<const char *> m_attributeNames;
    QString m_log;
    bool m_compiled;

    QVector<int> m_uniformLocs[QQuickShaderEffectMaterialKey::ShaderTypeCount];
    uint m_initialized : 1;
};

QQuickCustomMaterialShader::QQuickCustomMaterialShader(const QQuickShaderEffectMaterialKey &key, const QVector<QByteArray> &attributes)
    : m_key(key)
    , m_attributes(attributes)
    , m_compiled(false)
    , m_initialized(false)
{
    for (int i = 0; i < m_attributes.count(); ++i)
        m_attributeNames.append(m_attributes.at(i).constData());
    m_attributeNames.append(0);
}

void QQuickCustomMaterialShader::deactivate()
{
    QSGMaterialShader::deactivate();
    QOpenGLContext::currentContext()->functions()->glDisable(GL_CULL_FACE);
}

void QQuickCustomMaterialShader::updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    typedef QQuickShaderEffectMaterial::UniformData UniformData;

    Q_ASSERT(newEffect != 0);

    QQuickShaderEffectMaterial *material = static_cast<QQuickShaderEffectMaterial *>(newEffect);
    if (!material->m_emittedLogChanged && material->m_node) {
        material->m_emittedLogChanged = true;
        emit material->m_node->logAndStatusChanged(m_log, m_compiled ? QQuickShaderEffect::Compiled
                                                                     : QQuickShaderEffect::Error);
    }

    int textureProviderIndex = 0;
    if (!m_initialized) {
        for (int shaderType = 0; shaderType < QQuickShaderEffectMaterialKey::ShaderTypeCount; ++shaderType) {
            Q_ASSERT(m_uniformLocs[shaderType].isEmpty());
            m_uniformLocs[shaderType].reserve(material->uniforms[shaderType].size());
            for (int i = 0; i < material->uniforms[shaderType].size(); ++i) {
                const UniformData &d = material->uniforms[shaderType].at(i);
                QByteArray name = d.name;
                if (d.specialType == UniformData::Sampler) {
                    program()->setUniformValue(d.name.constData(), textureProviderIndex++);
                    // We don't need to store the sampler uniform locations, since their values
                    // only need to be set once. Look for the "qt_SubRect_" uniforms instead.
                    // These locations are used when binding the textures later.
                    name = "qt_SubRect_" + name;
                }
                m_uniformLocs[shaderType].append(program()->uniformLocation(name.constData()));
            }
        }
        m_initialized = true;
        textureProviderIndex = 0;
    }

    QOpenGLFunctions *functions = state.context()->functions();
    for (int shaderType = 0; shaderType < QQuickShaderEffectMaterialKey::ShaderTypeCount; ++shaderType) {
        for (int i = 0; i < material->uniforms[shaderType].size(); ++i) {
            const UniformData &d = material->uniforms[shaderType].at(i);
            int loc = m_uniformLocs[shaderType].at(i);
            if (d.specialType == UniformData::Sampler) {
                int idx = textureProviderIndex++;
                functions->glActiveTexture(GL_TEXTURE0 + idx);
                if (QSGTextureProvider *provider = material->textureProviders.at(idx)) {
                    if (QSGTexture *texture = provider->texture()) {

#ifndef QT_NO_DEBUG
                        if (!qsg_safeguard_texture(texture))
                            continue;
#endif

                        if (loc >= 0) {
                            QRectF r = texture->normalizedTextureSubRect();
                            program()->setUniformValue(loc, r.x(), r.y(), r.width(), r.height());
                        } else if (texture->isAtlasTexture() && (idx != 0 || !material->supportsAtlasTextures)) {
                            texture = texture->removedFromAtlas();
                        }
                        texture->bind();
                        continue;
                    }
                }
                functions->glBindTexture(GL_TEXTURE_2D, 0);
            } else if (d.specialType == UniformData::Opacity) {
                program()->setUniformValue(loc, state.opacity());
            } else if (d.specialType == UniformData::Matrix) {
                if (state.isMatrixDirty())
                    program()->setUniformValue(loc, state.combinedMatrix());
            } else if (d.specialType == UniformData::None) {
                switch (int(d.value.type())) {
                case QMetaType::QColor:
                    program()->setUniformValue(loc, qt_premultiply_color(qvariant_cast<QColor>(d.value)));
                    break;
                case QMetaType::Float:
                    program()->setUniformValue(loc, qvariant_cast<float>(d.value));
                    break;
                case QMetaType::Double:
                    program()->setUniformValue(loc, (float) qvariant_cast<double>(d.value));
                    break;
                case QMetaType::QTransform:
                    program()->setUniformValue(loc, qvariant_cast<QTransform>(d.value));
                    break;
                case QMetaType::Int:
                    program()->setUniformValue(loc, d.value.toInt());
                    break;
                case QMetaType::Bool:
                    program()->setUniformValue(loc, GLint(d.value.toBool()));
                    break;
                case QMetaType::QSize:
                case QMetaType::QSizeF:
                    program()->setUniformValue(loc, d.value.toSizeF());
                    break;
                case QMetaType::QPoint:
                case QMetaType::QPointF:
                    program()->setUniformValue(loc, d.value.toPointF());
                    break;
                case QMetaType::QRect:
                case QMetaType::QRectF:
                    {
                        QRectF r = d.value.toRectF();
                        program()->setUniformValue(loc, r.x(), r.y(), r.width(), r.height());
                    }
                    break;
                case QMetaType::QVector2D:
                    program()->setUniformValue(loc, qvariant_cast<QVector2D>(d.value));
                    break;
                case QMetaType::QVector3D:
                    program()->setUniformValue(loc, qvariant_cast<QVector3D>(d.value));
                    break;
                case QMetaType::QVector4D:
                    program()->setUniformValue(loc, qvariant_cast<QVector4D>(d.value));
                    break;
                case QMetaType::QQuaternion:
                    {
                        QQuaternion q = qvariant_cast<QQuaternion>(d.value);
                        program()->setUniformValue(loc, q.x(), q.y(), q.z(), q.scalar());
                    }
                    break;
                case QMetaType::QMatrix4x4:
                    program()->setUniformValue(loc, qvariant_cast<QMatrix4x4>(d.value));
                    break;
                default:
                    break;
                }
            }
        }
    }
    functions->glActiveTexture(GL_TEXTURE0);

    const QQuickShaderEffectMaterial *oldMaterial = static_cast<const QQuickShaderEffectMaterial *>(oldEffect);
    if (oldEffect == 0 || material->cullMode != oldMaterial->cullMode) {
        switch (material->cullMode) {
        case QQuickShaderEffectMaterial::FrontFaceCulling:
            functions->glEnable(GL_CULL_FACE);
            functions->glCullFace(GL_FRONT);
            break;
        case QQuickShaderEffectMaterial::BackFaceCulling:
            functions->glEnable(GL_CULL_FACE);
            functions->glCullFace(GL_BACK);
            break;
        default:
            functions->glDisable(GL_CULL_FACE);
            break;
        }
    }
}

char const *const *QQuickCustomMaterialShader::attributeNames() const
{
    return m_attributeNames.constData();
}

void QQuickCustomMaterialShader::compile()
{
    Q_ASSERT_X(!program()->isLinked(), "QQuickCustomMaterialShader::compile()", "Compile called multiple times!");

    m_log.clear();
    m_compiled = true;
    if (!program()->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader())) {
        m_log += QLatin1String("*** Vertex shader ***\n");
        m_log += program()->log();
        m_compiled = false;
    }
    if (!program()->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader())) {
        m_log += QLatin1String("*** Fragment shader ***\n");
        m_log += program()->log();
        m_compiled = false;
    }

    char const *const *attr = attributeNames();
#ifndef QT_NO_DEBUG
    int maxVertexAttribs = 0;
    QOpenGLContext::currentContext()->functions()->glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
    int attrCount = 0;
    while (attrCount < maxVertexAttribs && attr[attrCount])
        ++attrCount;
    if (attr[attrCount]) {
        qWarning("List of attribute names is too long.\n"
                 "Maximum number of attributes on this hardware is %i.\n"
                 "Vertex shader:\n%s\n"
                 "Fragment shader:\n%s\n",
                 maxVertexAttribs, vertexShader(), fragmentShader());
    }
#endif

    if (m_compiled) {
#ifndef QT_NO_DEBUG
        for (int i = 0; i < attrCount; ++i) {
#else
        for (int i = 0; attr[i]; ++i) {
#endif
            if (*attr[i])
                program()->bindAttributeLocation(attr[i], i);
        }
        m_compiled = program()->link();
        m_log += program()->log();
    }

    if (!m_compiled) {
        qWarning("QQuickCustomMaterialShader: Shader compilation failed:");
        qWarning() << program()->log();

        QSGShaderSourceBuilder::initializeProgramFromFiles(
            program(),
            QStringLiteral(":/items/shaders/shadereffectfallback.vert"),
            QStringLiteral(":/items/shaders/shadereffectfallback.frag"));

#ifndef QT_NO_DEBUG
        for (int i = 0; i < attrCount; ++i) {
#else
        for (int i = 0; attr[i]; ++i) {
#endif
            if (qstrcmp(attr[i], qtPositionAttributeName()) == 0)
                program()->bindAttributeLocation("v", i);
        }
        program()->link();
    }
}

const char *QQuickCustomMaterialShader::vertexShader() const
{
    return m_key.sourceCode[QQuickShaderEffectMaterialKey::VertexShader].constData();
}

const char *QQuickCustomMaterialShader::fragmentShader() const
{
    return m_key.sourceCode[QQuickShaderEffectMaterialKey::FragmentShader].constData();
}


bool QQuickShaderEffectMaterialKey::operator == (const QQuickShaderEffectMaterialKey &other) const
{
    if (className != other.className)
        return false;
    for (int shaderType = 0; shaderType < ShaderTypeCount; ++shaderType) {
        if (sourceCode[shaderType] != other.sourceCode[shaderType])
            return false;
    }
    return true;
}

bool QQuickShaderEffectMaterialKey::operator != (const QQuickShaderEffectMaterialKey &other) const
{
    return !(*this == other);
}

uint qHash(const QQuickShaderEffectMaterialKey &key)
{
    uint hash = qHash((void *)key.className);
    typedef QQuickShaderEffectMaterialKey Key;
    for (int shaderType = 0; shaderType < Key::ShaderTypeCount; ++shaderType)
        hash = hash * 31337 + qHash(key.sourceCode[shaderType]);
    return hash;
}


QHash<QQuickShaderEffectMaterialKey, QSharedPointer<QSGMaterialType> > QQuickShaderEffectMaterial::materialMap;

QQuickShaderEffectMaterial::QQuickShaderEffectMaterial(QQuickShaderEffectNode *node)
    : cullMode(NoCulling)
    , supportsAtlasTextures(false)
    , m_node(node)
    , m_emittedLogChanged(false)
{
    setFlag(Blending | RequiresFullMatrix, true);
}

QSGMaterialType *QQuickShaderEffectMaterial::type() const
{
    return m_type.data();
}

QSGMaterialShader *QQuickShaderEffectMaterial::createShader() const
{
    return new QQuickCustomMaterialShader(m_source, attributes);
}

bool QQuickShaderEffectMaterial::UniformData::operator == (const UniformData &other) const
{
    if (specialType != other.specialType)
        return false;
    if (name != other.name)
        return false;

    if (specialType == UniformData::Sampler) {
        QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(value));
        QQuickItem *otherSource = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(other.value));
        if (!source || !otherSource || !source->isTextureProvider() || !otherSource->isTextureProvider())
            return false;
        return source->textureProvider()->texture()->textureId() == otherSource->textureProvider()->texture()->textureId();
    } else {
        return value == other.value;
    }
}

int QQuickShaderEffectMaterial::compare(const QSGMaterial *o) const
{
    const QQuickShaderEffectMaterial *other = static_cast<const QQuickShaderEffectMaterial *>(o);
    if (!supportsAtlasTextures || !other->supportsAtlasTextures)
        return 1;
    if (bool(flags() & QSGMaterial::RequiresFullMatrix) || bool(other->flags() & QSGMaterial::RequiresFullMatrix))
        return 1;
    if (cullMode != other->cullMode)
        return 1;
    if (m_source != other->m_source)
        return 1;
    for (int shaderType = 0; shaderType < QQuickShaderEffectMaterialKey::ShaderTypeCount; ++shaderType) {
        if (uniforms[shaderType] != other->uniforms[shaderType])
            return 1;
    }
    return 0;
}

void QQuickShaderEffectMaterial::setProgramSource(const QQuickShaderEffectMaterialKey &source)
{
    m_source = source;
    m_emittedLogChanged = false;
    m_type = materialMap.value(m_source);
    if (m_type.isNull()) {
        m_type = QSharedPointer<QSGMaterialType>(new QSGMaterialType);
        materialMap.insert(m_source, m_type);
    }
}

void QQuickShaderEffectMaterial::updateTextures() const
{
    for (int i = 0; i < textureProviders.size(); ++i) {
        if (QSGTextureProvider *provider = textureProviders.at(i)) {
            if (QSGLayer *texture = qobject_cast<QSGLayer *>(provider->texture()))
                texture->updateTexture();
        }
    }
}

void QQuickShaderEffectMaterial::invalidateTextureProvider(QSGTextureProvider *provider)
{
    for (int i = 0; i < textureProviders.size(); ++i) {
        if (provider == textureProviders.at(i))
            textureProviders[i] = 0;
    }
}


QQuickShaderEffectNode::QQuickShaderEffectNode()
{
    QSGNode::setFlag(UsePreprocess, true);

#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QLatin1String("shadereffect"));
#endif
}

QQuickShaderEffectNode::~QQuickShaderEffectNode()
{
}

void QQuickShaderEffectNode::markDirtyTexture()
{
    markDirty(DirtyMaterial);
}

void QQuickShaderEffectNode::textureProviderDestroyed(QObject *object)
{
    Q_ASSERT(material());
    static_cast<QQuickShaderEffectMaterial *>(material())->invalidateTextureProvider(static_cast<QSGTextureProvider *>(object));
}

void QQuickShaderEffectNode::preprocess()
{
    Q_ASSERT(material());
    static_cast<QQuickShaderEffectMaterial *>(material())->updateTextures();
}

QT_END_NAMESPACE
