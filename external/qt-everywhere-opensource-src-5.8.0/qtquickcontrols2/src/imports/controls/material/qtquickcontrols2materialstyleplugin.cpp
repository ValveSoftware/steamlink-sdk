/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtQuickControls2/private/qquickstyleplugin_p.h>

#include "qquickmaterialstyle_p.h"
#include "qquickmaterialtheme_p.h"
#include "qquickmaterialprogressring_p.h"
#include "qquickmaterialprogressstrip_p.h"
#include "qquickmaterialripple_p.h"

#include <QtQuickControls2/private/qquickstyleselector_p.h>
#include <QtQuickControls2/private/qquickpaddedrectangle_p.h>
#include <QtQuickControls2/private/qquickcolorimageprovider_p.h>

static inline void initResources()
{
    Q_INIT_RESOURCE(qtquickcontrols2materialstyleplugin);
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtQuick_Controls_2_Material);
#endif
}

QT_BEGIN_NAMESPACE

class QtQuickControls2MaterialStylePlugin : public QQuickStylePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    QtQuickControls2MaterialStylePlugin(QObject *parent = nullptr);

    void registerTypes(const char *uri) override;
    void initializeEngine(QQmlEngine *engine, const char *uri) override;

    QString name() const override;
    QQuickProxyTheme *createTheme() const override;
};

QtQuickControls2MaterialStylePlugin::QtQuickControls2MaterialStylePlugin(QObject *parent) : QQuickStylePlugin(parent)
{
    initResources();
}

void QtQuickControls2MaterialStylePlugin::registerTypes(const char *uri)
{
    qmlRegisterUncreatableType<QQuickMaterialStyle>(uri, 2, 0, "Material", tr("Material is an attached property"));
    qmlRegisterRevision<QQuickMaterialStyle, 1>(uri, 2, 1);
}

void QtQuickControls2MaterialStylePlugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    QQuickStylePlugin::initializeEngine(engine, uri);

    engine->addImageProvider(name(), new QQuickColorImageProvider(QStringLiteral(":/qt-project.org/imports/QtQuick/Controls.2/Material/images")));

    QByteArray import = QByteArray(uri) + ".impl";
    qmlRegisterType<QQuickPaddedRectangle>(import, 2, 0, "PaddedRectangle");
    qmlRegisterRevision<QQuickPaddedRectangle, 1>(import, 2, 1);
    qmlRegisterType<QQuickMaterialProgressRing>(import, 2, 0, "ProgressRing");
    qmlRegisterType<QQuickMaterialProgressStrip>(import, 2, 0, "ProgressStrip");
    qmlRegisterType<QQuickMaterialRingAnimator>(import, 2, 0, "RingAnimator");
    qmlRegisterType<QQuickMaterialRipple>(import, 2, 0, "Ripple");
    qmlRegisterType<QQuickMaterialStripAnimator>(import, 2, 0, "StripAnimator");
    qmlRegisterType(typeUrl(QStringLiteral("BoxShadow.qml")), import, 2, 0, "BoxShadow");
    qmlRegisterType(typeUrl(QStringLiteral("CheckIndicator.qml")), import, 2, 0, "CheckIndicator");
    qmlRegisterType(typeUrl(QStringLiteral("ElevationEffect.qml")), import, 2, 0, "ElevationEffect");
    qmlRegisterType(typeUrl(QStringLiteral("RectangularGlow.qml")), import, 2, 0, "RectangularGlow");
    qmlRegisterType(typeUrl(QStringLiteral("SliderHandle.qml")), import, 2, 0, "SliderHandle");
    qmlRegisterType(typeUrl(QStringLiteral("SwitchIndicator.qml")), import, 2, 0, "SwitchIndicator");
}

QString QtQuickControls2MaterialStylePlugin::name() const
{
    return QStringLiteral("material");
}

QQuickProxyTheme *QtQuickControls2MaterialStylePlugin::createTheme() const
{
    return new QQuickMaterialTheme;
}

QT_END_NAMESPACE

#include "qtquickcontrols2materialstyleplugin.moc"
