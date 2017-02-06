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

#include "scxmlcppdumper.h"
#include "generator.h"

#include <QtScxml/private/qscxmlexecutablecontent_p.h>

#include <QtCore/qfileinfo.h>
#include <QtCore/qbuffer.h>
#include <QtCore/qfile.h>
#include <QtCore/qresource.h>

#include <algorithm>
#include <functional>

QT_BEGIN_NAMESPACE

using namespace QScxmlInternal;

namespace {

static const QString doNotEditComment = QString::fromLatin1(
            "//\n"
            "// Statemachine code from reading SCXML file '%1'\n"
            "// Created by: The Qt SCXML Compiler version %2 (Qt %3)\n"
            "// WARNING! All changes made in this file will be lost!\n"
            "//\n"
            );

static const QString revisionCheck = QString::fromLatin1(
            "#if !defined(Q_QSCXMLC_OUTPUT_REVISION)\n"
            "#error \"The header file '%1' doesn't include <qscxmltabledata.h>.\"\n"
            "#elif Q_QSCXMLC_OUTPUT_REVISION != %2\n"
            "#error \"This file was generated using the qscxmlc from %3. It\"\n"
            "#error \"cannot be used with the include files from this version of Qt.\"\n"
            "#error \"(The qscxmlc has changed too much.)\"\n"
            "#endif\n"
            );

QString cEscape(const QString &str)
{
    QString res;
    int lastI = 0;
    for (int i = 0; i < str.length(); ++i) {
        QChar c = str.at(i);
        if (c < QLatin1Char(' ') || c == QLatin1Char('\\') || c == QLatin1Char('\"')) {
            res.append(str.mid(lastI, i - lastI));
            lastI = i + 1;
            if (c == QLatin1Char('\\')) {
                res.append(QLatin1String("\\\\"));
            } else if (c == QLatin1Char('\"')) {
                res.append(QLatin1String("\\\""));
            } else if (c == QLatin1Char('\n')) {
                res.append(QLatin1String("\\n"));
            } else if (c == QLatin1Char('\r')) {
                res.append(QLatin1String("\\r"));
            } else {
                char buf[6];
                ushort cc = c.unicode();
                buf[0] = '\\';
                buf[1] = 'u';
                for (int i = 0; i < 4; ++i) {
                    ushort ccc = cc & 0xF;
                    buf[5 - i] = ccc <= 9 ? '0' + ccc : 'a' + ccc - 10;
                    cc >>= 4;
                }
                res.append(QLatin1String(&buf[0], 6));
            }
        }
    }
    if (lastI != 0) {
        res.append(str.mid(lastI));
        return res;
    }
    return str;
}

typedef QHash<QString, QString> Replacements;
static void genTemplate(QTextStream &out, const QString &filename, const Replacements &replacements)
{
    QResource file(filename);
    if (!file.isValid()) {
        qFatal("Unable to open template '%s'", qPrintable(filename));
    }
    QByteArray data;
    if (file.isCompressed() && file.size()) {
        data = qUncompress(file.data(), int(file.size()));
    } else {
        data = QByteArray::fromRawData(reinterpret_cast<const char *>(file.data()),
                                       int(file.size()));
    }
    const QString t = QString::fromLatin1(data);
    data.clear();

    int start = 0;
    for (int openIdx = t.indexOf(QStringLiteral("${"), start); openIdx >= 0; openIdx =
         t.indexOf(QStringLiteral("${"), start)) {
        out << t.midRef(start, openIdx - start);
        openIdx += 2;
        const int closeIdx = t.indexOf(QLatin1Char('}'), openIdx);
        Q_ASSERT(closeIdx >= openIdx);
        QString key = t.mid(openIdx, closeIdx - openIdx);
        if (!replacements.contains(key)) {
            qFatal("Replacing '%s' failed: no replacement found", qPrintable(key));
        }
        out << replacements.value(key);
        start = closeIdx + 1;
    }
    out << t.midRef(start);
}

static const char *headerStart =
        "#include <QScxmlStateMachine>\n"
        "#include <QString>\n"
        "#include <QVariant>\n"
        "\n";

using namespace DocumentModel;

QString createContainer(const QStringList &elements)
{
    QString result;
    if (elements.isEmpty()) {
        result += QStringLiteral("{}");
    } else {
        result += QStringLiteral("{ ") + elements.join(QStringLiteral(", ")) + QStringLiteral(" }");
    }
    return result;
}

static void generateList(QString &out, std::function<QString(int)> next)
{
    const int maxLineLength = 80;
    QString line;
    for (int i = 0; ; ++i) {
        const QString nr = next(i);
        if (nr.isNull())
            break;

        if (i != 0)
            line += QLatin1Char(',');

        if (line.length() + nr.length() + 1 > maxLineLength) {
            out += line + QLatin1Char('\n');
            line.clear();
        } else if (i != 0) {
            line += QLatin1Char(' ');
        }
        line += nr;
    }
    if (!line.isEmpty())
        out += line;
}

void generateTables(const GeneratedTableData &td, Replacements &replacements)
{
    { // instructions
        auto instr = td.theInstructions;
        QString out;
        generateList(out, [&instr](int idx) -> QString {
            if (instr.isEmpty() && idx == 0) // prevent generation of empty array
                return QStringLiteral("-1");
            if (idx < instr.size())
                return QString::number(instr.at(idx));
            else
                return QString();
        });
        replacements[QStringLiteral("theInstructions")] = out;
    }

    { // dataIds
        auto dataIds = td.theDataNameIds;
        QString out;
        generateList(out, [&dataIds](int idx) -> QString {
            if (dataIds.size() == 0 && idx == 0) // prevent generation of empty array
                return QStringLiteral("-1");
            if (idx < dataIds.size())
                return QString::number(dataIds[idx]);
            else
                return QString();
        });
        replacements[QStringLiteral("dataNameCount")] = QString::number(dataIds.size());
        replacements[QStringLiteral("dataIds")] = out;
    }

    { // evaluators
        auto evaluators = td.theEvaluators;
        QString out;
        generateList(out, [&evaluators](int idx) -> QString {
            if (evaluators.isEmpty() && idx == 0) // prevent generation of empty array
                return QStringLiteral("{ -1, -1 }");
            if (idx >= evaluators.size())
                return QString();

            const auto eval = evaluators.at(idx);
            return QStringLiteral("{ %1, %2 }").arg(eval.expr).arg(eval.context);
        });
        replacements[QStringLiteral("evaluatorCount")] = QString::number(evaluators.size());
        replacements[QStringLiteral("evaluators")] = out;
    }

    { // assignments
        auto assignments = td.theAssignments;
        QString out;
        generateList(out, [&assignments](int idx) -> QString {
            if (assignments.isEmpty() && idx == 0) // prevent generation of empty array
                return QStringLiteral("{ -1, -1, -1 }");
            if (idx >= assignments.size())
                return QString();

            auto assignment = assignments.at(idx);
            return QStringLiteral("{ %1, %2, %3 }")
                    .arg(assignment.dest).arg(assignment.expr).arg(assignment.context);
        });
        replacements[QStringLiteral("assignmentCount")] = QString::number(assignments.size());
        replacements[QStringLiteral("assignments")] = out;
    }

    { // foreaches
        auto foreaches = td.theForeaches;
        QString out;
        generateList(out, [&foreaches](int idx) -> QString {
            if (foreaches.isEmpty() && idx == 0) // prevent generation of empty array
                return QStringLiteral("{ -1, -1, -1, -1 }");
            if (idx >= foreaches.size())
                return QString();

            auto foreachItem = foreaches.at(idx);
            return QStringLiteral("{ %1, %2, %3, %4 }").arg(foreachItem.array).arg(foreachItem.item)
                    .arg(foreachItem.index).arg(foreachItem.context);
        });
        replacements[QStringLiteral("foreachCount")] = QString::number(foreaches.size());
        replacements[QStringLiteral("foreaches")] = out;
    }

    { // strings
        QString out;
        auto strings = td.theStrings;
        if (strings.isEmpty()) // prevent generation of empty array
            strings.append(QStringLiteral(""));
        int ucharCount = 0;
        generateList(out, [&ucharCount, &strings](int idx) -> QString {
            if (idx >= strings.size())
                return QString();

            const int length = strings.at(idx).size();
            const QString str = QStringLiteral("STR_LIT(%1, %2, %3)").arg(
                        QString::number(idx), QString::number(ucharCount), QString::number(length));
            ucharCount += length + 1;
            return str;
        });
        replacements[QStringLiteral("stringCount")] = QString::number(strings.size());
        replacements[QStringLiteral("strLits")] = out;

        out.clear();
        for (int i = 0, ei = strings.size(); i < ei; ++i) {
            const QString &string  = strings.at(i);
            QString result;
            if (i != 0)
                result += QLatin1Char('\n');
            for (int charPos = 0, eCharPos = string.size(); charPos < eCharPos; ++charPos) {
                result.append(QStringLiteral("0x%1,")
                              .arg(QString::number(string.at(charPos).unicode(), 16)));
            }
            result.append(QStringLiteral("0%1 // %2: %3")
                          .arg(QLatin1String(i < ei - 1 ? "," : ""), QString::number(i),
                               cEscape(string)));
            out += result;
        }
        replacements[QStringLiteral("uniLits")] = out;
        replacements[QStringLiteral("stringdataSize")] = QString::number(ucharCount + 1);
    }
}

void generateCppDataModelEvaluators(const GeneratedTableData::DataModelInfo &info,
                                    Replacements &replacements)
{
    const QString switchStart = QStringLiteral("    switch (id) {\n");
    const QString switchEnd = QStringLiteral("    default: break;\n    }");
    const QString unusedId = QStringLiteral("    Q_UNUSED(id);");
    QString stringEvals;
    if (!info.stringEvaluators.isEmpty()) {
        stringEvals += switchStart;
        for (auto it = info.stringEvaluators.constBegin(), eit = info.stringEvaluators.constEnd();
             it != eit; ++it) {
            stringEvals += QStringLiteral("    case %1:\n").arg(it.key());
            stringEvals += QStringLiteral("        return [this]()->QString{ return %1; }();\n")
                    .arg(it.value());
        }
        stringEvals += switchEnd;
    } else {
        stringEvals += unusedId;
    }
    replacements[QStringLiteral("evaluateToStringCases")] = stringEvals;

    QString boolEvals;
    if (!info.boolEvaluators.isEmpty()) {
        boolEvals += switchStart;
        for (auto it = info.boolEvaluators.constBegin(), eit = info.boolEvaluators.constEnd();
             it != eit; ++it) {
            boolEvals += QStringLiteral("    case %1:\n").arg(it.key());
            boolEvals += QStringLiteral("        return [this]()->bool{ return %1; }();\n")
                    .arg(it.value());
        }
        boolEvals += switchEnd;
    } else {
        boolEvals += unusedId;
    }
    replacements[QStringLiteral("evaluateToBoolCases")] = boolEvals;

    QString variantEvals;
    if (!info.variantEvaluators.isEmpty()) {
        variantEvals += switchStart;
        for (auto it = info.variantEvaluators.constBegin(), eit = info.variantEvaluators.constEnd();
             it != eit; ++it) {
            variantEvals += QStringLiteral("    case %1:\n").arg(it.key());
            variantEvals += QStringLiteral("        return [this]()->QVariant{ return %1; }();\n")
                    .arg(it.value());
        }
        variantEvals += switchEnd;
    } else {
        variantEvals += unusedId;
    }
    replacements[QStringLiteral("evaluateToVariantCases")] = variantEvals;

    QString voidEvals;
    if (!info.voidEvaluators.isEmpty()) {
        voidEvals = switchStart;
        for (auto it = info.voidEvaluators.constBegin(), eit = info.voidEvaluators.constEnd();
             it != eit; ++it) {
            voidEvals += QStringLiteral("    case %1:\n").arg(it.key());
            voidEvals += QStringLiteral("        [this]()->void{ %1 }();\n").arg(it.value());
            voidEvals += QStringLiteral("        return;\n");
        }
        voidEvals += switchEnd;
    } else {
        voidEvals += unusedId;
    }
    replacements[QStringLiteral("evaluateToVoidCases")] = voidEvals;
}

int createFactoryId(QStringList &factories, const QString &className,
                    const QString &namespacePrefix,
                    const QScxmlExecutableContent::InvokeInfo &invokeInfo,
                    const QVector<QScxmlExecutableContent::StringId> &namelist,
                    const QVector<QScxmlExecutableContent::ParameterInfo> &parameters)
{
    const int idx = factories.size();

    QString line = QStringLiteral("case %1: return new ").arg(QString::number(idx));
    if (invokeInfo.expr == QScxmlExecutableContent::NoInstruction) {
        line += QStringLiteral("QScxmlStaticScxmlServiceFactory(&%1::%2::staticMetaObject,")
                .arg(namespacePrefix, className);
    } else {
        line += QStringLiteral("QScxmlDynamicScxmlServiceFactory(");
    }
    line += QStringLiteral("invoke(%1, %2, %3, %4, %5, %6, %7), ")
            .arg(QString::number(invokeInfo.id),
                 QString::number(invokeInfo.prefix),
                 QString::number(invokeInfo.expr),
                 QString::number(invokeInfo.location),
                 QString::number(invokeInfo.context),
                 QString::number(invokeInfo.finalize))
            .arg(invokeInfo.autoforward ? QStringLiteral("true") : QStringLiteral("false"));
    {
        QStringList l;
        for (auto name : namelist) {
            l.append(QString::number(name));
        }
        line += QStringLiteral("%1, ").arg(createContainer(l));
    }
    {
        QStringList l;
        for (const auto &parameter : parameters) {
            l += QStringLiteral("param(%1, %2, %3)")
                    .arg(QString::number(parameter.name),
                         QString::number(parameter.expr),
                         QString::number(parameter.location));
        }
        line += QStringLiteral("%1);").arg(createContainer(l));
    }

    factories.append(line);
    return idx;
}
} // anonymous namespace

