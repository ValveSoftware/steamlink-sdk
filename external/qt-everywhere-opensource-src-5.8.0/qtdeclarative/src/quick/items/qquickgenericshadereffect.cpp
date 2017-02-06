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

#include <private/qquickgenericshadereffect_p.h>
#include <private/qquickwindow_p.h>
#include <private/qquickitem_p.h>
#include <QSignalMapper>

QT_BEGIN_NAMESPACE

// The generic shader effect is used when the scenegraph backend indicates
// SupportsShaderEffectNode. This, unlike the monolithic and interconnected (e.g.
// with particles) OpenGL variant, passes most of the work to a scenegraph node
// created via the adaptation layer, thus allowing different implementation in
// the backends.

QQuickGenericShaderEffect::QQuickGenericShaderEffect(QQuickShaderEffect *item, QObject *parent)
    : QObject(parent)
    , m_item(item)
    , m_meshResolution(1, 1)
    , m_mesh(nullptr)
    , m_cullMode(QQuickShaderEffect::NoCulling)
    , m_blending(true)
    , m_supportsAtlasTextures(false)
    , m_mgr(nullptr)
    , m_fragNeedsUpdate(true)
    , m_vertNeedsUpdate(true)
    , m_dirty(0)
{
    qRegisterMetaType<QSGGuiThreadShaderEffectManager::ShaderInfo::Type>("ShaderInfo::Type");
    for (int i = 0; i < NShader; ++i)
        m_inProgress[i] = nullptr;
}

QQuickGenericShaderEffect::~QQuickGenericShaderEffect()
{
    for (int i = 0; i < NShader; ++i) {
        disconnectSignals(Shader(i));
        for (const auto &sm : qAsConst(m_signalMappers[i]))
            delete sm.mapper;
    }

    delete m_mgr;
}

void QQuickGenericShaderEffect::setFragmentShader(const QByteArray &src)
{
    // Compare the actual values since they are often just filenames.
    // Optimizing by comparing constData() is a bad idea since seemingly static
    // strings in QML may in fact have different addresses when a binding
    // triggers assigning the "same" value to the property.
    if (m_fragShader == src)
        return;

    m_fragShader = src;

    m_fragNeedsUpdate = true;
    if (m_item->isComponentComplete())
        maybeUpdateShaders();

    emit m_item->fragmentShaderChanged();
}

void QQuickGenericShaderEffect::setVertexShader(const QByteArray &src)
{
    if (m_vertShader == src)
        return;

    m_vertShader = src;

    m_vertNeedsUpdate = true;
    if (m_item->isComponentComplete())
        maybeUpdateShaders();

    emit m_item->vertexShaderChanged();
}

void QQuickGenericShaderEffect::setBlending(bool enable)
{
    if (m_blending == enable)
        return;

    m_blending = enable;
    m_item->update();
    emit m_item->blendingChanged();
}

QVariant QQuickGenericShaderEffect::mesh() const
{
    return m_mesh ? qVariantFromValue(static_cast<QObject *>(m_mesh))
                  : qVariantFromValue(m_meshResolution);
}

void QQuickGenericShaderEffect::setMesh(const QVariant &mesh)
{
    QQuickShaderEffectMesh *newMesh = qobject_cast<QQuickShaderEffectMesh *>(qvariant_cast<QObject *>(mesh));
    if (newMesh && newMesh == m_mesh)
        return;

    if (m_mesh)
        disconnect(m_mesh, SIGNAL(geometryChanged()), this, 0);

    m_mesh = newMesh;

    if (m_mesh) {
        connect(m_mesh, SIGNAL(geometryChanged()), this, SLOT(markGeometryDirtyAndUpdate()));
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
                qWarning("ShaderEffect: mesh property must be a size or an object deriving from QQuickShaderEffectMesh");
        }
        m_defaultMesh.setResolution(m_meshResolution);
    }

    m_dirty |= QSGShaderEffectNode::DirtyShaderMesh;
    m_item->update();

    emit m_item->meshChanged();
}

