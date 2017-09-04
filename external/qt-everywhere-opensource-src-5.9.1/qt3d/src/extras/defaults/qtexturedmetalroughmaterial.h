/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#ifndef QT3DEXTRAS_QTEXTUREDMETALROUGHMATERIAL_H
#define QT3DEXTRAS_QTEXTUREDMETALROUGHMATERIAL_H

#include <Qt3DExtras/qt3dextras_global.h>
#include <Qt3DRender/qmaterial.h>

QT_BEGIN_NAMESPACE

namespace Qt3DExtras {

class QTexturedMetalRoughMaterialPrivate;

class QT3DEXTRASSHARED_EXPORT QTexturedMetalRoughMaterial : public Qt3DRender::QMaterial
{
    Q_OBJECT
    Q_PROPERTY(Qt3DRender::QAbstractTexture *baseColor READ baseColor WRITE setBaseColor NOTIFY baseColorChanged)
    Q_PROPERTY(Qt3DRender::QAbstractTexture *metalness READ metalness WRITE setMetalness NOTIFY metalnessChanged)
    Q_PROPERTY(Qt3DRender::QAbstractTexture *roughness READ roughness WRITE setRoughness NOTIFY roughnessChanged)
    Q_PROPERTY(Qt3DRender::QAbstractTexture *ambientOcclusion READ ambientOcclusion WRITE setAmbientOcclusion NOTIFY ambientOcclusionChanged)
    Q_PROPERTY(Qt3DRender::QAbstractTexture *normal READ normal WRITE setNormal NOTIFY normalChanged)

public:
    explicit QTexturedMetalRoughMaterial(Qt3DCore::QNode *parent = nullptr);
    ~QTexturedMetalRoughMaterial();

    Qt3DRender::QAbstractTexture *baseColor() const;
    Qt3DRender::QAbstractTexture *metalness() const;
    Qt3DRender::QAbstractTexture *roughness() const;
    Qt3DRender::QAbstractTexture *ambientOcclusion() const;
    Qt3DRender::QAbstractTexture *normal() const;

public Q_SLOTS:
    void setBaseColor(Qt3DRender::QAbstractTexture *baseColor);
    void setMetalness(Qt3DRender::QAbstractTexture *metalness);
    void setRoughness(Qt3DRender::QAbstractTexture *roughness);
    void setAmbientOcclusion(Qt3DRender::QAbstractTexture *ambientOcclusion);
    void setNormal(Qt3DRender::QAbstractTexture *normal);

Q_SIGNALS:
    void baseColorChanged(Qt3DRender::QAbstractTexture *baseColor);
    void metalnessChanged(Qt3DRender::QAbstractTexture *metalness);
    void roughnessChanged(Qt3DRender::QAbstractTexture *roughness);
    void ambientOcclusionChanged(Qt3DRender::QAbstractTexture *ambientOcclusion);
    void normalChanged(Qt3DRender::QAbstractTexture *normal);

protected:
    explicit QTexturedMetalRoughMaterial(QTexturedMetalRoughMaterialPrivate &dd, Qt3DCore::QNode *parent = nullptr);

private:
    Q_DECLARE_PRIVATE(QTexturedMetalRoughMaterial)
};

} // namespace Qt3DExtras

QT_END_NAMESPACE

#endif // QT3DEXTRAS_QTEXTUREDMETALROUGHMATERIAL_H
