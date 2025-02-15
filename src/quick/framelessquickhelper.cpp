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

#include "framelessquickhelper.h"
#include "framelessquickhelper_p.h"
#include "quickmicamaterial.h"
#include "quickwindowborder.h"
#include <FramelessHelper/Core/framelessmanager.h>
#include <FramelessHelper/Core/utils.h>
#include <FramelessHelper/Core/private/framelessconfig_p.h>
#include <FramelessHelper/Core/private/framelesshelpercore_global_p.h>
#ifdef Q_OS_WINDOWS
#  include <FramelessHelper/Core/private/winverhelper_p.h>
#endif // Q_OS_WINDOWS
#include <QtCore/qtimer.h>
#include <QtCore/qeventloop.h>
#include <QtCore/qloggingcategory.h>
#include <QtGui/qcursor.h>
#include <QtGui/qguiapplication.h>
#ifndef FRAMELESSHELPER_QUICK_NO_PRIVATE
#  if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#    include <QtGui/qpa/qplatformwindow.h> // For QWINDOWSIZE_MAX
#  else // (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#    include <QtGui/private/qwindow_p.h> // For QWINDOWSIZE_MAX
#  endif // (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#  include <QtQuick/private/qquickitem_p.h>
#endif // FRAMELESSHELPER_QUICK_NO_PRIVATE

#ifndef QWINDOWSIZE_MAX
#  define QWINDOWSIZE_MAX ((1 << 24) - 1)
#endif // QWINDOWSIZE_MAX

FRAMELESSHELPER_BEGIN_NAMESPACE

[[maybe_unused]] static Q_LOGGING_CATEGORY(lcFramelessQuickHelper, "wangwenx190.framelesshelper.quick.framelessquickhelper")

#ifdef FRAMELESSHELPER_QUICK_NO_DEBUG_OUTPUT
#  define INFO QT_NO_QDEBUG_MACRO()
#  define DEBUG QT_NO_QDEBUG_MACRO()
#  define WARNING QT_NO_QDEBUG_MACRO()
#  define CRITICAL QT_NO_QDEBUG_MACRO()
#else
#  define INFO qCInfo(lcFramelessQuickHelper)
#  define DEBUG qCDebug(lcFramelessQuickHelper)
#  define WARNING qCWarning(lcFramelessQuickHelper)
#  define CRITICAL qCCritical(lcFramelessQuickHelper)
#endif

using namespace Global;

struct FramelessQuickHelperData
{
    bool ready = false;
    SystemParameters params = {};
    QPointer<QQuickItem> titleBarItem = nullptr;
    QList<QPointer<QQuickItem>> hitTestVisibleItems = {};
    QPointer<QQuickItem> windowIconButton = nullptr;
    QPointer<QQuickItem> contextHelpButton = nullptr;
    QPointer<QQuickItem> minimizeButton = nullptr;
    QPointer<QQuickItem> maximizeButton = nullptr;
    QPointer<QQuickItem> closeButton = nullptr;
    QList<QRect> hitTestVisibleRects = {};
};

using FramelessQuickHelperInternal = QHash<WId, FramelessQuickHelperData>;

Q_GLOBAL_STATIC(FramelessQuickHelperInternal, g_framelessQuickHelperData)

FramelessQuickHelperPrivate::FramelessQuickHelperPrivate(FramelessQuickHelper *q) : QObject(q)
{
    Q_ASSERT(q);
    if (!q) {
        return;
    }
    q_ptr = q;
    // Workaround a MOC limitation: we can't emit a signal from the parent class.
    connect(q_ptr, &FramelessQuickHelper::windowChanged, q_ptr, &FramelessQuickHelper::windowChanged2);
}

FramelessQuickHelperPrivate::~FramelessQuickHelperPrivate()
{
    m_destroying = true;
    extendsContentIntoTitleBar(false);
    m_extendIntoTitleBar = std::nullopt;
}

FramelessQuickHelperPrivate *FramelessQuickHelperPrivate::get(FramelessQuickHelper *pub)
{
    Q_ASSERT(pub);
    if (!pub) {
        return nullptr;
    }
    return pub->d_func();
}

const FramelessQuickHelperPrivate *FramelessQuickHelperPrivate::get(const FramelessQuickHelper *pub)
{
    Q_ASSERT(pub);
    if (!pub) {
        return nullptr;
    }
    return pub->d_func();
}

bool FramelessQuickHelperPrivate::isContentExtendedIntoTitleBar() const
{
    const FramelessQuickHelperData *data = getWindowData();
    return (data ? data->ready : false);
}

void FramelessQuickHelperPrivate::extendsContentIntoTitleBar(const bool value)
{
    if (isContentExtendedIntoTitleBar() == value) {
        return;
    }
    if (value) {
        attach();
    } else {
        detach();
    }
    m_extendIntoTitleBar = value;
    if (!m_destroying) {
        emitSignalForAllInstances("extendsContentIntoTitleBarChanged");
    }
}

QQuickItem *FramelessQuickHelperPrivate::getTitleBarItem() const
{
    const FramelessQuickHelperData *data = getWindowData();
    return (data ? data->titleBarItem : nullptr);
}

