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

#include "utils.h"
#include "framelesshelper_windows.h"
#include "framelessmanager.h"
#include "framelessconfig_p.h"
#include "sysapiloader_p.h"
#include "registrykey_p.h"
#include "winverhelper_p.h"
#include "framelesshelpercore_global_p.h"
#include "versionnumber_p.h"
#include "scopeguard_p.h"
#include <optional>
#include <QtCore/qhash.h>
#include <QtCore/qloggingcategory.h>
#include <QtGui/qwindow.h>
#include <QtGui/qguiapplication.h>
#ifndef FRAMELESSHELPER_CORE_NO_PRIVATE
#  include <QtCore/private/qsystemerror_p.h>
#  if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#    include <QtGui/private/qguiapplication_p.h>
#  endif // (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#  include <QtGui/qpa/qplatformwindow.h>
#  if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#    include <QtGui/qpa/qplatformnativeinterface.h>
#  else // (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#    include <QtGui/qpa/qplatformwindow_p.h>
#  endif // (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#endif // FRAMELESSHELPER_CORE_NO_PRIVATE
#include <d2d1.h>

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
Q_DECLARE_METATYPE(QMargins)
#endif // (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))

FRAMELESSHELPER_BEGIN_NAMESPACE

[[maybe_unused]] static Q_LOGGING_CATEGORY(lcUtilsWin, "wangwenx190.framelesshelper.core.utils.win")

#ifdef FRAMELESSHELPER_CORE_NO_DEBUG_OUTPUT
#  define INFO QT_NO_QDEBUG_MACRO()
#  define DEBUG QT_NO_QDEBUG_MACRO()
#  define WARNING QT_NO_QDEBUG_MACRO()
#  define CRITICAL QT_NO_QDEBUG_MACRO()
#else
#  define INFO qCInfo(lcUtilsWin)
#  define DEBUG qCDebug(lcUtilsWin)
#  define WARNING qCWarning(lcUtilsWin)
#  define CRITICAL qCCritical(lcUtilsWin)
#endif

using namespace Global;

static constexpr const char kDpiNoAccessErrorMessage[] =
    "FramelessHelper doesn't have access to change the current process's DPI awareness mode,"
    " most likely due to it has been set externally already. Eg: application manifest file.";
static constexpr const char kQtWindowCustomMarginsVar[] = "_q_windowsCustomMargins";
FRAMELESSHELPER_STRING_CONSTANT2(SuccessMessageText, "The operation completed successfully.")
FRAMELESSHELPER_STRING_CONSTANT2(ErrorMessageTemplate, "Function %1() failed with error code %2: %3.")
FRAMELESSHELPER_STRING_CONSTANT(Composition)
FRAMELESSHELPER_STRING_CONSTANT(ColorizationColor)
FRAMELESSHELPER_STRING_CONSTANT(AppsUseLightTheme)
FRAMELESSHELPER_STRING_CONSTANT(WindowsCustomMargins)
FRAMELESSHELPER_STRING_CONSTANT(user32)
FRAMELESSHELPER_STRING_CONSTANT(dwmapi)
FRAMELESSHELPER_STRING_CONSTANT(winmm)
FRAMELESSHELPER_STRING_CONSTANT(shcore)
FRAMELESSHELPER_STRING_CONSTANT(uxtheme)
FRAMELESSHELPER_STRING_CONSTANT(GetWindowRect)
FRAMELESSHELPER_STRING_CONSTANT(DwmIsCompositionEnabled)
FRAMELESSHELPER_STRING_CONSTANT(SetWindowPos)
FRAMELESSHELPER_STRING_CONSTANT(DwmExtendFrameIntoClientArea)
FRAMELESSHELPER_STRING_CONSTANT(DwmGetColorizationColor)
FRAMELESSHELPER_STRING_CONSTANT(PostMessageW)
FRAMELESSHELPER_STRING_CONSTANT(MonitorFromWindow)
FRAMELESSHELPER_STRING_CONSTANT(GetMonitorInfoW)
FRAMELESSHELPER_STRING_CONSTANT(GetWindowPlacement)
FRAMELESSHELPER_STRING_CONSTANT(QueryPerformanceFrequency)
FRAMELESSHELPER_STRING_CONSTANT(QueryPerformanceCounter)
FRAMELESSHELPER_STRING_CONSTANT(DwmGetCompositionTimingInfo)
FRAMELESSHELPER_STRING_CONSTANT(SystemParametersInfoW)
#ifdef Q_PROCESSOR_X86_64
  FRAMELESSHELPER_STRING_CONSTANT(GetClassLongPtrW)
  FRAMELESSHELPER_STRING_CONSTANT(SetClassLongPtrW)
  FRAMELESSHELPER_STRING_CONSTANT(GetWindowLongPtrW)
  FRAMELESSHELPER_STRING_CONSTANT(SetWindowLongPtrW)
#else // Q_PROCESSOR_X86_64
  // WinUser.h defines G/SetClassLongPtr as G/SetClassLong due to the
  // "Ptr" suffixed APIs are not available on 32-bit platforms, so we
  // have to add the following workaround. Undefine the macros and then
  // redefine them is also an option but the following solution is more simple.
  FRAMELESSHELPER_STRING_CONSTANT2(GetClassLongPtrW, "GetClassLongW")
  FRAMELESSHELPER_STRING_CONSTANT2(SetClassLongPtrW, "SetClassLongW")
  FRAMELESSHELPER_STRING_CONSTANT2(GetWindowLongPtrW, "GetWindowLongW")
  FRAMELESSHELPER_STRING_CONSTANT2(SetWindowLongPtrW, "SetWindowLongW")
#endif // Q_PROCESSOR_X86_64
FRAMELESSHELPER_STRING_CONSTANT(ReleaseCapture)
FRAMELESSHELPER_STRING_CONSTANT(SetWindowTheme)
FRAMELESSHELPER_STRING_CONSTANT(SetProcessDpiAwarenessContext)
FRAMELESSHELPER_STRING_CONSTANT(SetProcessDpiAwareness)
FRAMELESSHELPER_STRING_CONSTANT(SetProcessDPIAware)
FRAMELESSHELPER_STRING_CONSTANT(GetDpiForMonitor)
FRAMELESSHELPER_STRING_CONSTANT(GetDC)
FRAMELESSHELPER_STRING_CONSTANT(ReleaseDC)
FRAMELESSHELPER_STRING_CONSTANT(GetDeviceCaps)
FRAMELESSHELPER_STRING_CONSTANT(DwmSetWindowAttribute)
FRAMELESSHELPER_STRING_CONSTANT(EnableMenuItem)
FRAMELESSHELPER_STRING_CONSTANT(SetMenuDefaultItem)
FRAMELESSHELPER_STRING_CONSTANT(HiliteMenuItem)
FRAMELESSHELPER_STRING_CONSTANT(TrackPopupMenu)
FRAMELESSHELPER_STRING_CONSTANT(ClientToScreen)
FRAMELESSHELPER_STRING_CONSTANT(DwmEnableBlurBehindWindow)
FRAMELESSHELPER_STRING_CONSTANT(SetWindowCompositionAttribute)
FRAMELESSHELPER_STRING_CONSTANT(GetSystemMetricsForDpi)
FRAMELESSHELPER_STRING_CONSTANT(timeGetDevCaps)
FRAMELESSHELPER_STRING_CONSTANT(timeBeginPeriod)
FRAMELESSHELPER_STRING_CONSTANT(timeEndPeriod)
FRAMELESSHELPER_STRING_CONSTANT(GetDpiForWindow)
FRAMELESSHELPER_STRING_CONSTANT(GetSystemDpiForProcess)
FRAMELESSHELPER_STRING_CONSTANT(GetDpiForSystem)
FRAMELESSHELPER_STRING_CONSTANT(DwmGetWindowAttribute)
FRAMELESSHELPER_STRING_CONSTANT(ntdll)
FRAMELESSHELPER_STRING_CONSTANT(RtlGetVersion)
FRAMELESSHELPER_STRING_CONSTANT(GetModuleHandleW)
FRAMELESSHELPER_STRING_CONSTANT(RegisterClassExW)
FRAMELESSHELPER_STRING_CONSTANT(CreateWindowExW)
FRAMELESSHELPER_STRING_CONSTANT(AccentColor)
FRAMELESSHELPER_STRING_CONSTANT(GetScaleFactorForMonitor)
FRAMELESSHELPER_STRING_CONSTANT(WallpaperStyle)
FRAMELESSHELPER_STRING_CONSTANT(TileWallpaper)
FRAMELESSHELPER_STRING_CONSTANT(UnregisterClassW)
FRAMELESSHELPER_STRING_CONSTANT(DestroyWindow)
FRAMELESSHELPER_STRING_CONSTANT(SetWindowThemeAttribute)
FRAMELESSHELPER_STRING_CONSTANT(CreateDCW)
FRAMELESSHELPER_STRING_CONSTANT(DeleteDC)
FRAMELESSHELPER_STRING_CONSTANT(d2d1)
FRAMELESSHELPER_STRING_CONSTANT(D2D1CreateFactory)
FRAMELESSHELPER_STRING_CONSTANT(ReloadSystemMetrics)
FRAMELESSHELPER_STRING_CONSTANT(SetPreferredAppMode)
FRAMELESSHELPER_STRING_CONSTANT(AllowDarkModeForApp)
FRAMELESSHELPER_STRING_CONSTANT(AllowDarkModeForWindow)
FRAMELESSHELPER_STRING_CONSTANT(FlushMenuThemes)
FRAMELESSHELPER_STRING_CONSTANT(RefreshImmersiveColorPolicyState)
FRAMELESSHELPER_STRING_CONSTANT(SetPropW)
FRAMELESSHELPER_STRING_CONSTANT(GetIsImmersiveColorUsingHighContrast)
FRAMELESSHELPER_STRING_CONSTANT(EnableNonClientDpiScaling)
FRAMELESSHELPER_STRING_CONSTANT(GetWindowDpiAwarenessContext)
FRAMELESSHELPER_STRING_CONSTANT(GetAwarenessFromDpiAwarenessContext)
FRAMELESSHELPER_STRING_CONSTANT(GetThreadDpiAwarenessContext)
FRAMELESSHELPER_STRING_CONSTANT(GetDpiAwarenessContextForProcess)
FRAMELESSHELPER_STRING_CONSTANT(GetCurrentProcess)
FRAMELESSHELPER_STRING_CONSTANT(GetProcessDpiAwareness)
FRAMELESSHELPER_STRING_CONSTANT(IsProcessDPIAware)
FRAMELESSHELPER_STRING_CONSTANT(AreDpiAwarenessContextsEqual)
FRAMELESSHELPER_STRING_CONSTANT(GetWindowDPI)
FRAMELESSHELPER_STRING_CONSTANT(AdjustWindowRectExForDpi)
FRAMELESSHELPER_STRING_CONSTANT(GetDpiMetrics)
FRAMELESSHELPER_STRING_CONSTANT(EnablePerMonitorDialogScaling)
FRAMELESSHELPER_STRING_CONSTANT(EnableChildWindowDpiMessage)
FRAMELESSHELPER_STRING_CONSTANT(GetForegroundWindow)
FRAMELESSHELPER_STRING_CONSTANT(SendMessageTimeoutW)
FRAMELESSHELPER_STRING_CONSTANT(AttachThreadInput)
FRAMELESSHELPER_STRING_CONSTANT(BringWindowToTop)
FRAMELESSHELPER_STRING_CONSTANT(SetActiveWindow)
FRAMELESSHELPER_STRING_CONSTANT(RedrawWindow)

struct Win32UtilsData
{
    WNDPROC originalWindowProc = nullptr;
    SystemParameters params = {};
};

struct Win32UtilsInternal
{
    QHash<WId, Win32UtilsData> data = {};
    QList<WId> micaWindowIds = {};
};

Q_GLOBAL_STATIC(Win32UtilsInternal, g_win32UtilsData)

[[nodiscard]] bool operator==(const POINT &lhs, const POINT &rhs) noexcept
{
    return ((lhs.x == rhs.x) && (lhs.y == rhs.y));
}

[[nodiscard]] bool operator!=(const POINT &lhs, const POINT &rhs) noexcept
{
    return !operator==(lhs, rhs);
}

[[nodiscard]] bool operator==(const SIZE &lhs, const SIZE &rhs) noexcept
{
    return ((lhs.cx == rhs.cx) && (lhs.cy == rhs.cy));
}

[[nodiscard]] bool operator!=(const SIZE &lhs, const SIZE &rhs) noexcept
{
    return !operator==(lhs, rhs);
}

[[nodiscard]] bool operator>(const SIZE &lhs, const SIZE &rhs) noexcept
{
    return ((lhs.cx * lhs.cy) > (rhs.cx * rhs.cy));
}

[[nodiscard]] bool operator>=(const SIZE &lhs, const SIZE &rhs) noexcept
{
    return (operator>(lhs, rhs) || operator==(lhs, rhs));
}

[[nodiscard]] bool operator<(const SIZE &lhs, const SIZE &rhs) noexcept
{
    return (operator!=(lhs, rhs) && !operator>(lhs, rhs));
}

[[nodiscard]] bool operator<=(const SIZE &lhs, const SIZE &rhs) noexcept
{
    return (operator<(lhs, rhs) || operator==(lhs, rhs));
}

[[nodiscard]] bool operator==(const RECT &lhs, const RECT &rhs) noexcept
{
    return ((lhs.left == rhs.left) && (lhs.top == rhs.top)
            && (lhs.right == rhs.right) && (lhs.bottom == rhs.bottom));
}

[[nodiscard]] bool operator!=(const RECT &lhs, const RECT &rhs) noexcept
{
    return !operator==(lhs, rhs);
}

[[nodiscard]] QPoint point2qpoint(const POINT &point)
{
    return QPoint{ int(point.x), int(point.y) };
}

[[nodiscard]] POINT qpoint2point(const QPoint &point)
{
    return POINT{ LONG(point.x()), LONG(point.y()) };
}

