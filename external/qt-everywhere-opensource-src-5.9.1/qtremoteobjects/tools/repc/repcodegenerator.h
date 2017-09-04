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

#ifndef REPCODEGENERATOR_H
#define REPCODEGENERATOR_H

#include <QSet>
#include <QString>
#include <QVector>

QT_BEGIN_NAMESPACE
struct AST;
struct ASTClass;
struct POD;
struct ASTEnum;

class QIODevice;
class QStringList;
class QTextStream;

class RepCodeGenerator
{
public:
    enum Mode
    {
        REPLICA,
        SOURCE,
        SIMPLE_SOURCE,
        MERGED
    };

    explicit RepCodeGenerator(QIODevice *outputDevice);

    void generate(const AST &ast, Mode mode, QString fileName);

    QByteArray classSignature(const ASTClass &ac);
private:
    void generateHeader(Mode mode, QTextStream &out, const AST &ast);
    QString generateMetaTypeRegistration(const QSet<QString> &metaTypes);
    QString generateMetaTypeRegistrationForEnums(const QVector<QString> &enums);
    void generateStreamOperatorsForEnums(QTextStream &out, const QVector<QString> &enums);

    void generatePOD(QTextStream &out, const POD &pod);
    void generateENUMs(QTextStream &out, const QVector<ASTEnum> &enums, const QString &className);
    void generateDeclarationsForEnums(QTextStream &out, const QVector<ASTEnum> &enums, bool generateQENUM=true);
    void generateStreamOperatorsForEnums(QTextStream &out, const QVector<ASTEnum> &enums, const QString &className);
    void generateConversionFunctionsForEnums(QTextStream &out, const QVector<ASTEnum> &enums);
    void generateENUM(QTextStream &out, const ASTEnum &en);
    QString formatQPropertyDeclarations(const POD &pod);
    QString formatConstructors(const POD &pod);
    QString formatPropertyGettersAndSetters(const POD &pod);
    QString formatSignals(const POD &pod);
    QString formatDataMembers(const POD &pod);
    QString formatMarshallingOperators(const POD &pod);

    void generateClass(Mode mode, QTextStream &out, const ASTClass &astClasses, const QString &metaTypeRegistrationCode);
    void generateSourceAPI(QTextStream &out, const ASTClass &astClass);

private:
    QIODevice *m_outputDevice;
    QHash<QString, QByteArray> m_globalEnumsPODs;
};

QT_END_NAMESPACE

#endif