void CppDumper::dump(TranslationUnit *unit)
{
    Q_ASSERT(unit);
    Q_ASSERT(unit->mainDocument);

    m_translationUnit = unit;

    QString namespacePrefix;
    if (!m_translationUnit->namespaceName.isEmpty()) {
        namespacePrefix = QStringLiteral("::%1").arg(m_translationUnit->namespaceName);
    }

    QStringList classNames;
    QVector<GeneratedTableData> tables;
    QVector<GeneratedTableData::MetaDataInfo> metaDataInfos;
    QVector<GeneratedTableData::DataModelInfo> dataModelInfos;
    QVector<QStringList> factories;
    auto docs = m_translationUnit->allDocuments;
    tables.resize(docs.size());
    metaDataInfos.resize(tables.size());
    dataModelInfos.resize(tables.size());
    factories.resize(tables.size());
    auto classnameForDocument = m_translationUnit->classnameForDocument;

    for (int i = 0, ei = docs.size(); i != ei; ++i) {
        auto doc = docs.at(i);
        auto metaDataInfo = &metaDataInfos[i];
        GeneratedTableData::build(doc, &tables[i], metaDataInfo, &dataModelInfos[i],
                                  [this, &factories, i, &classnameForDocument, &namespacePrefix](
                const QScxmlExecutableContent::InvokeInfo &invokeInfo,
                const QVector<QScxmlExecutableContent::StringId> &names,
                const QVector<QScxmlExecutableContent::ParameterInfo> &parameters,
                const QSharedPointer<DocumentModel::ScxmlDocument> &content) -> int {
            QString className;
            if (invokeInfo.expr == QScxmlExecutableContent::NoInstruction) {
                className = mangleIdentifier(classnameForDocument.value(content.data()));
            }
            return createFactoryId(factories[i], className, namespacePrefix,
                                   invokeInfo, names, parameters);
        });
        classNames.append(mangleIdentifier(classnameForDocument.value(doc)));
    }

    const QString headerName = QFileInfo(m_translationUnit->outHFileName).fileName();
    const QString headerGuard = headerName.toUpper()
            .replace(QLatin1Char('.'), QLatin1Char('_'))
            .replace(QLatin1Char('-'), QLatin1Char('_'));
    const QStringList forwardDecls = classNames.mid(1);
    writeHeaderStart(headerGuard, forwardDecls);
    writeImplStart();

    for (int i = 0, ei = tables.size(); i != ei; ++i) {
        const GeneratedTableData &table = tables.at(i);
        DocumentModel::ScxmlDocument *doc = docs.at(i);
        writeClass(classNames.at(i), metaDataInfos.at(i));
        writeImplBody(table, classNames.at(i), doc, factories.at(i), metaDataInfos.at(i));

        if (doc->root->dataModel == DocumentModel::Scxml::CppDataModel) {
            Replacements r;
            r[QStringLiteral("datamodel")] = doc->root->cppDataModelClassName;
            generateCppDataModelEvaluators(dataModelInfos.at(i), r);
            genTemplate(cpp, QStringLiteral(":/cppdatamodel.t"), r);
        }
    }

    writeHeaderEnd(headerGuard, classNames);
    writeImplEnd();
}