[[nodiscard]] QSize size2qsize(const SIZE &size)
{
    return QSize{ int(size.cx), int(size.cy) };
}

[[nodiscard]] SIZE qsize2size(const QSize &size)
{
    return SIZE{ LONG(size.width()), LONG(size.height()) };
}

[[nodiscard]] QRect rect2qrect(const RECT &rect)
{
    return QRect{ QPoint{ int(rect.left), int(rect.top) }, QSize{ int(RECT_WIDTH(rect)), int(RECT_HEIGHT(rect)) } };
}

[[nodiscard]] RECT qrect2rect(const QRect &qrect)
{
    return RECT{ LONG(qrect.left()), LONG(qrect.top()), LONG(qrect.right()), LONG(qrect.bottom()) };
}

[[nodiscard]] QString hwnd2str(const WId windowId)
{
    // NULL handle is allowed here.
    return FRAMELESSHELPER_STRING_LITERAL("0x") + QString::number(windowId, 16).toUpper().rightJustified(8, u'0');
}

[[nodiscard]] QString hwnd2str(const HWND hwnd)
{
    // NULL handle is allowed here.
    return hwnd2str(reinterpret_cast<WId>(hwnd));
}

[[nodiscard]] std::optional<MONITORINFOEXW> getMonitorForWindow(const HWND hwnd)
{
    Q_ASSERT(hwnd);
    if (!hwnd) {
        return std::nullopt;
    }
    // Use "MONITOR_DEFAULTTONEAREST" here so that we can still get the correct
    // monitor even if the window is minimized.
    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (!monitor) {
        WARNING << Utils::getSystemErrorMessage(kMonitorFromWindow);
        return std::nullopt;
    }
    MONITORINFOEXW monitorInfo;
    SecureZeroMemory(&monitorInfo, sizeof(monitorInfo));
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (GetMonitorInfoW(monitor, &monitorInfo) == FALSE) {
        WARNING << Utils::getSystemErrorMessage(kGetMonitorInfoW);
        return std::nullopt;
    }
    return monitorInfo;
};

[[maybe_unused]] [[nodiscard]] static inline QString qtWindowCustomMarginsProp()
{
    static const QString prop = QString::fromUtf8(kQtWindowCustomMarginsVar);
    return prop;
}

[[nodiscard]] static inline QString dwmRegistryKey()
{
    static const QString key = QString::fromWCharArray(kDwmRegistryKey);
    return key;
}

[[nodiscard]] static inline QString personalizeRegistryKey()
{
    static const QString key = QString::fromWCharArray(kPersonalizeRegistryKey);
    return key;
}

[[nodiscard]] static inline QString desktopRegistryKey()
{
    static const QString key = QString::fromWCharArray(kDesktopRegistryKey);
    return key;
}

[[nodiscard]] static inline QString dwmColorKeyName()
{
    static const QString name = QString::fromWCharArray(kDwmColorKeyName);
    return name;
}

[[nodiscard]] static inline bool doCompareWindowsVersion(const VersionNumber &targetOsVer)
{
    static const auto currentOsVer = []() -> std::optional<VersionNumber> {
        if (API_NT_AVAILABLE(RtlGetVersion)) {
            using RtlGetVersionPtr = _NTSTATUS(WINAPI *)(PRTL_OSVERSIONINFOW);
            const auto pRtlGetVersion =
                reinterpret_cast<RtlGetVersionPtr>(SysApiLoader::instance()->get(kntdll, kRtlGetVersion));
            RTL_OSVERSIONINFOEXW osvi;
            SecureZeroMemory(&osvi, sizeof(osvi));
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            if (pRtlGetVersion(reinterpret_cast<PRTL_OSVERSIONINFOW>(&osvi)) == _STATUS_SUCCESS) {
                return VersionNumber{int(osvi.dwMajorVersion), int(osvi.dwMinorVersion), int(osvi.dwBuildNumber)};
            }
        }
        return std::nullopt;
    }();
    if (currentOsVer.has_value()) {
        return (currentOsVer >= targetOsVer);
    }
    // We can fallback to "VerifyVersionInfoW" if we can't determine the current system
    // version, but this function will be affected by the manifest file of your application.
    // For example, if you don't claim your application supports Windows 10 explicitly
    // in the manifest file, Windows will assume your application only supports up to Windows
    // 8.1, so this function will be told the current system is at most Windows 8.1, to keep
    // good backward-compatiability. This behavior usually won't cause any issues if you
    // always use an appropriate manifest file for your application, however, it does cause
    // some issues for people who don't use the manifest file at all. There have been some
    // bug reports about it already.
    OSVERSIONINFOEXW osvi;
    SecureZeroMemory(&osvi, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = targetOsVer.Major;
    osvi.dwMinorVersion = targetOsVer.Minor;
    osvi.dwBuildNumber = targetOsVer.Patch;
    DWORDLONG dwlConditionMask = 0;
    static constexpr const auto op = VER_GREATER_EQUAL;
    VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, op);
    VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, op);
    VER_SET_CONDITION(dwlConditionMask, VER_BUILDNUMBER, op);
    return (VerifyVersionInfoW(&osvi, (VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER), dwlConditionMask) != FALSE);
}

[[nodiscard]] static inline QString getSystemErrorMessageImpl(const QString &function, const DWORD code)
{
    Q_ASSERT(!function.isEmpty());
    if (function.isEmpty()) {
        return {};
    }
    if (code == ERROR_SUCCESS) {
        return kSuccessMessageText;
    }
#ifdef FRAMELESSHELPER_CORE_NO_PRIVATE
    LPWSTR buf = nullptr;
    if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&buf), 0, nullptr) == 0) {
        return FRAMELESSHELPER_STRING_LITERAL("FormatMessageW() returned empty string.");
    }
    const QString errorText = QString::fromWCharArray(buf).trimmed();
    LocalFree(buf);
    buf = nullptr;
    return kErrorMessageTemplate.arg(function, QString::number(code), errorText);
#else // !FRAMELESSHELPER_CORE_NO_PRIVATE
    const QString errorText = QSystemError::windowsString(code);
    return kErrorMessageTemplate.arg(function, QString::number(code), errorText);
#endif // FRAMELESSHELPER_CORE_NO_PRIVATE
}

[[nodiscard]] static inline QString getSystemErrorMessageImpl(const QString &function, const HRESULT hr)
{
    Q_ASSERT(!function.isEmpty());
    if (function.isEmpty()) {
        return {};
    }
    if (SUCCEEDED(hr)) {
        return kSuccessMessageText;
    }
    const DWORD dwError = HRESULT_CODE(hr);
    return getSystemErrorMessageImpl(function, dwError);
}

static inline void moveWindowToMonitor(const HWND hwnd, const MONITORINFOEXW &activeMonitor)
{
    Q_ASSERT(hwnd);
    if (!hwnd) {
        return;
    }
    const std::optional<MONITORINFOEXW> currentMonitor = getMonitorForWindow(hwnd);
    if (!currentMonitor.has_value()) {
        WARNING << "Failed to retrieve the window's monitor.";
        return;
    }
    const RECT currentMonitorRect = currentMonitor.value().rcMonitor;
    const RECT activeMonitorRect = activeMonitor.rcMonitor;
    // We are in the same monitor, nothing to adjust here.
    if (currentMonitorRect == activeMonitorRect) {
        return;
    }
    RECT currentWindowRect = {};
    if (GetWindowRect(hwnd, &currentWindowRect) == FALSE) {
        WARNING << Utils::getSystemErrorMessage(kGetWindowRect);
        return;
    }
    const int currentWindowWidth = (currentWindowRect.right - currentWindowRect.left);
    const int currentWindowHeight = (currentWindowRect.bottom - currentWindowRect.top);
    const int currentWindowOffsetX = (currentWindowRect.left - currentMonitorRect.left);
    const int currentWindowOffsetY = (currentWindowRect.top - currentMonitorRect.top);
    const int newWindowX = (activeMonitorRect.left + currentWindowOffsetX);
    const int newWindowY = (activeMonitorRect.top + currentWindowOffsetY);
    static constexpr const UINT flags =
        (SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    if (SetWindowPos(hwnd, nullptr, newWindowX, newWindowY,
          currentWindowWidth, currentWindowHeight, flags) == FALSE) {
        WARNING << Utils::getSystemErrorMessage(kSetWindowPos);
    }
}

[[nodiscard]] static inline int getSystemMetrics2(const int index, const bool horizontal,
                                                  const quint32 dpi)
{
    Q_ASSERT(dpi != 0);
    if (dpi == 0) {
        return 0;
    }
    if (const int result = _GetSystemMetricsForDpi2(index, dpi); result > 0) {
        return result;
    }
    static constexpr const auto defaultDpi = qreal(USER_DEFAULT_SCREEN_DPI);
    const qreal currentDpr = (qreal(Utils::getPrimaryScreenDpi(horizontal)) / defaultDpi);
    const qreal requestedDpr = (qreal(dpi) / defaultDpi);
    return std::round(qreal(GetSystemMetrics(index)) / currentDpr * requestedDpr);
}

[[nodiscard]] static inline int getSystemMetrics2(const WId windowId, const int index,
                                                  const bool horizontal, const bool scaled)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return 0;
    }
    const UINT realDpi = Utils::getWindowDpi(windowId, horizontal);
    {
        const UINT dpi = (scaled ? realDpi : USER_DEFAULT_SCREEN_DPI);
        if (const int result = _GetSystemMetricsForDpi2(index, dpi); result > 0) {
            return result;
        }
    }
    // GetSystemMetrics() will always return a scaled value, so if we want to get an unscaled
    // one, we have to calculate it ourself.
    const qreal dpr = (scaled ? qreal(1) : (qreal(realDpi) / qreal(USER_DEFAULT_SCREEN_DPI)));
    return std::round(qreal(GetSystemMetrics(index)) / dpr);
}

[[maybe_unused]] [[nodiscard]] static inline
    DWORD qtEdgesToWin32Orientation(const Qt::Edges edges)
{
    if (edges == Qt::Edges{}) {
        return 0;
    }
    if (edges == (Qt::LeftEdge)) {
        return 0xF001; // SC_SIZELEFT
    } else if (edges == (Qt::RightEdge)) {
        return 0xF002; // SC_SIZERIGHT
    } else if (edges == (Qt::TopEdge)) {
        return 0xF003; // SC_SIZETOP
    } else if (edges == (Qt::TopEdge | Qt::LeftEdge)) {
        return 0xF004; // SC_SIZETOPLEFT
    } else if (edges == (Qt::TopEdge | Qt::RightEdge)) {
        return 0xF005; // SC_SIZETOPRIGHT
    } else if (edges == (Qt::BottomEdge)) {
        return 0xF006; // SC_SIZEBOTTOM
    } else if (edges == (Qt::BottomEdge | Qt::LeftEdge)) {
        return 0xF007; // SC_SIZEBOTTOMLEFT
    } else if (edges == (Qt::BottomEdge | Qt::RightEdge)) {
        return 0xF008; // SC_SIZEBOTTOMRIGHT
    } else {
        return 0xF000; // SC_SIZE
    }
}

