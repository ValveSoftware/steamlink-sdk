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
  plaincodemarker.h
*/

#ifndef PLAINCODEMARKER_H
#define PLAINCODEMARKER_H

#include "codemarker.h"

QT_BEGIN_NAMESPACE

class PlainCodeMarker : public CodeMarker
{
public:
    PlainCodeMarker();
    ~PlainCodeMarker();

    bool recognizeCode( const QString& code ) Q_DECL_OVERRIDE;
    bool recognizeExtension( const QString& ext ) Q_DECL_OVERRIDE;
    bool recognizeLanguage( const QString& lang ) Q_DECL_OVERRIDE;
    Atom::AtomType atomType() const Q_DECL_OVERRIDE;
    QString markedUpCode( const QString& code, const Node *relative, const Location &location ) Q_DECL_OVERRIDE;
    QString markedUpSynopsis( const Node *node, const Node *relative,
                              SynopsisStyle style ) Q_DECL_OVERRIDE;
    QString markedUpName( const Node *node ) Q_DECL_OVERRIDE;
    QString markedUpFullName( const Node *node, const Node *relative ) Q_DECL_OVERRIDE;
    QString markedUpEnumValue(const QString &enumValue, const Node *relative) Q_DECL_OVERRIDE;
    QString markedUpIncludes( const QStringList& includes ) Q_DECL_OVERRIDE;
    QString functionBeginRegExp( const QString& funcName ) Q_DECL_OVERRIDE;
    QString functionEndRegExp( const QString& funcName ) Q_DECL_OVERRIDE;
    QList<Section> sections(const Aggregate *innerNode, SynopsisStyle style, Status status) Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

#endif