void FramelessQuickHelperPrivate::setTitleBarItem(QQuickItem *value)
{
    Q_ASSERT(value);
    if (!value) {
        return;
    }
    FramelessQuickHelperData *data = getWindowDataMutable();
    if (!data || (data->titleBarItem == value)) {
        return;
    }
    data->titleBarItem = value;
    emitSignalForAllInstances("titleBarItemChanged");
}

void FramelessQuickHelperPrivate::attach()
{
    Q_Q(FramelessQuickHelper);
    QQuickWindow * const window = q->window();
    Q_ASSERT(window);
    if (!window) {
        return;
    }

    FramelessQuickHelperData * const data = getWindowDataMutable();
    if (!data || data->ready) {
        return;
    }

    SystemParameters params = {};
    params.getWindowId = [window]() -> WId { return window->winId(); };
    params.getWindowFlags = [window]() -> Qt::WindowFlags { return window->flags(); };
    params.setWindowFlags = [window](const Qt::WindowFlags flags) -> void { window->setFlags(flags); };
    params.getWindowSize = [window]() -> QSize { return window->size(); };
    params.setWindowSize = [window](const QSize &size) -> void { window->resize(size); };
    params.getWindowPosition = [window]() -> QPoint { return window->position(); };
    params.setWindowPosition = [window](const QPoint &pos) -> void { window->setX(pos.x()); window->setY(pos.y()); };
    params.getWindowScreen = [window]() -> QScreen * { return window->screen(); };
    params.isWindowFixedSize = [this]() -> bool { return isWindowFixedSize(); };
    params.setWindowFixedSize = [this](const bool value) -> void { setWindowFixedSize(value); };
    params.getWindowState = [window]() -> Qt::WindowState { return window->windowState(); };
    params.setWindowState = [window](const Qt::WindowState state) -> void { window->setWindowState(state); };
    params.getWindowHandle = [window]() -> QWindow * { return window; };
    params.windowToScreen = [window](const QPoint &pos) -> QPoint { return window->mapToGlobal(pos); };
    params.screenToWindow = [window](const QPoint &pos) -> QPoint { return window->mapFromGlobal(pos); };
    params.isInsideSystemButtons = [this](const QPoint &pos, SystemButtonType *button) -> bool {
        QuickGlobal::SystemButtonType button2 = QuickGlobal::SystemButtonType::Unknown;
        const bool result = isInSystemButtons(pos, &button2);
        *button = FRAMELESSHELPER_ENUM_QUICK_TO_CORE(SystemButtonType, button2);
        return result;
    };
    params.isInsideTitleBarDraggableArea = [this](const QPoint &pos) -> bool { return isInTitleBarDraggableArea(pos); };
    params.getWindowDevicePixelRatio = [window]() -> qreal { return window->effectiveDevicePixelRatio(); };
    params.setSystemButtonState = [this](const SystemButtonType button, const ButtonState state) -> void {
        setSystemButtonState(FRAMELESSHELPER_ENUM_CORE_TO_QUICK(SystemButtonType, button),
                             FRAMELESSHELPER_ENUM_CORE_TO_QUICK(ButtonState, state));
    };
    params.shouldIgnoreMouseEvents = [this](const QPoint &pos) -> bool { return shouldIgnoreMouseEvents(pos); };
    params.showSystemMenu = [this](const QPoint &pos) -> void { showSystemMenu(pos); };
    params.setProperty = [this](const char *name, const QVariant &value) -> void { setProperty(name, value); };
    params.getProperty = [this](const char *name, const QVariant &defaultValue) -> QVariant { return getProperty(name, defaultValue); };
    params.setCursor = [window](const QCursor &cursor) -> void { window->setCursor(cursor); };
    params.unsetCursor = [window]() -> void { window->unsetCursor(); };
    params.getWidgetHandle = []() -> QObject * { return nullptr; };
    params.forceChildrenRepaint = [this](const int delay) -> void { repaintAllChildren(delay); };

    FramelessManager::instance()->addWindow(&params);

    data->params = params;
    data->ready = true;

    // We have to wait for a little time before moving the top level window
    // , because the platform window may not finish initializing by the time
    // we reach here, and all the modifications from the Qt side will be lost
    // due to QPA will reset the position and size of the window during it's
    // initialization process.
    QTimer::singleShot(m_qpaWaitTime, this, [this](){
        m_qpaReady = true;
        if (FramelessConfig::instance()->isSet(Option::CenterWindowBeforeShow)) {
            moveWindowToDesktopCenter();
        }
        if (FramelessConfig::instance()->isSet(Option::EnableBlurBehindWindow)) {
            setBlurBehindWindowEnabled(true, {});
        }
        emitSignalForAllInstances("ready");
    });
}

