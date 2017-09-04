/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#ifndef QQUICKWEBENGINESCRIPT_H
#define QQUICKWEBENGINESCRIPT_H

#include <QtCore/qobject.h>
#include <QtCore/qurl.h>
#include <QtWebEngine/qtwebengineglobal.h>

QT_BEGIN_NAMESPACE
class QQuickWebEngineScriptPrivate;
class QQuickWebEngineView;

class Q_WEBENGINE_EXPORT QQuickWebEngineScript : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged FINAL)
    Q_PROPERTY(QUrl sourceUrl READ sourceUrl WRITE setSourceUrl NOTIFY sourceUrlChanged FINAL)
    Q_PROPERTY(QString sourceCode READ sourceCode WRITE setSourceCode NOTIFY sourceCodeChanged FINAL)
    Q_PROPERTY(InjectionPoint injectionPoint READ injectionPoint WRITE setInjectionPoint NOTIFY injectionPointChanged FINAL)
    Q_PROPERTY(ScriptWorldId worldId READ worldId WRITE setWorldId NOTIFY worldIdChanged FINAL)
    Q_PROPERTY(bool runOnSubframes READ runOnSubframes WRITE setRunOnSubframes NOTIFY runOnSubframesChanged FINAL)


public:
    enum InjectionPoint {
        Deferred,
        DocumentReady,
        DocumentCreation
    };
    Q_ENUM(InjectionPoint)

    enum ScriptWorldId {
        MainWorld = 0,
        ApplicationWorld,
        UserWorld
    };
    Q_ENUM(ScriptWorldId)

    explicit QQuickWebEngineScript(QObject *parent = Q_NULLPTR);
    ~QQuickWebEngineScript();
    Q_INVOKABLE QString toString() const;

    QString name() const;
    QUrl sourceUrl() const;
    QString sourceCode() const;
    InjectionPoint injectionPoint() const;
    ScriptWorldId worldId() const;
    bool runOnSubframes() const;

    Q_INVOKABLE void setName(const QString &name);
    Q_INVOKABLE void setSourceUrl(const QUrl &url);
    Q_INVOKABLE void setSourceCode(const QString &code);
    Q_INVOKABLE void setInjectionPoint(InjectionPoint injectionPoint);
    Q_INVOKABLE void setWorldId(ScriptWorldId scriptWorldId);
    Q_INVOKABLE void setRunOnSubframes(bool on);

Q_SIGNALS:
    void nameChanged(const QString &name);
    void sourceUrlChanged(const QUrl &url);
    void sourceCodeChanged(const QString &code);
    void injectionPointChanged(InjectionPoint injectionPoint);
    void worldIdChanged(ScriptWorldId scriptWorldId);
    void runOnSubframesChanged(bool on);

protected:
    void timerEvent(QTimerEvent *e) override;

private:
    friend class QQuickWebEngineProfilePrivate;
    friend class QQuickWebEngineViewPrivate;
    Q_DECLARE_PRIVATE(QQuickWebEngineScript)
    QScopedPointer<QQuickWebEngineScriptPrivate> d_ptr;
};
QT_END_NAMESPACE

#endif // QQUICKWEBENGINESCRIPT_H
