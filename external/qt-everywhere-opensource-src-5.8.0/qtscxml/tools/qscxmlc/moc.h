/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MOC_H
#define MOC_H

#include <QtCore/qmap.h>
#include <QtCore/qpair.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonarray.h>
#include <ctype.h>

QT_BEGIN_NAMESPACE

struct QMetaObject;

struct Type
{
    enum ReferenceType { NoReference, Reference, RValueReference, Pointer };

    inline Type() : isVolatile(false), isScoped(false), /*firstToken(NOTOKEN), */referenceType(NoReference) {}
    inline explicit Type(const QByteArray &_name)
        : name(_name), rawName(name), isVolatile(false), isScoped(false), /*firstToken(NOTOKEN),*/ referenceType(NoReference) {}
    QByteArray name;
    //When used as a return type, the type name may be modified to remove the references.
    // rawName is the type as found in the function signature
    QByteArray rawName;
    uint isVolatile : 1;
    uint isScoped : 1;
//    Token firstToken;
    ReferenceType referenceType;
};

struct EnumDef
{
    QByteArray name;
    QList<QByteArray> values;
    bool isEnumClass; // c++11 enum class
    EnumDef() : isEnumClass(false) {}
};

struct ArgumentDef
{
    ArgumentDef() : isDefault(false) {}
    Type type;
    QByteArray rightType, normalizedType, name;
    QByteArray typeNameForCast; // type name to be used in cast from void * in metacall
    bool isDefault;
};

struct FunctionDef
{
    FunctionDef(): returnTypeIsVolatile(false), access(Private), isConst(false), isVirtual(false), isStatic(false),
                   inlineCode(false), wasCloned(false), isCompat(false), isInvokable(false),
                   isScriptable(false), isSlot(false), isSignal(false), isPrivateSignal(false),
                   isConstructor(false), isDestructor(false), isAbstract(false), revision(0), implementation(0) {}
    Type type;
    QByteArray normalizedType;
    QByteArray tag;
    QByteArray name;
    bool returnTypeIsVolatile;

    QList<ArgumentDef> arguments;

    enum Access { Private, Protected, Public };
    Access access;
    bool isConst;
    bool isVirtual;
    bool isStatic;
    bool inlineCode;
    bool wasCloned;

    QByteArray inPrivateClass;
    bool isCompat;
    bool isInvokable;
    bool isScriptable;
    bool isSlot;
    bool isSignal;
    bool isPrivateSignal;
    bool isConstructor;
    bool isDestructor;
    bool isAbstract;

    int revision;

    const char *implementation;
};

struct PropertyDef
{
    PropertyDef():notifyId(-1), constant(false), final(false), gspec(ValueSpec), revision(0){}
    QByteArray name, type, member, read, write, reset, designable, scriptable, editable, stored, user, notify, inPrivateClass;
    int notifyId;
    bool constant;
    bool final;
    enum Specification  { ValueSpec, ReferenceSpec, PointerSpec };
    Specification gspec;
    bool stdCppSet() const {
        QByteArray s("set");
        s += toupper(name[0]);
        s += name.mid(1);
        return (s == write);
    }
    int revision;
};


struct ClassInfoDef
{
    QByteArray name;
    QByteArray value;
};

struct ClassDef {
    ClassDef():
        hasQObject(false), hasQGadget(false), notifyableProperties(0)
        , revisionedMethods(0), revisionedProperties(0), begin(0), end(0){}
    QByteArray classname;
    QByteArray qualified;
    QList<QPair<QByteArray, FunctionDef::Access> > superclassList;

    struct Interface
    {
        inline explicit Interface(const QByteArray &_className)
            : className(_className) {}
        QByteArray className;
        QByteArray interfaceId;
    };
    QList<QList<Interface> >interfaceList;

    bool hasQObject;
    bool hasQGadget;

    struct PluginData {
        QByteArray iid;
        QMap<QString, QJsonArray> metaArgs;
        QJsonDocument metaData;
    } pluginData;

    QList<FunctionDef> constructorList;
    QList<FunctionDef> signalList, slotList, methodList, publicList;
    int notifyableProperties;
    QList<PropertyDef> propertyList;
    QList<ClassInfoDef> classInfoList;
    QMap<QByteArray, bool> enumDeclarations;
    QList<EnumDef> enumList;
    QMap<QByteArray, QByteArray> flagAliases;
    int revisionedMethods;
    int revisionedProperties;

    int begin;
    int end;
};

struct NamespaceDef {
    QByteArray name;
    int begin;
    int end;
};

inline QByteArray noRef(const QByteArray &type)
{
    if (type.endsWith('&')) {
        if (type.endsWith("&&"))
            return type.left(type.length()-2);
        return type.left(type.length()-1);
    }
    return type;
}

QT_END_NAMESPACE

#endif // MOC_H