[[nodiscard]] static inline LRESULT CALLBACK SystemMenuHookWindowProc
    (const HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    Q_ASSERT(hWnd);
    if (!hWnd) {
        return 0;
    }
    const auto windowId = reinterpret_cast<WId>(hWnd);
    const auto it = g_win32UtilsData()->data.constFind(windowId);
    if (it == g_win32UtilsData()->data.constEnd()) {
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    const Win32UtilsData &data = it.value();
    const auto getNativePosFromMouse = [lParam]() -> QPoint {
        return {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    };
    const auto getNativeGlobalPosFromKeyboard = [hWnd, windowId]() -> QPoint {
        RECT windowPos = {};
        if (GetWindowRect(hWnd, &windowPos) == FALSE) {
            WARNING << Utils::getSystemErrorMessage(kGetWindowRect);
            return {};
        }
        const bool maxOrFull = (IsMaximized(hWnd) || Utils::isFullScreen(windowId));
        const int frameSizeX = Utils::getResizeBorderThickness(windowId, true, true);
        const bool frameBorderVisible = Utils::isWindowFrameBorderVisible();
        const int horizontalOffset = ((maxOrFull || !frameBorderVisible) ? 0 : frameSizeX);
        const auto verticalOffset = [windowId, frameBorderVisible, maxOrFull]() -> int {
            const int titleBarHeight = Utils::getTitleBarHeight(windowId, true);
            if (!frameBorderVisible) {
                return titleBarHeight;
            }
            const int frameSizeY = Utils::getResizeBorderThickness(windowId, false, true);
            if (WindowsVersionHelper::isWin11OrGreater()) {
                if (maxOrFull) {
                    return (titleBarHeight + frameSizeY);
                }
                return titleBarHeight;
            }
            if (maxOrFull) {
                return titleBarHeight;
            }
            return (titleBarHeight - frameSizeY);
        }();
        return {windowPos.left + horizontalOffset, windowPos.top + verticalOffset};
    };
    bool shouldShowSystemMenu = false;
    bool broughtByKeyboard = false;
    QPoint nativeGlobalPos = {};
    switch (uMsg) {
    case WM_RBUTTONUP: {
        const QPoint nativeLocalPos = getNativePosFromMouse();
        const QPoint qtScenePos = Utils::fromNativeLocalPosition(data.params.getWindowHandle(), nativeLocalPos);
        if (data.params.isInsideTitleBarDraggableArea(qtScenePos)) {
            POINT pos = {nativeLocalPos.x(), nativeLocalPos.y()};
            if (ClientToScreen(hWnd, &pos) == FALSE) {
                WARNING << Utils::getSystemErrorMessage(kClientToScreen);
                break;
            }
            shouldShowSystemMenu = true;
            nativeGlobalPos = {pos.x, pos.y};
        }
    } break;
    case WM_NCRBUTTONUP: {
        if (wParam == HTCAPTION) {
            shouldShowSystemMenu = true;
            nativeGlobalPos = getNativePosFromMouse();
        }
    } break;
    case WM_SYSCOMMAND: {
        const WPARAM filteredWParam = (wParam & 0xFFF0);
        if ((filteredWParam == SC_KEYMENU) && (lParam == VK_SPACE)) {
            shouldShowSystemMenu = true;
            broughtByKeyboard = true;
            nativeGlobalPos = getNativeGlobalPosFromKeyboard();
        }
    } break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        const bool altPressed = ((wParam == VK_MENU) || (GetKeyState(VK_MENU) < 0));
        const bool spacePressed = ((wParam == VK_SPACE) || (GetKeyState(VK_SPACE) < 0));
        if (altPressed && spacePressed) {
            shouldShowSystemMenu = true;
            broughtByKeyboard = true;
            nativeGlobalPos = getNativeGlobalPosFromKeyboard();
        }
    } break;
    default:
        break;
    }
    if (shouldShowSystemMenu) {
        Utils::showSystemMenu(windowId, nativeGlobalPos, broughtByKeyboard, &data.params);
        // QPA's internal code will handle system menu events separately, and its
        // behavior is not what we would want to see because it doesn't know our
        // window doesn't have any window frame now, so return early here to avoid
        // entering Qt's own handling logic.
        return 0; // Return 0 means we have handled this event.
    }
    Q_ASSERT(data.originalWindowProc);
    if (data.originalWindowProc) {
        // Hand over to Qt's original window proc function for events we are not
        // interested in.
        return CallWindowProcW(data.originalWindowProc, hWnd, uMsg, wParam, lParam);
    } else {
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}

bool Utils::isWindowsVersionOrGreater(const WindowsVersion version)
{
    return doCompareWindowsVersion(WindowsVersions.at(static_cast<int>(version)));
}

bool Utils::isDwmCompositionEnabled()
{
    // DWM composition is always enabled and can't be disabled since Windows 8.
    if (WindowsVersionHelper::isWin8OrGreater()) {
        return true;
    }
    const auto resultFromRegistry = []() -> bool {
        const RegistryKey registry(RegistryRootKey::CurrentUser, dwmRegistryKey());
        if (!registry.isValid()) {
            return false;
        }
        const DWORD value = registry.value<DWORD>(kComposition).value_or(0);
        return (value != 0);
    };
    if (!API_DWM_AVAILABLE(DwmIsCompositionEnabled)) {
        return resultFromRegistry();
    }
    BOOL enabled = FALSE;
    const HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmIsCompositionEnabled, &enabled);
    if (FAILED(hr)) {
        WARNING << getSystemErrorMessageImpl(kDwmIsCompositionEnabled, hr);
        return resultFromRegistry();
    }
    return (enabled != FALSE);
}

void Utils::triggerFrameChange(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    static constexpr const UINT swpFlags =
        (SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOSIZE
        | SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    if (SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, swpFlags) == FALSE) {
        WARNING << getSystemErrorMessage(kSetWindowPos);
        return;
    }
    static constexpr const UINT rdwFlags =
        (RDW_ERASE | RDW_FRAME | RDW_INVALIDATE
        | RDW_UPDATENOW | RDW_ALLCHILDREN);
    if (RedrawWindow(hwnd, nullptr, nullptr, rdwFlags) == FALSE) {
        WARNING << getSystemErrorMessage(kRedrawWindow);
    }
}

void Utils::updateWindowFrameMargins(const WId windowId, const bool reset)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    // We can't extend the window frame when DWM composition is disabled.
    // No need to try further in this case.
    if (!isDwmCompositionEnabled()) {
        return;
    }
    if (!API_DWM_AVAILABLE(DwmExtendFrameIntoClientArea)) {
        return;
    }
    const bool micaEnabled = g_win32UtilsData()->micaWindowIds.contains(windowId);
    const auto margins = [micaEnabled, reset]() -> MARGINS {
        // To make Mica/Mica Alt work for normal Win32 windows, we have to
        // let the window frame extend to the whole window (or disable the
        // redirection surface, but this will break GDI's rendering, so we
        // can't do this, unfortunately), so we can't change the window frame
        // margins in this case, otherwise Mica/Mica Alt will be broken.
        if (micaEnabled) {
            return {-1, -1, -1, -1};
        }
        if (reset || isWindowFrameBorderVisible()) {
            return {0, 0, 0, 0};
        }
        return {1, 1, 1, 1};
    }();
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    const HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmExtendFrameIntoClientArea, hwnd, &margins);
    if (FAILED(hr)) {
        WARNING << getSystemErrorMessageImpl(kDwmExtendFrameIntoClientArea, hr);
        return;
    }
    triggerFrameChange(windowId);
}

void Utils::updateInternalWindowFrameMargins(QWindow *window, const bool enable)
{
    Q_ASSERT(window);
    if (!window) {
        return;
    }
    const WId windowId = window->winId();
    const auto margins = [enable, windowId]() -> QMargins {
        if (!enable) {
            return {};
        }
        const int titleBarHeight = getTitleBarHeight(windowId, true);
        if (isWindowFrameBorderVisible()) {
            return {0, -titleBarHeight, 0, 0};
        } else {
            const int frameSizeX = getResizeBorderThickness(windowId, true, true);
            const int frameSizeY = getResizeBorderThickness(windowId, false, true);
            return {-frameSizeX, -titleBarHeight, -frameSizeX, -frameSizeY};
        }
    }();
    const QVariant marginsVar = QVariant::fromValue(margins);
    window->setProperty(kQtWindowCustomMarginsVar, marginsVar);
#ifndef FRAMELESSHELPER_CORE_NO_PRIVATE
#  if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    if (QPlatformWindow *platformWindow = window->handle()) {
        if (const auto ni = QGuiApplication::platformNativeInterface()) {
            ni->setWindowProperty(platformWindow, qtWindowCustomMarginsProp(), marginsVar);
        } else {
            WARNING << "Failed to retrieve the platform native interface.";
            return;
        }
    } else {
        WARNING << "Failed to retrieve the platform window.";
        return;
    }
#  else // (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    if (const auto platformWindow = dynamic_cast<QNativeInterface::Private::QWindowsWindow *>(window->handle())) {
        platformWindow->setCustomMargins(margins);
    } else {
        WARNING << "Failed to retrieve the platform window.";
        return;
    }
#  endif // (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#endif // FRAMELESSHELPER_CORE_NO_PRIVATE
    triggerFrameChange(windowId);
}

QString Utils::getSystemErrorMessage(const QString &function)
{
    Q_ASSERT(!function.isEmpty());
    if (function.isEmpty()) {
        return {};
    }
    const DWORD code = GetLastError();
    if (code == ERROR_SUCCESS) {
        return {};
    }
    return getSystemErrorMessageImpl(function, code);
}

QColor Utils::getDwmColorizationColor(bool *opaque, bool *ok)
{
    const auto resultFromRegistry = []() -> QColor {
        const RegistryKey registry(RegistryRootKey::CurrentUser, dwmRegistryKey());
        if (!registry.isValid()) {
            return kDefaultDarkGrayColor;
        }
        const QVariant value = registry.value(kColorizationColor);
        if (!value.isValid()) {
            return kDefaultDarkGrayColor;
        }
        return QColor::fromRgba(qvariant_cast<DWORD>(value));
    };
    if (!API_DWM_AVAILABLE(DwmGetColorizationColor)) {
        if (ok) {
            *ok = false;
        }
        return resultFromRegistry();
    }
    DWORD color = 0;
    BOOL bOpaque = FALSE;
    const HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmGetColorizationColor, &color, &bOpaque);
    if (FAILED(hr)) {
        WARNING << getSystemErrorMessageImpl(kDwmGetColorizationColor, hr);
        if (ok) {
            *ok = false;
        }
        return resultFromRegistry();
    }
    if (opaque) {
        *opaque = (bOpaque != FALSE);
    }
    if (ok) {
        *ok = true;
    }
    return QColor::fromRgba(color);
}

DwmColorizationArea Utils::getDwmColorizationArea()
{
    // It's a Win10 only feature. (TO BE VERIFIED)
    if (!WindowsVersionHelper::isWin10OrGreater()) {
        return DwmColorizationArea::None;
    }
    const RegistryKey themeRegistry(RegistryRootKey::CurrentUser, personalizeRegistryKey());
    const DWORD themeValue = themeRegistry.isValid() ? themeRegistry.value<DWORD>(dwmColorKeyName()).value_or(0) : 0;
    const RegistryKey dwmRegistry(RegistryRootKey::CurrentUser, dwmRegistryKey());
    const DWORD dwmValue = dwmRegistry.isValid() ? dwmRegistry.value<DWORD>(dwmColorKeyName()).value_or(0) : 0;
    const bool theme = (themeValue != 0);
    const bool dwm = (dwmValue != 0);
    if (theme && dwm) {
        return DwmColorizationArea::All;
    } else if (theme) {
        return DwmColorizationArea::StartMenu_TaskBar_ActionCenter;
    } else if (dwm) {
        return DwmColorizationArea::TitleBar_WindowBorder;
    }
    return DwmColorizationArea::None;
}

void Utils::showSystemMenu(const WId windowId, const QPoint &pos, const bool selectFirstEntry,
                           FramelessParamsConst params)
{
    Q_ASSERT(windowId);
    Q_ASSERT(params);
    if (!windowId || !params) {
        return;
    }

    const auto hWnd = reinterpret_cast<HWND>(windowId);
    const HMENU hMenu = GetSystemMenu(hWnd, FALSE);
    if (!hMenu) {
        // The corresponding window doesn't have a system menu, most likely due to the
        // lack of the "WS_SYSMENU" window style. This situation should not be treated
        // as an error so just ignore it and return early.
        return;
    }

    // Tweak the menu items according to the current window status and user settings.
    const bool disableRestore = params->getProperty(kSysMenuDisableRestoreVar, false).toBool();
    const bool disableMinimize = params->getProperty(kSysMenuDisableMinimizeVar, false).toBool();
    const bool disableMaximize = params->getProperty(kSysMenuDisableMaximizeVar, false).toBool();
    const bool maxOrFull = (IsMaximized(hWnd) || isFullScreen(windowId));
    const bool fixedSize = params->isWindowFixedSize();
    EnableMenuItem(hMenu, SC_RESTORE, (MF_BYCOMMAND | ((maxOrFull && !fixedSize && !disableRestore) ? MFS_ENABLED : MFS_DISABLED)));
    // The first menu item should be selected by default if the menu is brought
    // up by keyboard. I don't know how to pre-select a menu item but it seems
    // highlight can do the job. However, there's an annoying issue if we do
    // this manually: the highlighted menu item is really only highlighted,
    // not selected, so even if the mouse cursor hovers on other menu items
    // or the user navigates to other menu items through keyboard, the original
    // highlight bar will not move accordingly, the OS will generate another
    // highlight bar to indicate the current selected menu item, which will make
    // the menu look kind of weird. Currently I don't know how to fix this issue.
    HiliteMenuItem(hWnd, hMenu, SC_RESTORE, (MF_BYCOMMAND | (selectFirstEntry ? MFS_HILITE : MFS_UNHILITE)));
    EnableMenuItem(hMenu, SC_MOVE, (MF_BYCOMMAND | (!maxOrFull ? MFS_ENABLED : MFS_DISABLED)));
    EnableMenuItem(hMenu, SC_SIZE, (MF_BYCOMMAND | ((!maxOrFull && !fixedSize && !(disableMinimize || disableMaximize)) ? MFS_ENABLED : MFS_DISABLED)));
    EnableMenuItem(hMenu, SC_MINIMIZE, (MF_BYCOMMAND | (disableMinimize ? MFS_DISABLED : MFS_ENABLED)));
    EnableMenuItem(hMenu, SC_MAXIMIZE, (MF_BYCOMMAND | ((!maxOrFull && !fixedSize && !disableMaximize) ? MFS_ENABLED : MFS_DISABLED)));
    EnableMenuItem(hMenu, SC_CLOSE, (MF_BYCOMMAND | MFS_ENABLED));

    // The default menu item will appear in bold font. There can only be one default
    // menu item per menu at most. Set the item ID to "UINT_MAX" (or simply "-1")
    // can clear the default item for the given menu.
    SetMenuDefaultItem(hMenu, SC_CLOSE, FALSE);

    // Popup the system menu at the required position.
    const int result = TrackPopupMenu(hMenu, (TPM_RETURNCMD | (QGuiApplication::isRightToLeft()
                            ? TPM_RIGHTALIGN : TPM_LEFTALIGN)), pos.x(), pos.y(), 0, hWnd, nullptr);

    // Unhighlight the first menu item after the popup menu is closed, otherwise it will keep
    // highlighting until we unhighlight it manually.
    HiliteMenuItem(hWnd, hMenu, SC_RESTORE, (MF_BYCOMMAND | MFS_UNHILITE));

    if (result == 0) {
        // The user canceled the menu, no need to continue.
        return;
    }

    // Send the command that the user choses to the corresponding window.
    if (PostMessageW(hWnd, WM_SYSCOMMAND, result, 0) == FALSE) {
        WARNING << getSystemErrorMessage(kPostMessageW);
    }
}

bool Utils::isFullScreen(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return false;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    RECT windowRect = {};
    if (GetWindowRect(hwnd, &windowRect) == FALSE) {
        WARNING << getSystemErrorMessage(kGetWindowRect);
        return false;
    }
    const std::optional<MONITORINFOEXW> mi = getMonitorForWindow(hwnd);
    if (!mi.has_value()) {
        WARNING << "Failed to retrieve the window's monitor.";
        return false;
    }
    // Compare to the full area of the screen, not the work area.
    return (windowRect == mi.value().rcMonitor);
}