void CppDumper::writeHeaderStart(const QString &headerGuard, const QStringList &forwardDecls)
{
    h << doNotEditComment.arg(m_translationUnit->scxmlFileName,
                              QString::number(Q_QSCXMLC_OUTPUT_REVISION),
                              QString::fromLatin1(QT_VERSION_STR))
      << endl;

    h << QStringLiteral("#ifndef ") << headerGuard << endl
      << QStringLiteral("#define ") << headerGuard << endl
      << endl;
    h << l(headerStart);
    if (!m_translationUnit->namespaceName.isEmpty())
        h << l("namespace ") << m_translationUnit->namespaceName << l(" {") << endl << endl;

    if (!forwardDecls.isEmpty()) {
        for (const QString &forwardDecl : forwardDecls)
            h << QStringLiteral("class %1;").arg(forwardDecl) << endl;
        h << endl;
    }
}

void CppDumper::writeClass(const QString &className, const GeneratedTableData::MetaDataInfo &info)
{
    Replacements r;
    r[QStringLiteral("classname")] = className;
    r[QStringLiteral("properties")] = generatePropertyDecls(info);
    genTemplate(h, QStringLiteral(":/decl.t"), r);
}

void CppDumper::writeHeaderEnd(const QString &headerGuard, const QStringList &metatypeDecls)
{
    QString ns;
    if (!m_translationUnit->namespaceName.isEmpty()) {
        h << QStringLiteral("} // %1 namespace ").arg(m_translationUnit->namespaceName) << endl
          << endl;
        ns = QStringLiteral("::%1").arg(m_translationUnit->namespaceName);
    }

    for (const QString &name : metatypeDecls) {
        h << QStringLiteral("Q_DECLARE_METATYPE(%1::%2*)").arg(ns, name) << endl;
    }
    h << endl;

    h << QStringLiteral("#endif // ") << headerGuard << endl;
}