void FramelessQuickHelperPrivate::detach()
{
    Q_Q(FramelessQuickHelper);
    QQuickWindow * const w = q->window();
    if (!w) {
        return;
    }
    const WId windowId = w->winId();
    const auto it = g_framelessQuickHelperData()->constFind(windowId);
    if (it == g_framelessQuickHelperData()->constEnd()) {
        return;
    }
    g_framelessQuickHelperData()->erase(it);
    FramelessManager::instance()->removeWindow(windowId);
}

void FramelessQuickHelperPrivate::setSystemButton(QQuickItem *item, const QuickGlobal::SystemButtonType buttonType)
{
    Q_ASSERT(item);
    Q_ASSERT(buttonType != QuickGlobal::SystemButtonType::Unknown);
    if (!item || (buttonType == QuickGlobal::SystemButtonType::Unknown)) {
        return;
    }
    FramelessQuickHelperData *data = getWindowDataMutable();
    if (!data) {
        return;
    }
    switch (buttonType) {
    case QuickGlobal::SystemButtonType::WindowIcon:
        data->windowIconButton = item;
        break;
    case QuickGlobal::SystemButtonType::Help:
        data->contextHelpButton = item;
        break;
    case QuickGlobal::SystemButtonType::Minimize:
        data->minimizeButton = item;
        break;
    case QuickGlobal::SystemButtonType::Maximize:
    case QuickGlobal::SystemButtonType::Restore:
        data->maximizeButton = item;
        break;
    case QuickGlobal::SystemButtonType::Close:
        data->closeButton = item;
        break;
    case QuickGlobal::SystemButtonType::Unknown:
        Q_UNREACHABLE_RETURN(static_cast<void>(0));
    }
}

void FramelessQuickHelperPrivate::setHitTestVisible(QQuickItem *item, const bool visible)
{
    Q_ASSERT(item);
    if (!item) {
        return;
    }
    FramelessQuickHelperData *data = getWindowDataMutable();
    if (!data) {
        return;
    }
    if (visible) {
        data->hitTestVisibleItems.append(item);
    } else {
        data->hitTestVisibleItems.removeAll(item);
    }
}

void FramelessQuickHelperPrivate::setHitTestVisible(const QRect &rect, const bool visible)
{
    Q_ASSERT(rect.isValid());
    if (!rect.isValid()) {
        return;
    }
    FramelessQuickHelperData *data = getWindowDataMutable();
    if (!data) {
        return;
    }
    if (visible) {
        data->hitTestVisibleRects.append(rect);
    } else {
        data->hitTestVisibleRects.removeAll(rect);
    }
}

void FramelessQuickHelperPrivate::setHitTestVisible(QObject *object, const bool visible)
{
    Q_ASSERT(object);
    if (!object) {
        return;
    }
    const auto item = qobject_cast<QQuickItem *>(object);
    Q_ASSERT(item);
    if (!item) {
        return;
    }
    setHitTestVisible(item, visible);
}

void FramelessQuickHelperPrivate::showSystemMenu(const QPoint &pos)
{
    Q_Q(FramelessQuickHelper);
    const QQuickWindow * const window = q->window();
    if (!window) {
        return;
    }
    const WId windowId = window->winId();
    const QPoint nativePos = Utils::toNativeGlobalPosition(window, pos);
#ifdef Q_OS_WINDOWS
    Utils::showSystemMenu(windowId, nativePos, false, &getWindowData()->params);
#elif defined(Q_OS_LINUX)
    Utils::openSystemMenu(windowId, nativePos);
#else
    Q_UNUSED(windowId);
    Q_UNUSED(nativePos);
#endif
}

void FramelessQuickHelperPrivate::windowStartSystemMove2(const QPoint &pos)
{
    Q_Q(FramelessQuickHelper);
    QQuickWindow * const window = q->window();
    if (!window) {
        return;
    }
    Utils::startSystemMove(window, pos);
}

void FramelessQuickHelperPrivate::windowStartSystemResize2(const Qt::Edges edges, const QPoint &pos)
{
    Q_Q(FramelessQuickHelper);
    QQuickWindow * const window = q->window();
    if (!window) {
        return;
    }
    if (edges == Qt::Edges{}) {
        return;
    }
    Utils::startSystemResize(window, edges, pos);
}

void FramelessQuickHelperPrivate::moveWindowToDesktopCenter()
{
    Q_Q(FramelessQuickHelper);
    QQuickWindow * const window = q->window();
    if (!window) {
        return;
    }
    Utils::moveWindowToDesktopCenter(&getWindowData()->params, true);
}

void FramelessQuickHelperPrivate::bringWindowToFront()
{
    Q_Q(FramelessQuickHelper);
    QQuickWindow * const window = q->window();
    if (!window) {
        return;
    }
#ifdef Q_OS_WINDOWS
    Utils::bringWindowToFront(window->winId());
#else
    if (window->visibility() == QQuickWindow::Hidden) {
        window->show();
    }
    if (window->visibility() == QQuickWindow::Minimized) {
#  if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
        window->setWindowStates(window->windowStates() & ~Qt::WindowMinimized);
#  else
        window->showNormal();
#  endif
    }
    window->raise();
    window->requestActivate();
#endif
}