bool Utils::isWindowNoState(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return false;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    WINDOWPLACEMENT wp;
    SecureZeroMemory(&wp, sizeof(wp));
    wp.length = sizeof(wp); // This line is important! Don't miss it!
    if (GetWindowPlacement(hwnd, &wp) == FALSE) {
        WARNING << getSystemErrorMessage(kGetWindowPlacement);
        return false;
    }
    return ((wp.showCmd == SW_NORMAL) || (wp.showCmd == SW_RESTORE));
}

void Utils::syncWmPaintWithDwm()
{
    // No need to sync with DWM if DWM composition is disabled.
    if (!isDwmCompositionEnabled()) {
        return;
    }
    if (!(API_WINMM_AVAILABLE(timeGetDevCaps)
        && API_WINMM_AVAILABLE(timeBeginPeriod)
        && API_WINMM_AVAILABLE(timeEndPeriod)
        && API_DWM_AVAILABLE(DwmGetCompositionTimingInfo))) {
        return;
    }
    // Dirty hack to workaround the resize flicker caused by DWM.
    LARGE_INTEGER freq = {};
    if (QueryPerformanceFrequency(&freq) == FALSE) {
        WARNING << getSystemErrorMessage(kQueryPerformanceFrequency);
        return;
    }
    _TIMECAPS tc = {};
    if (API_CALL_FUNCTION4(winmm, timeGetDevCaps, &tc, sizeof(tc)) != MMSYSERR_NOERROR) {
        WARNING << "timeGetDevCaps() failed.";
        return;
    }
    const UINT ms_granularity = tc.wPeriodMin;
    if (API_CALL_FUNCTION4(winmm, timeBeginPeriod, ms_granularity) != TIMERR_NOERROR) {
        WARNING << "timeBeginPeriod() failed.";
        return;
    }
    LARGE_INTEGER now0 = {};
    if (QueryPerformanceCounter(&now0) == FALSE) {
        WARNING << getSystemErrorMessage(kQueryPerformanceCounter);
        return;
    }
    // ask DWM where the vertical blank falls
    DWM_TIMING_INFO dti;
    SecureZeroMemory(&dti, sizeof(dti));
    dti.cbSize = sizeof(dti);
    const HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmGetCompositionTimingInfo, nullptr, &dti);
    if (FAILED(hr)) {
        WARNING << getSystemErrorMessage(kDwmGetCompositionTimingInfo);
        return;
    }
    LARGE_INTEGER now1 = {};
    if (QueryPerformanceCounter(&now1) == FALSE) {
        WARNING << getSystemErrorMessage(kQueryPerformanceCounter);
        return;
    }
    // - DWM told us about SOME vertical blank
    //   - past or future, possibly many frames away
    // - convert that into the NEXT vertical blank
    const LONGLONG period = dti.qpcRefreshPeriod;
    const LONGLONG dt = dti.qpcVBlank - now1.QuadPart;
    LONGLONG w = 0, m = 0;
    if (dt >= 0) {
        w = dt / period;
    } else {
        // reach back to previous period
        // - so m represents consistent position within phase
        w = -1 + dt / period;
    }
    m = dt - (period * w);
    Q_ASSERT(m >= 0);
    Q_ASSERT(m < period);
    const qreal m_ms = (qreal(1000) * qreal(m) / qreal(freq.QuadPart));
    Sleep(static_cast<DWORD>(std::round(m_ms)));
    if (API_CALL_FUNCTION4(winmm, timeEndPeriod, ms_granularity) != TIMERR_NOERROR) {
        WARNING << "timeEndPeriod() failed.";
    }
}

bool Utils::isHighContrastModeEnabled()
{
    HIGHCONTRASTW hc;
    SecureZeroMemory(&hc, sizeof(hc));
    hc.cbSize = sizeof(hc);
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0) == FALSE) {
        WARNING << getSystemErrorMessage(kSystemParametersInfoW);
        return false;
    }
    return (hc.dwFlags & HCF_HIGHCONTRASTON);
}

quint32 Utils::getPrimaryScreenDpi(const bool horizontal)
{
    // GetDesktopWindow(): The desktop window will always be in the primary monitor.
    if (const HMONITOR hMonitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY)) {
        // GetDpiForMonitor() is only available on Windows 8 and onwards.
        if (API_SHCORE_AVAILABLE(GetDpiForMonitor)) {
            UINT dpiX = 0, dpiY = 0;
            const HRESULT hr = API_CALL_FUNCTION4(shcore, GetDpiForMonitor, hMonitor, _MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
            if (SUCCEEDED(hr) && (dpiX > 0) && (dpiY > 0)) {
                return (horizontal ? dpiX : dpiY);
            } else {
                WARNING << getSystemErrorMessageImpl(kGetDpiForMonitor, hr);
            }
        }
        // GetScaleFactorForMonitor() is only available on Windows 8 and onwards.
        if (API_SHCORE_AVAILABLE(GetScaleFactorForMonitor)) {
            _DEVICE_SCALE_FACTOR factor = _DEVICE_SCALE_FACTOR_INVALID;
            const HRESULT hr = API_CALL_FUNCTION4(shcore, GetScaleFactorForMonitor, hMonitor, &factor);
            if (SUCCEEDED(hr) && (factor != _DEVICE_SCALE_FACTOR_INVALID)) {
                return quint32(std::round(qreal(USER_DEFAULT_SCREEN_DPI) * qreal(factor) / qreal(100)));
            } else {
                WARNING << getSystemErrorMessageImpl(kGetScaleFactorForMonitor, hr);
            }
        }
        // This solution is supported on Windows 2000 and onwards.
        MONITORINFOEXW monitorInfo;
        SecureZeroMemory(&monitorInfo, sizeof(monitorInfo));
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (GetMonitorInfoW(hMonitor, &monitorInfo) != FALSE) {
            if (const HDC hdc = CreateDCW(monitorInfo.szDevice, monitorInfo.szDevice, nullptr, nullptr)) {
                bool valid = false;
                const int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
                const int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
                if ((dpiX > 0) && (dpiY > 0)) {
                    valid = true;
                } else {
                    WARNING << getSystemErrorMessage(kGetDeviceCaps);
                }
                if (DeleteDC(hdc) == FALSE) {
                    WARNING << getSystemErrorMessage(kDeleteDC);
                }
                if (valid) {
                    return (horizontal ? dpiX : dpiY);
                }
            } else {
                WARNING << getSystemErrorMessage(kCreateDCW);
            }
        } else {
            WARNING << getSystemErrorMessage(kGetMonitorInfoW);
        }
    } else {
        WARNING << getSystemErrorMessage(kMonitorFromWindow);
    }

    // Using Direct2D to get the primary monitor's DPI is only available on Windows 7
    // and onwards, but it has been marked as deprecated by Microsoft.
    if (API_D2D_AVAILABLE(D2D1CreateFactory)) {
        using D2D1CreateFactoryPtr =
            HRESULT(WINAPI *)(D2D1_FACTORY_TYPE, REFIID, CONST D2D1_FACTORY_OPTIONS *, void **);
        const auto pD2D1CreateFactory =
            reinterpret_cast<D2D1CreateFactoryPtr>(SysApiLoader::instance()->get(kd2d1, kD2D1CreateFactory));
        ID2D1Factory *d2dFactory = nullptr;
        HRESULT hr = pD2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory),
                                        nullptr, reinterpret_cast<void **>(&d2dFactory));
        if (SUCCEEDED(hr)) {
            // We want to get the newest system DPI, so refresh the system metrics
            // manually to ensure that.
            hr = d2dFactory->ReloadSystemMetrics();
            if (SUCCEEDED(hr)) {
                FLOAT dpiX = FLOAT(0), dpiY = FLOAT(0);
                QT_WARNING_PUSH
                QT_WARNING_DISABLE_DEPRECATED
                d2dFactory->GetDesktopDpi(&dpiX, &dpiY);
                QT_WARNING_POP
                if ((dpiX > FLOAT(0)) && (dpiY > FLOAT(0))) {
                    return (horizontal ? quint32(std::round(dpiX)) : quint32(std::round(dpiY)));
                } else {
                    WARNING << "GetDesktopDpi() failed.";
                }
            } else {
                WARNING << getSystemErrorMessageImpl(kReloadSystemMetrics, hr);
            }
        } else {
            WARNING << getSystemErrorMessageImpl(kD2D1CreateFactory, hr);
        }
        if (d2dFactory) {
            d2dFactory->Release();
            d2dFactory = nullptr;
        }
    }

    // Our last hope to get the DPI of the primary monitor, if all the above
    // solutions failed, however, it won't happen in most cases.
    if (const HDC hdc = GetDC(nullptr)) {
        bool valid = false;
        const int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        const int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
        if ((dpiX > 0) && (dpiY > 0)) {
            valid = true;
        } else {
            WARNING << getSystemErrorMessage(kGetDeviceCaps);
        }
        if (ReleaseDC(nullptr, hdc) == 0) {
            WARNING << getSystemErrorMessage(kReleaseDC);
        }
        if (valid) {
            return (horizontal ? dpiX : dpiY);
        }
    } else {
        WARNING << getSystemErrorMessage(kGetDC);
    }

    // We should never go here, but let's make it extra safe. Just assume we
    // are not scaled (96 DPI) if we really can't get the real DPI.
    return USER_DEFAULT_SCREEN_DPI;
}

quint32 Utils::getWindowDpi(const WId windowId, const bool horizontal)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return USER_DEFAULT_SCREEN_DPI;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    {
        if (const UINT dpi = _GetDpiForWindow2(hwnd)) {
            return dpi;
        }
        // ERROR_CALL_NOT_IMPLEMENTED: the function is not available on
        // current platform, not an error.
        if (GetLastError() != ERROR_CALL_NOT_IMPLEMENTED) {
            WARNING << getSystemErrorMessage(kGetDpiForWindow);
        }
    }
    if (API_USER_AVAILABLE(GetSystemDpiForProcess)) {
        const HANDLE process = GetCurrentProcess();
        if (process) {
            const UINT dpi = API_CALL_FUNCTION4(user32, GetSystemDpiForProcess, process);
            if (dpi > 0) {
                return dpi;
            } else {
                WARNING << getSystemErrorMessage(kGetSystemDpiForProcess);
            }
        } else {
            WARNING << getSystemErrorMessage(kGetCurrentProcess);
        }
    }
    if (API_USER_AVAILABLE(GetDpiForSystem)) {
        const UINT dpi = API_CALL_FUNCTION4(user32, GetDpiForSystem);
        if (dpi > 0) {
            return dpi;
        } else {
            WARNING << getSystemErrorMessage(kGetDpiForSystem);
        }
    }
    if (const HDC hdc = GetDC(hwnd)) {
        bool valid = false;
        const int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        const int dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
        if ((dpiX > 0) && (dpiY > 0)) {
            valid = true;
        } else {
            WARNING << getSystemErrorMessage(kGetDeviceCaps);
        }
        if (ReleaseDC(hwnd, hdc) == 0) {
            WARNING << getSystemErrorMessage(kReleaseDC);
        }
        if (valid) {
            return (horizontal ? dpiX : dpiY);
        }
    } else {
        WARNING << getSystemErrorMessage(kGetDC);
    }
    return getPrimaryScreenDpi(horizontal);
}

quint32 Utils::getResizeBorderThicknessForDpi(const bool horizontal, const quint32 dpi)
{
    Q_ASSERT(dpi != 0);
    if (dpi == 0) {
        return 0;
    }
    if (horizontal) {
        return (getSystemMetrics2(SM_CXSIZEFRAME, true, dpi)
                + getSystemMetrics2(SM_CXPADDEDBORDER, true, dpi));
    } else {
        return (getSystemMetrics2(SM_CYSIZEFRAME, false, dpi)
                + getSystemMetrics2(SM_CYPADDEDBORDER, false, dpi));
    }
}

quint32 Utils::getResizeBorderThickness(const WId windowId, const bool horizontal, const bool scaled)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return 0;
    }
    if (horizontal) {
        return (getSystemMetrics2(windowId, SM_CXSIZEFRAME, true, scaled)
                + getSystemMetrics2(windowId, SM_CXPADDEDBORDER, true, scaled));
    } else {
        return (getSystemMetrics2(windowId, SM_CYSIZEFRAME, false, scaled)
                + getSystemMetrics2(windowId, SM_CYPADDEDBORDER, false, scaled));
    }
}

quint32 Utils::getCaptionBarHeightForDpi(const quint32 dpi)
{
    Q_ASSERT(dpi != 0);
    if (dpi == 0) {
        return 0;
    }
    return getSystemMetrics2(SM_CYCAPTION, false, dpi);
}

quint32 Utils::getCaptionBarHeight(const WId windowId, const bool scaled)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return 0;
    }
    return getSystemMetrics2(windowId, SM_CYCAPTION, false, scaled);
}

quint32 Utils::getTitleBarHeightForDpi(const quint32 dpi)
{
    Q_ASSERT(dpi != 0);
    if (dpi == 0) {
        return 0;
    }
    return (getCaptionBarHeightForDpi(dpi) + getResizeBorderThicknessForDpi(false, dpi));
}

quint32 Utils::getTitleBarHeight(const WId windowId, const bool scaled)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return 0;
    }
    return (getCaptionBarHeight(windowId, scaled) + getResizeBorderThickness(windowId, false, scaled));
}

quint32 Utils::getFrameBorderThicknessForDpi(const quint32 dpi)
{
    Q_ASSERT(dpi != 0);
    if (dpi == 0) {
        return 0;
    }
    // There's no window frame border before Windows 10.
    if (!WindowsVersionHelper::isWin10OrGreater()) {
        return 0;
    }
    const qreal dpr = (qreal(dpi) / qreal(USER_DEFAULT_SCREEN_DPI));
    return std::round(qreal(kDefaultWindowFrameBorderThickness) * dpr);
}

