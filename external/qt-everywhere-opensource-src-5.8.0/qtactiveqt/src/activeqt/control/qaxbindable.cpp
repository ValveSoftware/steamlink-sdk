/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the ActiveQt framework of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qaxbindable.h"

#include <qmetaobject.h>

#include <qt_windows.h> // for IUnknown
#include "../shared/qaxtypes.h"

QT_BEGIN_NAMESPACE

/*!
    \class QAxBindable
    \brief The QAxBindable class provides an interface between a
    QWidget and an ActiveX client.

    \inmodule QAxServer

    The functions provided by this class allow an ActiveX control to
    communicate property changes to a client application. Inherit
    your control class from both QWidget (directly or indirectly) and
    this class to get access to this class's functions. The
    \l{moc}{meta-object compiler} requires you to inherit from
    QWidget first.

    \snippet src_activeqt_control_qaxbindable.cpp 0

    When implementing the property write function, use
    requestPropertyChange() to get permission from the ActiveX client
    application to change this property. When the property changes,
    call propertyChanged() to notify the ActiveX client application
    about the change. If a fatal error occurs in the control, use the
    static reportError() function to notify the client.

    Use the interface returned by clientSite() to call the ActiveX
    client. To implement additional COM interfaces in your ActiveX
    control, reimplement createAggregate() to return a new object of a
    QAxAggregated subclass.

    The ActiveQt \l{activeqt/opengl}{OpenGL} example shows how to use
    QAxBindable to implement additional COM interfaces.

    \sa QAxAggregated, QAxFactory, {ActiveQt Framework}
*/

/*!
    Constructs an empty QAxBindable object.
*/
QAxBindable::QAxBindable()
:activex(0)
{
}

/*!
    Destroys the QAxBindable object.
*/
QAxBindable::~QAxBindable()
{
}

/*!
    Call this function to request permission to change the property
    \a property from the client that is hosting this ActiveX control.
    Returns true if the client allows the change; otherwise returns
    false.

    This function is usually called first in the write function for \a
    property, and writing is abandoned if the function returns false.

    \snippet src_activeqt_control_qaxbindable.cpp 1

    \sa propertyChanged()
*/
bool QAxBindable::requestPropertyChange(const char *property)
{
    if (!activex)
        return true;

    return activex->emitRequestPropertyChange(property);
}

/*!
    Call this function to notify the client that is hosting this
    ActiveX control that the property \a property has been changed.

    This function is usually called at the end of the property's write
    function.

    \sa requestPropertyChange()
*/
void QAxBindable::propertyChanged(const char *property)
{
    if (!activex)
        return;

    activex->emitPropertyChanged(property);
}

/*!
    Returns a pointer to the client site interface for this ActiveX object,
    or null if no client site has been set.

    Call \c QueryInterface() on the returned interface to get the
    interface you want to call.
*/
IUnknown *QAxBindable::clientSite() const
{
    if (!activex)
        return 0;

    return activex->clientSite();
}

/*!
    Reimplement this function when you want to implement additional
    COM interfaces in the ActiveX control, or when you want to provide
    alternative implementations of COM interfaces. Return a new object
    of a QAxAggregated subclass.

    The default implementation returns the null pointer.
*/
QAxAggregated *QAxBindable::createAggregate()
{
    return 0;
}

/*!
    Reports an error to the client application. \a code is a
    control-defined error code. \a desc is a human-readable description
    of the error intended for the application user. \a src is the name
    of the source for the error, typically the ActiveX server name. \a
    context can be the location of a help file with more information
    about the error. If \a context ends with a number in brackets,
    e.g. [12], this number will be interpreted as the context ID in
    the help file.
*/
void QAxBindable::reportError(int code, const QString &src, const QString &desc, const QString &context)
{
    if (!activex)
        return;

    activex->reportError(code, src, desc, context);
}

/*!
    \since 4.1

    If the COM object supports a MIME type then this function is called
    to initialize the COM object from the data \a source in \a format.
    You have to open \a source for reading before you can read from it.

    Returns true to indicate success. If the function returns false,
    then ActiveQt will process the data by setting the properties
    through the meta object system.

    If you reimplement this function you also have to implement
    writeData().  The default implementation does nothing and returns
    false.

    \warning ActiveX controls embedded in HTML can use either the
    \c type and \c data attribute of the \c object tag to read data,
    or use a list of \c param tags to initialize properties. If
    \c param tags are used, then Internet Explorer will ignore the
    \c data attribute, and readData will not be called.

    \sa writeData()
*/
bool QAxBindable::readData(QIODevice *source, const QString &format)
{
    Q_UNUSED(source);
    Q_UNUSED(format);
    return false;
}

/*!
    \since 4.1

    If the COM object supports a MIME type then this function is called
    to store the COM object into \a sink.
    You have to open \a sink for writing before you can write to it.

    Returns true to indicate success. If the function returns false,
    then ActiveQt will serialize the object by storing the property
    values.

    If you reimplement this function you also have to implement
    readData(). The default implementation does nothing and returns
    false.

    \sa readData()
*/
bool QAxBindable::writeData(QIODevice *sink)
{
    Q_UNUSED(sink);
    return false;
}

QT_END_NAMESPACE
