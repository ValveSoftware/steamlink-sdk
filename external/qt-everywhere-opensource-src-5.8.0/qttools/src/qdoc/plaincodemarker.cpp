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

#include "plaincodemarker.h"

QT_BEGIN_NAMESPACE

PlainCodeMarker::PlainCodeMarker()
{
}

PlainCodeMarker::~PlainCodeMarker()
{
}

bool PlainCodeMarker::recognizeCode( const QString& /* code */ )
{
    return true;
}

bool PlainCodeMarker::recognizeExtension( const QString& /* ext */ )
{
    return true;
}

bool PlainCodeMarker::recognizeLanguage( const QString& /* lang */ )
{
    return false;
}

Atom::AtomType PlainCodeMarker::atomType() const
{
    return Atom::Code;
}

QString PlainCodeMarker::markedUpCode( const QString& code,
                                       const Node * /* relative */,
                                       const Location & /* location */ )
{
    return protect( code );
}

QString PlainCodeMarker::markedUpSynopsis( const Node * /* node */,
                                           const Node * /* relative */,
                                           SynopsisStyle /* style */ )
{
    return "foo";
}

QString PlainCodeMarker::markedUpName( const Node * /* node */ )
{
    return QString();
}

QString PlainCodeMarker::markedUpFullName( const Node * /* node */,
                                           const Node * /* relative */ )
{
    return QString();
}

QString PlainCodeMarker::markedUpEnumValue(const QString & /* enumValue */,
                                           const Node * /* relative */)
{
    return QString();
}

QString PlainCodeMarker::markedUpIncludes( const QStringList& /* includes */ )
{
    return QString();
}

QString PlainCodeMarker::functionBeginRegExp( const QString& /* funcName */ )
{
    return QString();
}

QString PlainCodeMarker::functionEndRegExp( const QString& /* funcName */ )
{
    return QString();
}

QList<Section> PlainCodeMarker::sections(const Aggregate * /* innerNode */,
                                         SynopsisStyle /* style */,
                                         Status /* status */)
{
    return QList<Section>();
}

QT_END_NAMESPACE
