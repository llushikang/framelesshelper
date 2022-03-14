/*
 * MIT License
 *
 * Copyright (C) 2022 by wangwenx190 (Yuhang Zhao)
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

#include "widget.h"
#include <QtCore/qdatetime.h>
#include <QtGui/qpainter.h>
#include <QtGui/qevent.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qpushbutton.h>
#include <framelesswindowsmanager.h>
#include <utilities.h>

FRAMELESSHELPER_USE_NAMESPACE

static const QColor systemLightColor = QStringLiteral("#f0f0f0");
static const QColor systemDarkColor = QStringLiteral("#202020");

static const QString mainStyleSheet = QStringLiteral(R"(#MainWidget {
    background-color: %1;
}

#TitleBarWidget {
    background-color: %2;
}

#WindowTitleLabel {
    color: %3;
}

#MinimizeButton, #MaximizeButton, #CloseButton {
    border-style: none;
    background-color: transparent;
}

#MinimizeButton:hover, #MaximizeButton:hover {
    background-color: #c7c7c7;
}

#MinimizeButton:pressed, #MaximizeButton:pressed {
    background-color: #808080;
}

#CloseButton:hover {
    background-color: #e81123;
}

#CloseButton:pressed {
    background-color: #8c0a15;
}

#ClockLabel {
    color: %4;
}
)");

[[nodiscard]] static inline bool isTitleBarColorized()
{
    const DwmColorizationArea area = Utilities::getDwmColorizationArea();
    return ((area == DwmColorizationArea::TitleBar_WindowBorder) || (area == DwmColorizationArea::All));
}

Widget::Widget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    createWinId();
    setupUi();
    startTimer(500);
}

Widget::~Widget() = default;

void Widget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    initFramelessHelperOnce();
}

void Widget::timerEvent(QTimerEvent *event)
{
    QWidget::timerEvent(event);
    if (m_clockLabel) {
        m_clockLabel->setText(QTime::currentTime().toString(QStringLiteral("hh:mm:ss")));
    }
}

void Widget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    bool shouldUpdate = false;
    if (event->type() == QEvent::WindowStateChange) {
#ifdef Q_OS_WINDOWS
        if (Utilities::isWin10OrGreater()) {
            if (isMaximized() || isFullScreen()) {
                setContentsMargins(0, 0, 0, 0);
            } else if (!isMinimized()) {
                resetContentsMargins();
            }
        }
#endif
        updateSystemButtonIcons();
        shouldUpdate = true;
    } else if (event->type() == QEvent::ActivationChange) {
        shouldUpdate = true;
    }
    if (shouldUpdate) {
        updateStyleSheet();
    }
}

void Widget::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
#ifdef Q_OS_WINDOWS
    if ((windowState() == Qt::WindowNoState) && Utilities::isWin10OrGreater() && !Utilities::isWin11OrGreater()) {
        QPainter painter(this);
        painter.save();
        QPen pen = {};
        pen.setColor(Utilities::getFrameBorderColor(isActiveWindow()));
        pen.setWidth(1);
        painter.setPen(pen);
        painter.drawLine(0, 0, width(), 0);
        painter.restore();
    }
#endif
}

void Widget::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    const Qt::MouseButton button = event->button();
    if ((button != Qt::LeftButton) && (button != Qt::RightButton)) {
        return;
    }
    if (isInTitleBarDraggableArea(event->pos())) {
        if (button == Qt::LeftButton) {
            Utilities::startSystemMove(windowHandle());
        } else {
#ifdef Q_OS_WINDOWS
#  if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            const QPointF globalPos = event->globalPosition();
#  else
            const QPointF globalPos = event->globalPos();
#  endif
            const QPointF pos = globalPos * devicePixelRatioF();
            Utilities::showSystemMenu(winId(), pos);
#endif
        }
    }
}

void Widget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QWidget::mouseDoubleClickEvent(event);
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (isInTitleBarDraggableArea(event->pos())) {
        m_maximizeButton->click();
    }
}

void Widget::initFramelessHelperOnce()
{
    if (m_inited) {
        return;
    }
    m_inited = true;
    FramelessWindowsManager::addWindow(windowHandle());
    connect(FramelessWindowsManager::instance(), &FramelessWindowsManager::themeChanged, this, [this](){
        updateStyleSheet();
        updateSystemButtonIcons();
    });
}

void Widget::setupUi()
{
    const int titleBarHeight = 30;
    const QSize systemButtonSize = {int(qRound(qreal(titleBarHeight) * 1.5)), titleBarHeight};
    const QSize systemIconSize = {16, 16};
    setObjectName(QStringLiteral("MainWidget"));
    setWindowTitle(tr("Hello, World!"));
    resize(800, 600);
    m_titleBarWidget = new QWidget(this);
    m_titleBarWidget->setObjectName(QStringLiteral("TitleBarWidget"));
    m_titleBarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_titleBarWidget->setFixedHeight(titleBarHeight);
    m_windowTitleLabel = new QLabel(m_titleBarWidget);
    m_windowTitleLabel->setObjectName(QStringLiteral("WindowTitleLabel"));
    m_windowTitleLabel->setFrameShape(QFrame::NoFrame);
    QFont titleFont = font();
    titleFont.setPointSize(11);
    m_windowTitleLabel->setFont(titleFont);
    m_windowTitleLabel->setText(windowTitle());
    connect(this, &Widget::windowTitleChanged, m_windowTitleLabel, &QLabel::setText);
    m_minimizeButton = new QPushButton(m_titleBarWidget);
    m_minimizeButton->setObjectName(QStringLiteral("MinimizeButton"));
    m_minimizeButton->setFixedSize(systemButtonSize);
    m_minimizeButton->setIconSize(systemIconSize);
    m_minimizeButton->setToolTip(tr("Minimize"));
    connect(m_minimizeButton, &QPushButton::clicked, this, &Widget::showMinimized);
    m_maximizeButton = new QPushButton(m_titleBarWidget);
    m_maximizeButton->setObjectName(QStringLiteral("MaximizeButton"));
    m_maximizeButton->setFixedSize(systemButtonSize);
    m_maximizeButton->setIconSize(systemIconSize);
    m_maximizeButton->setToolTip(tr("Maximize"));
    connect(m_maximizeButton, &QPushButton::clicked, this, [this](){
        if (isMaximized() || isFullScreen()) {
            showNormal();
            m_maximizeButton->setToolTip(tr("Maximize"));
        } else {
            showMaximized();
            m_maximizeButton->setToolTip(tr("Restore"));
        }
        updateSystemButtonIcons();
    });
    m_closeButton = new QPushButton(m_titleBarWidget);
    m_closeButton->setObjectName(QStringLiteral("CloseButton"));
    m_closeButton->setFixedSize(systemButtonSize);
    m_closeButton->setIconSize(systemIconSize);
    m_closeButton->setToolTip(tr("Close"));
    connect(m_closeButton, &QPushButton::clicked, this, &Widget::close);
    updateSystemButtonIcons();
    const auto titleBarLayout = new QHBoxLayout(m_titleBarWidget);
    titleBarLayout->setContentsMargins(0, 0, 0, 0);
    titleBarLayout->setSpacing(0);
    titleBarLayout->addSpacerItem(new QSpacerItem(10, 10));
    titleBarLayout->addWidget(m_windowTitleLabel);
    titleBarLayout->addStretch();
    titleBarLayout->addWidget(m_minimizeButton);
    titleBarLayout->addWidget(m_maximizeButton);
    titleBarLayout->addWidget(m_closeButton);
    m_titleBarWidget->setLayout(titleBarLayout);
    m_clockLabel = new QLabel(this);
    m_clockLabel->setObjectName(QStringLiteral("ClockLabel"));
    m_clockLabel->setFrameShape(QFrame::NoFrame);
    QFont clockFont = font();
    clockFont.setBold(true);
    clockFont.setPointSize(70);
    m_clockLabel->setFont(clockFont);
    const auto contentLayout = new QHBoxLayout;
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    contentLayout->addStretch();
    contentLayout->addWidget(m_clockLabel);
    contentLayout->addStretch();
    const auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_titleBarWidget);
    mainLayout->addStretch();
    mainLayout->addLayout(contentLayout);
    mainLayout->addStretch();
    setLayout(mainLayout);
    resetContentsMargins();
    updateStyleSheet();
}

bool Widget::isInTitleBarDraggableArea(const QPoint &pos) const
{
    QRegion draggableArea = {0, 0, m_titleBarWidget->width(), m_titleBarWidget->height()};
    draggableArea -= m_minimizeButton->geometry();
    draggableArea -= m_maximizeButton->geometry();
    draggableArea -= m_closeButton->geometry();
    return draggableArea.contains(pos);
}

void Widget::updateStyleSheet()
{
    const bool active = isActiveWindow();
    const bool dark = Utilities::shouldAppsUseDarkMode();
    const bool colorizedTitleBar = isTitleBarColorized();
    const QColor titleBarWidgetBackgroundColor = [active, colorizedTitleBar, dark]() -> QColor {
        if (active) {
            if (colorizedTitleBar) {
                return Utilities::getDwmColorizationColor();
            } else {
                if (dark) {
                    return QColor(Qt::black);
                } else {
                    return QColor(Qt::white);
                }
            }
        } else {
            if (dark) {
                return systemDarkColor;
            } else {
                return QColor(Qt::white);
            }
        }
    }();
    const QColor windowTitleLabelTextColor = (active ? ((dark || colorizedTitleBar) ? Qt::white : Qt::black) : Qt::darkGray);
    const QColor clockLabelTextColor = (dark ? Qt::white : Qt::black);
    const QColor mainWidgetBackgroundColor = (dark ? systemDarkColor : systemLightColor);
    setStyleSheet(mainStyleSheet.arg(mainWidgetBackgroundColor.name(), titleBarWidgetBackgroundColor.name(),
                           windowTitleLabelTextColor.name(), clockLabelTextColor.name()));
    update();
}

void Widget::updateSystemButtonIcons()
{
    const QString prefix = ((Utilities::shouldAppsUseDarkMode() || isTitleBarColorized()) ? QStringLiteral("light") : QStringLiteral("dark"));
    m_minimizeButton->setIcon(QIcon(QStringLiteral(":/images/%1/chrome-minimize.svg").arg(prefix)));
    if (isMaximized() || isFullScreen()) {
        m_maximizeButton->setIcon(QIcon(QStringLiteral(":/images/%1/chrome-restore.svg").arg(prefix)));
    } else {
        m_maximizeButton->setIcon(QIcon(QStringLiteral(":/images/%1/chrome-maximize.svg").arg(prefix)));
    }
    m_closeButton->setIcon(QIcon(QStringLiteral(":/images/%1/chrome-close.svg").arg(prefix)));
}

void Widget::resetContentsMargins()
{
#ifdef Q_OS_WINDOWS
    if (Utilities::isWin10OrGreater()) {
        setContentsMargins(0, 1, 0, 0);
    }
#endif
}
