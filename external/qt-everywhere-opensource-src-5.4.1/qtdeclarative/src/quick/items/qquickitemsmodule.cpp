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

#include "qquickitemsmodule_p.h"

#include "qquickitem.h"
#include "qquickitem_p.h"
#include "qquickitemgrabresult.h"
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
#include "qquicklistview_p.h"
#include "qquickgridview_p.h"
#include "qquickpathview_p.h"
#include "qquickitemviewtransition_p.h"
#include <private/qquickpath_p.h>
#include <private/qquickpathinterpolator_p.h>
#include "qquickpositioners_p.h"
#include "qquickrepeater_p.h"
#include "qquickloader_p.h"
#include "qquickanimatedimage_p.h"
#include "qquickflipable_p.h"
#include "qquicktranslate_p.h"
#include "qquickstateoperations_p.h"
#include "qquickitemanimation_p.h"
#include <private/qquickshadereffect_p.h>
#include <QtQuick/private/qquickshadereffectsource_p.h>
//#include <private/qquickpincharea_p.h>
#include <QtQuick/private/qquickcanvasitem_p.h>
#include <QtQuick/private/qquickcontext2d_p.h>
#include "qquicksprite_p.h"
#include "qquickspritesequence_p.h"
#include "qquickanimatedsprite_p.h"
#include "qquickdrag_p.h"
#include "qquickdroparea_p.h"
#include "qquickmultipointtoucharea_p.h"
#include "qquickopenglinfo_p.h"
#include <private/qqmlmetatype_p.h>
#include <QtQuick/private/qquickaccessibleattached_p.h>

static QQmlPrivate::AutoParentResult qquickitem_autoParent(QObject *obj, QObject *parent)
{
    // When setting a parent (especially during dynamic object creation) in QML,
    // also try to set up the analogous item/window relationship.
    QQuickItem *parentItem = qmlobject_cast<QQuickItem *>(parent);
    if (parentItem) {
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
    } else {
        QQuickWindow *parentWindow = qmlobject_cast<QQuickWindow *>(parent);
        if (parentWindow) {
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
        }
    }
    return QQmlPrivate::IncompatibleParent;
}

