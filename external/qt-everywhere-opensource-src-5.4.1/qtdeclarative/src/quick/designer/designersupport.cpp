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

#include "designersupport.h"
#include <private/qquickitem_p.h>

#include <QtQuick/private/qquickshadereffectsource_p.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQml/private/qabstractanimationjob_p.h>
#include <private/qqmlengine_p.h>
#include <private/qquickview_p.h>
#include <private/qsgrenderloop_p.h>
#include <QtQuick/private/qquickstategroup_p.h>
#include <QtGui/QImage>
#include <private/qqmlvme_p.h>
#include <private/qqmlcomponentattached_p.h>
#include <private/qqmldata_p.h>

#include "designerwindowmanager_p.h"


QT_BEGIN_NAMESPACE

DesignerSupport::DesignerSupport()
{
}

DesignerSupport::~DesignerSupport()
{
    QHash<QQuickItem*, QSGLayer*>::iterator iterator;

    for (iterator = m_itemTextureHash.begin(); iterator != m_itemTextureHash.end(); ++iterator) {
        QSGLayer *texture = iterator.value();
        QQuickItem *item = iterator.key();
        QQuickItemPrivate::get(item)->derefFromEffectItem(true);
        delete texture;
    }
}

void DesignerSupport::refFromEffectItem(QQuickItem *referencedItem, bool hide)
{
    if (referencedItem == 0)
        return;

    QQuickItemPrivate::get(referencedItem)->refFromEffectItem(hide);
    QQuickWindowPrivate::get(referencedItem->window())->updateDirtyNode(referencedItem);

    Q_ASSERT(QQuickItemPrivate::get(referencedItem)->rootNode());

    if (!m_itemTextureHash.contains(referencedItem)) {
        QSGRenderContext *rc = QQuickWindowPrivate::get(referencedItem->window())->context;
        QSGLayer *texture = rc->sceneGraphContext()->createLayer(rc);

        texture->setLive(true);
        texture->setItem(QQuickItemPrivate::get(referencedItem)->rootNode());
        texture->setRect(referencedItem->boundingRect());
        texture->setSize(referencedItem->boundingRect().size().toSize());
        texture->setRecursive(true);
#ifndef QT_OPENGL_ES
        if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL)
            texture->setFormat(GL_RGBA8);
        else
            texture->setFormat(GL_RGBA);
#else
        texture->setFormat(GL_RGBA);
#endif
        texture->setHasMipmaps(false);

        m_itemTextureHash.insert(referencedItem, texture);
    }
}

void DesignerSupport::derefFromEffectItem(QQuickItem *referencedItem, bool unhide)
{
    if (referencedItem == 0)
        return;

    delete m_itemTextureHash.take(referencedItem);
    QQuickItemPrivate::get(referencedItem)->derefFromEffectItem(unhide);
}

QImage DesignerSupport::renderImageForItem(QQuickItem *referencedItem, const QRectF &boundingRect, const QSize &imageSize)
{
    if (referencedItem == 0 || referencedItem->parentItem() == 0) {
        qDebug() << __FILE__ << __LINE__ << "Warning: Item can be rendered.";
        return QImage();
    }

    QSGLayer *renderTexture = m_itemTextureHash.value(referencedItem);

    Q_ASSERT(renderTexture);
    if (renderTexture == 0)
         return QImage();
    renderTexture->setRect(boundingRect);
    renderTexture->setSize(imageSize);
    renderTexture->setItem(QQuickItemPrivate::get(referencedItem)->rootNode());
    renderTexture->markDirtyTexture();
    renderTexture->updateTexture();

    QImage renderImage = renderTexture->toImage();
    renderImage = renderImage.mirrored(false, true);

    if (renderImage.size().isEmpty())
        qDebug() << __FILE__ << __LINE__ << "Warning: Image is empty.";

    return renderImage;
}

bool DesignerSupport::isDirty(QQuickItem *referencedItem, DirtyType dirtyType)
{
    if (referencedItem == 0)
        return false;

    return QQuickItemPrivate::get(referencedItem)->dirtyAttributes & dirtyType;
}

void DesignerSupport::addDirty(QQuickItem *referencedItem, DesignerSupport::DirtyType dirtyType)
{
    if (referencedItem == 0)
        return;

    QQuickItemPrivate::get(referencedItem)->dirtyAttributes |= dirtyType;
}

void DesignerSupport::resetDirty(QQuickItem *referencedItem)
{
    if (referencedItem == 0)
        return;

    QQuickItemPrivate::get(referencedItem)->dirtyAttributes = 0x0;
    QQuickItemPrivate::get(referencedItem)->removeFromDirtyList();
}

QTransform DesignerSupport::windowTransform(QQuickItem *referencedItem)
{
    if (referencedItem == 0)
        return QTransform();

    return QQuickItemPrivate::get(referencedItem)->itemToWindowTransform();
}

QTransform DesignerSupport::parentTransform(QQuickItem *referencedItem)
{
    if (referencedItem == 0)
        return QTransform();

    QTransform parentTransform;

    QQuickItemPrivate::get(referencedItem)->itemToParentTransform(parentTransform);

    return parentTransform;
}

