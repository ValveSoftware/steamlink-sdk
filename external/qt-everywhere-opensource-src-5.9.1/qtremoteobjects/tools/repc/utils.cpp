/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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

#include "utils.h"

#include "repparser.h"

#include <moc.h>

#define _(X) QString::fromLatin1(X)

QT_BEGIN_NAMESPACE

static QByteArray join(const QVector<QByteArray> &array, const QByteArray &separator)
{
    QByteArray res;
    const int sz = array.size();
    if (!sz)
        return res;
    for (int i = 0; i < sz - 1; i++)
        res += array.at(i) + separator;
    res += array.at(sz - 1);
    return res;
}

static QVector<QByteArray> generateProperties(const QVector<PropertyDef> &properties, bool isPod=false)
{
    QVector<QByteArray> ret;
    for (const PropertyDef& property : properties) {
        if (!isPod && property.notifyId == -1 && !property.constant) {
            qWarning() << "Skipping property" << property.name << "because is non-notifiable & non-constant";
            continue; // skip non-notifiable properties
        }
        QByteArray prop = property.type + " " + property.name;
        if (property.constant)
            prop += " CONSTANT";
        if (property.write.isEmpty() && !property.read.isEmpty())
            prop += " READONLY";
        ret << prop;
    }
    return ret;
}

static QByteArray generateFunctions(const QByteArray &type, const QVector<FunctionDef> &functionList)
{
    QByteArray ret;
    for (const FunctionDef &func : functionList) {
        if (func.access != FunctionDef::Public)
            continue;

        ret += type + "(" + func.normalizedType + " " + func.name + "(";
        const int sz = func.arguments.size();
        if (sz) {
            for (int i = 0; i < sz - 1 ; i++) {
                const ArgumentDef &arg = func.arguments.at(i);
                ret += arg.normalizedType + " " + arg.name + ", ";
            }

            const ArgumentDef &arg = func.arguments.at(sz -1);
            ret += arg.normalizedType + " " + arg.name;
        }
        ret += "));\n";
    }
    return ret;
}

static bool highToLowSort(int a, int b)
{
    return a > b;
}

static QVector<FunctionDef> cleanedSignalList(const ClassDef &cdef)
{
    auto ret = cdef.signalList;
    QVector<int> positions;
    for (const PropertyDef &prop :  qAsConst(cdef.propertyList)) {
        if (prop.notifyId != -1) {
            Q_ASSERT(prop.notify == ret.at(prop.notifyId).name);
            positions.push_back(prop.notifyId);
        }
    }
    std::sort(positions.begin(), positions.end(), highToLowSort);
    for (int pos : qAsConst(positions))
        ret.removeAt(pos);
    return ret;
}

static QVector<FunctionDef> cleanedSlotList(const ClassDef &cdef)
{
    auto ret = cdef.slotList;
    for (const PropertyDef &prop : qAsConst(cdef.propertyList)) {
        if (!prop.write.isEmpty()) {
            auto it = ret.begin();
            while (it != ret.end()) {
                const FunctionDef& fdef = *it;
                if (fdef.name == prop.write &&
                    fdef.arguments.size() == 1 &&
                    fdef.arguments[0].type.name == prop.type) {
                    ret.erase(it);
                    break;
                }
            }
        }
    }
    return ret;
}

QByteArray generateClass(const ClassDef &cdef, bool alwaysGenerateClass /* = false */)
{
    const auto signalList = cleanedSignalList(cdef);
    if (signalList.isEmpty() && cdef.slotList.isEmpty() && !alwaysGenerateClass)
        return "POD " + cdef.classname + "(" + join(generateProperties(cdef.propertyList, true), ", ") + ")\n";

    QByteArray ret("class " + cdef.classname + "\n{\n");
    if (!cdef.propertyList.isEmpty())
        ret += "    PROP(" + join(generateProperties(cdef.propertyList), ");\n    PROP(") + ");\n";
    ret += generateFunctions("    SLOT", cleanedSlotList(cdef));
    ret += generateFunctions("    SIGNAL", signalList);
    ret += "}\n";
    return ret;
}

static QVector<PODAttribute> propertyList2PODAttributes(const QVector<PropertyDef> &list)
{
    QVector<PODAttribute> ret;
    for (const PropertyDef &prop : list)
        ret.push_back(PODAttribute(_(prop.type), _(prop.name)));
    return ret;
}

QVector<ASTProperty> propertyList2AstProperties(const QVector<PropertyDef> &list)
{
    QVector<ASTProperty> ret;
    for (const PropertyDef &property : list) {
        if (property.notifyId == -1 && !property.constant) {
            qWarning() << "Skipping property" << property.name << "because is non-notifiable & non-constant";
            continue; // skip non-notifiable properties
        }
        ASTProperty prop;
        prop.name = _(property.name);
        prop.type = _(property.type);
        prop.modifier = property.constant
                        ? ASTProperty::Constant
                        : property.write.isEmpty() && !property.read.isEmpty()
                          ? ASTProperty::ReadOnly
                          : ASTProperty::ReadWrite;
        ret.push_back(prop);
    }
    return ret;
}

QVector<ASTFunction> functionList2AstFunctionList(const QVector<FunctionDef> &list)
{
    QVector<ASTFunction> ret;
    for (const FunctionDef &fdef : list) {
        if (fdef.access != FunctionDef::Public)
            continue;

        ASTFunction func;
        func.name = _(fdef.name);
        func.returnType = _(fdef.type.name);
        for (const ArgumentDef &arg : fdef.arguments)
            func.params.push_back(ASTDeclaration(_(arg.type.name), _(arg.name)));
        ret.push_back(func);
    }
    return ret;
}

AST classList2AST(const QVector<ClassDef> &classList)
{
    AST ret;
    for (const ClassDef &cdef : classList) {
        const auto signalList = cleanedSignalList(cdef);
        if (signalList.isEmpty() && cdef.slotList.isEmpty()) {
            POD pod;
            pod.name = _(cdef.classname);
            pod.attributes = propertyList2PODAttributes(cdef.propertyList);
            ret.pods.push_back(pod);
        } else {
            ASTClass cl(_(cdef.classname));
            cl.properties = propertyList2AstProperties(cdef.propertyList);
            cl.signalsList = functionList2AstFunctionList(signalList);
            cl.slotsList = functionList2AstFunctionList(cleanedSlotList(cdef));
            ret.classes.push_back(cl);
        }
    }
    return ret;
}

QT_END_NAMESPACE
