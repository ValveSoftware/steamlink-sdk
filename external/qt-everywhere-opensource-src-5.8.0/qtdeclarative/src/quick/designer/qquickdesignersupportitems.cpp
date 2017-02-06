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

#include "qquickdesignersupportitems_p.h"
#include "qquickdesignersupportproperties_p.h"

#include <private/qabstractanimation_p.h>
#include <private/qobject_p.h>
#include <private/qquickbehavior_p.h>
#include <private/qquicktext_p.h>
#include <private/qquicktextinput_p.h>
#include <private/qquicktextedit_p.h>
#include <private/qquicktransition_p.h>

#include <private/qquickanimation_p.h>
#include <private/qqmlmetatype_p.h>
#include <private/qqmltimer_p.h>

QT_BEGIN_NAMESPACE

static void (*fixResourcePathsForObjectCallBack)(QObject*) = 0;

static void stopAnimation(QObject *object)
{
    if (object == 0)
        return;

    QQuickTransition *transition = qobject_cast<QQuickTransition*>(object);
    QQuickAbstractAnimation *animation = qobject_cast<QQuickAbstractAnimation*>(object);
    QQmlTimer *timer = qobject_cast<QQmlTimer*>(object);
    if (transition) {
       transition->setFromState(QString());
       transition->setToState(QString());
    } else if (animation) {
//        QQuickScriptAction *scriptAimation = qobject_cast<QQuickScriptAction*>(animation);
//        if (scriptAimation) FIXME
//            scriptAimation->setScript(QQmlScriptString());
        animation->setLoops(1);
        animation->complete();
        animation->setDisableUserControl();
    } else if (timer) {
        timer->blockSignals(true);
    }
}

static void allSubObjects(QObject *object, QObjectList &objectList)
{
    // don't add null pointer and stop if the object is already in the list
    if (!object || objectList.contains(object))
        return;

    objectList.append(object);

    for (int index = QObject::staticMetaObject.propertyOffset();
         index < object->metaObject()->propertyCount();
         index++) {
        QMetaProperty metaProperty = object->metaObject()->property(index);

        // search recursive in property objects
        if (metaProperty.isReadable()
                && metaProperty.isWritable()
                && QQmlMetaType::isQObject(metaProperty.userType())) {
            if (qstrcmp(metaProperty.name(), "parent")) {
                QObject *propertyObject = QQmlMetaType::toQObject(metaProperty.read(object));
                allSubObjects(propertyObject, objectList);
            }

        }

        // search recursive in property object lists
        if (metaProperty.isReadable()
                && QQmlMetaType::isList(metaProperty.userType())) {
            QQmlListReference list(object, metaProperty.name());
            if (list.canCount() && list.canAt()) {
                for (int i = 0; i < list.count(); i++) {
                    QObject *propertyObject = list.at(i);
                    allSubObjects(propertyObject, objectList);

                }
            }
        }
    }

    // search recursive in object children list
    for (QObject *childObject : object->children()) {
        allSubObjects(childObject, objectList);
    }

    // search recursive in quick item childItems list
    QQuickItem *quickItem = qobject_cast<QQuickItem*>(object);
    if (quickItem) {
        const auto childItems = quickItem->childItems();
        for (QQuickItem *childItem : childItems)
            allSubObjects(childItem, objectList);
    }
}

void QQuickDesignerSupportItems::tweakObjects(QObject *object)
{
    QObjectList objectList;
    allSubObjects(object, objectList);
    for (QObject* childObject : qAsConst(objectList)) {
        stopAnimation(childObject);
        if (fixResourcePathsForObjectCallBack)
            fixResourcePathsForObjectCallBack(childObject);
    }
}

static QObject *createDummyWindow(QQmlEngine *engine)
{
    QQmlComponent component(engine, QUrl(QStringLiteral("qrc:/qtquickplugin/mockfiles/Window.qml")));
    return component.create();
}

static bool isWindowMetaObject(const QMetaObject *metaObject)
{
    if (metaObject) {
        if (metaObject->className() == QByteArrayLiteral("QWindow"))
            return true;

        return isWindowMetaObject(metaObject->superClass());
    }

    return false;
}

static bool isWindow(QObject *object) {
    if (object)
        return isWindowMetaObject(object->metaObject());

    return false;
}

static QQmlType *getQmlType(const QString &typeName, int majorNumber, int minorNumber)
{
     return QQmlMetaType::qmlType(typeName, majorNumber, minorNumber);
}

