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

#ifndef QAXWIDGET_H
#define QAXWIDGET_H

#include <ActiveQt/qaxbase.h>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class QAxHostWindow;
class QAxAggregated;

class QAxClientSite;
class QAxWidgetPrivate;

class QAxWidget : public QWidget, public QAxBase
{
    Q_OBJECT_FAKE
public:
    QObject* qObject() const Q_DECL_OVERRIDE { return const_cast<QAxWidget *>(this); }
    const char *className() const Q_DECL_OVERRIDE;

    explicit QAxWidget(QWidget* parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    explicit QAxWidget(const QString &c, QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    explicit QAxWidget(IUnknown *iface, QWidget *parent = Q_NULLPTR, Qt::WindowFlags f = Qt::WindowFlags());
    ~QAxWidget();

    void clear() Q_DECL_OVERRIDE;
    bool doVerb(const QString &verb);

    QSize sizeHint() const Q_DECL_OVERRIDE;
    QSize minimumSizeHint() const Q_DECL_OVERRIDE;

    virtual QAxAggregated *createAggregate();

protected:
    bool initialize(IUnknown **) Q_DECL_OVERRIDE;
    virtual bool createHostWindow(bool);
    bool createHostWindow(bool, const QByteArray&);

    void changeEvent(QEvent *e) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *) Q_DECL_OVERRIDE;

    virtual bool translateKeyEvent(int message, int keycode) const;

    void connectNotify(const QMetaMethod &signal) Q_DECL_OVERRIDE;
    const QMetaObject *fallbackMetaObject() const Q_DECL_OVERRIDE;
private:
    friend class QAxClientSite;
    QAxClientSite *container;

    QAxWidgetPrivate *d;
    const QMetaObject *parentMetaObject() const Q_DECL_OVERRIDE;
};

template <> inline QAxWidget *qobject_cast<QAxWidget*>(const QObject *o)
{
    void *result = o ? const_cast<QObject *>(o)->qt_metacast("QAxWidget") : Q_NULLPTR;
    return static_cast<QAxWidget *>(result);
}

template <> inline QAxWidget *qobject_cast<QAxWidget*>(QObject *o)
{
    void *result = o ? o->qt_metacast("QAxWidget") : Q_NULLPTR;
    return static_cast<QAxWidget *>(result);
}

QT_END_NAMESPACE

#endif // QAXWIDGET_H