void QQuickGenericShaderEffect::setCullMode(QQuickShaderEffect::CullMode face)
{
    if (m_cullMode == face)
        return;

    m_cullMode = face;
    m_item->update();
    emit m_item->cullModeChanged();
}

void QQuickGenericShaderEffect::setSupportsAtlasTextures(bool supports)
{
    if (m_supportsAtlasTextures == supports)
        return;

    m_supportsAtlasTextures = supports;
    markGeometryDirtyAndUpdate();
    emit m_item->supportsAtlasTexturesChanged();
}

QString QQuickGenericShaderEffect::parseLog()
{
    maybeUpdateShaders();
    return log();
}

QString QQuickGenericShaderEffect::log() const
{
    QSGGuiThreadShaderEffectManager *mgr = shaderEffectManager();
    if (!mgr)
        return QString();

    return mgr->log();
}

QQuickShaderEffect::Status QQuickGenericShaderEffect::status() const
{
    QSGGuiThreadShaderEffectManager *mgr = shaderEffectManager();
    if (!mgr)
        return QQuickShaderEffect::Uncompiled;

    return QQuickShaderEffect::Status(mgr->status());
}

void QQuickGenericShaderEffect::handleEvent(QEvent *event)
{
    if (event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *e = static_cast<QDynamicPropertyChangeEvent *>(event);
        for (int shaderType = 0; shaderType < NShader; ++shaderType) {
            const auto &vars(m_shaders[shaderType].shaderInfo.variables);
            for (int idx = 0; idx < vars.count(); ++idx) {
                if (vars[idx].name == e->propertyName()) {
                    propertyChanged((shaderType << 16) | idx);
                    break;
                }
            }
        }
    }
}

void QQuickGenericShaderEffect::handleGeometryChanged(const QRectF &, const QRectF &)
{
    m_dirty |= QSGShaderEffectNode::DirtyShaderGeometry;
}

QSGNode *QQuickGenericShaderEffect::handleUpdatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *)
{
    QSGShaderEffectNode *node = static_cast<QSGShaderEffectNode *>(oldNode);

    if (m_item->width() <= 0 || m_item->height() <= 0) {
        delete node;
        return nullptr;
    }

    // Do not change anything while a new shader is being reflected or compiled.
    if (m_inProgress[Vertex] || m_inProgress[Fragment])
        return node;

    // The manager should be already created on the gui thread. Just take that instance.
    QSGGuiThreadShaderEffectManager *mgr = shaderEffectManager();
    if (!mgr) {
        delete node;
        return nullptr;
    }

    if (!node) {
        QSGRenderContext *rc = QQuickWindowPrivate::get(m_item->window())->context;
        node = rc->sceneGraphContext()->createShaderEffectNode(rc, mgr);
        m_dirty = QSGShaderEffectNode::DirtyShaderAll;
    }

    QSGShaderEffectNode::SyncData sd;
    sd.dirty = m_dirty;
    sd.cullMode = QSGShaderEffectNode::CullMode(m_cullMode);
    sd.blending = m_blending;
    sd.vertex.shader = &m_shaders[Vertex];
    sd.vertex.dirtyConstants = &m_dirtyConstants[Vertex];
    sd.vertex.dirtyTextures = &m_dirtyTextures[Vertex];
    sd.fragment.shader = &m_shaders[Fragment];
    sd.fragment.dirtyConstants = &m_dirtyConstants[Fragment];
    sd.fragment.dirtyTextures = &m_dirtyTextures[Fragment];
    node->syncMaterial(&sd);

    if (m_dirty & QSGShaderEffectNode::DirtyShaderMesh) {
        node->setGeometry(nullptr);
        m_dirty &= ~QSGShaderEffectNode::DirtyShaderMesh;
        m_dirty |= QSGShaderEffectNode::DirtyShaderGeometry;
    }

    if (m_dirty & QSGShaderEffectNode::DirtyShaderGeometry) {
        const QRectF rect(0, 0, m_item->width(), m_item->height());
        QQuickShaderEffectMesh *mesh = m_mesh ? m_mesh : &m_defaultMesh;
        QSGGeometry *geometry = node->geometry();

        const QRectF srcRect = node->updateNormalizedTextureSubRect(m_supportsAtlasTextures);
        geometry = mesh->updateGeometry(geometry, 2, 0, srcRect, rect);

        node->setFlag(QSGNode::OwnsGeometry, false);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry, true);

        m_dirty &= ~QSGShaderEffectNode::DirtyShaderGeometry;
    }

    m_dirty = 0;
    for (int i = 0; i < NShader; ++i) {
        m_dirtyConstants[i].clear();
        m_dirtyTextures[i].clear();
    }

    return node;
}