static bool isCrashingType(QQmlType *type)
{
    if (type) {
        if (type->qmlTypeName() == QLatin1String("QtMultimedia/MediaPlayer"))
            return true;

        if (type->qmlTypeName() == QLatin1String("QtMultimedia/Audio"))
            return true;

        if (type->qmlTypeName() == QLatin1String("QtQuick.Controls/MenuItem"))
            return true;

        if (type->qmlTypeName() == QLatin1String("QtQuick.Controls/Menu"))
            return true;

        if (type->qmlTypeName() == QLatin1String("QtQuick/Timer"))
            return true;
    }

    return false;
}

QObject *QQuickDesignerSupportItems::createPrimitive(const QString &typeName, int majorNumber, int minorNumber, QQmlContext *context)
{
    ComponentCompleteDisabler disableComponentComplete;

    Q_UNUSED(disableComponentComplete)

    QObject *object = 0;
    QQmlType *type = getQmlType(typeName, majorNumber, minorNumber);

    if (isCrashingType(type)) {
        object = new QObject;
    } else if (type) {
        if ( type->isComposite()) {
             object = createComponent(type->sourceUrl(), context);
        } else
        {
            if (type->typeName() == "QQmlComponent") {
                object = new QQmlComponent(context->engine(), 0);
            } else  {
                object = type->create();
            }
        }

        if (isWindow(object)) {
            delete object;
            object = createDummyWindow(context->engine());
        }

    }

    if (!object) {
        qWarning() << "QuickDesigner: Cannot create an object of type"
                   << QString::fromLatin1("%1 %2,%3").arg(typeName).arg(majorNumber).arg(minorNumber)
                   << "- type isn't known to declarative meta type system";
    }

    tweakObjects(object);

    if (object && QQmlEngine::contextForObject(object) == 0)
        QQmlEngine::setContextForObject(object, context);

    QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);

    return object;
}

QObject *QQuickDesignerSupportItems::createComponent(const QUrl &componentUrl, QQmlContext *context)
{
    ComponentCompleteDisabler disableComponentComplete;
    Q_UNUSED(disableComponentComplete)

    QQmlComponent component(context->engine(), componentUrl);

    QObject *object = component.beginCreate(context);
    tweakObjects(object);
    component.completeCreate();
    QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);

    if (component.isError()) {
        qWarning() << "Error in:" << Q_FUNC_INFO << componentUrl;
        const auto errors = component.errors();
        for (const QQmlError &error : errors)
            qWarning() << error;
    }
    return object;
}

bool QQuickDesignerSupportItems::objectWasDeleted(QObject *object)
{
    return QObjectPrivate::get(object)->wasDeleted;
}

void QQuickDesignerSupportItems::disableNativeTextRendering(QQuickItem *item)
{
    QQuickText *text = qobject_cast<QQuickText*>(item);
    if (text)
        text->setRenderType(QQuickText::QtRendering);

    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(item);
    if (textInput)
        textInput->setRenderType(QQuickTextInput::QtRendering);

    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(item);
    if (textEdit)
        textEdit->setRenderType(QQuickTextEdit::QtRendering);
}

void QQuickDesignerSupportItems::disableTextCursor(QQuickItem *item)
{
    const auto childItems = item->childItems();
    for (QQuickItem *childItem : childItems)
        disableTextCursor(childItem);

    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(item);
    if (textInput)
        textInput->setCursorVisible(false);

    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(item);
    if (textEdit)
        textEdit->setCursorVisible(false);
}

void QQuickDesignerSupportItems::disableTransition(QObject *object)
{
    QQuickTransition *transition = qobject_cast<QQuickTransition*>(object);
    Q_ASSERT(transition);
    const QString invalidState = QLatin1String("invalidState");
    transition->setToState(invalidState);
    transition->setFromState(invalidState);
}

void QQuickDesignerSupportItems::disableBehaivour(QObject *object)
{
    QQuickBehavior* behavior = qobject_cast<QQuickBehavior*>(object);
    Q_ASSERT(behavior);
    behavior->setEnabled(false);
}

void QQuickDesignerSupportItems::stopUnifiedTimer()
{
    QUnifiedTimer::instance()->setSlowdownFactor(0.00001);
    QUnifiedTimer::instance()->setSlowModeEnabled(true);
}

void QQuickDesignerSupportItems::registerFixResourcePathsForObjectCallBack(void (*callback)(QObject *))
{
    fixResourcePathsForObjectCallBack = callback;
}

QT_END_NAMESPACE


