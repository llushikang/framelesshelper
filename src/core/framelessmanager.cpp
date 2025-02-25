/*
 * MIT License
 *
 * Copyright (C) 2021-2023 by wangwenx190 (Yuhang Zhao)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "framelessmanager.h"
#include "framelessmanager_p.h"
#include "framelesshelper_qt.h"
#include "framelessconfig_p.h"
#include "framelesshelpercore_global_p.h"
#include "utils.h"
#ifdef Q_OS_WINDOWS
#  include "framelesshelper_win.h"
#  include "winverhelper_p.h"
#endif
#include <QtCore/qvariant.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qloggingcategory.h>
#include <QtGui/qfontdatabase.h>
#include <QtGui/qwindow.h>
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
#  include <QtGui/qguiapplication.h>
#  include <QtGui/qstylehints.h>
#endif // (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))

FRAMELESSHELPER_BEGIN_NAMESPACE

[[maybe_unused]] static Q_LOGGING_CATEGORY(lcFramelessManager, "wangwenx190.framelesshelper.core.framelessmanager")

#ifdef FRAMELESSHELPER_CORE_NO_DEBUG_OUTPUT
#  define INFO QT_NO_QDEBUG_MACRO()
#  define DEBUG QT_NO_QDEBUG_MACRO()
#  define WARNING QT_NO_QDEBUG_MACRO()
#  define CRITICAL QT_NO_QDEBUG_MACRO()
#else
#  define INFO qCInfo(lcFramelessManager)
#  define DEBUG qCDebug(lcFramelessManager)
#  define WARNING qCWarning(lcFramelessManager)
#  define CRITICAL qCCritical(lcFramelessManager)
#endif

using namespace Global;

using FramelessManagerData = QList<WId>;

Q_GLOBAL_STATIC(FramelessManagerData, g_framelessManagerData)

static constexpr const int kEventDelayInterval = 1000;

#ifndef FRAMELESSHELPER_CORE_NO_BUNDLE_RESOURCE
[[nodiscard]] static inline QString iconFontFamilyName()
{
    static const auto result = []() -> QString {
#ifdef Q_OS_WINDOWS
        if (WindowsVersionHelper::isWin11OrGreater()) {
            return FRAMELESSHELPER_STRING_LITERAL("Segoe Fluent Icons");
        }
        if (WindowsVersionHelper::isWin10OrGreater()) {
            return FRAMELESSHELPER_STRING_LITERAL("Segoe MDL2 Assets");
        }
#endif // Q_OS_WINDOWS
        return FRAMELESSHELPER_STRING_LITERAL("iconfont");
    }();
    return result;
}
#endif // FRAMELESSHELPER_CORE_NO_BUNDLE_RESOURCE

FramelessManagerPrivate::FramelessManagerPrivate(FramelessManager *q) : QObject(q)
{
    Q_ASSERT(q);
    if (!q) {
        return;
    }
    q_ptr = q;
    initialize();
}

FramelessManagerPrivate::~FramelessManagerPrivate() = default;

FramelessManagerPrivate *FramelessManagerPrivate::get(FramelessManager *pub)
{
    Q_ASSERT(pub);
    if (!pub) {
        return nullptr;
    }
    return pub->d_func();
}

const FramelessManagerPrivate *FramelessManagerPrivate::get(const FramelessManager *pub)
{
    Q_ASSERT(pub);
    if (!pub) {
        return nullptr;
    }
    return pub->d_func();
}

void FramelessManagerPrivate::initializeIconFont()
{
#ifndef FRAMELESSHELPER_CORE_NO_BUNDLE_RESOURCE
    static bool inited = false;
    if (inited) {
        return;
    }
    inited = true;
    framelesshelpercore_initResource();
    // We always register this font because it's our only fallback.
    const int id = QFontDatabase::addApplicationFont(FRAMELESSHELPER_STRING_LITERAL(":/org.wangwenx190.FramelessHelper/resources/fonts/iconfont.ttf"));
    if (id < 0) {
        WARNING << "Failed to load icon font.";
    } else {
        DEBUG << "Successfully registered icon font.";
    }
#endif // FRAMELESSHELPER_CORE_NO_BUNDLE_RESOURCE
}

QFont FramelessManagerPrivate::getIconFont()
{
#ifdef FRAMELESSHELPER_CORE_NO_BUNDLE_RESOURCE
    return {};
#else // !FRAMELESSHELPER_CORE_NO_BUNDLE_RESOURCE
    static const auto font = []() -> QFont {
        QFont f = {};
        f.setFamily(iconFontFamilyName());
#  ifdef Q_OS_MACOS
        f.setPointSize(10);
#  else // !Q_OS_MACOS
        f.setPointSize(8);
#  endif // Q_OS_MACOS
        return f;
    }();
    return font;
#endif // FRAMELESSHELPER_CORE_NO_BUNDLE_RESOURCE
}

SystemTheme FramelessManagerPrivate::systemTheme() const
{
    // The user's choice has top priority.
    if (isThemeOverrided()) {
        return m_overrideTheme.value();
    }
    return m_systemTheme;
}

QColor FramelessManagerPrivate::systemAccentColor() const
{
    return m_accentColor;
}

QString FramelessManagerPrivate::wallpaper() const
{
    return m_wallpaper;
}

WallpaperAspectStyle FramelessManagerPrivate::wallpaperAspectStyle() const
{
    return m_wallpaperAspectStyle;
}

void FramelessManagerPrivate::addWindow(FramelessParamsConst params)
{
    Q_ASSERT(params);
    if (!params) {
        return;
    }
    const WId windowId = params->getWindowId();
    if (g_framelessManagerData()->contains(windowId)) {
        return;
    }
    g_framelessManagerData()->append(windowId);
    static const bool pureQt = usePureQtImplementation();
    if (pureQt) {
        FramelessHelperQt::addWindow(params);
    }
#ifdef Q_OS_WINDOWS
    if (!pureQt) {
        FramelessHelperWin::addWindow(params);
    }
    Utils::installSystemMenuHook(windowId, params);
#endif
    connect(params->getWindowHandle(), &QWindow::destroyed, FramelessManager::instance(), [windowId](){ removeWindow(windowId); });
}

void FramelessManagerPrivate::removeWindow(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    if (!g_framelessManagerData()->contains(windowId)) {
        return;
    }
    g_framelessManagerData()->removeAll(windowId);
    static const bool pureQt = usePureQtImplementation();
    if (pureQt) {
        FramelessHelperQt::removeWindow(windowId);
    }
#ifdef Q_OS_WINDOWS
    if (!pureQt) {
        FramelessHelperWin::removeWindow(windowId);
    }
    Utils::removeSysMenuHook(windowId);
    Utils::removeMicaWindow(windowId);
#endif
}

void FramelessManagerPrivate::notifySystemThemeHasChangedOrNot()
{
    m_themeTimer.start();
}

void FramelessManagerPrivate::notifyWallpaperHasChangedOrNot()
{
    m_wallpaperTimer.start();
}

void FramelessManagerPrivate::doNotifySystemThemeHasChangedOrNot()
{
    const SystemTheme currentSystemTheme = (Utils::shouldAppsUseDarkMode() ? SystemTheme::Dark : SystemTheme::Light);
    const QColor currentAccentColor = Utils::getAccentColor();
#ifdef Q_OS_WINDOWS
    const DwmColorizationArea currentColorizationArea = Utils::getDwmColorizationArea();
#endif
    bool notify = false;
    if (m_systemTheme != currentSystemTheme) {
        m_systemTheme = currentSystemTheme;
        notify = true;
    }
    if (m_accentColor != currentAccentColor) {
        m_accentColor = currentAccentColor;
        notify = true;
    }
#ifdef Q_OS_WINDOWS
    if (m_colorizationArea != currentColorizationArea) {
        m_colorizationArea = currentColorizationArea;
        notify = true;
    }
#endif
    // Don't emit the signal if the user has overrided the global theme.
    if (notify && !isThemeOverrided()) {
        Q_Q(FramelessManager);
        Q_EMIT q->systemThemeChanged();
        DEBUG.nospace() << "System theme changed. Current theme: " << m_systemTheme
                        << ", accent color: " << m_accentColor.name(QColor::HexArgb).toUpper()
#ifdef Q_OS_WINDOWS
                        << ", colorization area: " << m_colorizationArea
#endif
                        << '.';
    }
}

void FramelessManagerPrivate::doNotifyWallpaperHasChangedOrNot()
{
    const QString currentWallpaper = Utils::getWallpaperFilePath();
    const WallpaperAspectStyle currentWallpaperAspectStyle = Utils::getWallpaperAspectStyle();
    bool notify = false;
    if (m_wallpaper != currentWallpaper) {
        m_wallpaper = currentWallpaper;
        notify = true;
    }
    if (m_wallpaperAspectStyle != currentWallpaperAspectStyle) {
        m_wallpaperAspectStyle = currentWallpaperAspectStyle;
        notify = true;
    }
    if (notify) {
        Q_Q(FramelessManager);
        Q_EMIT q->wallpaperChanged();
        DEBUG.nospace() << "Wallpaper changed. Current wallpaper: " << m_wallpaper
                        << ", aspect style: " << m_wallpaperAspectStyle << '.';
    }
}

bool FramelessManagerPrivate::usePureQtImplementation()
{
    static const auto result = []() -> bool {
#ifdef Q_OS_WINDOWS
        return FramelessConfig::instance()->isSet(Option::UseCrossPlatformQtImplementation);
#else
        return true;
#endif
    }();
    return result;
}

void FramelessManagerPrivate::setOverrideTheme(const SystemTheme theme)
{
    if ((!m_overrideTheme.has_value() && (theme == SystemTheme::Unknown))
        || (m_overrideTheme.has_value() && (m_overrideTheme.value() == theme))) {
        return;
    }
    if (theme == SystemTheme::Unknown) {
        m_overrideTheme = std::nullopt;
    } else {
        m_overrideTheme = theme;
    }
    Q_Q(FramelessManager);
    Q_EMIT q->systemThemeChanged();
}

bool FramelessManagerPrivate::isThemeOverrided() const
{
    return (m_overrideTheme.value_or(SystemTheme::Unknown) != SystemTheme::Unknown);
}

void FramelessManagerPrivate::initialize()
{
    m_themeTimer.setInterval(kEventDelayInterval);
    m_themeTimer.callOnTimeout(this, [this](){
        m_themeTimer.stop();
        doNotifySystemThemeHasChangedOrNot();
    });
    m_wallpaperTimer.setInterval(kEventDelayInterval);
    m_wallpaperTimer.callOnTimeout(this, [this](){
        m_wallpaperTimer.stop();
        doNotifyWallpaperHasChangedOrNot();
    });
    m_systemTheme = (Utils::shouldAppsUseDarkMode() ? SystemTheme::Dark : SystemTheme::Light);
    m_accentColor = Utils::getAccentColor();
#ifdef Q_OS_WINDOWS
    m_colorizationArea = Utils::getDwmColorizationArea();
#endif
    m_wallpaper = Utils::getWallpaperFilePath();
    m_wallpaperAspectStyle = Utils::getWallpaperAspectStyle();
    DEBUG.nospace() << "Current system theme: " << m_systemTheme
                    << ", accent color: " << m_accentColor.name(QColor::HexArgb).toUpper()
#ifdef Q_OS_WINDOWS
                    << ", colorization area: " << m_colorizationArea
#endif
                    << ", wallpaper: " << m_wallpaper
                    << ", aspect style: " << m_wallpaperAspectStyle
                    << '.';
    // We are doing some tricks in our Windows message handling code, so
    // we don't use Qt's theme notifier on Windows. But for other platforms
    // we want to use as many Qt functionalities as possible.
#if ((QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)) && !defined(Q_OS_WINDOWS))
    QStyleHints * const styleHints = QGuiApplication::styleHints();
    Q_ASSERT(styleHints);
    if (styleHints) {
        connect(styleHints, &QStyleHints::colorSchemeChanged, this, [this](const Qt::ColorScheme colorScheme){
            Q_UNUSED(colorScheme);
            notifySystemThemeHasChangedOrNot();
        });
    }
#endif // ((QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)) && !defined(Q_OS_WINDOWS))
    static bool flagSet = false;
    if (!flagSet) {
        flagSet = true;
        // Set a global flag so that people can check whether FramelessHelper is being
        // used without actually accessing the FramelessHelper interface.
        static constexpr const char flag[] = "__FRAMELESSHELPER__";
        const int ver = FramelessHelper::Core::version().version;
        qputenv(flag, QByteArray::number(ver));
        qApp->setProperty(flag, ver);
    }
}

FramelessManager::FramelessManager(QObject *parent) :
    QObject(parent), d_ptr(new FramelessManagerPrivate(this))
{
}

FramelessManager::~FramelessManager() = default;

FramelessManager *FramelessManager::instance()
{
    static FramelessManager manager;
    return &manager;
}

SystemTheme FramelessManager::systemTheme() const
{
    Q_D(const FramelessManager);
    return d->systemTheme();
}

QColor FramelessManager::systemAccentColor() const
{
    Q_D(const FramelessManager);
    return d->systemAccentColor();
}

QString FramelessManager::wallpaper() const
{
    Q_D(const FramelessManager);
    return d->wallpaper();
}

WallpaperAspectStyle FramelessManager::wallpaperAspectStyle() const
{
    Q_D(const FramelessManager);
    return d->wallpaperAspectStyle();
}

void FramelessManager::addWindow(FramelessParamsConst params)
{
    Q_D(FramelessManager);
    d->addWindow(params);
}

void FramelessManager::removeWindow(const WId windowId)
{
    Q_D(FramelessManager);
    d->removeWindow(windowId);
}

void FramelessManager::setOverrideTheme(const SystemTheme theme)
{
    Q_D(FramelessManager);
    d->setOverrideTheme(theme);
}

FRAMELESSHELPER_END_NAMESPACE