void QQuickGenericShaderEffect::maybeUpdateShaders()
{
    if (m_vertNeedsUpdate)
        m_vertNeedsUpdate = !updateShader(Vertex, m_vertShader);
    if (m_fragNeedsUpdate)
        m_fragNeedsUpdate = !updateShader(Fragment, m_fragShader);
    if (m_vertNeedsUpdate || m_fragNeedsUpdate) {
        // This function is invoked either from componentComplete or in a
        // response to a previous invocation's polish() request. If this is
        // case #1 then updateShader can fail due to not having a window or
        // scenegraph ready. Schedule the polish to try again later. In case #2
        // the backend probably does not have shadereffect support so there is
        // nothing to do for us here.
        if (!m_item->window() || !m_item->window()->isSceneGraphInitialized())
            m_item->polish();
    }
}

void QQuickGenericShaderEffect::handleItemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value)
{
    // Move the window ref.
    if (change == QQuickItem::ItemSceneChange) {
        for (int shaderType = 0; shaderType < NShader; ++shaderType) {
            for (const auto &vd : qAsConst(m_shaders[shaderType].varData)) {
                if (vd.specialType == QSGShaderEffectNode::VariableData::Source) {
                    QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(vd.value));
                    if (source) {
                        if (value.window)
                            QQuickItemPrivate::get(source)->refWindow(value.window);
                        else
                            QQuickItemPrivate::get(source)->derefWindow();
                    }
                }
            }
        }
    }
}

QSGGuiThreadShaderEffectManager *QQuickGenericShaderEffect::shaderEffectManager() const
{
    if (!m_mgr) {
        // return null if this is not the gui thread and not already created
        if (QThread::currentThread() != m_item->thread())
            return m_mgr;
        QQuickWindow *w = m_item->window();
        if (w) { // note: just the window, don't care about isSceneGraphInitialized() here
            m_mgr = QQuickWindowPrivate::get(w)->context->sceneGraphContext()->createGuiThreadShaderEffectManager();
            if (m_mgr) {
                connect(m_mgr, SIGNAL(logAndStatusChanged()), m_item, SIGNAL(logChanged()));
                connect(m_mgr, SIGNAL(logAndStatusChanged()), m_item, SIGNAL(statusChanged()));
                connect(m_mgr, SIGNAL(textureChanged()), this, SLOT(markGeometryDirtyAndUpdateIfSupportsAtlas()));
                connect(m_mgr, &QSGGuiThreadShaderEffectManager::shaderCodePrepared, this, &QQuickGenericShaderEffect::shaderCodePrepared);
            }
        }
    }

    return m_mgr;
}

void QQuickGenericShaderEffect::disconnectSignals(Shader shaderType)
{
    for (auto &sm : m_signalMappers[shaderType]) {
        if (sm.active) {
            sm.active = false;
            QObject::disconnect(m_item, nullptr, sm.mapper, SLOT(map()));
            QObject::disconnect(sm.mapper, SIGNAL(mapped(int)), this, SLOT(propertyChanged(int)));
        }
    }
    for (const auto &vd : qAsConst(m_shaders[shaderType].varData)) {
        if (vd.specialType == QSGShaderEffectNode::VariableData::Source) {
            QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(vd.value));
            if (source) {
                if (m_item->window())
                    QQuickItemPrivate::get(source)->derefWindow();
                QObject::disconnect(source, SIGNAL(destroyed(QObject*)), this, SLOT(sourceDestroyed(QObject*)));
            }
        }
    }
}