QString propertyNameForAnchorLine(const QQuickAnchorLine::AnchorLine &anchorLine)
{
    switch (anchorLine) {
        case QQuickAnchorLine::Left: return QLatin1String("left");
        case QQuickAnchorLine::Right: return QLatin1String("right");
        case QQuickAnchorLine::Top: return QLatin1String("top");
        case QQuickAnchorLine::Bottom: return QLatin1String("bottom");
        case QQuickAnchorLine::HCenter: return QLatin1String("horizontalCenter");
        case QQuickAnchorLine::VCenter: return QLatin1String("verticalCenter");
        case QQuickAnchorLine::Baseline: return QLatin1String("baseline");
        case QQuickAnchorLine::Invalid:
        default: return QString();
    }
}

bool isValidAnchorName(const QString &name)
{
    static QStringList anchorNameList(QStringList() << QLatin1String("anchors.top")
                                                    << QLatin1String("anchors.left")
                                                    << QLatin1String("anchors.right")
                                                    << QLatin1String("anchors.bottom")
                                                    << QLatin1String("anchors.verticalCenter")
                                                    << QLatin1String("anchors.horizontalCenter")
                                                    << QLatin1String("anchors.fill")
                                                    << QLatin1String("anchors.centerIn")
                                                    << QLatin1String("anchors.baseline"));

    return anchorNameList.contains(name);
}

bool DesignerSupport::isAnchoredTo(QQuickItem *fromItem, QQuickItem *toItem)
{
#ifndef QT_NO_DYNAMIC_CAST
    Q_ASSERT(dynamic_cast<QQuickItemPrivate*>(QQuickItemPrivate::get(fromItem)));
#endif
    QQuickItemPrivate *fromItemPrivate = static_cast<QQuickItemPrivate*>(QQuickItemPrivate::get(fromItem));
    QQuickAnchors *anchors = fromItemPrivate->anchors();
    return anchors->fill() == toItem
            || anchors->centerIn() == toItem
            || anchors->bottom().item == toItem
            || anchors->top().item == toItem
            || anchors->left().item == toItem
            || anchors->right().item == toItem
            || anchors->verticalCenter().item == toItem
            || anchors->horizontalCenter().item == toItem
            || anchors->baseline().item == toItem;
}

bool DesignerSupport::areChildrenAnchoredTo(QQuickItem *fromItem, QQuickItem *toItem)
{
    foreach (QQuickItem *childItem, fromItem->childItems()) {
        if (childItem) {
            if (isAnchoredTo(childItem, toItem))
                return true;

            if (areChildrenAnchoredTo(childItem, toItem))
                return true;
        }
    }

    return false;
}

QQuickAnchors *anchors(QQuickItem *item)
{
    QQuickItemPrivate *itemPrivate = static_cast<QQuickItemPrivate*>(QQuickItemPrivate::get(item));
    return itemPrivate->anchors();
}

QQuickAnchors::Anchor anchorLineFlagForName(const QString &name)
{
    if (name == QLatin1String("anchors.top"))
        return QQuickAnchors::TopAnchor;

    if (name == QLatin1String("anchors.left"))
        return QQuickAnchors::LeftAnchor;

    if (name == QLatin1String("anchors.bottom"))
         return QQuickAnchors::BottomAnchor;

    if (name == QLatin1String("anchors.right"))
        return QQuickAnchors::RightAnchor;

    if (name == QLatin1String("anchors.horizontalCenter"))
        return QQuickAnchors::HCenterAnchor;

    if (name == QLatin1String("anchors.verticalCenter"))
         return QQuickAnchors::VCenterAnchor;

    if (name == QLatin1String("anchors.baseline"))
         return QQuickAnchors::BaselineAnchor;


    Q_ASSERT_X(false, Q_FUNC_INFO, "wrong anchor name - this should never happen");
    return QQuickAnchors::LeftAnchor;
}

bool DesignerSupport::hasAnchor(QQuickItem *item, const QString &name)
{
    if (!isValidAnchorName(name))
        return false;

    if (name == QLatin1String("anchors.fill"))
        return anchors(item)->fill() != 0;

    if (name == QLatin1String("anchors.centerIn"))
        return anchors(item)->centerIn() != 0;

    if (name == QLatin1String("anchors.right"))
        return anchors(item)->right().item != 0;

    if (name == QLatin1String("anchors.top"))
        return anchors(item)->top().item != 0;

    if (name == QLatin1String("anchors.left"))
        return anchors(item)->left().item != 0;

    if (name == QLatin1String("anchors.bottom"))
        return anchors(item)->bottom().item != 0;

    if (name == QLatin1String("anchors.horizontalCenter"))
        return anchors(item)->horizontalCenter().item != 0;

    if (name == QLatin1String("anchors.verticalCenter"))
        return anchors(item)->verticalCenter().item != 0;

    if (name == QLatin1String("anchors.baseline"))
        return anchors(item)->baseline().item != 0;

    return anchors(item)->usedAnchors().testFlag(anchorLineFlagForName(name));
}

