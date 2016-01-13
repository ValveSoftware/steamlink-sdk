/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include <QUrl>

#include "qabstracturiresolver.h"

QT_BEGIN_NAMESPACE

/*!
  \class QAbstractUriResolver
  \brief The QAbstractUriResolver class is a callback interface for resolving Uniform Resource Identifiers.
  \since 4.4
  \reentrant
  \ingroup xml-tools
  \inmodule QtXmlPatterns
  A Uniform Resource Identifier (URI) is a string that uniquely
  identifies a resource. URIs are versatile global identifiers. It is
  often useful to transform a URI that identifies something logical
  into a URI that locates something physical (a URL), or to simply map
  a URI to a different URI. QAbstractUriResolver::resolve() provides
  this functionality.

  For example, one could write a QAbstractUriResolver subclass that
  rewrites library ISBN number URIs as book title URLs, e.g.,
  \e{urn:isbn:0-345-33973-8} would be rewritten as
  \e{file:///books/returnOfTheKing.doc}. Or a QAbstractUriResolver
  subclass could be written for a web browser to let the web browser
  protect the user's private files by mapping incoming requests for
  them to null URIs.

  \sa {http://en.wikipedia.org/wiki/Uniform_Resource_Identifier}
*/

/*!
  Constructs a QAbstractUriResolver with the specified \a parent.
 */
QAbstractUriResolver::QAbstractUriResolver(QObject *parent) : QObject(parent)
{
}

/*!
  Destructor.
 */
QAbstractUriResolver::~QAbstractUriResolver()
{
}

/*!
  \fn QUrl QAbstractUriResolver::resolve(const QUrl &relative, const QUrl &baseURI) const

  Returns the \a relative URI resolved using the \a baseURI.

  The caller guarantees that both \a relative and \a baseURI are
  valid, and that \a baseURI is absolute. \a relative can be relative,
  absolute, or empty.

  The returned QUrl can be a default constructed QUrl. If it is not a
  default constructed QUrl, it will be absolute and valid. If a default
  constructed QUrl is returned, it means the \a relative URI was not
  accepted to be resolved.

  If the reimplemented resolve() function decides it has nothing to do
  about resolving the \a relative URI, it should simply return the \a
  relative URI resolved against the \a baseURI, i.e.:

  \snippet code/src_xmlpatterns_api_qabstracturiresolver.cpp 0

  \sa QUrl::isRelative(), QUrl::isValid()
 */

QT_END_NAMESPACE