struct ShaderInfoCache
{
    bool contains(const QByteArray &key) const
    {
        return m_shaderInfoCache.contains(key);
    }

    QSGGuiThreadShaderEffectManager::ShaderInfo value(const QByteArray &key) const
    {
        return m_shaderInfoCache.value(key);
    }

    void insert(const QByteArray &key, const QSGGuiThreadShaderEffectManager::ShaderInfo &value)
    {
        m_shaderInfoCache.insert(key, value);
    }

    QHash<QByteArray, QSGGuiThreadShaderEffectManager::ShaderInfo> m_shaderInfoCache;
};

Q_GLOBAL_STATIC(ShaderInfoCache, shaderInfoCache)

bool QQuickGenericShaderEffect::updateShader(Shader shaderType, const QByteArray &src)
{
    QSGGuiThreadShaderEffectManager *mgr = shaderEffectManager();
    if (!mgr)
        return false;

    const bool texturesSeparate = mgr->hasSeparateSamplerAndTextureObjects();

    disconnectSignals(shaderType);

    m_shaders[shaderType].shaderInfo = QSGGuiThreadShaderEffectManager::ShaderInfo();
    m_shaders[shaderType].varData.clear();

    if (!src.isEmpty()) {
        if (shaderInfoCache()->contains(src)) {
            m_shaders[shaderType].shaderInfo = shaderInfoCache()->value(src);
            m_shaders[shaderType].hasShaderCode = true;
        } else {
            // Each prepareShaderCode call needs its own work area, hence the
            // dynamic alloc. If there are calls in progress, let those run to
            // finish, their results can then simply be ignored because
            // m_inProgress indicates what we care about.
            m_inProgress[shaderType] = new QSGGuiThreadShaderEffectManager::ShaderInfo;
            const QSGGuiThreadShaderEffectManager::ShaderInfo::Type typeHint =
                    shaderType == Vertex ? QSGGuiThreadShaderEffectManager::ShaderInfo::TypeVertex
                                         : QSGGuiThreadShaderEffectManager::ShaderInfo::TypeFragment;
            // Figure out what input parameters and variables are used in the
            // shader. For file-based shader source/bytecode this is where the data
            // is pulled in from the file. Some backends may choose to do
            // source->bytecode compilation as well in this step.
            mgr->prepareShaderCode(typeHint, src, m_inProgress[shaderType]);
            // the rest is handled in shaderCodePrepared()
            return true;
        }
    } else {
        m_shaders[shaderType].hasShaderCode = false;
        if (shaderType == Fragment) {
            // With built-in shaders hasShaderCode is set to false and all
            // metadata is empty, as it is left up to the node to provide a
            // built-in default shader and its metadata. However, in case of
            // the built-in fragment shader the value for 'source' has to be
            // provided and monitored like with an application-provided shader.
            QSGGuiThreadShaderEffectManager::ShaderInfo::Variable v;
            v.name = QByteArrayLiteral("source");
            v.bindPoint = 0;
            v.type = texturesSeparate ? QSGGuiThreadShaderEffectManager::ShaderInfo::Texture
                                      : QSGGuiThreadShaderEffectManager::ShaderInfo::Sampler;
            m_shaders[shaderType].shaderInfo.variables.append(v);
        }
    }

    updateShaderVars(shaderType);
    m_dirty |= QSGShaderEffectNode::DirtyShaders;
    m_item->update();
    return true;
}

