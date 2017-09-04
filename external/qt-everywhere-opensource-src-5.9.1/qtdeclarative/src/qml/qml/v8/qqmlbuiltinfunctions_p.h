/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQMLBUILTINFUNCTIONS_P_H
#define QQMLBUILTINFUNCTIONS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qqmlglobal_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qjsengine_p.h>

QT_BEGIN_NAMESPACE

class QQmlEngine;

namespace QV4 {

namespace Heap {

struct QtObject : Object {
    void init(QQmlEngine *qmlEngine);
    QObject *platform;
    QObject *application;

    enum { Finished = -1 };
    int enumeratorIterator;
    int keyIterator;

    bool isComplete() const
    { return enumeratorIterator == Finished; }
};

struct ConsoleObject : Object {
    void init();
};

struct QQmlBindingFunction : FunctionObject {
    void init(const QV4::FunctionObject *originalFunction);
};

}

struct QtObject : Object
{
    V4_OBJECT2(QtObject, Object)

    static ReturnedValue get(const Managed *m, String *name, bool *hasProperty);
    static void advanceIterator(Managed *m, ObjectIterator *it, Value *name, uint *index, Property *p, PropertyAttributes *attributes);

    static void method_isQtObject(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_rgba(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_hsla(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_hsva(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_colorEqual(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_font(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_rect(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_point(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_size(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_vector2d(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_vector3d(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_vector4d(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_quaternion(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_matrix4x4(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_lighter(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_darker(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_tint(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_formatDate(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_formatTime(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_formatDateTime(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_openUrlExternally(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_fontFamilies(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_md5(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_btoa(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_atob(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_quit(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_exit(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_resolvedUrl(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_createQmlObject(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_createComponent(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_locale(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_binding(const BuiltinFunction *, Scope &scope, CallData *callData);

    static void method_get_platform(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_get_application(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_get_inputMethod(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_get_styleHints(const BuiltinFunction *, Scope &scope, CallData *callData);

    static void method_callLater(const BuiltinFunction *, Scope &scope, CallData *callData);

private:
    void addAll();
    ReturnedValue findAndAdd(const QString *name, bool &foundProperty) const;
};

struct ConsoleObject : Object
{
    V4_OBJECT2(ConsoleObject, Object)

    static void method_error(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_log(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_info(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_profile(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_profileEnd(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_time(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_timeEnd(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_count(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_trace(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_warn(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_assert(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_exception(const BuiltinFunction *, Scope &scope, CallData *callData);

};

struct Q_QML_PRIVATE_EXPORT GlobalExtensions {
    static void init(Object *globalObject, QJSEngine::Extensions extensions);

#if QT_CONFIG(translation)
    static void method_qsTranslate(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_qsTranslateNoOp(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_qsTr(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_qsTrNoOp(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_qsTrId(const BuiltinFunction *, Scope &scope, CallData *callData);
    static void method_qsTrIdNoOp(const BuiltinFunction *, Scope &scope, CallData *callData);
#endif
    static void method_gc(const BuiltinFunction *, Scope &scope, CallData *callData);

    // on String:prototype
    static void method_string_arg(const BuiltinFunction *, Scope &scope, CallData *callData);

};

struct QQmlBindingFunction : public QV4::FunctionObject
{
    V4_OBJECT2(QQmlBindingFunction, FunctionObject)

    QQmlSourceLocation currentLocation() const; // from caller stack trace
};

}

QT_END_NAMESPACE

#endif // QQMLBUILTINFUNCTIONS_P_H
