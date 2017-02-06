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

#include "qquickitemsmodule_p.h"

#include "qquickitem.h"
#include "qquickitem_p.h"
#include "qquickevents_p_p.h"
#include "qquickrectangle_p.h"
#include "qquickfocusscope_p.h"
#include "qquicktext_p.h"
#include "qquicktextinput_p.h"
#include "qquicktextedit_p.h"
#include "qquicktextdocument.h"
#include "qquickimage_p.h"
#include "qquickborderimage_p.h"
#include "qquickscalegrid_p_p.h"
#include "qquickmousearea_p.h"
#include "qquickpincharea_p.h"
#include "qquickflickable_p.h"
#include "qquickflickable_p_p.h"
#if QT_CONFIG(quick_listview)
#include "qquicklistview_p.h"
#endif
#if QT_CONFIG(quick_gridview)
#include "qquickgridview_p.h"
#endif
#if QT_CONFIG(quick_pathview)
#include "qquickpathview_p.h"
#endif
#if QT_CONFIG(quick_viewtransitions)
#include "qquickitemviewtransition_p.h"
#endif
#if QT_CONFIG(quick_path)
#include <private/qquickpath_p.h>
#include <private/qquickpathinterpolator_p.h>
#endif
#if QT_CONFIG(quick_positioners)
#include "qquickpositioners_p.h"
#endif
#include "qquickrepeater_p.h"
#include "qquickloader_p.h"
#if QT_CONFIG(quick_animatedimage)
#include "qquickanimatedimage_p.h"
#endif
#if QT_CONFIG(quick_flipable)
#include "qquickflipable_p.h"
#endif
#include "qquicktranslate_p.h"
#include "qquickstateoperations_p.h"
#include "qquickitemanimation_p.h"
//#include <private/qquickpincharea_p.h>
#if QT_CONFIG(quick_canvas)
#include <QtQuick/private/qquickcanvasitem_p.h>
#include <QtQuick/private/qquickcontext2d_p.h>
#endif
#include "qquickitemgrabresult.h"
#if QT_CONFIG(quick_sprite)
#include "qquicksprite_p.h"
#include "qquickspritesequence_p.h"
#include "qquickanimatedsprite_p.h"
#endif
#ifndef QT_NO_OPENGL
# include "qquickopenglinfo_p.h"
#endif
#include "qquickgraphicsinfo_p.h"
#if QT_CONFIG(quick_shadereffect)
#include <QtQuick/private/qquickshadereffectsource_p.h>
#include "qquickshadereffect_p.h"
#include "qquickshadereffectmesh_p.h"
#endif
#include "qquickdrag_p.h"
#include "qquickdroparea_p.h"
#include "qquickmultipointtoucharea_p.h"
#include <private/qqmlmetatype_p.h>
#include <QtQuick/private/qquickaccessibleattached_p.h>

static QQmlPrivate::AutoParentResult qquickitem_autoParent(QObject *obj, QObject *parent)
{
    // When setting a parent (especially during dynamic object creation) in QML,
    // also try to set up the analogous item/window relationship.
    if (QQuickItem *parentItem = qmlobject_cast<QQuickItem *>(parent)) {
        QQuickItem *item = qmlobject_cast<QQuickItem *>(obj);
        if (item) {
            // An Item has another Item
            item->setParentItem(parentItem);
            return QQmlPrivate::Parented;
        } else if (parentItem->window()) {
            QQuickWindow *win = qmlobject_cast<QQuickWindow *>(obj);
            if (win) {
                // A Window inside an Item should be transient for that item's window
                win->setTransientParent(parentItem->window());
                return QQmlPrivate::Parented;
            }
        }
        return QQmlPrivate::IncompatibleObject;
    } else if (QQuickWindow *parentWindow = qmlobject_cast<QQuickWindow *>(parent)) {
        QQuickWindow *win = qmlobject_cast<QQuickWindow *>(obj);
        if (win) {
            // A Window inside a Window should be transient for it
            win->setTransientParent(parentWindow);
            return QQmlPrivate::Parented;
        } else {
            QQuickItem *item = qmlobject_cast<QQuickItem *>(obj);
            if (item) {
                // The parent of an Item inside a Window is actually the implicit content Item
                item->setParentItem(parentWindow->contentItem());
                return QQmlPrivate::Parented;
            }
        }
        return QQmlPrivate::IncompatibleObject;
    } else if (qmlobject_cast<QQuickItem *>(obj)) {
        return QQmlPrivate::IncompatibleParent;
    }
    return QQmlPrivate::IncompatibleObject;
}