bool FramelessQuickHelperPrivate::isWindowFixedSize() const
{
    Q_Q(const FramelessQuickHelper);
    const QQuickWindow * const window = q->window();
    if (!window) {
        return false;
    }
    if (window->flags() & Qt::MSWindowsFixedSizeDialogHint) {
        return true;
    }
    const QSize minSize = window->minimumSize();
    const QSize maxSize = window->maximumSize();
    if (!minSize.isEmpty() && !maxSize.isEmpty() && (minSize == maxSize)) {
        return true;
    }
    return false;
}

void FramelessQuickHelperPrivate::setWindowFixedSize(const bool value)
{
    Q_Q(FramelessQuickHelper);
    QQuickWindow * const window = q->window();
    if (!window) {
        return;
    }
    if (isWindowFixedSize() == value) {
        return;
    }
    if (value) {
        const QSize size = window->size();
        window->setMinimumSize(size);
        window->setMaximumSize(size);
    } else {
        window->setMinimumSize(kDefaultWindowSize);
        window->setMaximumSize(QSize(QWINDOWSIZE_MAX, QWINDOWSIZE_MAX));
    }
#ifdef Q_OS_WINDOWS
    Utils::setAeroSnappingEnabled(window->winId(), !value);
#endif
    emitSignalForAllInstances("windowFixedSizeChanged");
}

void FramelessQuickHelperPrivate::emitSignalForAllInstances(const char *signal)
{
    Q_ASSERT(signal);
    Q_ASSERT(*signal != '\0');
    if (!signal || (*signal == '\0')) {
        return;
    }
    Q_Q(FramelessQuickHelper);
    const QQuickWindow * const window = q->window();
    if (!window) {
        return;
    }
    const auto rootObject = (window->contentItem() ?
            qobject_cast<const QObject *>(window->contentItem())
            : qobject_cast<const QObject *>(window));
    const auto instances = rootObject->findChildren<FramelessQuickHelper *>();
    if (instances.isEmpty()) {
        return;
    }
    for (auto &&instance : std::as_const(instances)) {
        QMetaObject::invokeMethod(instance, signal);
    }
}

bool FramelessQuickHelperPrivate::isBlurBehindWindowEnabled() const
{
    return m_blurBehindWindowEnabled;
}

void FramelessQuickHelperPrivate::setBlurBehindWindowEnabled(const bool value, const QColor &color)
{
    Q_Q(FramelessQuickHelper);
    QQuickWindow * const window = q->window();
    if (!window) {
        return;
    }
    if (m_blurBehindWindowEnabled == value) {
        return;
    }
    if (Utils::isBlurBehindWindowSupported()) {
        QuickGlobal::BlurMode mode = QuickGlobal::BlurMode::Disable;
        if (value) {
            if (!m_savedWindowBackgroundColor.isValid()) {
                m_savedWindowBackgroundColor = window->color();
            }
            window->setColor(kDefaultTransparentColor);
            mode = QuickGlobal::BlurMode::Default;
        } else {
            if (m_savedWindowBackgroundColor.isValid()) {
                window->setColor(m_savedWindowBackgroundColor);
                m_savedWindowBackgroundColor = {};
            }
            mode = QuickGlobal::BlurMode::Disable;
        }
        if (Utils::setBlurBehindWindowEnabled(window->winId(),
            FRAMELESSHELPER_ENUM_QUICK_TO_CORE(BlurMode, mode), color)) {
            m_blurBehindWindowEnabled = value;
            emitSignalForAllInstances("blurBehindWindowEnabledChanged");
        } else {
            WARNING << "Failed to enable/disable blur behind window.";
        }
    } else {
        m_blurBehindWindowEnabled = value;
        findOrCreateMicaMaterial()->setVisible(m_blurBehindWindowEnabled);
        emitSignalForAllInstances("blurBehindWindowEnabledChanged");
    }
}

void FramelessQuickHelperPrivate::setProperty(const char *name, const QVariant &value)
{
    Q_ASSERT(name);
    Q_ASSERT(*name != '\0');
    Q_ASSERT(value.isValid());
    if (!name || (*name == '\0') || !value.isValid()) {
        return;
    }
    Q_Q(FramelessQuickHelper);
    QQuickWindow * const window = q->window();
    if (!window) {
        return;
    }
    window->setProperty(name, value);
}

QVariant FramelessQuickHelperPrivate::getProperty(const char *name, const QVariant &defaultValue)
{
    Q_ASSERT(name);
    Q_ASSERT(*name != '\0');
    if (!name || (*name == '\0')) {
        return {};
    }
    Q_Q(FramelessQuickHelper);
    const QQuickWindow * const window = q->window();
    if (!window) {
        return {};
    }
    const QVariant value = window->property(name);
    return (value.isValid() ? value : defaultValue);
}