quint32 Utils::getFrameBorderThickness(const WId windowId, const bool scaled)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return 0;
    }
    // There's no window frame border before Windows 10.
    if (!WindowsVersionHelper::isWin10OrGreater()) {
        return 0;
    }
    if (!API_DWM_AVAILABLE(DwmGetWindowAttribute)) {
        return 0;
    }
    const UINT dpi = getWindowDpi(windowId, true);
    const qreal scaleFactor = (qreal(dpi) / qreal(USER_DEFAULT_SCREEN_DPI));
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    UINT value = 0;
    const HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmGetWindowAttribute, hwnd,
        _DWMWA_VISIBLE_FRAME_BORDER_THICKNESS, &value, sizeof(value));
    if (SUCCEEDED(hr)) {
        const qreal dpr = (scaled ? 1.0 : scaleFactor);
        return std::round(qreal(value) / dpr);
    } else {
        const qreal dpr = (scaled ? scaleFactor : 1.0);
        return std::round(qreal(kDefaultWindowFrameBorderThickness) * dpr);
    }
}

QColor Utils::getFrameBorderColor(const bool active)
{
    // There's no window frame border before Windows 10.
    // So we just return a default value which is based on most window managers.
    if (!WindowsVersionHelper::isWin10OrGreater()) {
        return (active ? kDefaultBlackColor : kDefaultDarkGrayColor);
    }
    const bool dark = (FramelessManager::instance()->systemTheme() == SystemTheme::Dark);
    if (active) {
        if (isFrameBorderColorized()) {
            return getAccentColor();
        }
        return (dark ? kDefaultFrameBorderActiveColor : kDefaultTransparentColor);
    } else {
        return (dark ? kDefaultFrameBorderInactiveColorDark : kDefaultFrameBorderInactiveColorLight);
    }
}

void Utils::maybeFixupQtInternals(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    if (qEnvironmentVariableIntValue("FRAMELESSHELPER_WINDOWS_DONT_FIX_QT")) {
        return;
    }
    bool shouldUpdateFrame = false;
    const auto hwnd = reinterpret_cast<HWND>(windowId);
#if 0
    SetLastError(ERROR_SUCCESS);
    const auto classStyle = static_cast<DWORD>(GetClassLongPtrW(hwnd, GCL_STYLE));
    if (classStyle != 0) {
        // CS_HREDRAW/CS_VREDRAW will trigger a repaint event when the window size changes
        // horizontally/vertically, which will cause flicker and jitter during window resizing,
        // mostly for the applications which do all the painting by themselves (eg: Qt).
        // So we remove these flags from the window class here. Qt by default won't add them
        // but let's make it extra safe in case the user may add them by accident.
        static constexpr const DWORD badClassStyle = (CS_HREDRAW | CS_VREDRAW);
        if (classStyle & badClassStyle) {
            SetLastError(ERROR_SUCCESS);
            if (SetClassLongPtrW(hwnd, GCL_STYLE, (classStyle & ~badClassStyle)) == 0) {
                WARNING << getSystemErrorMessage(kSetClassLongPtrW);
            } else {
                shouldUpdateFrame = true;
            }
        }
    } else {
        WARNING << getSystemErrorMessage(kGetClassLongPtrW);
    }
#endif
    SetLastError(ERROR_SUCCESS);
    const auto windowStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    if (windowStyle == 0) {
        WARNING << getSystemErrorMessage(kGetWindowLongPtrW);
    } else {
        // Qt by default adds the "WS_POPUP" flag to all Win32 windows it created and maintained,
        // which is not a good thing (although it won't cause any obvious issues in most cases
        // either), because popup windows have some different behavior with normal overlapped
        // windows, for example, it will affect DWM's default policy. And Qt will also lack some
        // necessary window styles in some cases (caused by misconfigured setWindowFlag(s) calls)
        // and this will also break the normal functionalities for our windows, so we do the
        // correction here unconditionally.
        static constexpr const DWORD badWindowStyle = WS_POPUP;
        static constexpr const DWORD goodWindowStyle = (WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        if ((windowStyle & badWindowStyle) || !(windowStyle & goodWindowStyle)) {
            SetLastError(ERROR_SUCCESS);
            if (SetWindowLongPtrW(hwnd, GWL_STYLE, ((windowStyle & ~badWindowStyle) | goodWindowStyle)) == 0) {
                WARNING << getSystemErrorMessage(kSetWindowLongPtrW);
            } else {
                shouldUpdateFrame = true;
            }
        }
    }
    SetLastError(ERROR_SUCCESS);
    const auto extendedWindowStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    if (extendedWindowStyle == 0) {
        WARNING << getSystemErrorMessage(kGetWindowLongPtrW);
    } else {
        static constexpr const DWORD badWindowStyle = (WS_EX_OVERLAPPEDWINDOW | WS_EX_STATICEDGE | WS_EX_DLGMODALFRAME | WS_EX_CONTEXTHELP);
        static constexpr const DWORD goodWindowStyle = WS_EX_APPWINDOW;
        if ((extendedWindowStyle & badWindowStyle) || !(extendedWindowStyle & goodWindowStyle)) {
            SetLastError(ERROR_SUCCESS);
            if (SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ((extendedWindowStyle & ~badWindowStyle) | goodWindowStyle)) == 0) {
                WARNING << getSystemErrorMessage(kSetWindowLongPtrW);
            } else {
                shouldUpdateFrame = true;
            }
        }
    }
    if (shouldUpdateFrame) {
        triggerFrameChange(windowId);
    }
}

void Utils::startSystemMove(QWindow *window, const QPoint &globalPos)
{
    Q_UNUSED(globalPos);
    Q_ASSERT(window);
    if (!window) {
        return;
    }
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    window->startSystemMove();
#else
    if (ReleaseCapture() == FALSE) {
        WARNING << getSystemErrorMessage(kReleaseCapture);
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    if (PostMessageW(hwnd, WM_SYSCOMMAND, 0xF012 /*SC_DRAGMOVE*/, 0) == FALSE) {
        WARNING << getSystemErrorMessage(kPostMessageW);
    }
#endif
}

void Utils::startSystemResize(QWindow *window, const Qt::Edges edges, const QPoint &globalPos)
{
    Q_UNUSED(globalPos);
    Q_ASSERT(window);
    if (!window) {
        return;
    }
    if (edges == Qt::Edges{}) {
        return;
    }
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    window->startSystemResize(edges);
#else
    if (ReleaseCapture() == FALSE) {
        WARNING << getSystemErrorMessage(kReleaseCapture);
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    if (PostMessageW(hwnd, WM_SYSCOMMAND, qtEdgesToWin32Orientation(edges), 0) == FALSE) {
        WARNING << getSystemErrorMessage(kPostMessageW);
    }
#endif
}

bool Utils::isWindowFrameBorderVisible()
{
    static const auto result = []() -> bool {
        const FramelessConfig * const config = FramelessConfig::instance();
        if (config->isSet(Option::UseCrossPlatformQtImplementation)) {
            return false;
        }
        if (config->isSet(Option::ForceShowWindowFrameBorder)) {
            return true;
        }
        if (config->isSet(Option::ForceHideWindowFrameBorder)) {
            return false;
        }
        return WindowsVersionHelper::isWin10OrGreater();
    }();
    return result;
}

bool Utils::isTitleBarColorized()
{
    // CHECK: is it supported on win7?
    if (!WindowsVersionHelper::isWin10OrGreater()) {
        return false;
    }
    const DwmColorizationArea area = getDwmColorizationArea();
    return ((area == DwmColorizationArea::TitleBar_WindowBorder) || (area == DwmColorizationArea::All));
}

bool Utils::isFrameBorderColorized()
{
    return isTitleBarColorized();
}

void Utils::installSystemMenuHook(const WId windowId, FramelessParamsConst params)
{
    Q_ASSERT(windowId);
    Q_ASSERT(params);
    if (!windowId || !params) {
        return;
    }
    const auto it = g_win32UtilsData()->data.constFind(windowId);
    if (it != g_win32UtilsData()->data.constEnd()) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    SetLastError(ERROR_SUCCESS);
    const auto originalWindowProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
    Q_ASSERT(originalWindowProc);
    if (!originalWindowProc) {
        WARNING << getSystemErrorMessage(kGetWindowLongPtrW);
        return;
    }
    SetLastError(ERROR_SUCCESS);
    if (SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SystemMenuHookWindowProc)) == 0) {
        WARNING << getSystemErrorMessage(kSetWindowLongPtrW);
        return;
    }
    //triggerFrameChange(windowId); // Crash
    Win32UtilsData data = {};
    data.originalWindowProc = originalWindowProc;
    data.params = *params;
    g_win32UtilsData()->data.insert(windowId, data);
}

void Utils::uninstallSystemMenuHook(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    const auto it = g_win32UtilsData()->data.constFind(windowId);
    if (it == g_win32UtilsData()->data.constEnd()) {
        return;
    }
    const Win32UtilsData &data = it.value();
    Q_ASSERT(data.originalWindowProc);
    if (!data.originalWindowProc) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    SetLastError(ERROR_SUCCESS);
    if (SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(data.originalWindowProc)) == 0) {
        WARNING << getSystemErrorMessage(kSetWindowLongPtrW);
        return;
    }
    //triggerFrameChange(windowId); // Crash
    g_win32UtilsData()->data.erase(it);
}

void Utils::setAeroSnappingEnabled(const WId windowId, const bool enable)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    SetLastError(ERROR_SUCCESS);
    const auto oldWindowStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    if (oldWindowStyle == 0) {
        WARNING << getSystemErrorMessage(kGetWindowLongPtrW);
        return;
    }
    // The key is the existence of the "WS_THICKFRAME" flag.
    // But we should also disallow window maximize if Aero Snapping is disabled.
    static constexpr const DWORD resizableFlags = (WS_THICKFRAME | WS_MAXIMIZEBOX);
    const auto newWindowStyle = [enable, oldWindowStyle]() -> DWORD {
        if (enable) {
            return ((oldWindowStyle & ~WS_POPUP) | resizableFlags);
        } else {
            return ((oldWindowStyle & ~resizableFlags) | WS_POPUP);
        }
    }();
    SetLastError(ERROR_SUCCESS);
    if (SetWindowLongPtrW(hwnd, GWL_STYLE, static_cast<LONG_PTR>(newWindowStyle)) == 0) {
        WARNING << getSystemErrorMessage(kSetWindowLongPtrW);
        return;
    }
    triggerFrameChange(windowId);
}

void Utils::tryToEnableHighestDpiAwarenessLevel()
{
    bool isHighestAlready = false;
    const DpiAwareness currentAwareness = getDpiAwarenessForCurrentProcess(&isHighestAlready);
    DEBUG << "Current DPI awareness mode:" << currentAwareness;
    if (isHighestAlready) {
        return;
    }
    if (API_USER_AVAILABLE(SetProcessDpiAwarenessContext)) {
        const auto SetProcessDpiAwarenessContext2 = [](const _DPI_AWARENESS_CONTEXT context) -> bool {
            Q_ASSERT(context);
            if (!context) {
                return false;
            }
            if (API_CALL_FUNCTION4(user32, SetProcessDpiAwarenessContext, context) != FALSE) {
                return true;
            }
            const DWORD dwError = GetLastError();
            // "ERROR_ACCESS_DENIED" means set externally (mostly due to manifest file).
            // Any attempt to change the DPI awareness mode through API will always fail,
            // so we treat this situation as succeeded.
            if (dwError == ERROR_ACCESS_DENIED) {
                DEBUG << kDpiNoAccessErrorMessage;
                return true;
            }
            WARNING << getSystemErrorMessageImpl(kSetProcessDpiAwarenessContext, dwError);
            return false;
        };
        if (currentAwareness == DpiAwareness::PerMonitorVersion2) {
            return;
        }
        if (SetProcessDpiAwarenessContext2(_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return;
        }
        if (currentAwareness == DpiAwareness::PerMonitor) {
            return;
        }
        if (SetProcessDpiAwarenessContext2(_DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
            return;
        }
        if (currentAwareness == DpiAwareness::System) {
            return;
        }
        if (SetProcessDpiAwarenessContext2(_DPI_AWARENESS_CONTEXT_SYSTEM_AWARE)) {
            return;
        }
        if (currentAwareness == DpiAwareness::Unaware_GdiScaled) {
            return;
        }
        if (SetProcessDpiAwarenessContext2(_DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED)) {
            return;
        }
    }
    if (API_SHCORE_AVAILABLE(SetProcessDpiAwareness)) {
        const auto SetProcessDpiAwareness2 = [](const _PROCESS_DPI_AWARENESS pda) -> bool {
            const HRESULT hr = API_CALL_FUNCTION4(shcore, SetProcessDpiAwareness, pda);
            if (SUCCEEDED(hr)) {
                return true;
            }
            // "E_ACCESSDENIED" means set externally (mostly due to manifest file).
            // Any attempt to change the DPI awareness mode through API will always fail,
            // so we treat this situation as succeeded.
            if (hr == E_ACCESSDENIED) {
                DEBUG << kDpiNoAccessErrorMessage;
                return true;
            }
            WARNING << getSystemErrorMessageImpl(kSetProcessDpiAwareness, hr);
            return false;
        };
        if (currentAwareness == DpiAwareness::PerMonitorVersion2) {
            return;
        }
        if (SetProcessDpiAwareness2(_PROCESS_PER_MONITOR_V2_DPI_AWARE)) {
            return;
        }
        if (currentAwareness == DpiAwareness::PerMonitor) {
            return;
        }
        if (SetProcessDpiAwareness2(_PROCESS_PER_MONITOR_DPI_AWARE)) {
            return;
        }
        if (currentAwareness == DpiAwareness::System) {
            return;
        }
        if (SetProcessDpiAwareness2(_PROCESS_SYSTEM_DPI_AWARE)) {
            return;
        }
        if (currentAwareness == DpiAwareness::Unaware_GdiScaled) {
            return;
        }
        if (SetProcessDpiAwareness2(_PROCESS_DPI_UNAWARE_GDISCALED)) {
            return;
        }
    }
    // Some really old MinGW SDK may lack this function, we workaround this
    // issue by always load it dynamically at runtime.
    if (API_USER_AVAILABLE(SetProcessDPIAware)) {
        if (currentAwareness == DpiAwareness::System) {
            return;
        }
        if (API_CALL_FUNCTION4(user32, SetProcessDPIAware) == FALSE) {
            WARNING << getSystemErrorMessage(kSetProcessDPIAware);
        }
    }
}

void Utils::updateGlobalWin32ControlsTheme(const WId windowId, const bool dark)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    // There's no global dark theme for common Win32 controls before Win10 1809.
    if (!WindowsVersionHelper::isWin10RS5OrGreater()) {
        return;
    }
    if (!API_THEME_AVAILABLE(SetWindowTheme)) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    const HRESULT hr = API_CALL_FUNCTION(uxtheme, SetWindowTheme, hwnd,
        (dark ? kSystemDarkThemeResourceName : kSystemLightThemeResourceName), nullptr);
    if (FAILED(hr)) {
        WARNING << getSystemErrorMessageImpl(kSetWindowTheme, hr);
    }
}

bool Utils::shouldAppsUseDarkMode_windows()
{
    // The global dark mode was first introduced in Windows 10 1607.
    if (!WindowsVersionHelper::isWin10RS1OrGreater() || isHighContrastModeEnabled()) {
        return false;
    }
#ifndef FRAMELESSHELPER_CORE_NO_PRIVATE
#  if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    if (const auto app = qApp->nativeInterface<QNativeInterface::Private::QWindowsApplication>()) {
        return app->isDarkMode();
    } else {
        WARNING << "QWindowsApplication is not available.";
    }
#  elif (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    if (const auto ni = QGuiApplication::platformNativeInterface()) {
        return ni->property("darkMode").toBool();
    } else {
        WARNING << "Failed to retrieve the platform native interface.";
    }
