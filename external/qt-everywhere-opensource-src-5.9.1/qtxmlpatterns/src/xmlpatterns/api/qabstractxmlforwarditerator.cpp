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

/*!
  \class QAbstractXmlForwardIterator
  \brief The QAbstractXmlForwardIterator class is a base class for forward iterators.
  \reentrant
  \since 4.4
  \ingroup xml-tools
  \inmodule QtXmlPatterns
  \internal

  This abstract base class is for creating iterators for
  traversing custom data structures modeled to look like XML.
  An item can be instantiated in QAbstractXmlForwardIterator if:
  \list

  \li It has a default constructor, a copy constructor, and an
  assignment operator, and

  \li It has an appropriate qIsForwardIteratorEnd() function.
  \endlist

   @ingroup Patternist_iterators
   @author Frans Englich <frans.englich@nokia.com>
 */

/*!
 \typedef QAbstractXmlForwardIterator::Ptr

 A smart pointer wrapping an instance of a QAbstractXmlForwardIterator subclass.
 */

/*!
 \typedef QAbstractXmlForwardIterator::List
 A QList containing QAbstractXmlForwardIterator::Ptr instances.
 */

/*!
 \typedef QAbstractXmlForwardIterator::Vector
 A QVector containing QAbstractXmlForwardIterator::Ptr instances.
 */

/*!
  \fn QAbstractXmlForwardIterator::QAbstractXmlForwardIterator()

  Default constructor.
 */

/*!
  \fn QAbstractXmlForwardIterator::~QAbstractXmlForwardIterator()

  Destructor.
 */

/*!
  \fn T QAbstractXmlForwardIterator::next() = 0;

 Returns the next item in the sequence, or
 a null object if the end has been reached.
 */

/*!
  \fn T QAbstractXmlForwardIterator::current() const = 0;

  Returns the current item in the sequence. If this function is called
  before the first call to next(), a null object is returned. If the
  end of the sequence has been reached, a null object is returned.
 */

/*!
  \fn qint64 QAbstractXmlForwardIterator::position() const = 0;

   Returns the current position in the sequence represented
   by \e this.

   The first position is 1, not 0. If next() hasn't been called, 0 is
   returned. If \e this has reached the end, -1 is returned.
 */

/*!
  \fn bool qIsForwardIteratorEnd(const T &unit)
  \since 4.4
  \relates QAbstractXmlForwardIterator

  The Callback QAbstractXmlForwardIterator uses for determining
  whether \a unit is the end of a sequence.

  If \a unit is a value that would signal the end of a sequence
  (typically a default constructed value), this function returns \c
  true, otherwise \c false.

  This implementation works for any type that has a boolean operator.
  For example, this function should work satisfactory for pointers.
 */

/*!
  \fn qint64 QAbstractXmlForwardIterator::count()
  \internal

   Determines the number of items this QAbstractXmlForwardIterator
   represents.

   Note that this function is not \c const. It modifies the
   QAbstractXmlForwardIterator. The reason for this is efficiency. If
   this QAbstractXmlForwardIterator must not be changed, get a copy()
   before performing the count.

   The default implementation simply calls next() until the end is
   reached. Hence, it may be of interest to override this function if
   the sub-class knows a better way of computing its count.

   The number of items in the sequence is returned.
 */

/*!
  \fn QAbstractXmlForwardIterator<T>::Ptr QAbstractXmlForwardIterator::toReversed();
  \internal

  Returns a reverse iterator for the sequence.

  This function may modify the iterator, it can be considered a
  function that evaluates this QAbstractXmlForwardIterator. It is not
  a \e getter, but potentially alters the iterator in the same way the
  next() function does. If this QAbstractXmlForwardIterator must not
  be modified, such that it can be used for evaluation with next(),
  use a copy().
 */

/*!
  \fn QList<T> QAbstractXmlForwardIterator<T>::toList();
  \internal

   Performs a copy of this QAbstractXmlForwardIterator(with copy()),
   and returns its items in a QList. Thus, this function acts as a
   conversion function, converting the sequence to a QList.

   This function may modify the iterator. It is not a \e getter, but
   potentially alters the iterator in the same way the next() function
   does. If this QAbstractXmlForwardIterator must not be modified,
   such that it can be used for evaluation with next(), use a copy().
 */

/*!
  \fn T QAbstractXmlForwardIterator::last();
  \internal

   Returns the item at the end of this QAbstractXmlForwardIterator.
   The default implementation calls next() until the end is reached.
 */

/*!
  \fn T QAbstractXmlForwardIterator::isEmpty();
  \internal
  Returns true if the sequence is empty.
 */

/*!
  \fn qint64 QAbstractXmlForwardIterator::sizeHint() const;
  \internal

  Gives a hint to the size of the contained sequence. The hint is
  assumed to be as close as possible to the actual size.

  If no sensible estimate can be computed, -1 should be returned.
 */

/*!
  \fn typename QAbstractXmlForwardIterator<T>::Ptr QAbstractXmlForwardIterator::copy() const;
  \internal

   Copies this QAbstractXmlForwardIterator and returns the copy.

   A copy and the original instance are completely independent of each
   other. Because evaluating an QAbstractXmlForwardIterator modifies
   it, one should always use a copy when an
   QAbstractXmlForwardIterator needs to be used several times.
 */

/*!
  \class QPatternist::ListIteratorPlatform
  \brief Helper class for ListIterator, and should only be instantiated through sub-classing.
  \reentrant
  \since 4.4
  \internal
  \ingroup xml-tools

   ListIteratorPlatform iterates an InputList with instances
   of InputType. For every item in it, it returns an item from it,
   that is converted to OutputType by calling a function on Derived
   that has the following signature:

   \snippet code/src_xmlpatterns_api_qabstractxmlforwarditerator.cpp 0

   TODO Document why this class doesn't duplicate ItemMappingIterator.
 */

/*!
  \fn QPatternist::ListIteratorPlatform::ListIteratorPlatform(const ListType &list);

  Constructs a ListIteratorPlatform that walks the given \a list.
 */

/*!
  \class QPatternist::ListIterator
  \brief Bridges values in Qt's QList container class into an QAbstractXmlForwardIterator.
  \reentrant
  \since 4.4
  \internal
  \ingroup xml-tools

   ListIterator takes a reference to a QList<T> instance and allows
   access to that list via its QAbstractXmlForwardIterator interface.
   ListIterator is parameterized with the type to iterate over, e.g.,
   Item or Expression::Ptr.

   ListIterator is used by the ExpressionSequence to create an
   iterator over its operands. The iterator will be passed to a
   MappingIterator.
 */

/*!
   \fn QPatternist::makeListIterator(const QList<T> &qList)
   \relates QPatternist::ListIterator

   An object generator for ListIterator.

   makeListIterator() is a convenience function to avoid specifying
   the full template instantiation for ListIterator.  Conceptually, it
   is identical to Qt's qMakePair().

 */
