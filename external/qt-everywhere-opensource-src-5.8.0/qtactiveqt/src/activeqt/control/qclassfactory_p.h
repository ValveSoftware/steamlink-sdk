/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QCLASSFACTORY_P_H
#define QCLASSFACTORY_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <qcoreapplication.h>
#include <ocidl.h>

QT_BEGIN_NAMESPACE

// COM Factory class, mapping COM requests to ActiveQt requests.
// One instance of this class for each ActiveX the server can provide.
class QClassFactory : public IClassFactory2
{
public:
    QClassFactory(CLSID clsid);

    virtual ~QClassFactory();

    // IUnknown
    unsigned long WINAPI AddRef();
    unsigned long WINAPI Release();
    HRESULT WINAPI QueryInterface(REFIID iid, LPVOID *iface);

    HRESULT WINAPI CreateInstanceHelper(IUnknown *pUnkOuter, REFIID iid, void **ppObject);

    // IClassFactory
    HRESULT WINAPI CreateInstance(IUnknown *pUnkOuter, REFIID iid, void **ppObject);

    HRESULT WINAPI LockServer(BOOL fLock);
    // IClassFactory2
    HRESULT WINAPI RequestLicKey(DWORD, BSTR *pKey);

    HRESULT WINAPI GetLicInfo(LICINFO *pLicInfo);

    HRESULT WINAPI CreateInstanceLic(IUnknown *pUnkOuter, IUnknown * /* pUnkReserved */, REFIID iid, BSTR bKey, PVOID *ppObject);

    static void cleanupCreatedApplication(QCoreApplication &app);

    QString className;

protected:
    CRITICAL_SECTION refCountSection;
    LONG ref;
    bool licensed;
    QString classKey;
};

QT_END_NAMESPACE

#endif // QCLASSFACTORY_P_H