#  else // (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    // Qt gained the ability to detect the system dark mode setting only since 5.15.
    // We should detect it ourself on versions below that.
#  endif // (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#endif // FRAMELESSHELPER_CORE_NO_PRIVATE
    // Starting from Windows 10 1903, "ShouldAppsUseDarkMode()" (exported by UXTHEME.DLL,
    // ordinal number 132) always return "TRUE" (actually, a random non-zero number at
    // runtime), so we can't use it due to this unreliability. In this case, we just simply
    // read the user's setting from the registry instead, it's not elegant but at least
    // it works well.
    // However, reverse engineering of Win11's Task Manager reveals that Microsoft still
    // uses this function internally to determine the system theme, and the Task Manager
    // can correctly respond to the theme change event indeed. But strangely, I've checked
    // that it's still broken on Win11 22H2. What's going on here?
    if (WindowsVersionHelper::isWin10RS5OrGreater()
        && !WindowsVersionHelper::isWin1019H1OrGreater()) {
        return (_ShouldAppsUseDarkMode() != FALSE);
    }
    const auto resultFromRegistry = []() -> bool {
        const RegistryKey registry(RegistryRootKey::CurrentUser, personalizeRegistryKey());
        if (!registry.isValid()) {
            return false;
        }
        const DWORD value = registry.value<DWORD>(kAppsUseLightTheme).value_or(0);
        return (value == 0);
    };
    return resultFromRegistry();
}

void Utils::setCornerStyleForWindow(const WId windowId, const WindowCornerStyle style)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    // We cannot change the window corner style until Windows 11.
    if (!WindowsVersionHelper::isWin11OrGreater()) {
        return;
    }
    if (!API_DWM_AVAILABLE(DwmSetWindowAttribute)) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    const auto wcp = [style]() -> _DWM_WINDOW_CORNER_PREFERENCE {
        switch (style) {
        case WindowCornerStyle::Default:
            return _DWMWCP_DEFAULT;
        case WindowCornerStyle::Square:
            return _DWMWCP_DONOTROUND;
        case WindowCornerStyle::Round:
            return _DWMWCP_ROUND;
        }
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_MSVC(4702)
        Q_UNREACHABLE_RETURN(_DWMWCP_DEFAULT);
        QT_WARNING_POP
    }();
    const HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmSetWindowAttribute,
        hwnd, _DWMWA_WINDOW_CORNER_PREFERENCE, &wcp, sizeof(wcp));
    if (FAILED(hr)) {
        WARNING << getSystemErrorMessageImpl(kDwmSetWindowAttribute, hr);
    }
}

bool Utils::setBlurBehindWindowEnabled(const WId windowId, const BlurMode mode, const QColor &color)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return false;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    if (WindowsVersionHelper::isWin8OrGreater()) {
        if (!(API_DWM_AVAILABLE(DwmSetWindowAttribute)
            && API_DWM_AVAILABLE(DwmExtendFrameIntoClientArea))) {
            WARNING << "Blur behind window is not available on current platform.";
            return false;
        }
        const auto restoreWindowFrameMargins = [windowId]() -> void {
            g_win32UtilsData()->micaWindowIds.removeAll(windowId);
            updateWindowFrameMargins(windowId, false);
        };
        const bool preferMicaAlt = (qEnvironmentVariableIntValue("FRAMELESSHELPER_PREFER_MICA_ALT") != 0);
        const auto blurMode = [mode, preferMicaAlt]() -> BlurMode {
            if ((mode == BlurMode::Disable) || (mode == BlurMode::Windows_Aero)) {
                return mode;
            }
            if (((mode == BlurMode::Windows_Mica) || (mode == BlurMode::Windows_MicaAlt))
                && !WindowsVersionHelper::isWin11OrGreater()) {
                WARNING << "The Mica material is not supported on your system, fallback to the Acrylic blur instead...";
                if (WindowsVersionHelper::isWin10OrGreater()) {
                    return BlurMode::Windows_Acrylic;
                }
                WARNING << "The Acrylic blur is not supported on your system, fallback to the traditional DWM blur instead...";
                return BlurMode::Windows_Aero;
            }
            if ((mode == BlurMode::Windows_Acrylic) && !WindowsVersionHelper::isWin10OrGreater()) {
                WARNING << "The Acrylic blur is not supported on your system, fallback to the traditional DWM blur instead...";
                return BlurMode::Windows_Aero;
            }
            if (mode == BlurMode::Default) {
                if (WindowsVersionHelper::isWin11OrGreater()) {
                    return (preferMicaAlt ? BlurMode::Windows_MicaAlt : BlurMode::Windows_Mica);
                }
                if (WindowsVersionHelper::isWin10OrGreater()) {
                    return BlurMode::Windows_Acrylic;
                }
                return BlurMode::Windows_Aero;
            }
            QT_WARNING_PUSH
            QT_WARNING_DISABLE_MSVC(4702)
            Q_UNREACHABLE_RETURN(BlurMode::Default);
            QT_WARNING_POP
        }();
        if (blurMode == BlurMode::Disable) {
            bool result = true;
            if (WindowsVersionHelper::isWin1122H2OrGreater()) {
                const _DWM_SYSTEMBACKDROP_TYPE dwmsbt = _DWMSBT_NONE;
                const HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmSetWindowAttribute,
                    hwnd, _DWMWA_SYSTEMBACKDROP_TYPE, &dwmsbt, sizeof(dwmsbt));
                if (FAILED(hr)) {
                    result = false;
                    WARNING << getSystemErrorMessageImpl(kDwmSetWindowAttribute, hr);
                }
            } else if (WindowsVersionHelper::isWin11OrGreater()) {
                const BOOL enable = FALSE;
                HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmSetWindowAttribute,
                    hwnd, _DWMWA_MICA_EFFECT, &enable, sizeof(enable));
                if (FAILED(hr)) {
                    result = false;
                    WARNING << getSystemErrorMessageImpl(kDwmSetWindowAttribute, hr);
                }
            } else {
                ACCENT_POLICY policy;
                SecureZeroMemory(&policy, sizeof(policy));
                policy.AccentState = ACCENT_DISABLED;
                policy.AccentFlags = ACCENT_NONE;
                WINDOWCOMPOSITIONATTRIBDATA wcad;
                SecureZeroMemory(&wcad, sizeof(wcad));
                wcad.Attrib = WCA_ACCENT_POLICY;
                wcad.pvData = &policy;
                wcad.cbData = sizeof(policy);
                if (_SetWindowCompositionAttribute(hwnd, &wcad) == FALSE) {
                    result = false;
                    WARNING << getSystemErrorMessage(kSetWindowCompositionAttribute);
                }
            }
            if (WindowsVersionHelper::isWin11OrGreater()) {
                restoreWindowFrameMargins();
            }
            return result;
        } else {
            if ((blurMode == BlurMode::Windows_Mica) || (blurMode == BlurMode::Windows_MicaAlt)) {
                g_win32UtilsData()->micaWindowIds.append(windowId);
                // By giving a negative value, DWM will extend the window frame into the whole
                // client area. We need this step because the Mica material can only be applied
                // to the non-client area of a window. Without this step, you'll get a window
                // with a pure black background.
                // Actually disabling the redirection surface (by enabling WS_EX_NOREDIRECTIONBITMAP
                // when you call CreateWindow(), it won't have any effect if you set it after the
                // window has been created) can achieve the same effect with extending the window
                // frame, however, it will completely break GDI's rendering, so sadly we can't choose
                // this solution. But this can be used if you can make sure your application don't
                // use GDI at all, for example, you only use Direct3D to draw your window (like
                // UWP/WPF applications). And one additional note, it will also break OpenGL and Vulkan
                // due to they also use the legacy swap chain model. In theory you can try this flag
                // for Qt Quick applications when the rhi backend is Direct3D, however, some elements
                // will still be broken because Qt Quick still use GDI to render some native controls
                // such as the window menu.
                const MARGINS margins = {-1, -1, -1, -1};
                HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmExtendFrameIntoClientArea, hwnd, &margins);
                if (SUCCEEDED(hr)) {
                    if (WindowsVersionHelper::isWin1122H2OrGreater()) {
                        const auto dwmsbt = (
                            ((blurMode == BlurMode::Windows_MicaAlt) || preferMicaAlt)
                                ? _DWMSBT_TABBEDWINDOW : _DWMSBT_MAINWINDOW);
                        hr = API_CALL_FUNCTION(dwmapi, DwmSetWindowAttribute, hwnd,
                            _DWMWA_SYSTEMBACKDROP_TYPE, &dwmsbt, sizeof(dwmsbt));
                        if (SUCCEEDED(hr)) {
                            return true;
                        } else {
                            WARNING << getSystemErrorMessageImpl(kDwmSetWindowAttribute, hr);
                        }
                    } else {
                        const BOOL enable = TRUE;
                        hr = API_CALL_FUNCTION(dwmapi, DwmSetWindowAttribute,
                            hwnd, _DWMWA_MICA_EFFECT, &enable, sizeof(enable));
                        if (SUCCEEDED(hr)) {
                            return true;
                        } else {
                            WARNING << getSystemErrorMessageImpl(kDwmSetWindowAttribute, hr);
                        }
                    }
                } else {
                    WARNING << getSystemErrorMessageImpl(kDwmExtendFrameIntoClientArea, hr);
                }
                restoreWindowFrameMargins();
            } else {
                ACCENT_POLICY policy;
                SecureZeroMemory(&policy, sizeof(policy));
                if (blurMode == BlurMode::Windows_Acrylic) {
                    policy.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
                    policy.AccentFlags = ACCENT_ENABLE_LUMINOSITY;
                    const auto gradientColor = [&color]() -> QColor {
                        if (color.isValid()) {
                            return color;
                        }
                        QColor clr = ((FramelessManager::instance()->systemTheme() == SystemTheme::Dark) ? kDefaultSystemDarkColor : kDefaultSystemLightColor);
                        clr.setAlphaF(0.9f);
                        return clr;
                    }();
                    // This API expects the #AABBGGRR format.
                    policy.dwGradientColor = DWORD(qRgba(gradientColor.blue(),
                        gradientColor.green(), gradientColor.red(), gradientColor.alpha()));
                } else if (blurMode == BlurMode::Windows_Aero) {
                    policy.AccentState = ACCENT_ENABLE_BLURBEHIND;
                    policy.AccentFlags = ACCENT_NONE;
                } else {
                    QT_WARNING_PUSH
                    QT_WARNING_DISABLE_MSVC(4702)
                    Q_UNREACHABLE_RETURN(false);
                    QT_WARNING_POP
                }
                WINDOWCOMPOSITIONATTRIBDATA wcad;
                SecureZeroMemory(&wcad, sizeof(wcad));
                wcad.Attrib = WCA_ACCENT_POLICY;
                wcad.pvData = &policy;
                wcad.cbData = sizeof(policy);
                if (_SetWindowCompositionAttribute(hwnd, &wcad) != FALSE) {
                    if ((blurMode == BlurMode::Windows_Acrylic)
                        && !WindowsVersionHelper::isWin11OrGreater()) {
                        DEBUG << "Enabling the Acrylic blur for Win32 windows on Windows 10 "
                                 "is very buggy. The only recommended way by Microsoft is to "
                                 "use the XAML Island technology or use pure UWP instead. If "
                                 "you find your window becomes very laggy during moving and "
                                 "resizing, please disable the Acrylic blur immediately (or "
                                 "disable the transparent effect in your personalize settings).";
                    }
                    return true;
                }
                WARNING << getSystemErrorMessage(kSetWindowCompositionAttribute);
            }
        }
    } else {
        // We prefer to use "DwmEnableBlurBehindWindow" on Windows 7 because it behaves
        // better than the undocumented API.
        if (!API_DWM_AVAILABLE(DwmEnableBlurBehindWindow)) {
            WARNING << "Blur behind window is not available on current platform.";
            return false;
        }
        DWM_BLURBEHIND dwmbb;
        SecureZeroMemory(&dwmbb, sizeof(dwmbb));
        dwmbb.dwFlags = DWM_BB_ENABLE;
        dwmbb.fEnable = [mode]() -> BOOL {
            if (mode == BlurMode::Disable) {
                return FALSE;
            }
            if ((mode != BlurMode::Default) && (mode != BlurMode::Windows_Aero)) {
                WARNING << "The only supported blur mode on Windows 7 is the traditional DWM blur.";
            }
            return TRUE;
        }();
        const HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmEnableBlurBehindWindow, hwnd, &dwmbb);
        if (SUCCEEDED(hr)) {
            return true;
        }
        WARNING << getSystemErrorMessageImpl(kDwmEnableBlurBehindWindow, hr);
    }
    return false;
}

