/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

/*
  cppcodemarker.h
*/

#ifndef CPPCODEMARKER_H
#define CPPCODEMARKER_H

#include "codemarker.h"

QT_BEGIN_NAMESPACE

class CppCodeMarker : public CodeMarker
{
    Q_DECLARE_TR_FUNCTIONS(QDoc::CppCodeMarker)

public:
    CppCodeMarker();
    ~CppCodeMarker();

    virtual bool recognizeCode(const QString& code) Q_DECL_OVERRIDE;
    virtual bool recognizeExtension(const QString& ext) Q_DECL_OVERRIDE;
    virtual bool recognizeLanguage(const QString& lang) Q_DECL_OVERRIDE;
    virtual Atom::AtomType atomType() const Q_DECL_OVERRIDE;
    virtual QString markedUpCode(const QString& code,
                                 const Node *relative,
                                 const Location &location) Q_DECL_OVERRIDE;
    virtual QString markedUpSynopsis(const Node *node,
                                     const Node *relative,
                                     SynopsisStyle style) Q_DECL_OVERRIDE;
    virtual QString markedUpQmlItem(const Node *node, bool summary) Q_DECL_OVERRIDE;
    virtual QString markedUpName(const Node *node) Q_DECL_OVERRIDE;
    virtual QString markedUpFullName(const Node *node, const Node *relative) Q_DECL_OVERRIDE;
    virtual QString markedUpEnumValue(const QString &enumValue, const Node *relative) Q_DECL_OVERRIDE;
    virtual QString markedUpIncludes(const QStringList& includes) Q_DECL_OVERRIDE;
    virtual QString functionBeginRegExp(const QString& funcName) Q_DECL_OVERRIDE;
    virtual QString functionEndRegExp(const QString& funcName) Q_DECL_OVERRIDE;
    virtual QList<Section> sections(const Aggregate *innerNode,
                                    SynopsisStyle style,
                                    Status status) Q_DECL_OVERRIDE;
    virtual QList<Section> qmlSections(Aggregate* aggregate,
                                       SynopsisStyle style,
                                       Status status = Okay) Q_DECL_OVERRIDE;

private:
    QString addMarkUp(const QString& protectedCode,
                      const Node *relative,
                      const Location &location);
};

QT_END_NAMESPACE

#endif