QuickMicaMaterial *FramelessQuickHelperPrivate::findOrCreateMicaMaterial() const
{
    Q_Q(const FramelessQuickHelper);
    const QQuickWindow * const window = q->window();
    if (!window) {
        return nullptr;
    }
    QQuickItem * const rootItem = window->contentItem();
    if (const auto item = rootItem->findChild<QuickMicaMaterial *>()) {
        return item;
    }
    if (const auto item = window->findChild<QuickMicaMaterial *>()) {
        return item;
    }
    const auto item = new QuickMicaMaterial;
    item->setParent(rootItem);
    item->setParentItem(rootItem);
    item->setZ(-999); // Make sure it always stays on the bottom.
#ifndef FRAMELESSHELPER_QUICK_NO_PRIVATE
    QQuickItemPrivate::get(item)->anchors()->setFill(rootItem);
#endif // FRAMELESSHELPER_QUICK_NO_PRIVATE
    return item;
}

QuickWindowBorder *FramelessQuickHelperPrivate::findOrCreateWindowBorder() const
{
    Q_Q(const FramelessQuickHelper);
    const QQuickWindow * const window = q->window();
    if (!window) {
        return nullptr;
    }
    QQuickItem * const rootItem = window->contentItem();
    if (const auto item = rootItem->findChild<QuickWindowBorder *>()) {
        return item;
    }
    if (const auto item = window->findChild<QuickWindowBorder *>()) {
        return item;
    }
    const auto item = new QuickWindowBorder;
    item->setParent(rootItem);
    item->setParentItem(rootItem);
    item->setZ(999); // Make sure it always stays on the top.
#ifndef FRAMELESSHELPER_QUICK_NO_PRIVATE
    QQuickItemPrivate::get(item)->anchors()->setFill(rootItem);
#endif // FRAMELESSHELPER_QUICK_NO_PRIVATE
    return item;
}

FramelessQuickHelper *FramelessQuickHelperPrivate::findOrCreateFramelessHelper(QObject *object)
{
    Q_ASSERT(object);
    if (!object) {
        return nullptr;
    }
    QObject *parent = nullptr;
    QQuickItem *parentItem = nullptr;
    if (const auto objWindow = qobject_cast<QQuickWindow *>(object)) {
        if (QQuickItem * const item = objWindow->contentItem()) {
            parent = item;
            parentItem = item;
        } else {
            parent = objWindow;
        }
    } else if (const auto item = qobject_cast<QQuickItem *>(object)) {
        if (QQuickWindow * const itemWindow = item->window()) {
            if (QQuickItem * const contentItem = itemWindow->contentItem()) {
                parent = contentItem;
                parentItem = contentItem;
            } else {
                parent = itemWindow;
                parentItem = item;
            }
        } else {
            parent = item;
            parentItem = item;
        }
    } else {
        parent = object;
    }
    FramelessQuickHelper *instance = parent->findChild<FramelessQuickHelper *>();
    if (!instance) {
        instance = new FramelessQuickHelper;
        instance->setParentItem(parentItem);
        instance->setParent(parent);
        // No need to do this here, we'll do it once the item has been assigned to a specific window.
        //instance->extendsContentIntoTitleBar();
    }
    return instance;
}

bool FramelessQuickHelperPrivate::isReady() const
{
    return m_qpaReady;
}

void FramelessQuickHelperPrivate::waitForReady()
{
    if (m_qpaReady) {
        return;
    }
#if 1
    QEventLoop loop;
    Q_Q(FramelessQuickHelper);
    const QMetaObject::Connection connection = connect(
        q, &FramelessQuickHelper::ready, &loop, &QEventLoop::quit);
    loop.exec();
    disconnect(connection);
#else
    while (!m_qpaReady) {
        QCoreApplication::processEvents();
    }
#endif
}

void FramelessQuickHelperPrivate::repaintAllChildren(const quint32 delay) const
{
    Q_Q(const FramelessQuickHelper);
    QQuickWindow * const window = q->window();
    if (!window) {
        return;
    }
    const auto update = [window]() -> void {
        window->requestUpdate();
#ifdef Q_OS_WINDOWS
        // Sync the internal window frame margins with the latest DPI, otherwise
        // we will get wrong window sizes after the DPI change.
        Utils::updateInternalWindowFrameMargins(window, true);
#endif // Q_OS_WINDOWS
        const QList<QQuickItem *> items = window->findChildren<QQuickItem *>();
        if (items.isEmpty()) {
            return;
        }
        for (auto &&item : std::as_const(items)) {
            // Only items with the "QQuickItem::ItemHasContents" flag enabled are allowed to call "update()".
            if (item->flags() & QQuickItem::ItemHasContents) {
                item->update();
            }
        }
    };
    if (delay > 0) {
        QTimer::singleShot(delay, this, update);
    } else {
        update();
    }
}

quint32 FramelessQuickHelperPrivate::readyWaitTime() const
{
    return m_qpaWaitTime;
}

void FramelessQuickHelperPrivate::setReadyWaitTime(const quint32 time)
{
    if (m_qpaWaitTime == time) {
        return;
    }
    m_qpaWaitTime = time;
}

QRect FramelessQuickHelperPrivate::mapItemGeometryToScene(const QQuickItem * const item) const
{
    Q_ASSERT(item);
    if (!item) {
        return {};
    }
    const QPointF originPoint = item->mapToScene(QPointF(0.0, 0.0));
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    const QSizeF size = item->size();
#else
    const QSizeF size = {item->width(), item->height()};
#endif
    return QRectF(originPoint, size).toRect();
}