QColor Utils::getAccentColor_windows()
{
    // According to my experiments, this AccentColor will be exactly the same with
    // ColorizationColor, what's the meaning of it? But Microsoft products
    // usually read this setting instead of using DwmGetColorizationColor(),
    // so we'd better also do the same thing.
    // There's no Windows API to get this value, so we can only read it
    // directly from the registry.
    const QColor alternative = getDwmColorizationColor();
    const RegistryKey registry(RegistryRootKey::CurrentUser, dwmRegistryKey());
    if (!registry.isValid()) {
        return alternative;
    }
    const QVariant value = registry.value(kAccentColor);
    if (!value.isValid()) {
        return alternative;
    }
    // The retrieved value is in the #AABBGGRR format, we need to
    // convert it to the #AARRGGBB format which Qt expects.
    const QColor abgr = QColor::fromRgba(qvariant_cast<DWORD>(value));
    if (!abgr.isValid()) {
        return alternative;
    }
    return QColor(abgr.blue(), abgr.green(), abgr.red(), abgr.alpha());
}

QString Utils::getWallpaperFilePath()
{
    wchar_t path[MAX_PATH] = {};
    if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0) == FALSE) {
        WARNING << getSystemErrorMessage(kSystemParametersInfoW);
        return {};
    }
    return QString::fromWCharArray(path);
}

WallpaperAspectStyle Utils::getWallpaperAspectStyle()
{
    static constexpr const auto defaultStyle = WallpaperAspectStyle::Fill;
    const RegistryKey registry(RegistryRootKey::CurrentUser, desktopRegistryKey());
    if (!registry.isValid()) {
        return defaultStyle;
    }
    const DWORD wallpaperStyle = registry.value<DWORD>(kWallpaperStyle).value_or(0);
    switch (wallpaperStyle) {
    case 0: {
        const DWORD tileWallpaper = registry.value<DWORD>(kTileWallpaper).value_or(0);
        if (tileWallpaper != 0) {
            return WallpaperAspectStyle::Tile;
        }
        return WallpaperAspectStyle::Center;
    }
    case 2:
        return WallpaperAspectStyle::Stretch; // Ignore aspect ratio to fill.
    case 6:
        return WallpaperAspectStyle::Fit; // Keep aspect ratio to fill, but don't expand/crop.
    case 10:
        return WallpaperAspectStyle::Fill; // Keep aspect ratio to fill, expand/crop if necessary.
    case 22:
        return WallpaperAspectStyle::Span; // ???
    default:
        return defaultStyle;
    }
}

bool Utils::isBlurBehindWindowSupported()
{
    static const auto result = []() -> bool {
        if (FramelessConfig::instance()->isSet(Option::ForceNativeBackgroundBlur)) {
            return true;
        }
        if (FramelessConfig::instance()->isSet(Option::ForceNonNativeBackgroundBlur)) {
            return false;
        }
        return WindowsVersionHelper::isWin11OrGreater();
    }();
    return result;
}

void Utils::hideOriginalTitleBarElements(const WId windowId, const bool disable)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    static constexpr const DWORD validBits = (WTNCA_NODRAWCAPTION | WTNCA_NODRAWICON | WTNCA_NOSYSMENU);
    const DWORD mask = (disable ? validBits : 0);
    const HRESULT hr = _SetWindowThemeNonClientAttributes(hwnd, mask, mask);
    if (FAILED(hr)) {
        WARNING << getSystemErrorMessageImpl(kSetWindowThemeAttribute, hr);
    }
}

void Utils::setQtDarkModeAwareEnabled(const bool enable)
{
#ifdef FRAMELESSHELPER_CORE_NO_PRIVATE
    Q_UNUSED(enable);
#else // !FRAMELESSHELPER_CORE_NO_PRIVATE
#  if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    // We'll call QPA functions, so we have to ensure that the QGuiApplication
    // instance has already been created and initialized, because the platform
    // integration infrastructure is created and maintained by QGuiApplication.
    if (!qGuiApp) {
        return;
    }
    using App = QNativeInterface::Private::QWindowsApplication;
    if (const auto app = qApp->nativeInterface<App>()) {
        app->setDarkModeHandling([enable]() -> App::DarkModeHandling {
            if (!enable) {
                return {}; // Clear the flags.
            }
#    if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
            // Enabling the DarkModeWindowFrames flag will save us the call of the
            // DwmSetWindowAttribute function. Qt will adjust the non-client area
            // (title bar & frame border) automatically.
            // Enabling the DarkModeStyle flag will make Qt Widgets apply dark theme
            // automatically when the system is in dark mode, but before Qt6.5 it's
            // own dark theme is really broken, so don't use it before 6.5.
            // There's no global dark theme for Qt Quick applications, so setting this
            // flag has no effect for pure Qt Quick applications.
            return {App::DarkModeWindowFrames | App::DarkModeStyle};
#    else // (QT_VERSION < QT_VERSION_CHECK(6, 5, 0))
            // Don't try to use the broken dark theme for Qt Widgets applications.
            // For Qt Quick applications this is also enough. There's no global dark
            // theme for them anyway.
            return {App::DarkModeWindowFrames};
#    endif // (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
        }());
    } else {
        WARNING << "QWindowsApplication is not available.";
    }
#  else // (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    Q_UNUSED(enable);
#  endif // (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#endif // FRAMELESSHELPER_CORE_NO_PRIVATE
}

void Utils::registerThemeChangeNotification()
{
    // On Windows we don't need to subscribe to the theme change event
    // manually. Windows will send the theme change notification to all
    // top level windows by default.
}

void Utils::refreshWin32ThemeResources(const WId windowId, const bool dark)
{
    // Code learned from the following repositories. Thank very much for their great effort!
    // https://github.com/ysc3839/win32-darkmode/blob/master/win32-darkmode/DarkMode.h
    // https://github.com/TortoiseGit/TortoiseGit/blob/master/src/TortoiseGitBlame/MainFrm.cpp
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    // We have no way to adjust such things until Win10 1809.
    if (!WindowsVersionHelper::isWin10RS5OrGreater()) {
        return;
    }
    if (!API_DWM_AVAILABLE(DwmSetWindowAttribute)) {
        return;
    }
    const auto hWnd = reinterpret_cast<HWND>(windowId);
    const DWORD borderFlag = (WindowsVersionHelper::isWin1020H1OrGreater()
        ? _DWMWA_USE_IMMERSIVE_DARK_MODE : _DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1);
    const BOOL darkFlag = (dark ? TRUE : FALSE);
    WINDOWCOMPOSITIONATTRIBDATA wcad;
    SecureZeroMemory(&wcad, sizeof(wcad));
    wcad.Attrib = WCA_USEDARKMODECOLORS;
    wcad.pvData = const_cast<BOOL *>(&darkFlag);
    wcad.cbData = sizeof(darkFlag);
    if (dark) {
        if (_AllowDarkModeForWindow(hWnd, darkFlag) == FALSE) {
            WARNING << getSystemErrorMessage(kAllowDarkModeForWindow);
        }
        if (WindowsVersionHelper::isWin1019H1OrGreater()) {
            if (_SetWindowCompositionAttribute(hWnd, &wcad) == FALSE) {
                WARNING << getSystemErrorMessage(kSetWindowCompositionAttribute);
            }
        } else {
            if (SetPropW(hWnd, kDarkModePropertyName,
                reinterpret_cast<HANDLE>(static_cast<INT_PTR>(darkFlag))) == FALSE) {
                WARNING << getSystemErrorMessage(kSetPropW);
            }
        }
        const HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmSetWindowAttribute, hWnd, borderFlag, &darkFlag, sizeof(darkFlag));
        if (FAILED(hr)) {
            WARNING << getSystemErrorMessageImpl(kDwmSetWindowAttribute, hr);
        }
        SetLastError(ERROR_SUCCESS);
        _FlushMenuThemes();
        if (GetLastError() != ERROR_SUCCESS) {
            WARNING << getSystemErrorMessage(kFlushMenuThemes);
        }
        SetLastError(ERROR_SUCCESS);
        _RefreshImmersiveColorPolicyState();
        if (GetLastError() != ERROR_SUCCESS) {
            WARNING << getSystemErrorMessage(kRefreshImmersiveColorPolicyState);
        }
    } else {
        if (_AllowDarkModeForWindow(hWnd, darkFlag) == FALSE) {
            WARNING << getSystemErrorMessage(kAllowDarkModeForWindow);
        }
        if (WindowsVersionHelper::isWin1019H1OrGreater()) {
            if (_SetWindowCompositionAttribute(hWnd, &wcad) == FALSE) {
                WARNING << getSystemErrorMessage(kSetWindowCompositionAttribute);
            }
        } else {
            if (SetPropW(hWnd, kDarkModePropertyName,
                reinterpret_cast<HANDLE>(static_cast<INT_PTR>(darkFlag))) == FALSE) {
                WARNING << getSystemErrorMessage(kSetPropW);
            }
        }
        const HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmSetWindowAttribute, hWnd, borderFlag, &darkFlag, sizeof(darkFlag));
        if (FAILED(hr)) {
            WARNING << getSystemErrorMessageImpl(kDwmSetWindowAttribute, hr);
        }
        SetLastError(ERROR_SUCCESS);
        _FlushMenuThemes();
        if (GetLastError() != ERROR_SUCCESS) {
            WARNING << getSystemErrorMessage(kFlushMenuThemes);
        }
        SetLastError(ERROR_SUCCESS);
        _RefreshImmersiveColorPolicyState();
        if (GetLastError() != ERROR_SUCCESS) {
            WARNING << getSystemErrorMessage(kRefreshImmersiveColorPolicyState);
        }
    }
}

void Utils::enableNonClientAreaDpiScalingForWindow(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    if (!API_USER_AVAILABLE(EnableNonClientDpiScaling)) {
        return;
    }
    // The PMv2 DPI awareness mode will take care of it for us.
    if (getDpiAwarenessForCurrentProcess() == DpiAwareness::PerMonitorVersion2) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    if (API_CALL_FUNCTION4(user32, EnableNonClientDpiScaling, hwnd) == FALSE) {
        WARNING << getSystemErrorMessage(kEnableNonClientDpiScaling);
    }
}