void QQuickGenericShaderEffect::shaderCodePrepared(bool ok, QSGGuiThreadShaderEffectManager::ShaderInfo::Type typeHint,
                                                   const QByteArray &src, QSGGuiThreadShaderEffectManager::ShaderInfo *result)
{
    const Shader shaderType = typeHint == QSGGuiThreadShaderEffectManager::ShaderInfo::TypeVertex ? Vertex : Fragment;

    // If another call was made to updateShader() for the same shader type in
    // the meantime then our results are useless, just drop them.
    if (result != m_inProgress[shaderType]) {
        delete result;
        return;
    }

    m_shaders[shaderType].shaderInfo = *result;
    delete result;
    m_inProgress[shaderType] = nullptr;

    if (!ok) {
        qWarning("ShaderEffect: shader preparation failed for %s\n%s\n", src.constData(), qPrintable(log()));
        m_shaders[shaderType].hasShaderCode = false;
        return;
    }

    m_shaders[shaderType].hasShaderCode = true;
    shaderInfoCache()->insert(src, m_shaders[shaderType].shaderInfo);
    updateShaderVars(shaderType);
    m_dirty |= QSGShaderEffectNode::DirtyShaders;
    m_item->update();
}

void QQuickGenericShaderEffect::updateShaderVars(Shader shaderType)
{
    QSGGuiThreadShaderEffectManager *mgr = shaderEffectManager();
    if (!mgr)
        return;

    const bool texturesSeparate = mgr->hasSeparateSamplerAndTextureObjects();

    const int varCount = m_shaders[shaderType].shaderInfo.variables.count();
    m_shaders[shaderType].varData.resize(varCount);

    // Reuse signal mappers as much as possible since the mapping is based on
    // the index and shader type which are both constant.
    if (m_signalMappers[shaderType].count() < varCount)
        m_signalMappers[shaderType].resize(varCount);

    // Hook up the signals to get notified about changes for properties that
    // correspond to variables in the shader. Store also the values.
    for (int i = 0; i < varCount; ++i) {
        const auto &v(m_shaders[shaderType].shaderInfo.variables.at(i));
        QSGShaderEffectNode::VariableData &vd(m_shaders[shaderType].varData[i]);
        const bool isSpecial = v.name.startsWith("qt_"); // special names not mapped to properties
        if (isSpecial) {
            if (v.name == QByteArrayLiteral("qt_Opacity"))
                vd.specialType = QSGShaderEffectNode::VariableData::Opacity;
            else if (v.name == QByteArrayLiteral("qt_Matrix"))
                vd.specialType = QSGShaderEffectNode::VariableData::Matrix;
            else if (v.name.startsWith("qt_SubRect_"))
                vd.specialType = QSGShaderEffectNode::VariableData::SubRect;
            continue;
        }

        // The value of a property corresponding to a sampler is the source
        // item ref, unless there are separate texture objects in which case
        // the sampler is ignored (here).
        if (v.type == QSGGuiThreadShaderEffectManager::ShaderInfo::Sampler) {
            if (texturesSeparate) {
                vd.specialType = QSGShaderEffectNode::VariableData::Unused;
                continue;
            } else {
                vd.specialType = QSGShaderEffectNode::VariableData::Source;
            }
        } else if (v.type == QSGGuiThreadShaderEffectManager::ShaderInfo::Texture) {
            Q_ASSERT(texturesSeparate);
            vd.specialType = QSGShaderEffectNode::VariableData::Source;
        } else {
            vd.specialType = QSGShaderEffectNode::VariableData::None;
        }

        // Find the property on the ShaderEffect item.
        const int propIdx = m_item->metaObject()->indexOfProperty(v.name.constData());
        if (propIdx >= 0) {
            QMetaProperty mp = m_item->metaObject()->property(propIdx);
            if (!mp.hasNotifySignal())
                qWarning("ShaderEffect: property '%s' does not have notification method", v.name.constData());

            // Have a QSignalMapper that emits mapped() with an index+type on each property change notify signal.
            auto &sm(m_signalMappers[shaderType][i]);
            if (!sm.mapper) {
                sm.mapper = new QSignalMapper;
                sm.mapper->setMapping(m_item, i | (shaderType << 16));
            }
            sm.active = true;
            const QByteArray signalName = '2' + mp.notifySignal().methodSignature();
            QObject::connect(m_item, signalName, sm.mapper, SLOT(map()));
            QObject::connect(sm.mapper, SIGNAL(mapped(int)), this, SLOT(propertyChanged(int)));
        } else {
            // Do not warn for dynamic properties.
            if (!m_item->property(v.name.constData()).isValid())
                qWarning("ShaderEffect: '%s' does not have a matching property!", v.name.constData());
        }

        vd.value = m_item->property(v.name.constData());

        if (vd.specialType == QSGShaderEffectNode::VariableData::Source) {
            QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(vd.value));
            if (source) {
                if (m_item->window())
                    QQuickItemPrivate::get(source)->refWindow(m_item->window());
                QObject::connect(source, SIGNAL(destroyed(QObject*)), this, SLOT(sourceDestroyed(QObject*)));
            }
        }
    }
}