bool FramelessQuickHelperPrivate::isInSystemButtons(const QPoint &pos, QuickGlobal::SystemButtonType *button) const
{
    Q_ASSERT(button);
    if (!button) {
        return false;
    }
    const FramelessQuickHelperData *data = getWindowData();
    if (!data) {
        return false;
    }
    *button = QuickGlobal::SystemButtonType::Unknown;
    if (data->windowIconButton && data->windowIconButton->isVisible() && data->windowIconButton->isEnabled()) {
        if (mapItemGeometryToScene(data->windowIconButton).contains(pos)) {
            *button = QuickGlobal::SystemButtonType::WindowIcon;
            return true;
        }
    }
    if (data->contextHelpButton && data->contextHelpButton->isVisible() && data->contextHelpButton->isEnabled()) {
        if (mapItemGeometryToScene(data->contextHelpButton).contains(pos)) {
            *button = QuickGlobal::SystemButtonType::Help;
            return true;
        }
    }
    if (data->minimizeButton && data->minimizeButton->isVisible() && data->minimizeButton->isEnabled()) {
        if (mapItemGeometryToScene(data->minimizeButton).contains(pos)) {
            *button = QuickGlobal::SystemButtonType::Minimize;
            return true;
        }
    }
    if (data->maximizeButton && data->maximizeButton->isVisible() && data->maximizeButton->isEnabled()) {
        if (mapItemGeometryToScene(data->maximizeButton).contains(pos)) {
            *button = QuickGlobal::SystemButtonType::Maximize;
            return true;
        }
    }
    if (data->closeButton && data->closeButton->isVisible() && data->closeButton->isEnabled()) {
        if (mapItemGeometryToScene(data->closeButton).contains(pos)) {
            *button = QuickGlobal::SystemButtonType::Close;
            return true;
        }
    }
    return false;
}

bool FramelessQuickHelperPrivate::isInTitleBarDraggableArea(const QPoint &pos) const
{
    const FramelessQuickHelperData *data = getWindowData();
    if (!data) {
        return false;
    }
    if (!data->titleBarItem) {
        // There's no title bar at all, the mouse will always be in the client area.
        return false;
    }
    if (!data->titleBarItem->isVisible() || !data->titleBarItem->isEnabled()) {
        // The title bar is hidden or disabled for some reason, treat it as there's no title bar.
        return false;
    }
    Q_Q(const FramelessQuickHelper);
    const QQuickWindow * const window = q->window();
    if (!window) {
        // The FramelessQuickHelper item has not been attached to a specific window yet,
        // so we assume there's no title bar.
        return false;
    }
    const QRect windowRect = {QPoint(0, 0), window->size()};
    const QRect titleBarRect = mapItemGeometryToScene(data->titleBarItem);
    if (!titleBarRect.intersects(windowRect)) {
        // The title bar is totally outside of the window for some reason,
        // also treat it as there's no title bar.
        return false;
    }
    QRegion region = titleBarRect;
    const auto systemButtons = {
        data->windowIconButton, data->contextHelpButton,
        data->minimizeButton, data->maximizeButton,
        data->closeButton
    };
    for (auto &&button : std::as_const(systemButtons)) {
        if (button && button->isVisible() && button->isEnabled()) {
            region -= mapItemGeometryToScene(button);
        }
    }
    if (!data->hitTestVisibleItems.isEmpty()) {
        for (auto &&item : std::as_const(data->hitTestVisibleItems)) {
            if (item && item->isVisible() && item->isEnabled()) {
                region -= mapItemGeometryToScene(item);
            }
        }
    }
    if (!data->hitTestVisibleRects.isEmpty()) {
        for (auto &&rect : std::as_const(data->hitTestVisibleRects)) {
            if (rect.isValid()) {
                region -= rect;
            }
        }
    }
    return region.contains(pos);
}

bool FramelessQuickHelperPrivate::shouldIgnoreMouseEvents(const QPoint &pos) const
{
    Q_Q(const FramelessQuickHelper);
    const QQuickWindow * const window = q->window();
    if (!window) {
        return false;
    }
    const auto withinFrameBorder = [&pos, window]() -> bool {
        if (pos.y() < kDefaultResizeBorderThickness) {
            return true;
        }
#ifdef Q_OS_WINDOWS
        if (Utils::isWindowFrameBorderVisible()) {
            return false;
        }
#endif
        return ((pos.x() < kDefaultResizeBorderThickness)
                || (pos.x() >= (window->width() - kDefaultResizeBorderThickness)));
    }();
    return ((window->visibility() == QQuickWindow::Windowed) && withinFrameBorder);
}