static void qt_quickitems_defineModule(const char *uri, int major, int minor)
{
    QQmlPrivate::RegisterAutoParent autoparent = { 0, &qquickitem_autoParent };
    QQmlPrivate::qmlregister(QQmlPrivate::AutoParentRegistration, &autoparent);
    QQuickItemPrivate::registerAccessorProperties();

#ifdef QT_NO_MOVIE
    qmlRegisterTypeNotAvailable(uri,major,minor,"AnimatedImage", qApp->translate("QQuickAnimatedImage","Qt was built without support for QMovie"));
#else
    qmlRegisterType<QQuickAnimatedImage>(uri,major,minor,"AnimatedImage");
#endif
    qmlRegisterType<QQuickBorderImage>(uri,major,minor,"BorderImage");
    qmlRegisterType<QQuickColumn>(uri,major,minor,"Column");
    qmlRegisterType<QQuickFlickable>(uri,major,minor,"Flickable");
    qmlRegisterType<QQuickFlipable>(uri,major,minor,"Flipable");
    qmlRegisterType<QQuickFlow>(uri,major,minor,"Flow");
//    qmlRegisterType<QQuickFocusPanel>(uri,major,minor,"FocusPanel");
    qmlRegisterType<QQuickFocusScope>(uri,major,minor,"FocusScope");
    qmlRegisterType<QQuickGradient>(uri,major,minor,"Gradient");
    qmlRegisterType<QQuickGradientStop>(uri,major,minor,"GradientStop");
    qmlRegisterType<QQuickGrid>(uri,major,minor,"Grid");
    qmlRegisterType<QQuickGridView>(uri,major,minor,"GridView");
    qmlRegisterType<QQuickImage>(uri,major,minor,"Image");
    qmlRegisterType<QQuickItem>(uri,major,minor,"Item");
    qmlRegisterType<QQuickListView>(uri,major,minor,"ListView");
    qmlRegisterType<QQuickLoader>(uri,major,minor,"Loader");
    qmlRegisterType<QQuickMouseArea>(uri,major,minor,"MouseArea");
    qmlRegisterType<QQuickPath>(uri,major,minor,"Path");
    qmlRegisterType<QQuickPathAttribute>(uri,major,minor,"PathAttribute");
    qmlRegisterType<QQuickPathCubic>(uri,major,minor,"PathCubic");
    qmlRegisterType<QQuickPathLine>(uri,major,minor,"PathLine");
    qmlRegisterType<QQuickPathPercent>(uri,major,minor,"PathPercent");
    qmlRegisterType<QQuickPathQuad>(uri,major,minor,"PathQuad");
    qmlRegisterType<QQuickPathCatmullRomCurve>("QtQuick",2,0,"PathCurve");
    qmlRegisterType<QQuickPathArc>("QtQuick",2,0,"PathArc");
    qmlRegisterType<QQuickPathSvg>("QtQuick",2,0,"PathSvg");
    qmlRegisterType<QQuickPathView>(uri,major,minor,"PathView");
    qmlRegisterUncreatableType<QQuickBasePositioner>(uri,major,minor,"Positioner",
                                                  QStringLiteral("Positioner is an abstract type that is only available as an attached property."));
#ifndef QT_NO_VALIDATOR
    qmlRegisterType<QQuickIntValidator>(uri,major,minor,"IntValidator");
    qmlRegisterType<QQuickDoubleValidator>(uri,major,minor,"DoubleValidator");
    qmlRegisterType<QRegExpValidator>(uri,major,minor,"RegExpValidator");
#endif
    qmlRegisterType<QQuickRectangle>(uri,major,minor,"Rectangle");
    qmlRegisterType<QQuickRepeater>(uri,major,minor,"Repeater");
    qmlRegisterType<QQuickRow>(uri,major,minor,"Row");
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
    qmlRegisterType<QQuickViewSection>(uri,major,minor,"ViewSection");

    qmlRegisterType<QQuickItemGrabResult>();
    qmlRegisterType<QQuickItemLayer>();
    qmlRegisterType<QQuickAnchors>();
    qmlRegisterType<QQuickKeyEvent>();
    qmlRegisterType<QQuickMouseEvent>();
    qmlRegisterType<QQuickWheelEvent>();
    qmlRegisterType<QQuickCloseEvent>();
    qmlRegisterType<QQuickTransform>();
    qmlRegisterType<QQuickPathElement>();
    qmlRegisterType<QQuickCurve>();
    qmlRegisterType<QQuickScaleGrid>();
    qmlRegisterType<QQuickTextLine>();
#ifndef QT_NO_VALIDATOR
    qmlRegisterType<QValidator>();
#endif
    qmlRegisterType<QQuickPen>();
    qmlRegisterType<QQuickFlickableVisibleArea>();
    qRegisterMetaType<QQuickAnchorLine>("QQuickAnchorLine");

    qmlRegisterType<QQuickTextDocument>();


    qmlRegisterUncreatableType<QQuickKeyNavigationAttached>(uri,major,minor,"KeyNavigation",QQuickKeyNavigationAttached::tr("KeyNavigation is only available via attached properties"));
    qmlRegisterUncreatableType<QQuickKeysAttached>(uri,major,minor,"Keys",QQuickKeysAttached::tr("Keys is only available via attached properties"));
    qmlRegisterUncreatableType<QQuickLayoutMirroringAttached>(uri,major,minor,"LayoutMirroring", QQuickLayoutMirroringAttached::tr("LayoutMirroring is only available via attached properties"));
    qmlRegisterUncreatableType<QQuickViewTransitionAttached>(uri,major,minor,"ViewTransition",QQuickViewTransitionAttached::tr("ViewTransition is only available via attached properties"));

    qmlRegisterType<QQuickPinchArea>(uri,major,minor,"PinchArea");
    qmlRegisterType<QQuickPinch>(uri,major,minor,"Pinch");
    qmlRegisterType<QQuickPinchEvent>();

    qmlRegisterType<QQuickShaderEffect>("QtQuick", 2, 0, "ShaderEffect");
    qmlRegisterType<QQuickShaderEffectSource>("QtQuick", 2, 0, "ShaderEffectSource");
    qmlRegisterUncreatableType<QQuickShaderEffectMesh>("QtQuick", 2, 0, "ShaderEffectMesh", QQuickShaderEffectMesh::tr("Cannot create instance of abstract class ShaderEffectMesh."));
    qmlRegisterType<QQuickGridMesh>("QtQuick", 2, 0, "GridMesh");

    qmlRegisterUncreatableType<QQuickPaintedItem>("QtQuick", 2, 0, "PaintedItem", QQuickPaintedItem::tr("Cannot create instance of abstract class PaintedItem"));

    qmlRegisterType<QQuickCanvasItem>("QtQuick", 2, 0, "Canvas");

    qmlRegisterType<QQuickSprite>("QtQuick", 2, 0, "Sprite");
    qmlRegisterType<QQuickAnimatedSprite>("QtQuick", 2, 0, "AnimatedSprite");
    qmlRegisterType<QQuickSpriteSequence>("QtQuick", 2, 0, "SpriteSequence");

    qmlRegisterType<QQuickParentChange>(uri, major, minor,"ParentChange");
    qmlRegisterType<QQuickAnchorChanges>(uri, major, minor,"AnchorChanges");
    qmlRegisterType<QQuickAnchorSet>();
    qmlRegisterType<QQuickAnchorAnimation>(uri, major, minor,"AnchorAnimation");
    qmlRegisterType<QQuickParentAnimation>(uri, major, minor,"ParentAnimation");
    qmlRegisterType<QQuickPathAnimation>("QtQuick",2,0,"PathAnimation");
    qmlRegisterType<QQuickPathInterpolator>("QtQuick",2,0,"PathInterpolator");

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
    qmlRegisterType<QQuickGrid, 1>(uri, 2, 1, "Grid");
    qmlRegisterUncreatableType<QQuickItemView, 1>(uri, 2, 1, "ItemView", QQuickItemView::tr("ItemView is an abstract base class"));
    qmlRegisterUncreatableType<QQuickItemView, 2>(uri, 2, 3, "ItemView", QQuickItemView::tr("ItemView is an abstract base class"));
    qmlRegisterType<QQuickListView, 1>(uri, 2, 1, "ListView");
    qmlRegisterType<QQuickGridView, 1>(uri, 2, 1, "GridView");
    qmlRegisterType<QQuickTextEdit, 1>(uri, 2, 1, "TextEdit");

    qmlRegisterType<QQuickText, 2>(uri, 2, 2, "Text");
    qmlRegisterType<QQuickTextEdit, 2>(uri, 2, 2, "TextEdit");

    qmlRegisterType<QQuickText, 3>(uri, 2, 3, "Text");
    qmlRegisterType<QQuickTextEdit, 3>(uri, 2, 3, "TextEdit");
    qmlRegisterType<QQuickImage, 1>(uri, 2, 3,"Image");

    qmlRegisterType<QQuickItem, 2>(uri, 2, 4, "Item");
    qmlRegisterType<QQuickListView, 2>(uri, 2, 4, "ListView");
    qmlRegisterType<QQuickMouseArea, 1>(uri, 2, 4, "MouseArea");
    qmlRegisterType<QQuickShaderEffect, 1>(uri, 2, 4, "ShaderEffect");
    qmlRegisterUncreatableType<QQuickOpenGLInfo>(uri, 2, 4,"OpenGLInfo", QQuickOpenGLInfo::tr("OpenGLInfo is only available via attached properties"));
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