DpiAwareness Utils::getDpiAwarenessForCurrentProcess(bool *highest)
{
    if ((API_USER_AVAILABLE(GetDpiAwarenessContextForProcess)
         || API_USER_AVAILABLE(GetThreadDpiAwarenessContext))
        && API_USER_AVAILABLE(AreDpiAwarenessContextsEqual)
        && API_USER_AVAILABLE(GetAwarenessFromDpiAwarenessContext)) {
        const auto context = []() -> _DPI_AWARENESS_CONTEXT {
            if (API_USER_AVAILABLE(GetDpiAwarenessContextForProcess)) {
                const HANDLE process = GetCurrentProcess();
                if (process) {
                    const _DPI_AWARENESS_CONTEXT result = API_CALL_FUNCTION4(user32, GetDpiAwarenessContextForProcess, process);
                    if (result) {
                        return result;
                    }
                    WARNING << getSystemErrorMessage(kGetDpiAwarenessContextForProcess);
                } else {
                    WARNING << getSystemErrorMessage(kGetCurrentProcess);
                }
            }
            const _DPI_AWARENESS_CONTEXT result = API_CALL_FUNCTION4(user32, GetThreadDpiAwarenessContext);
            if (result) {
                return result;
            }
            WARNING << getSystemErrorMessage(kGetThreadDpiAwarenessContext);
            return nullptr;
        }();
        if (!context) {
            return DpiAwareness::Unknown;
        }
        auto result = DpiAwareness::Unknown;
        // We have to use another API to compare PMv2 and GdiScaled because it seems the
        // GetAwarenessFromDpiAwarenessContext() function won't give us these two values.
        if (API_CALL_FUNCTION4(user32, AreDpiAwarenessContextsEqual, context,
                _DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE) {
            result = DpiAwareness::PerMonitorVersion2;
        } else if (API_CALL_FUNCTION4(user32, AreDpiAwarenessContextsEqual, context,
                          _DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED) != FALSE) {
            result = DpiAwareness::Unaware_GdiScaled;
        } else {
            const _DPI_AWARENESS awareness = API_CALL_FUNCTION4(user32, GetAwarenessFromDpiAwarenessContext, context);
            switch (awareness) {
            case _DPI_AWARENESS_INVALID:
                break;
            case _DPI_AWARENESS_UNAWARE:
                result = DpiAwareness::Unaware;
                break;
            case _DPI_AWARENESS_SYSTEM_AWARE:
                result = DpiAwareness::System;
                break;
            case _DPI_AWARENESS_PER_MONITOR_AWARE:
                result = DpiAwareness::PerMonitor;
                break;
            case _DPI_AWARENESS_PER_MONITOR_V2_AWARE:
                result = DpiAwareness::PerMonitorVersion2;
                break;
            case _DPI_AWARENESS_UNAWARE_GDISCALED:
                result = DpiAwareness::Unaware_GdiScaled;
                break;
            }
        }
        if (highest) {
            *highest = (result == DpiAwareness::PerMonitorVersion2);
        }
        return result;
    }
    if (API_SHCORE_AVAILABLE(GetProcessDpiAwareness)) {
        _PROCESS_DPI_AWARENESS pda = _PROCESS_DPI_UNAWARE;
        const HRESULT hr = API_CALL_FUNCTION4(shcore, GetProcessDpiAwareness, nullptr, &pda);
        if (FAILED(hr)) {
            WARNING << getSystemErrorMessageImpl(kGetProcessDpiAwareness, hr);
            return DpiAwareness::Unknown;
        }
        auto result = DpiAwareness::Unknown;
        switch (pda) {
        case _PROCESS_DPI_UNAWARE:
            result = DpiAwareness::Unaware;
            break;
        case _PROCESS_SYSTEM_DPI_AWARE:
            result = DpiAwareness::System;
            break;
        case _PROCESS_PER_MONITOR_DPI_AWARE:
            result = DpiAwareness::PerMonitor;
            break;
        case _PROCESS_PER_MONITOR_V2_DPI_AWARE:
            result = DpiAwareness::PerMonitorVersion2;
            break;
        case _PROCESS_DPI_UNAWARE_GDISCALED:
            result = DpiAwareness::Unaware_GdiScaled;
            break;
        }
        if (highest) {
            *highest = (result == DpiAwareness::PerMonitor);
        }
        return result;
    }
    if (API_USER_AVAILABLE(IsProcessDPIAware)) {
        const BOOL isAware = API_CALL_FUNCTION(user32, IsProcessDPIAware);
        const auto result = ((isAware == FALSE) ? DpiAwareness::Unaware : DpiAwareness::System);
        if (highest) {
            *highest = (result == DpiAwareness::System);
        }
        return result;
    }
    return DpiAwareness::Unknown;
}

void Utils::fixupChildWindowsDpiMessage(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    // This hack is only available on Windows 10 and newer, and starting from
    // Win10 build 14986 it become useless due to the PMv2 DPI awareness mode
    // already takes care of it for us.
    if (!WindowsVersionHelper::isWin10OrGreater()
        || (WindowsVersionHelper::isWin10RS2OrGreater()
            && (getDpiAwarenessForCurrentProcess() == DpiAwareness::PerMonitorVersion2))) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    if (_EnableChildWindowDpiMessage2(hwnd, TRUE) != FALSE) {
        return;
    }
    // This API is not available on current platform, it's fine.
    if (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) {
        return;
    }
    WARNING << getSystemErrorMessage(kEnableChildWindowDpiMessage);
}

void Utils::fixupDialogsDpiScaling()
{
    // This hack is only available on Windows 10 and newer, and starting from
    // Win10 build 14986 it become useless due to the PMv2 DPI awareness mode
    // already takes care of it for us.
    if (!WindowsVersionHelper::isWin10OrGreater()
        || (WindowsVersionHelper::isWin10RS2OrGreater()
            && (getDpiAwarenessForCurrentProcess() == DpiAwareness::PerMonitorVersion2))) {
        return;
    }
    if (_EnablePerMonitorDialogScaling2() != FALSE) {
        return;
    }
    // This API is not available on current platform, it's fine.
    if (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED) {
        return;
    }
    WARNING << getSystemErrorMessage(kEnablePerMonitorDialogScaling);
}

void Utils::setDarkModeAllowedForApp(const bool allow)
{
    // This hack is only available since Win10 1809.
    if (!WindowsVersionHelper::isWin10RS5OrGreater()) {
        return;
    }
    // This hack is necessary to let AllowDarkModeForWindow() work ...
    if (WindowsVersionHelper::isWin1019H1OrGreater()) {
        if (_SetPreferredAppMode(allow ? PAM_AUTO : PAM_DEFAULT) == PAM_MAX) {
            WARNING << getSystemErrorMessage(kSetPreferredAppMode);
        }
    } else {
        if (_AllowDarkModeForApp(allow ? TRUE : FALSE) == FALSE) {
            WARNING << getSystemErrorMessage(kAllowDarkModeForApp);
        }
    }
}

void Utils::bringWindowToFront(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    const HWND oldForegroundWindow = GetForegroundWindow();
    if (!oldForegroundWindow) {
        // The foreground window can be NULL, it's not an API error.
        return;
    }
    const std::optional<MONITORINFOEXW> activeMonitor = getMonitorForWindow(oldForegroundWindow);
    if (!activeMonitor.has_value()) {
        WARNING << "Failed to retrieve the window's monitor.";
        return;
    }
    // We need to show the window first, otherwise we won't be able to bring it to front.
    if (IsWindowVisible(hwnd) == FALSE) {
        ShowWindow(hwnd, SW_SHOW);
    }
    if (IsMinimized(hwnd)) {
        // Restore the window if it is minimized.
        ShowWindow(hwnd, SW_RESTORE);
        // Once we've been restored, throw us on the active monitor.
        moveWindowToMonitor(hwnd, activeMonitor.value());
        // When the window is restored, it will always become the foreground window.
        // So return early here, we don't need the following code to bring it to front.
        return;
    }
    // OK, our window is not minimized, so now we will try to bring it to front manually.
    // First try to send a message to the current foreground window to check whether
    // it is currently hanging or not.
    static constexpr const UINT kTimeout = 1000;
    if (SendMessageTimeoutW(oldForegroundWindow, WM_NULL, 0, 0,
            SMTO_BLOCK | SMTO_ABORTIFHUNG | SMTO_NOTIMEOUTIFNOTHUNG, kTimeout, nullptr) == 0) {
        if (GetLastError() == ERROR_TIMEOUT) {
            WARNING << "The foreground window hangs, can't activate current window.";
        } else {
            WARNING << getSystemErrorMessage(kSendMessageTimeoutW);
        }
        return;
    }
    const DWORD windowThreadProcessId = GetWindowThreadProcessId(oldForegroundWindow, nullptr);
    const DWORD currentThreadId = GetCurrentThreadId();
    // We won't be able to change a window's Z order if it's not our own window,
    // so we use this small technique to pretend the foreground window is ours.
    if (AttachThreadInput(windowThreadProcessId, currentThreadId, TRUE) == FALSE) {
        WARNING << getSystemErrorMessage(kAttachThreadInput);
        return;
    }
    // And also don't forget to disconnect from it.
    volatile const auto cleanup = qScopeGuard([windowThreadProcessId, currentThreadId]() -> void {
        if (AttachThreadInput(windowThreadProcessId, currentThreadId, FALSE) == FALSE) {
            WARNING << getSystemErrorMessage(kAttachThreadInput);
        }
    });
    Q_UNUSED(cleanup);
    // Make our window be the first one in the Z order.
    if (BringWindowToTop(hwnd) == FALSE) {
        WARNING << getSystemErrorMessage(kBringWindowToTop);
        return;
    }
    // Activate the window too. This will force us to the virtual desktop this
    // window is on, if it's on another virtual desktop.
    if (SetActiveWindow(hwnd) == nullptr) {
        WARNING << getSystemErrorMessage(kSetActiveWindow);
        return;
    }
    // Throw us on the active monitor.
    moveWindowToMonitor(hwnd, activeMonitor.value());
}

QPoint Utils::getWindowPlacementOffset(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return {};
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    SetLastError(ERROR_SUCCESS);
    const auto exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    if (exStyle == 0) {
        WARNING << getSystemErrorMessage(kGetWindowLongPtrW);
        return {};
    }
    // Tool windows are special and they don't need any offset.
    if (exStyle & WS_EX_TOOLWINDOW) {
        return {};
    }
    const std::optional<MONITORINFOEXW> mi = getMonitorForWindow(hwnd);
    if (!mi.has_value()) {
        WARNING << "Failed to retrieve the window's monitor.";
        return {};
    }
    const RECT work = mi.value().rcWork;
    const RECT total = mi.value().rcMonitor;
    return {work.left - total.left, work.top - total.top};
}

QRect Utils::getWindowRestoreGeometry(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return {};
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    WINDOWPLACEMENT wp;
    SecureZeroMemory(&wp, sizeof(wp));
    wp.length = sizeof(wp);
    if (GetWindowPlacement(hwnd, &wp) == FALSE) {
        WARNING << getSystemErrorMessage(kGetWindowPlacement);
        return {};
    }
    return rect2qrect(wp.rcNormalPosition).translated(getWindowPlacementOffset(windowId));
}

void Utils::removeMicaWindow(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    g_win32UtilsData()->micaWindowIds.removeAll(windowId);
}

void Utils::removeSysMenuHook(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return;
    }
    const auto it = g_win32UtilsData()->data.constFind(windowId);
    if (it == g_win32UtilsData()->data.constEnd()) {
        return;
    }
    g_win32UtilsData()->data.erase(it);
}

quint64 Utils::queryMouseButtonState()
{
    quint64 result = 0;
    if (::GetKeyState(VK_LBUTTON) < 0) {
        result |= MK_LBUTTON;
    }
    if (::GetKeyState(VK_RBUTTON) < 0) {
        result |= MK_RBUTTON;
    }
    if (::GetKeyState(VK_SHIFT) < 0) {
        result |= MK_SHIFT;
    }
    if (::GetKeyState(VK_CONTROL) < 0) {
        result |= MK_CONTROL;
    }
    if (::GetKeyState(VK_MBUTTON) < 0) {
        result |= MK_MBUTTON;
    }
    if (::GetKeyState(VK_XBUTTON1) < 0) {
        result |= MK_XBUTTON1;
    }
    if (::GetKeyState(VK_XBUTTON2) < 0) {
        result |= MK_XBUTTON2;
    }
    return result;
}

bool Utils::isValidWindow(const WId windowId, const bool checkVisible, const bool checkTopLevel)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return false;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    if (::IsWindow(hwnd) == FALSE) {
        return false;
    }
    const LONG_PTR styles = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
    if ((styles == 0) || (styles & WS_DISABLED)) {
        return false;
    }
    const LONG_PTR exStyles = ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((exStyles != 0) && (exStyles & WS_EX_TOOLWINDOW)) {
        return false;
    }
    RECT rect = { 0, 0, 0, 0 };
    if (::GetWindowRect(hwnd, &rect) == FALSE) {
        return false;
    }
    if ((rect.left >= rect.right) || (rect.top >= rect.bottom)) {
        return false;
    }
    if (checkVisible) {
        if (::IsWindowVisible(hwnd) == FALSE) {
            return false;
        }
    }
    if (checkTopLevel) {
        if (::GetAncestor(hwnd, GA_ROOT) != hwnd) {
            return false;
        }
    }
    return true;
}

bool Utils::updateFramebufferTransparency(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return false;
    }
    if (!API_DWM_AVAILABLE(DwmEnableBlurBehindWindow)) {
        WARNING << "DwmEnableBlurBehindWindow() is not available on current platform.";
        return false;
    }
    // DwmEnableBlurBehindWindow() won't be functional if DWM composition
    // is not enabled, so we bail out early if this is the case.
    if (!isDwmCompositionEnabled()) {
        return false;
    }
    const auto hwnd = reinterpret_cast<HWND>(windowId);
    bool opaque = false;
    bool ok = false;
    std::ignore = getDwmColorizationColor(&opaque, &ok);
    if (WindowsVersionHelper::isWin8OrGreater() || (ok && !opaque)) {
#if 0 // Windows QPA will always do this for us.
        DWM_BLURBEHIND bb;
        SecureZeroMemory(&bb, sizeof(bb));
        bb.dwFlags = (DWM_BB_ENABLE | DWM_BB_BLURREGION);
        bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
        bb.fEnable = TRUE;
        const HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmEnableBlurBehindWindow, hwnd, &bb);
        if (bb.hRgnBlur) {
            if (DeleteObject(bb.hRgnBlur) == FALSE) {
                WARNING << getSystemErrorMessage(kDeleteObject);
            }
        }
        if (FAILED(hr)) {
            WARNING << getSystemErrorMessageImpl(kDwmEnableBlurBehindWindow, hr);
            return false;
        }
#endif
    } else {
        // HACK: Disable framebuffer transparency on Windows 7 when the
        //       colorization color is opaque, because otherwise the window
        //       contents is blended additively with the previous frame instead
        //       of replacing it
        DWM_BLURBEHIND bb;
        SecureZeroMemory(&bb, sizeof(bb));
        bb.dwFlags = DWM_BB_ENABLE;
        bb.fEnable = FALSE;
        const HRESULT hr = API_CALL_FUNCTION(dwmapi, DwmEnableBlurBehindWindow, hwnd, &bb);
        if (FAILED(hr)) {
            WARNING << getSystemErrorMessageImpl(kDwmEnableBlurBehindWindow, hr);
            return false;
        }
    }
    return true;
}

QMargins Utils::getWindowSystemFrameMargins(const WId windowId)
{
    Q_ASSERT(windowId);
    if (!windowId) {
        return {};
    }
    const auto horizontalMargin = int(getResizeBorderThickness(windowId, true, true));
    const auto verticalMargin = int(getResizeBorderThickness(windowId, false, true));
    return QMargins{ horizontalMargin, verticalMargin, horizontalMargin, verticalMargin };
}

QMargins Utils::getWindowCustomFrameMargins(const QWindow *window)
{
    Q_ASSERT(window);
    if (!window) {
        return {};
    }
#ifndef FRAMELESSHELPER_CORE_NO_PRIVATE
#  if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    if (QPlatformWindow *platformWindow = window->handle()) {
        if (const auto ni = QGuiApplication::platformNativeInterface()) {
            const QVariant marginsVar = ni->windowProperty(platformWindow, qtWindowCustomMarginsProp());
            if (marginsVar.isValid() && !marginsVar.isNull()) {
                return qvariant_cast<QMargins>(marginsVar);
            }
        } else {
            WARNING << "Failed to retrieve the platform native interface.";
        }
    } else {
        WARNING << "Failed to retrieve the platform window.";
    }
#  else // (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    if (const auto platformWindow = dynamic_cast<QNativeInterface::Private::QWindowsWindow *>(window->handle())) {
        return platformWindow->customMargins();
    } else {
        WARNING << "Failed to retrieve the platform window.";
    }
#  endif // (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#endif // FRAMELESSHELPER_CORE_NO_PRIVATE
    const QVariant marginsVar = window->property(kQtWindowCustomMarginsVar);
    if (marginsVar.isValid() && !marginsVar.isNull()) {
        return qvariant_cast<QMargins>(marginsVar);
    }
    return {};
}

FRAMELESSHELPER_END_NAMESPACE