static void qt_quickitems_defineModule(const char *uri, int major, int minor)
{
    QQmlPrivate::RegisterAutoParent autoparent = { 0, &qquickitem_autoParent };
    QQmlPrivate::qmlregister(QQmlPrivate::AutoParentRegistration, &autoparent);

#if !QT_CONFIG(quick_animatedimage)
    qmlRegisterTypeNotAvailable(uri,major,minor,"AnimatedImage", QCoreApplication::translate("QQuickAnimatedImage","Qt was built without support for QMovie"));
#else
    qmlRegisterType<QQuickAnimatedImage>(uri,major,minor,"AnimatedImage");
#endif
    qmlRegisterType<QQuickBorderImage>(uri,major,minor,"BorderImage");
    qmlRegisterType<QQuickFlickable>(uri,major,minor,"Flickable");
#if QT_CONFIG(quick_flipable)
    qmlRegisterType<QQuickFlipable>(uri,major,minor,"Flipable");
#endif
//    qmlRegisterType<QQuickFocusPanel>(uri,major,minor,"FocusPanel");
    qmlRegisterType<QQuickFocusScope>(uri,major,minor,"FocusScope");
    qmlRegisterType<QQuickGradient>(uri,major,minor,"Gradient");
    qmlRegisterType<QQuickGradientStop>(uri,major,minor,"GradientStop");
#if QT_CONFIG(quick_positioners)
    qmlRegisterType<QQuickColumn>(uri,major,minor,"Column");
    qmlRegisterType<QQuickFlow>(uri,major,minor,"Flow");
    qmlRegisterType<QQuickGrid>(uri,major,minor,"Grid");
    qmlRegisterUncreatableType<QQuickBasePositioner>(uri,major,minor,"Positioner",
                                                  QStringLiteral("Positioner is an abstract type that is only available as an attached property."));
    qmlRegisterType<QQuickRow>(uri,major,minor,"Row");
#endif
#if QT_CONFIG(quick_gridview)
    qmlRegisterType<QQuickGridView>(uri,major,minor,"GridView");
#endif
    qmlRegisterType<QQuickImage>(uri,major,minor,"Image");
    qmlRegisterType<QQuickItem>(uri,major,minor,"Item");
#if QT_CONFIG(quick_listview)
    qmlRegisterType<QQuickListView>(uri,major,minor,"ListView");
    qmlRegisterType<QQuickViewSection>(uri,major,minor,"ViewSection");
#endif
    qmlRegisterType<QQuickLoader>(uri,major,minor,"Loader");
    qmlRegisterType<QQuickMouseArea>(uri,major,minor,"MouseArea");
#if QT_CONFIG(quick_path)
    qmlRegisterType<QQuickPath>(uri,major,minor,"Path");
    qmlRegisterType<QQuickPathAttribute>(uri,major,minor,"PathAttribute");
    qmlRegisterType<QQuickPathCubic>(uri,major,minor,"PathCubic");
    qmlRegisterType<QQuickPathLine>(uri,major,minor,"PathLine");
    qmlRegisterType<QQuickPathPercent>(uri,major,minor,"PathPercent");
    qmlRegisterType<QQuickPathQuad>(uri,major,minor,"PathQuad");
    qmlRegisterType<QQuickPathCatmullRomCurve>("QtQuick",2,0,"PathCurve");
    qmlRegisterType<QQuickPathArc>("QtQuick",2,0,"PathArc");
    qmlRegisterType<QQuickPathSvg>("QtQuick",2,0,"PathSvg");
#endif
#if QT_CONFIG(quick_pathview)
    qmlRegisterType<QQuickPathView>(uri,major,minor,"PathView");
#endif
    qmlRegisterType<QQuickRectangle>(uri,major,minor,"Rectangle");
    qmlRegisterType<QQuickRepeater>(uri,major,minor,"Repeater");
    qmlRegisterType<QQuickTranslate>(uri,major,minor,"Translate");
    qmlRegisterType<QQuickRotation>(uri,major,minor,"Rotation");
    qmlRegisterType<QQuickScale>(uri,major,minor,"Scale");
    qmlRegisterType<QQuickMatrix4x4>(uri,2,3,"Matrix4x4");
    qmlRegisterType<QQuickText>(uri,major,minor,"Text");
    qmlRegisterType<QQuickTextEdit>(uri,major,minor,"TextEdit");
    qmlRegisterType<QQuickTextEdit,1>(uri,2,1,"TextEdit");
    qmlRegisterType<QQuickTextInput>(uri,major,minor,"TextInput");
    qmlRegisterType<QQuickTextInput,2>(uri,2,2,"TextInput");
    qmlRegisterType<QQuickTextInput,3>(uri,2,4,"TextInput");
    qmlRegisterType<QQuickItemGrabResult>();
#if QT_CONFIG(quick_shadereffect)
    qmlRegisterType<QQuickItemLayer>();
#endif
    qmlRegisterType<QQuickAnchors>();
    qmlRegisterType<QQuickKeyEvent>();
    qmlRegisterType<QQuickMouseEvent>();
    qmlRegisterType<QQuickWheelEvent>();
    qmlRegisterType<QQuickCloseEvent>();
    qmlRegisterType<QQuickTransform>();
#if QT_CONFIG(quick_path)
    qmlRegisterType<QQuickPathElement>();
    qmlRegisterType<QQuickCurve>();
#endif
    qmlRegisterType<QQuickScaleGrid>();
    qmlRegisterType<QQuickTextLine>();
    qmlRegisterType<QQuickPen>();
    qmlRegisterType<QQuickFlickableVisibleArea>();
    qRegisterMetaType<QQuickAnchorLine>("QQuickAnchorLine");

    qmlRegisterType<QQuickTextDocument>();


    qmlRegisterUncreatableType<QQuickKeyNavigationAttached>(uri,major,minor,"KeyNavigation",QQuickKeyNavigationAttached::tr("KeyNavigation is only available via attached properties"));
    qmlRegisterUncreatableType<QQuickKeysAttached>(uri,major,minor,"Keys",QQuickKeysAttached::tr("Keys is only available via attached properties"));
    qmlRegisterUncreatableType<QQuickLayoutMirroringAttached>(uri,major,minor,"LayoutMirroring", QQuickLayoutMirroringAttached::tr("LayoutMirroring is only available via attached properties"));
#if QT_CONFIG(quick_viewtransitions)
    qmlRegisterUncreatableType<QQuickViewTransitionAttached>(uri,major,minor,"ViewTransition",QQuickViewTransitionAttached::tr("ViewTransition is only available via attached properties"));
#endif

    qmlRegisterType<QQuickPinchArea>(uri,major,minor,"PinchArea");
    qmlRegisterType<QQuickPinch>(uri,major,minor,"Pinch");
    qmlRegisterType<QQuickPinchEvent>();

#if QT_CONFIG(quick_shadereffect)
    qmlRegisterType<QQuickShaderEffectSource>("QtQuick", 2, 0, "ShaderEffectSource");
    qmlRegisterUncreatableType<QQuickShaderEffectMesh>("QtQuick", 2, 0, "ShaderEffectMesh", QQuickShaderEffectMesh::tr("Cannot create instance of abstract class ShaderEffectMesh."));
    qmlRegisterType<QQuickGridMesh>("QtQuick", 2, 0, "GridMesh");
    qmlRegisterType<QQuickShaderEffect>("QtQuick", 2, 0, "ShaderEffect");
#endif

    qmlRegisterUncreatableType<QQuickPaintedItem>("QtQuick", 2, 0, "PaintedItem", QQuickPaintedItem::tr("Cannot create instance of abstract class PaintedItem"));

#if QT_CONFIG(quick_canvas)
    qmlRegisterType<QQuickCanvasItem>("QtQuick", 2, 0, "Canvas");
#endif

#if QT_CONFIG(quick_sprite)
    qmlRegisterType<QQuickSprite>("QtQuick", 2, 0, "Sprite");
    qmlRegisterType<QQuickAnimatedSprite>("QtQuick", 2, 0, "AnimatedSprite");
    qmlRegisterType<QQuickSpriteSequence>("QtQuick", 2, 0, "SpriteSequence");
#endif

    qmlRegisterType<QQuickParentChange>(uri, major, minor,"ParentChange");
    qmlRegisterType<QQuickAnchorChanges>(uri, major, minor,"AnchorChanges");
    qmlRegisterType<QQuickAnchorSet>();
    qmlRegisterType<QQuickAnchorAnimation>(uri, major, minor,"AnchorAnimation");
    qmlRegisterType<QQuickParentAnimation>(uri, major, minor,"ParentAnimation");
#if QT_CONFIG(quick_canvas)
    qmlRegisterType<QQuickPathAnimation>("QtQuick",2,0,"PathAnimation");
    qmlRegisterType<QQuickPathInterpolator>("QtQuick",2,0,"PathInterpolator");
#endif

#ifndef QT_NO_DRAGANDDROP
    qmlRegisterType<QQuickDropArea>("QtQuick", 2, 0, "DropArea");
    qmlRegisterType<QQuickDropEvent>();
    qmlRegisterType<QQuickDropAreaDrag>();
    qmlRegisterUncreatableType<QQuickDrag>("QtQuick", 2, 0, "Drag", QQuickDragAttached::tr("Drag is only available via attached properties"));
#endif

    qmlRegisterType<QQuickMultiPointTouchArea>("QtQuick", 2, 0, "MultiPointTouchArea");
    qmlRegisterType<QQuickTouchPoint>("QtQuick", 2, 0, "TouchPoint");
    qmlRegisterType<QQuickGrabGestureEvent>();

#ifndef QT_NO_ACCESSIBILITY
    qmlRegisterUncreatableType<QQuickAccessibleAttached>("QtQuick", 2, 0, "Accessible",QQuickAccessibleAttached::tr("Accessible is only available via attached properties"));
#endif

    qmlRegisterType<QQuickItem, 1>(uri, 2, 1,"Item");
#if QT_CONFIG(quick_positioners)
    qmlRegisterType<QQuickGrid, 1>(uri, 2, 1, "Grid");
#endif
#if QT_CONFIG(quick_itemview)
    qmlRegisterUncreatableType<QQuickItemView, 1>(uri, 2, 1, "ItemView", QQuickItemView::tr("ItemView is an abstract base class"));
    qmlRegisterUncreatableType<QQuickItemView, 2>(uri, 2, 3, "ItemView", QQuickItemView::tr("ItemView is an abstract base class"));
#endif
#if QT_CONFIG(quick_listview)
    qmlRegisterType<QQuickListView, 1>(uri, 2, 1, "ListView");
#endif
#if QT_CONFIG(quick_gridview)
    qmlRegisterType<QQuickGridView, 1>(uri, 2, 1, "GridView");
#endif
    qmlRegisterType<QQuickTextEdit, 1>(uri, 2, 1, "TextEdit");

    qmlRegisterType<QQuickText, 2>(uri, 2, 2, "Text");
    qmlRegisterType<QQuickTextEdit, 2>(uri, 2, 2, "TextEdit");

    qmlRegisterType<QQuickText, 3>(uri, 2, 3, "Text");
    qmlRegisterType<QQuickTextEdit, 3>(uri, 2, 3, "TextEdit");
    qmlRegisterType<QQuickImage, 1>(uri, 2, 3,"Image");

    qmlRegisterType<QQuickItem, 2>(uri, 2, 4, "Item");
#if QT_CONFIG(quick_listview)
    qmlRegisterType<QQuickListView, 2>(uri, 2, 4, "ListView");
#endif
    qmlRegisterType<QQuickMouseArea, 1>(uri, 2, 4, "MouseArea");
#if QT_CONFIG(quick_shadereffect)
    qmlRegisterType<QQuickShaderEffect, 1>(uri, 2, 4, "ShaderEffect");
#endif

#ifndef QT_NO_OPENGL
    qmlRegisterUncreatableType<QQuickOpenGLInfo>(uri, 2, 4,"OpenGLInfo", QQuickOpenGLInfo::tr("OpenGLInfo is only available via attached properties"));
#endif
    qmlRegisterType<QQuickPinchArea, 1>(uri, 2, 5,"PinchArea");
    qmlRegisterType<QQuickImage, 2>(uri, 2, 5,"Image");
    qmlRegisterType<QQuickMouseArea, 2>(uri, 2, 5, "MouseArea");

    qmlRegisterType<QQuickText, 6>(uri, 2, 6, "Text");
    qmlRegisterType<QQuickTextEdit, 6>(uri, 2, 6, "TextEdit");
    qmlRegisterType<QQuickTextInput, 6>(uri, 2, 6, "TextInput");
#if QT_CONFIG(quick_positioners)
    qmlRegisterUncreatableType<QQuickBasePositioner, 6>(uri, 2, 6, "Positioner",
                                                  QStringLiteral("Positioner is an abstract type that is only available as an attached property."));
    qmlRegisterType<QQuickColumn, 6>(uri, 2, 6, "Column");
    qmlRegisterType<QQuickRow, 6>(uri, 2, 6, "Row");
    qmlRegisterType<QQuickGrid, 6>(uri, 2, 6, "Grid");
    qmlRegisterType<QQuickFlow, 6>(uri, 2, 6, "Flow");
#endif
    qmlRegisterUncreatableType<QQuickEnterKeyAttached, 6>(uri, 2, 6, "EnterKey",
                                                           QQuickEnterKeyAttached::tr("EnterKey is only available via attached properties"));
#if QT_CONFIG(quick_shadereffect)
    qmlRegisterType<QQuickShaderEffectSource, 1>(uri, 2, 6, "ShaderEffectSource");
#endif

    qmlRegisterType<QQuickItem, 7>(uri, 2, 7, "Item");
#if QT_CONFIG(quick_listview)
    qmlRegisterType<QQuickListView, 7>(uri, 2, 7, "ListView");
#endif
#if QT_CONFIG(quick_gridview)
    qmlRegisterType<QQuickGridView, 7>(uri, 2, 7, "GridView");
#endif
    qmlRegisterType<QQuickTextInput, 7>(uri, 2, 7, "TextInput");
    qmlRegisterType<QQuickTextEdit, 7>(uri, 2, 7, "TextEdit");
#if QT_CONFIG(quick_pathview)
    qmlRegisterType<QQuickPathView, 7>(uri, 2, 7, "PathView");
#endif

    qmlRegisterUncreatableType<QQuickMouseEvent, 7>(uri, 2, 7, nullptr, QQuickMouseEvent::tr("MouseEvent is only available within handlers in MouseArea"));

    qmlRegisterUncreatableType<QQuickGraphicsInfo>(uri, 2, 8,"GraphicsInfo", QQuickGraphicsInfo::tr("GraphicsInfo is only available via attached properties"));
#if QT_CONFIG(quick_shadereffect)
    qmlRegisterType<QQuickBorderImageMesh>("QtQuick", 2, 8, "BorderImageMesh");
#endif
}

static void initResources()
{
    Q_INIT_RESOURCE(items);
}

QT_BEGIN_NAMESPACE

void QQuickItemsModule::defineModule()
{
    initResources();

    QByteArray name = "QtQuick";
    int majorVersion = 2;
    int minorVersion = 0;

    qt_quickitems_defineModule(name, majorVersion, minorVersion);
}

QT_END_NAMESPACE