void FramelessQuickHelperPrivate::setSystemButtonState(const QuickGlobal::SystemButtonType button,
                                                       const QuickGlobal::ButtonState state)
{
#ifdef FRAMELESSHELPER_QUICK_NO_PRIVATE
    Q_UNUSED(button);
    Q_UNUSED(state);
#else // !FRAMELESSHELPER_QUICK_NO_PRIVATE
    Q_ASSERT(button != QuickGlobal::SystemButtonType::Unknown);
    if (button == QuickGlobal::SystemButtonType::Unknown) {
        return;
    }
    const FramelessQuickHelperData *data = getWindowData();
    if (!data) {
        return;
    }
    QQuickItem *quickButton = nullptr;
    switch (button) {
    case QuickGlobal::SystemButtonType::WindowIcon:
        quickButton = data->windowIconButton;
        break;
    case QuickGlobal::SystemButtonType::Help:
        quickButton = data->contextHelpButton;
        break;
    case QuickGlobal::SystemButtonType::Minimize:
        quickButton = data->minimizeButton;
        break;
    case QuickGlobal::SystemButtonType::Maximize:
    case QuickGlobal::SystemButtonType::Restore:
        quickButton = data->maximizeButton;
        break;
    case QuickGlobal::SystemButtonType::Close:
        quickButton = data->closeButton;
        break;
    case QuickGlobal::SystemButtonType::Unknown:
        Q_UNREACHABLE_RETURN(void(0));
    }
    if (!quickButton) {
        return;
    }
    const auto updateButtonState = [state](QQuickItem *btn) -> void {
        Q_ASSERT(btn);
        if (!btn) {
            return;
        }
        const QQuickWindow *window = btn->window();
        Q_ASSERT(window);
        if (!window) {
            return;
        }
        const QScreen *screen = (window->screen() ? window->screen() : QGuiApplication::primaryScreen());
        const QPoint globalPos = (screen ? QCursor::pos(screen) : QCursor::pos());
        const QPoint localPos = btn->mapFromGlobal(globalPos).toPoint();
        const QPoint scenePos = window->mapFromGlobal(globalPos);
        const auto underMouse = [btn, &globalPos]() -> bool {
            const QPointF originPoint = btn->mapToGlobal(QPointF{ 0, 0 });
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
            const QSizeF size = btn->size();
#else
            const auto size = QSizeF{ btn->width(), btn->height() };
#endif
            return QRectF{ originPoint, size }.contains(globalPos);
        }();
        Utils::emulateQtMouseEvent(btn, window, FRAMELESSHELPER_ENUM_QUICK_TO_CORE(ButtonState, state), globalPos, scenePos, localPos, underMouse);
    };
    updateButtonState(quickButton);
#endif // FRAMELESSHELPER_QUICK_NO_PRIVATE
}

const FramelessQuickHelperData *FramelessQuickHelperPrivate::getWindowData() const
{
    Q_Q(const FramelessQuickHelper);
    const QQuickWindow * const window = q->window();
    //Q_ASSERT(window);
    if (!window) {
        return nullptr;
    }
    const WId windowId = window->winId();
    auto it = g_framelessQuickHelperData()->find(windowId);
    if (it == g_framelessQuickHelperData()->end()) {
        it = g_framelessQuickHelperData()->insert(windowId, {});
    }
    return &it.value();
}

FramelessQuickHelperData *FramelessQuickHelperPrivate::getWindowDataMutable() const
{
    Q_Q(const FramelessQuickHelper);
    const QQuickWindow * const window = q->window();
    //Q_ASSERT(window);
    if (!window) {
        return nullptr;
    }
    const WId windowId = window->winId();
    auto it = g_framelessQuickHelperData()->find(windowId);
    if (it == g_framelessQuickHelperData()->end()) {
        it = g_framelessQuickHelperData()->insert(windowId, {});
    }
    return &it.value();
}

void FramelessQuickHelperPrivate::rebindWindow()
{
    Q_Q(FramelessQuickHelper);
    QQuickWindow * const window = q->window();
    if (!window) {
        return;
    }
    QQuickItem * const rootItem = window->contentItem();
    const QObject * const p = q->parent();
    const QQuickItem * const pItem = q->parentItem();
    if (rootItem) {
        if ((pItem != rootItem) || (p != rootItem)) {
            q->setParentItem(rootItem);
            q->setParent(rootItem);
        }
    } else {
        if (pItem != nullptr) {
            q->setParentItem(nullptr);
        }
        if (p != window) {
            q->setParent(window);
        }
    }
    if (m_extendIntoTitleBar.value_or(true)) {
        extendsContentIntoTitleBar(true);
    }
}

FramelessQuickHelper::FramelessQuickHelper(QQuickItem *parent)
    : QQuickItem(parent), d_ptr(new FramelessQuickHelperPrivate(this))
{
}

FramelessQuickHelper::~FramelessQuickHelper() = default;

FramelessQuickHelper *FramelessQuickHelper::get(QObject *object)
{
    Q_ASSERT(object);
    if (!object) {
        return nullptr;
    }
    return FramelessQuickHelperPrivate::findOrCreateFramelessHelper(object);
}

FramelessQuickHelper *FramelessQuickHelper::qmlAttachedProperties(QObject *parentObject)
{
    Q_ASSERT(parentObject);
    if (!parentObject) {
        return nullptr;
    }
    return get(parentObject);
}