void CppDumper::writeImplStart()
{
    cpp << doNotEditComment.arg(m_translationUnit->scxmlFileName,
                                QString::number(Q_QSCXMLC_OUTPUT_REVISION),
                                l(QT_VERSION_STR))
        << endl;

    QStringList includes;
    for (DocumentModel::ScxmlDocument *doc : qAsConst(m_translationUnit->allDocuments)) {
        switch (doc->root->dataModel) {
        case DocumentModel::Scxml::NullDataModel:
            includes += l("QScxmlNullDataModel");
            break;
        case DocumentModel::Scxml::JSDataModel:
            includes += l("QScxmlEcmaScriptDataModel");
            break;
        case DocumentModel::Scxml::CppDataModel:
            includes += doc->root->cppDataModelHeaderName;
            break;
        }

    }
    includes.sort();
    includes.removeDuplicates();

    QString headerName = QFileInfo(m_translationUnit->outHFileName).fileName();
    cpp << l("#include \"") << headerName << l("\"") << endl;
    cpp << endl
        << QStringLiteral("#include <qscxmlinvokableservice.h>") << endl
        << QStringLiteral("#include <qscxmltabledata.h>") << endl;
    for (const QString &inc : qAsConst(includes)) {
        cpp << l("#include <") << inc << l(">") << endl;
    }
    cpp << endl
        << revisionCheck.arg(m_translationUnit->scxmlFileName,
                             QString::number(Q_QSCXMLC_OUTPUT_REVISION),
                             QString::fromLatin1(QT_VERSION_STR))
        << endl;
    if (!m_translationUnit->namespaceName.isEmpty())
        cpp << l("namespace ") << m_translationUnit->namespaceName << l(" {") << endl << endl;
}