bool QQuickGenericShaderEffect::sourceIsUnique(QQuickItem *source, Shader typeToSkip, int indexToSkip) const
{
    for (int shaderType = 0; shaderType < NShader; ++shaderType) {
        for (int idx = 0; idx < m_shaders[shaderType].varData.count(); ++idx) {
            if (shaderType != typeToSkip || idx != indexToSkip) {
                const auto &vd(m_shaders[shaderType].varData[idx]);
                if (vd.specialType == QSGShaderEffectNode::VariableData::Source && qvariant_cast<QObject *>(vd.value) == source)
                    return false;
            }
        }
    }
    return true;
}

void QQuickGenericShaderEffect::propertyChanged(int mappedId)
{
    const Shader type = Shader(mappedId >> 16);
    const int idx = mappedId & 0xFFFF;
    const auto &v(m_shaders[type].shaderInfo.variables[idx]);
    auto &vd(m_shaders[type].varData[idx]);

    if (vd.specialType == QSGShaderEffectNode::VariableData::Source) {
        QQuickItem *source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(vd.value));
        if (source) {
            if (m_item->window())
                QQuickItemPrivate::get(source)->derefWindow();
            // QObject::disconnect() will disconnect all matching connections.
            // If the same source has been attached to two separate
            // textures/samplers, then changing one of them would trigger both
            // to be disconnected. So check first.
            if (sourceIsUnique(source, type, idx))
                QObject::disconnect(source, SIGNAL(destroyed(QObject*)), this, SLOT(sourceDestroyed(QObject*)));
        }

        vd.value = m_item->property(v.name.constData());

        source = qobject_cast<QQuickItem *>(qvariant_cast<QObject *>(vd.value));
        if (source) {
            // 'source' needs a window to get a scene graph node. It usually gets one through its
            // parent, but if the source item is "inline" rather than a reference -- i.e.
            // "property variant source: Image { }" instead of "property variant source: foo" -- it
            // will not get a parent. In those cases, 'source' should get the window from 'item'.
            if (m_item->window())
                QQuickItemPrivate::get(source)->refWindow(m_item->window());
            QObject::connect(source, SIGNAL(destroyed(QObject*)), this, SLOT(sourceDestroyed(QObject*)));
        }

        m_dirty |= QSGShaderEffectNode::DirtyShaderTexture;
        m_dirtyTextures[type].insert(idx);

     } else {
        vd.value = m_item->property(v.name.constData());
        m_dirty |= QSGShaderEffectNode::DirtyShaderConstant;
        m_dirtyConstants[type].insert(idx);
    }

    m_item->update();
}

void QQuickGenericShaderEffect::sourceDestroyed(QObject *object)
{
    for (int shaderType = 0; shaderType < NShader; ++shaderType) {
        for (auto &vd : m_shaders[shaderType].varData) {
            if (vd.specialType == QSGShaderEffectNode::VariableData::Source && vd.value.canConvert<QObject *>()) {
                if (qvariant_cast<QObject *>(vd.value) == object)
                    vd.value = QVariant();
            }
        }
    }
}

void QQuickGenericShaderEffect::markGeometryDirtyAndUpdate()
{
    m_dirty |= QSGShaderEffectNode::DirtyShaderGeometry;
    m_item->update();
}

void QQuickGenericShaderEffect::markGeometryDirtyAndUpdateIfSupportsAtlas()
{
    if (m_supportsAtlasTextures)
        markGeometryDirtyAndUpdate();
}

QT_END_NAMESPACE
