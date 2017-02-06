/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "qdesigner.h"
#include "qdesigner_settings.h"
#include "qdesigner_toolwindow.h"
#include "qdesigner_workbench.h"

#include <QtDesigner/QDesignerSettingsInterface>

#include <abstractformeditor.h>
#include <qdesigner_utils_p.h>
#include <previewmanager_p.h>

#include <QtCore/QVariant>
#include <QtCore/QDir>

#include <QtWidgets/QDesktopWidget>
#include <QtWidgets/QStyle>
#include <QtWidgets/QListView>

#include <QtCore/qdebug.h>

enum { debugSettings = 0 };

QT_BEGIN_NAMESPACE

static const char *newFormShowKey = "newFormDialog/ShowOnStartup";

// Change the version whenever the arrangement changes significantly.
static const char *mainWindowStateKey = "MainWindowState45";
static const char *toolBarsStateKey = "ToolBarsState45";

static const char *backupOrgListKey = "backup/fileListOrg";
static const char *backupBakListKey = "backup/fileListBak";
static const char *recentFilesListKey = "recentFilesList";

QDesignerSettings::QDesignerSettings(QDesignerFormEditorInterface *core) :
    qdesigner_internal::QDesignerSharedSettings(core)
{
}

void QDesignerSettings::setValue(const QString &key, const QVariant &value)
{
    settings()->setValue(key, value);
}

QVariant QDesignerSettings::value(const QString &key, const QVariant &defaultValue) const
{
    return settings()->value(key, defaultValue);
}

static inline QChar modeChar(UIMode mode)
{
    return QLatin1Char(static_cast<char>(mode) + '0');
}

void QDesignerSettings::saveGeometryFor(const QWidget *w)
{
    Q_ASSERT(w && !w->objectName().isEmpty());
    QDesignerSettingsInterface *s = settings();
    const bool visible = w->isVisible();
    if (debugSettings)
        qDebug() << Q_FUNC_INFO << w << "visible=" << visible;
    s->beginGroup(w->objectName());
    s->setValue(QStringLiteral("visible"), visible);
    s->setValue(QStringLiteral("geometry"), w->saveGeometry());
    s->endGroup();
}

void QDesignerSettings::restoreGeometry(QWidget *w, QRect fallBack) const
{
    Q_ASSERT(w && !w->objectName().isEmpty());
    const QString key = w->objectName();
    const QByteArray ba(settings()->value(key + QStringLiteral("/geometry")).toByteArray());
    const bool visible = settings()->value(key + QStringLiteral("/visible"), true).toBool();

    if (debugSettings)
        qDebug() << Q_FUNC_INFO << w << fallBack << "visible=" << visible;
    if (ba.isEmpty()) {
        /// Apply default geometry, check for null and maximal size
        if (fallBack.isNull())
            fallBack = QRect(QPoint(0, 0), w->sizeHint());
        if (fallBack.size() == QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX)) {
            w->setWindowState(w->windowState() | Qt::WindowMaximized);
        } else {
            w->move(fallBack.topLeft());
            w->resize(fallBack.size());
        }
    } else {
        w->restoreGeometry(ba);
    }

    if (visible)
        w->show();
}

QStringList QDesignerSettings::recentFilesList() const
{
    return settings()->value(QLatin1String(recentFilesListKey)).toStringList();
}

void QDesignerSettings::setRecentFilesList(const QStringList &sl)
{
    settings()->setValue(QLatin1String(recentFilesListKey), sl);
}

void QDesignerSettings::setShowNewFormOnStartup(bool showIt)
{
    settings()->setValue(QLatin1String(newFormShowKey), showIt);
}

bool QDesignerSettings::showNewFormOnStartup() const
{
    return settings()->value(QLatin1String(newFormShowKey), true).toBool();
}

QByteArray QDesignerSettings::mainWindowState(UIMode mode) const
{
    return settings()->value(QLatin1String(mainWindowStateKey) + modeChar(mode)).toByteArray();
}

void QDesignerSettings::setMainWindowState(UIMode mode, const QByteArray &mainWindowState)
{
    settings()->setValue(QLatin1String(mainWindowStateKey) + modeChar(mode), mainWindowState);
}

QByteArray QDesignerSettings::toolBarsState(UIMode mode) const
{
    QString key = QLatin1String(toolBarsStateKey);
    key += modeChar(mode);
    return settings()->value(key).toByteArray();
}

void QDesignerSettings::setToolBarsState(UIMode mode, const QByteArray &toolBarsState)
{
    QString key = QLatin1String(toolBarsStateKey);
    key += modeChar(mode);
    settings()->setValue(key, toolBarsState);
}

void QDesignerSettings::clearBackup()
{
    QDesignerSettingsInterface *s = settings();
    s->remove(QLatin1String(backupOrgListKey));
    s->remove(QLatin1String(backupBakListKey));
}

void QDesignerSettings::setBackup(const QMap<QString, QString> &map)
{
    const QStringList org = map.keys();
    const QStringList bak = map.values();

    QDesignerSettingsInterface *s = settings();
    s->setValue(QLatin1String(backupOrgListKey), org);
    s->setValue(QLatin1String(backupBakListKey), bak);
}

QMap<QString, QString> QDesignerSettings::backup() const
{
    const QStringList org = settings()->value(QLatin1String(backupOrgListKey), QStringList()).toStringList();
    const QStringList bak = settings()->value(QLatin1String(backupBakListKey), QStringList()).toStringList();

    QMap<QString, QString> map;
    const int orgCount = org.count();
    for (int i = 0; i < orgCount; ++i)
        map.insert(org.at(i), bak.at(i));

    return map;
}

void QDesignerSettings::setUiMode(UIMode mode)
{
    QDesignerSettingsInterface *s = settings();
    s->beginGroup(QStringLiteral("UI"));
    s->setValue(QStringLiteral("currentMode"), mode);
    s->endGroup();
}

UIMode QDesignerSettings::uiMode() const
{
#ifdef Q_OS_MACOS
    const UIMode defaultMode = TopLevelMode;
#else
    const UIMode defaultMode = DockedMode;
#endif
    UIMode uiMode = static_cast<UIMode>(value(QStringLiteral("UI/currentMode"), defaultMode).toInt());
    return uiMode;
}

void QDesignerSettings::setToolWindowFont(const ToolWindowFontSettings &fontSettings)
{
    QDesignerSettingsInterface *s = settings();
    s->beginGroup(QStringLiteral("UI"));
    s->setValue(QStringLiteral("font"), fontSettings.m_font);
    s->setValue(QStringLiteral("useFont"), fontSettings.m_useFont);
    s->setValue(QStringLiteral("writingSystem"), fontSettings.m_writingSystem);
    s->endGroup();
}

ToolWindowFontSettings QDesignerSettings::toolWindowFont() const
{
    ToolWindowFontSettings fontSettings;
    fontSettings.m_writingSystem =
            static_cast<QFontDatabase::WritingSystem>(value(QStringLiteral("UI/writingSystem"),
                                                            QFontDatabase::Any).toInt());
    fontSettings.m_font = qvariant_cast<QFont>(value(QStringLiteral("UI/font")));
    fontSettings.m_useFont =
            settings()->value(QStringLiteral("UI/useFont"), QVariant(false)).toBool();
    return fontSettings;
}

QT_END_NAMESPACE