void CppDumper::writeImplBody(const GeneratedTableData &table,
                              const QString &className,
                              DocumentModel::ScxmlDocument *doc,
                              const QStringList &factory,
                              const GeneratedTableData::MetaDataInfo &info)
{
    QString dataModelField, dataModelInitialization;
    switch (doc->root->dataModel) {
    case DocumentModel::Scxml::NullDataModel:
        dataModelField = l("QScxmlNullDataModel dataModel;");
        dataModelInitialization = l("stateMachine.setDataModel(&dataModel);");
        break;
    case DocumentModel::Scxml::JSDataModel:
        dataModelField = l("QScxmlEcmaScriptDataModel dataModel;");
        dataModelInitialization = l("stateMachine.setDataModel(&dataModel);");
        break;
    case DocumentModel::Scxml::CppDataModel:
        dataModelField = QStringLiteral("// Data model %1 is set from outside.").arg(
                    doc->root->cppDataModelClassName);
        dataModelInitialization = dataModelField;
        break;
    }

    QString name;
    if (table.theName == -1) {
        name = QStringLiteral("QString()");
    } else {
        name = QStringLiteral("string(%1)").arg(table.theName);
    }

    QString serviceFactories;
    if (factory.isEmpty()) {
        serviceFactories = QStringLiteral("    Q_UNUSED(id);\n    Q_UNREACHABLE();");
    } else {
        serviceFactories = QStringLiteral("    switch (id) {\n        ")
                + factory.join(QStringLiteral("\n        "))
                + QStringLiteral("\n        default: Q_UNREACHABLE();\n    }");
    }


    Replacements r;
    r[QStringLiteral("classname")] = className;
    r[QStringLiteral("name")] = name;
    r[QStringLiteral("initialSetup")] = QString::number(table.initialSetup());
    generateTables(table, r);
    r[QStringLiteral("dataModelField")] = dataModelField;
    r[QStringLiteral("dataModelInitialization")] = dataModelInitialization;
    r[QStringLiteral("theStateMachineTable")] =
            GeneratedTableData::toString(table.stateMachineTable());
    r[QStringLiteral("metaObject")] = generateMetaObject(className, info);
    r[QStringLiteral("serviceFactories")] = serviceFactories;
    genTemplate(cpp, QStringLiteral(":/data.t"), r);
}