QQuickItem *DesignerSupport::anchorFillTargetItem(QQuickItem *item)
{
    return anchors(item)->fill();
}

QQuickItem *DesignerSupport::anchorCenterInTargetItem(QQuickItem *item)
{
    return anchors(item)->centerIn();
}



QPair<QString, QObject*> DesignerSupport::anchorLineTarget(QQuickItem *item, const QString &name, QQmlContext *context)
{
    QObject *targetObject = 0;
    QString targetName;

    if (name == QLatin1String("anchors.fill")) {
        targetObject = anchors(item)->fill();
    } else if (name == QLatin1String("anchors.centerIn")) {
        targetObject = anchors(item)->centerIn();
    } else {
        QQmlProperty metaProperty(item, name, context);
        if (!metaProperty.isValid())
            return QPair<QString, QObject*>();

        QQuickAnchorLine anchorLine = metaProperty.read().value<QQuickAnchorLine>();
        if (anchorLine.anchorLine != QQuickAnchorLine::Invalid) {
            targetObject = anchorLine.item;
            targetName = propertyNameForAnchorLine(anchorLine.anchorLine);
        }

    }

    return QPair<QString, QObject*>(targetName, targetObject);
}

void DesignerSupport::resetAnchor(QQuickItem *item, const QString &name)
{
    if (name == QLatin1String("anchors.fill")) {
        anchors(item)->resetFill();
    } else if (name == QLatin1String("anchors.centerIn")) {
        anchors(item)->resetCenterIn();
    } else if (name == QLatin1String("anchors.top")) {
        anchors(item)->resetTop();
    } else if (name == QLatin1String("anchors.left")) {
        anchors(item)->resetLeft();
    } else if (name == QLatin1String("anchors.right")) {
        anchors(item)->resetRight();
    } else if (name == QLatin1String("anchors.bottom")) {
        anchors(item)->resetBottom();
    } else if (name == QLatin1String("anchors.horizontalCenter")) {
        anchors(item)->resetHorizontalCenter();
    } else if (name == QLatin1String("anchors.verticalCenter")) {
        anchors(item)->resetVerticalCenter();
    } else if (name == QLatin1String("anchors.baseline")) {
        anchors(item)->resetBaseline();
    }
}

void DesignerSupport::emitComponentCompleteSignalForAttachedProperty(QQuickItem *item)
{
    QQmlData *data = QQmlData::get(item);
    if (data && data->context) {
        QQmlComponentAttached *componentAttached = data->context->componentAttached;
        if (componentAttached) {
            emit componentAttached->completed();
        }
    }
}

QList<QObject*> DesignerSupport::statesForItem(QQuickItem *item)
{
    QList<QObject*> objectList;
    QList<QQuickState *> stateList = QQuickItemPrivate::get(item)->_states()->states();

    objectList.reserve(stateList.size());
    foreach (QQuickState* state, stateList)
        objectList.append(state);

    return objectList;
}

bool DesignerSupport::isComponentComplete(QQuickItem *item)
{
    return static_cast<QQuickItemPrivate*>(QQuickItemPrivate::get(item))->componentComplete;
}

int DesignerSupport::borderWidth(QQuickItem *item)
{
    QQuickRectangle *rectangle = qobject_cast<QQuickRectangle*>(item);
    if (rectangle)
        return rectangle->border()->width();

    return 0;
}

void DesignerSupport::refreshExpressions(QQmlContext *context)
{
    QQmlContextPrivate::get(context)->data->refreshExpressions();
}

void DesignerSupport::setRootItem(QQuickView *view, QQuickItem *item)
{
    QQuickViewPrivate::get(view)->setRootObject(item);
}

bool DesignerSupport::isValidWidth(QQuickItem *item)
{
    return QQuickItemPrivate::get(item)->heightValid;
}

bool DesignerSupport::isValidHeight(QQuickItem *item)
{
    return QQuickItemPrivate::get(item)->widthValid;
}

void DesignerSupport::updateDirtyNode(QQuickItem *item)
{
    if (item->window())
        QQuickWindowPrivate::get(item->window())->updateDirtyNode(item);
}

void DesignerSupport::activateDesignerWindowManager()
{
    QSGRenderLoop::setInstance(new DesignerWindowManager);
}

void DesignerSupport::activateDesignerMode()
{
    QQmlEnginePrivate::activateDesignerMode();
}

void DesignerSupport::disableComponentComplete()
{
    QQmlVME::disableComponentComplete();
}

void DesignerSupport::enableComponentComplete()
{
    QQmlVME::enableComponentComplete();
}

void DesignerSupport::createOpenGLContext(QQuickWindow *window)
{
    DesignerWindowManager::createOpenGLContext(window);
}

void DesignerSupport::polishItems(QQuickWindow *window)
{
    QQuickWindowPrivate::get(window)->polishItems();
}

QT_END_NAMESPACE