QQuickItem *FramelessQuickHelper::titleBarItem() const
{
    Q_D(const FramelessQuickHelper);
    return d->getTitleBarItem();
}

bool FramelessQuickHelper::isWindowFixedSize() const
{
    Q_D(const FramelessQuickHelper);
    return d->isWindowFixedSize();
}

bool FramelessQuickHelper::isBlurBehindWindowEnabled() const
{
    Q_D(const FramelessQuickHelper);
    return d->isBlurBehindWindowEnabled();
}

bool FramelessQuickHelper::isContentExtendedIntoTitleBar() const
{
    Q_D(const FramelessQuickHelper);
    return d->isContentExtendedIntoTitleBar();
}

QuickMicaMaterial *FramelessQuickHelper::micaMaterial() const
{
    Q_D(const FramelessQuickHelper);
    return d->findOrCreateMicaMaterial();
}

QuickWindowBorder *FramelessQuickHelper::windowBorder() const
{
    Q_D(const FramelessQuickHelper);
    return d->findOrCreateWindowBorder();
}

bool FramelessQuickHelper::isReady() const
{
    Q_D(const FramelessQuickHelper);
    return d->isReady();
}

void FramelessQuickHelper::waitForReady()
{
    Q_D(FramelessQuickHelper);
    d->waitForReady();
}

void FramelessQuickHelper::extendsContentIntoTitleBar(const bool value)
{
    Q_D(FramelessQuickHelper);
    d->extendsContentIntoTitleBar(value);
}

void FramelessQuickHelper::setTitleBarItem(QQuickItem *value)
{
    Q_ASSERT(value);
    if (!value) {
        return;
    }
    Q_D(FramelessQuickHelper);
    d->setTitleBarItem(value);
}

void FramelessQuickHelper::setSystemButton(QQuickItem *item, const QuickGlobal::SystemButtonType buttonType)
{
    Q_ASSERT(item);
    Q_ASSERT(buttonType != QuickGlobal::SystemButtonType::Unknown);
    if (!item || (buttonType == QuickGlobal::SystemButtonType::Unknown)) {
        return;
    }
    Q_D(FramelessQuickHelper);
    d->setSystemButton(item, buttonType);
}

void FramelessQuickHelper::setHitTestVisible(QQuickItem *item, const bool visible)
{
    setHitTestVisible_item(item, visible);
}

void FramelessQuickHelper::setHitTestVisible_rect(const QRect &rect, const bool visible)
{
    Q_ASSERT(rect.isValid());
    if (!rect.isValid()) {
        return;
    }
    Q_D(FramelessQuickHelper);
    d->setHitTestVisible(rect, visible);
}

void FramelessQuickHelper::setHitTestVisible_object(QObject *object, const bool visible)
{
    Q_ASSERT(object);
    if (!object) {
        return;
    }
    Q_D(FramelessQuickHelper);
    d->setHitTestVisible(object, visible);
}

void FramelessQuickHelper::setHitTestVisible_item(QQuickItem *item, const bool visible)
{
    Q_ASSERT(item);
    if (!item) {
        return;
    }
    Q_D(FramelessQuickHelper);
    d->setHitTestVisible(item, visible);
}

void FramelessQuickHelper::showSystemMenu(const QPoint &pos)
{
    Q_D(FramelessQuickHelper);
    d->showSystemMenu(pos);
}

void FramelessQuickHelper::windowStartSystemMove2(const QPoint &pos)
{
    Q_D(FramelessQuickHelper);
    d->windowStartSystemMove2(pos);
}

void FramelessQuickHelper::windowStartSystemResize2(const Qt::Edges edges, const QPoint &pos)
{
    if (edges == Qt::Edges{}) {
        return;
    }
    Q_D(FramelessQuickHelper);
    d->windowStartSystemResize2(edges, pos);
}

void FramelessQuickHelper::moveWindowToDesktopCenter()
{
    Q_D(FramelessQuickHelper);
    d->moveWindowToDesktopCenter();
}

void FramelessQuickHelper::bringWindowToFront()
{
    Q_D(FramelessQuickHelper);
    d->bringWindowToFront();
}

void FramelessQuickHelper::setWindowFixedSize(const bool value)
{
    Q_D(FramelessQuickHelper);
    d->setWindowFixedSize(value);
}

void FramelessQuickHelper::setBlurBehindWindowEnabled(const bool value)
{
    Q_D(FramelessQuickHelper);
    d->setBlurBehindWindowEnabled(value, {});
}

void FramelessQuickHelper::itemChange(const ItemChange change, const ItemChangeData &value)
{
    QQuickItem::itemChange(change, value);
    if ((change == ItemSceneChange) && value.window) {
        Q_D(FramelessQuickHelper);
        d->rebindWindow();
    }
}

void FramelessQuickHelper::classBegin()
{
    QQuickItem::classBegin();
}

void FramelessQuickHelper::componentComplete()
{
    QQuickItem::componentComplete();
}

FRAMELESSHELPER_END_NAMESPACE