void CppDumper::writeImplEnd()
{
    if (!m_translationUnit->namespaceName.isEmpty()) {
        cpp << endl
            << QStringLiteral("} // %1 namespace").arg(m_translationUnit->namespaceName) << endl;
    }
}

/*!
 * \internal
 * Mangles \a str to be a unique C++ identifier. Characters that are invalid for C++ identifiers
 * are replaced by the pattern \c _0x<hex>_ where <hex> is the hexadecimal unicode
 * representation of the character. As identifiers with leading underscores followed by either
 * another underscore or a capital letter are reserved in C++, we also escape those, by escaping
 * the first underscore, using the above method.
 *
 * We keep track of all identifiers we have used so far and if we find two different names that
 * map to the same mangled identifier by the above method, we append underscores to the new one
 * until the result is unique.
 *
 * \note
 * Although C++11 allows for non-ascii (unicode) characters to be used in identifiers,
 * many compilers forgot to read the spec and do not implement this. Some also do not
 * implement C99 identifiers, because that is \e {at the implementation's discretion}. So,
 * we are stuck with plain old boring identifiers.
 */
QString CppDumper::mangleIdentifier(const QString &str)
{
    auto isNonDigit = [](QChar c) -> bool {
        return (c >= QLatin1Char('a') && c <= QLatin1Char('z')) ||
                (c >= QLatin1Char('A') && c <= QLatin1Char('Z')) ||
                c == QLatin1Char('_');
    };

    Q_ASSERT(!str.isEmpty());

    QString mangled;
    mangled.reserve(str.size());

    int i = 0;
    if (str.startsWith(QLatin1Char('_')) && str.size() > 1) {
        QChar ch = str.at(1);
        if (ch == QLatin1Char('_')
                || (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'))) {
            mangled += QLatin1String("_0x5f_");
            ++i;
        }
    }

    for (int ei = str.length(); i != ei; ++i) {
        auto c = str.at(i).unicode();
        if ((c >= QLatin1Char('0') && c <= QLatin1Char('9')) || isNonDigit(c)) {
            mangled += c;
        } else {
            mangled += QLatin1String("_0x") + QString::number(c, 16) + QLatin1Char('_');
        }
    }

    while (true) {
        auto it = m_mangledToOriginal.constFind(mangled);
        if (it == m_mangledToOriginal.constEnd()) {
            m_mangledToOriginal.insert(mangled, str);
            break;
        } else if (it.value() == str) {
            break;
        }
        mangled += QStringLiteral("_"); // append underscores until we get a unique name
    }

    return mangled;
}

QString CppDumper::generatePropertyDecls(const GeneratedTableData::MetaDataInfo &info)
{
    QString decls;

    for (const QString &stateName : info.stateNames) {
        if (!stateName.isEmpty())
            decls += QString::fromLatin1("    Q_PROPERTY(bool %1)\n").arg(stateName);
    }

    return decls;
}

QString CppDumper::generateMetaObject(const QString &className,
                                      const GeneratedTableData::MetaDataInfo &info)
{
    ClassDef classDef;
    classDef.classname = className.toUtf8();
    classDef.qualified = classDef.classname;
    classDef.superclassList << qMakePair(QByteArray("QScxmlStateMachine"), FunctionDef::Public);
    classDef.hasQObject = true;
    FunctionDef constructor;
    constructor.name = className.toUtf8();
    constructor.access = FunctionDef::Public;
    constructor.isInvokable = true;
    constructor.isConstructor = true;

    ArgumentDef arg;
    arg.type.name = "QObject *";
    arg.type.rawName = arg.type.name;
    arg.normalizedType = arg.type.name;
    arg.name = "parent";
    arg.typeNameForCast = arg.type.name + "*";
    constructor.arguments.append(arg);
    classDef.constructorList.append(constructor);

    // stateNames:
    int stateIdx = 0;
    for (const QString &stateName : info.stateNames) {
        if (stateName.isEmpty())
            continue;

        QByteArray mangledStateName = stateName.toUtf8();

        FunctionDef signal;
        signal.type.name = "void";
        signal.type.rawName = signal.type.name;
        signal.normalizedType = signal.type.name;
        signal.name = mangledStateName + "Changed";
        signal.access = FunctionDef::Public;
        signal.isSignal = true;
        signal.implementation = "QMetaObject::activate(_o, &staticMetaObject, %d, _a);";

        ArgumentDef arg;
        arg.type.name = "bool";
        arg.type.rawName = arg.type.name;
        arg.normalizedType = arg.type.name;
        arg.name = "active";
        arg.typeNameForCast = arg.type.name + "*";
        signal.arguments << arg;
        classDef.signalList << signal;

        ++classDef.notifyableProperties;
        PropertyDef prop;
        prop.name = stateName.toUtf8();
        prop.type = "bool";
        prop.read = "isActive(" + QByteArray::number(stateIdx++) + ")";
        prop.notify = mangledStateName + "Changed";
        prop.notifyId = classDef.signalList.size() - 1;
        prop.gspec = PropertyDef::ValueSpec;
        prop.scriptable = "true";
        classDef.propertyList << prop;
    }

    // sub-statemachines:
    QHash<QByteArray, QByteArray> knownQObjectClasses;
    knownQObjectClasses.insert(QByteArray("QScxmlStateMachine"), QByteArray());

    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    Generator(&classDef, QList<QByteArray>(), knownQObjectClasses,
              QHash<QByteArray, QByteArray>(), buf).generateCode();
    buf.close();
    return QString::fromUtf8(buf.buffer());
}

QT_END_NAMESPACE
