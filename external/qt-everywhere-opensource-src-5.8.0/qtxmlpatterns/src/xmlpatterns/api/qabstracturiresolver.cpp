/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

