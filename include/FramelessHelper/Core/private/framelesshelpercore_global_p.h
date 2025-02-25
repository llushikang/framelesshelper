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

#pragma once

#include <FramelessHelper/Core/framelesshelpercore_global.h>
#include <functional>

QT_BEGIN_NAMESPACE
class QScreen;
QT_END_NAMESPACE

FRAMELESSHELPER_BEGIN_NAMESPACE

using InitializeHookCallback = std::function<void()>;
using UninitializeHookCallback = std::function<void()>;

using GetWindowFlagsCallback = std::function<Qt::WindowFlags()>;
using SetWindowFlagsCallback = std::function<void(const Qt::WindowFlags)>;
using GetWindowSizeCallback = std::function<QSize()>;
using SetWindowSizeCallback = std::function<void(const QSize &)>;
using GetWindowPositionCallback = std::function<QPoint()>;
using SetWindowPositionCallback = std::function<void(const QPoint &)>;
using GetWindowScreenCallback = std::function<QScreen *()>;
using IsWindowFixedSizeCallback = std::function<bool()>;
using SetWindowFixedSizeCallback = std::function<void(const bool)>;
using GetWindowStateCallback = std::function<Qt::WindowState()>;
using SetWindowStateCallback = std::function<void(const Qt::WindowState)>;
using GetWindowHandleCallback = std::function<QWindow *()>;
using WindowToScreenCallback = std::function<QPoint(const QPoint &)>;
using ScreenToWindowCallback = std::function<QPoint(const QPoint &)>;
using IsInsideSystemButtonsCallback = std::function<bool(const QPoint &, Global::SystemButtonType *)>;
using IsInsideTitleBarDraggableAreaCallback = std::function<bool(const QPoint &)>;
using GetWindowDevicePixelRatioCallback = std::function<qreal()>;
using SetSystemButtonStateCallback = std::function<void(const Global::SystemButtonType, const Global::ButtonState)>;
using GetWindowIdCallback = std::function<WId()>;
using ShouldIgnoreMouseEventsCallback = std::function<bool(const QPoint &)>;
using ShowSystemMenuCallback = std::function<void(const QPoint &)>;
using SetPropertyCallback = std::function<void(const char *, const QVariant &)>;
using GetPropertyCallback = std::function<QVariant(const char *, const QVariant &)>;
using SetCursorCallback = std::function<void(const QCursor &)>;
using UnsetCursorCallback = std::function<void()>;
using GetWidgetHandleCallback = std::function<QObject *()>;
using ForceChildrenRepaintCallback = std::function<void(const int)>;

struct SystemParameters
{
    GetWindowFlagsCallback getWindowFlags = nullptr;
    SetWindowFlagsCallback setWindowFlags = nullptr;
    GetWindowSizeCallback getWindowSize = nullptr;
    SetWindowSizeCallback setWindowSize = nullptr;
    GetWindowPositionCallback getWindowPosition = nullptr;
    SetWindowPositionCallback setWindowPosition = nullptr;
    GetWindowScreenCallback getWindowScreen = nullptr;
    IsWindowFixedSizeCallback isWindowFixedSize = nullptr;
    SetWindowFixedSizeCallback setWindowFixedSize = nullptr;
    GetWindowStateCallback getWindowState = nullptr;
    SetWindowStateCallback setWindowState = nullptr;
    GetWindowHandleCallback getWindowHandle = nullptr;
    WindowToScreenCallback windowToScreen = nullptr;
    ScreenToWindowCallback screenToWindow = nullptr;
    IsInsideSystemButtonsCallback isInsideSystemButtons = nullptr;
    IsInsideTitleBarDraggableAreaCallback isInsideTitleBarDraggableArea = nullptr;
    GetWindowDevicePixelRatioCallback getWindowDevicePixelRatio = nullptr;
    SetSystemButtonStateCallback setSystemButtonState = nullptr;
    GetWindowIdCallback getWindowId = nullptr;
    ShouldIgnoreMouseEventsCallback shouldIgnoreMouseEvents = nullptr;
    ShowSystemMenuCallback showSystemMenu = nullptr;
    SetPropertyCallback setProperty = nullptr;
    GetPropertyCallback getProperty = nullptr;
    SetCursorCallback setCursor = nullptr;
    UnsetCursorCallback unsetCursor = nullptr;
    GetWidgetHandleCallback getWidgetHandle = nullptr;
    ForceChildrenRepaintCallback forceChildrenRepaint = nullptr;
};

using FramelessParams = SystemParameters *;
using FramelessParamsConst = const SystemParameters *;
using FramelessParamsRef = SystemParameters &;
using FramelessParamsConstRef = const SystemParameters &;

FRAMELESSHELPER_CORE_API void registerInitializeHook(const InitializeHookCallback &cb);
FRAMELESSHELPER_CORE_API void registerUninitializeHook(const UninitializeHookCallback &cb);

FRAMELESSHELPER_END_NAMESPACE

#define DECLARE_SIZE_COMPARE_OPERATORS(Type1, Type2) \
  [[maybe_unused]] [[nodiscard]] static inline constexpr bool operator>(const Type1 &lhs, const Type2 &rhs) noexcept \
  { \
      return ((lhs.width() * lhs.height()) > (rhs.width() * rhs.height())); \
  } \
  [[maybe_unused]] [[nodiscard]] static inline constexpr bool operator>=(const Type1 &lhs, const Type2 &rhs) noexcept \
  { \
      return (operator>(lhs, rhs) || operator==(lhs, rhs)); \
  } \
  [[maybe_unused]] [[nodiscard]] static inline constexpr bool operator<(const Type1 &lhs, const Type2 &rhs) noexcept \
  { \
      return (operator!=(lhs, rhs) && !operator>(lhs, rhs)); \
  } \
  [[maybe_unused]] [[nodiscard]] static inline constexpr bool operator<=(const Type1 &lhs, const Type2 &rhs) noexcept \
  { \
      return (operator<(lhs, rhs) || operator==(lhs, rhs)); \
  }
