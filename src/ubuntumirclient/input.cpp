/*
 * Copyright (C) 2014-2016 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Local
#include "input.h"
#include "integration.h"
#include "nativeinterface.h"
#include "screen.h"
#include "window.h"
#include "logging.h"
#include "orientationchangeevent_p.h"

// Qt
#include <QtCore/QThread>
#include <QtCore/qglobal.h>
#include <QtCore/QCoreApplication>
#include <private/qguiapplication_p.h>
#include <qpa/qplatforminputcontext.h>
#include <qpa/qwindowsysteminterface.h>
#include <QTextCodec>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <mir_toolkit/mir_client_library.h>

Q_LOGGING_CATEGORY(ubuntumirclientInput, "ubuntumirclient.input", QtWarningMsg)

namespace
{

// XKB Keysyms which do not map directly to Qt types (i.e. Unicode points)
static const uint32_t KeyTable[] = {
    XKB_KEY_Escape,                  Qt::Key_Escape,
    XKB_KEY_Tab,                     Qt::Key_Tab,
    XKB_KEY_ISO_Left_Tab,            Qt::Key_Backtab,
    XKB_KEY_BackSpace,               Qt::Key_Backspace,
    XKB_KEY_Return,                  Qt::Key_Return,
    XKB_KEY_Insert,                  Qt::Key_Insert,
    XKB_KEY_Delete,                  Qt::Key_Delete,
    XKB_KEY_Clear,                   Qt::Key_Delete,
    XKB_KEY_Pause,                   Qt::Key_Pause,
    XKB_KEY_Print,                   Qt::Key_Print,

    XKB_KEY_Home,                    Qt::Key_Home,
    XKB_KEY_End,                     Qt::Key_End,
    XKB_KEY_Left,                    Qt::Key_Left,
    XKB_KEY_Up,                      Qt::Key_Up,
    XKB_KEY_Right,                   Qt::Key_Right,
    XKB_KEY_Down,                    Qt::Key_Down,
    XKB_KEY_Prior,                   Qt::Key_PageUp,
    XKB_KEY_Next,                    Qt::Key_PageDown,

    XKB_KEY_Shift_L,                 Qt::Key_Shift,
    XKB_KEY_Shift_R,                 Qt::Key_Shift,
    XKB_KEY_Shift_Lock,              Qt::Key_Shift,
    XKB_KEY_Control_L,               Qt::Key_Control,
    XKB_KEY_Control_R,               Qt::Key_Control,
    XKB_KEY_Meta_L,                  Qt::Key_Meta,
    XKB_KEY_Meta_R,                  Qt::Key_Meta,
    XKB_KEY_Alt_L,                   Qt::Key_Alt,
    XKB_KEY_Alt_R,                   Qt::Key_Alt,
    XKB_KEY_Caps_Lock,               Qt::Key_CapsLock,
    XKB_KEY_Num_Lock,                Qt::Key_NumLock,
    XKB_KEY_Scroll_Lock,             Qt::Key_ScrollLock,
    XKB_KEY_Super_L,                 Qt::Key_Super_L,
    XKB_KEY_Super_R,                 Qt::Key_Super_R,
    XKB_KEY_Menu,                    Qt::Key_Menu,
    XKB_KEY_Hyper_L,                 Qt::Key_Hyper_L,
    XKB_KEY_Hyper_R,                 Qt::Key_Hyper_R,
    XKB_KEY_Help,                    Qt::Key_Help,

    XKB_KEY_KP_Space,                Qt::Key_Space,
    XKB_KEY_KP_Tab,                  Qt::Key_Tab,
    XKB_KEY_KP_Enter,                Qt::Key_Enter,
    XKB_KEY_KP_Home,                 Qt::Key_Home,
    XKB_KEY_KP_Left,                 Qt::Key_Left,
    XKB_KEY_KP_Up,                   Qt::Key_Up,
    XKB_KEY_KP_Right,                Qt::Key_Right,
    XKB_KEY_KP_Down,                 Qt::Key_Down,
    XKB_KEY_KP_Prior,                Qt::Key_PageUp,
    XKB_KEY_KP_Next,                 Qt::Key_PageDown,
    XKB_KEY_KP_End,                  Qt::Key_End,
    XKB_KEY_KP_Begin,                Qt::Key_Clear,
    XKB_KEY_KP_Insert,               Qt::Key_Insert,
    XKB_KEY_KP_Delete,               Qt::Key_Delete,
    XKB_KEY_KP_Equal,                Qt::Key_Equal,
    XKB_KEY_KP_Multiply,             Qt::Key_Asterisk,
    XKB_KEY_KP_Add,                  Qt::Key_Plus,
    XKB_KEY_KP_Separator,            Qt::Key_Comma,
    XKB_KEY_KP_Subtract,             Qt::Key_Minus,
    XKB_KEY_KP_Decimal,              Qt::Key_Period,
    XKB_KEY_KP_Divide,               Qt::Key_Slash,

    XKB_KEY_ISO_Level3_Shift,        Qt::Key_AltGr,
    XKB_KEY_Multi_key,               Qt::Key_Multi_key,
    XKB_KEY_Codeinput,               Qt::Key_Codeinput,
    XKB_KEY_SingleCandidate,         Qt::Key_SingleCandidate,
    XKB_KEY_MultipleCandidate,       Qt::Key_MultipleCandidate,
    XKB_KEY_PreviousCandidate,       Qt::Key_PreviousCandidate,

    // dead keys
    XKB_KEY_dead_grave,              Qt::Key_Dead_Grave,
    XKB_KEY_dead_acute,              Qt::Key_Dead_Acute,
    XKB_KEY_dead_circumflex,         Qt::Key_Dead_Circumflex,
    XKB_KEY_dead_tilde,              Qt::Key_Dead_Tilde,
    XKB_KEY_dead_macron,             Qt::Key_Dead_Macron,
    XKB_KEY_dead_breve,              Qt::Key_Dead_Breve,
    XKB_KEY_dead_abovedot,           Qt::Key_Dead_Abovedot,
    XKB_KEY_dead_diaeresis,          Qt::Key_Dead_Diaeresis,
    XKB_KEY_dead_abovering,          Qt::Key_Dead_Abovering,
    XKB_KEY_dead_doubleacute,        Qt::Key_Dead_Doubleacute,
    XKB_KEY_dead_caron,              Qt::Key_Dead_Caron,
    XKB_KEY_dead_cedilla,            Qt::Key_Dead_Cedilla,
    XKB_KEY_dead_ogonek,             Qt::Key_Dead_Ogonek,
    XKB_KEY_dead_iota,               Qt::Key_Dead_Iota,
    XKB_KEY_dead_voiced_sound,       Qt::Key_Dead_Voiced_Sound,
    XKB_KEY_dead_semivoiced_sound,   Qt::Key_Dead_Semivoiced_Sound,
    XKB_KEY_dead_belowdot,           Qt::Key_Dead_Belowdot,
    XKB_KEY_dead_hook,               Qt::Key_Dead_Hook,
    XKB_KEY_dead_horn,               Qt::Key_Dead_Horn,

    XKB_KEY_Mode_switch,             Qt::Key_Mode_switch,
    XKB_KEY_script_switch,           Qt::Key_Mode_switch,
    XKB_KEY_XF86AudioRaiseVolume,    Qt::Key_VolumeUp,
    XKB_KEY_XF86AudioLowerVolume,    Qt::Key_VolumeDown,
    XKB_KEY_XF86PowerOff,            Qt::Key_PowerOff,
    XKB_KEY_XF86PowerDown,           Qt::Key_PowerDown,

    0,                          0
};

Qt::WindowState mirSurfaceStateToWindowState(MirSurfaceState state)
{
    switch (state) {
    case mir_surface_state_fullscreen:
        return Qt::WindowFullScreen;
    case mir_surface_state_maximized:
    case mir_surface_state_vertmaximized:
    case mir_surface_state_horizmaximized:
        return Qt::WindowMaximized;
    case mir_surface_state_minimized:
        return Qt::WindowMinimized;
    case mir_surface_state_hidden:
        // We should be handling this state separately.
        Q_ASSERT(false);
    case mir_surface_state_restored:
    case mir_surface_state_unknown:
    default:
        return Qt::WindowNoState;
    }
}

} // namespace

class UbuntuEvent : public QEvent
{
public:
    UbuntuEvent(UbuntuWindow* window, const MirEvent *event, QEvent::Type type)
        : QEvent(type), window(window) {
        nativeEvent = mir_event_ref(event);
    }
    ~UbuntuEvent()
    {
        mir_event_unref(nativeEvent);
    }

    QPointer<UbuntuWindow> window;
    const MirEvent *nativeEvent;
};

UbuntuInput::UbuntuInput(UbuntuClientIntegration* integration)
    : QObject(nullptr)
    , mIntegration(integration)
    , mEventFilterType(static_cast<UbuntuNativeInterface*>(
        integration->nativeInterface())->genericEventFilterType())
    , mEventType(static_cast<QEvent::Type>(QEvent::registerEventType()))
    , mLastFocusedWindow(nullptr)
{
    // Initialize touch device.
    mTouchDevice = new QTouchDevice;
    mTouchDevice->setType(QTouchDevice::TouchScreen);
    mTouchDevice->setCapabilities(
            QTouchDevice::Position | QTouchDevice::Area | QTouchDevice::Pressure |
            QTouchDevice::NormalizedPosition);
    QWindowSystemInterface::registerTouchDevice(mTouchDevice);
}

UbuntuInput::~UbuntuInput()
{
  // Qt will take care of deleting mTouchDevice.
}

static const char* nativeEventTypeToStr(MirEventType t)
{
    switch (t)
    {
    case mir_event_type_key:
        return "mir_event_type_key";
    case mir_event_type_motion:
        return "mir_event_type_motion";
    case mir_event_type_surface:
        return "mir_event_type_surface";
    case mir_event_type_resize:
        return "mir_event_type_resize";
    case mir_event_type_prompt_session_state_change:
        return "mir_event_type_prompt_session_state_change";
    case mir_event_type_orientation:
        return "mir_event_type_orientation";
    case mir_event_type_close_surface:
        return "mir_event_type_close_surface";
    case mir_event_type_input:
        return "mir_event_type_input";
    case mir_event_type_keymap:
        return "mir_event_type_keymap";
    case mir_event_type_input_configuration:
        return "mir_event_type_input_configuration";
    case mir_event_type_surface_output:
        return "mir_event_type_surface_output";
    case mir_event_type_input_device_state:
        return "mir_event_type_input_device_state";
    default:
        return "invalid";
    }
}

void UbuntuInput::customEvent(QEvent* event)
{
    Q_ASSERT(QThread::currentThread() == thread());
    UbuntuEvent* ubuntuEvent = static_cast<UbuntuEvent*>(event);
    const MirEvent *nativeEvent = ubuntuEvent->nativeEvent;

    if ((ubuntuEvent->window == nullptr) || (ubuntuEvent->window->window() == nullptr)) {
        qCWarning(ubuntumirclient) << "Attempted to deliver an event to a non-existent window, ignoring.";
        return;
    }

    // Event filtering.
    long result;
    if (QWindowSystemInterface::handleNativeEvent(
            ubuntuEvent->window->window(), mEventFilterType,
            const_cast<void *>(static_cast<const void *>(nativeEvent)), &result) == true) {
        qCDebug(ubuntumirclient, "event filtered out by native interface");
        return;
    }

    qCDebug(ubuntumirclientInput, "customEvent(type=%s)", nativeEventTypeToStr(mir_event_get_type(nativeEvent)));

    // Event dispatching.
    switch (mir_event_get_type(nativeEvent))
    {
    case mir_event_type_input:
        dispatchInputEvent(ubuntuEvent->window, mir_event_get_input_event(nativeEvent));
        break;
    case mir_event_type_resize:
    {
        auto resizeEvent = mir_event_get_resize_event(nativeEvent);

        // Enable workaround for Screen rotation
        auto const targetWindow = ubuntuEvent->window;
        if (targetWindow) {
            auto const screen = static_cast<UbuntuScreen*>(targetWindow->screen());
            if (screen) {
                screen->handleWindowSurfaceResize(
                        mir_resize_event_get_width(resizeEvent),
                        mir_resize_event_get_height(resizeEvent));
            }

            targetWindow->handleSurfaceResized(
                        mir_resize_event_get_width(resizeEvent),
                        mir_resize_event_get_height(resizeEvent));
        }
        break;
    }
    case mir_event_type_surface:
        handleSurfaceEvent(ubuntuEvent->window, mir_event_get_surface_event(nativeEvent));
        break;
    case mir_event_type_surface_output:
        handleSurfaceOutputEvent(ubuntuEvent->window, mir_event_get_surface_output_event(nativeEvent));
        break;
    case mir_event_type_orientation:
        dispatchOrientationEvent(ubuntuEvent->window->window(), mir_event_get_orientation_event(nativeEvent));
        break;
    case mir_event_type_close_surface:
        QWindowSystemInterface::handleCloseEvent(ubuntuEvent->window->window());
        break;
    default:
        qCDebug(ubuntumirclient, "unhandled event type: %d", static_cast<int>(mir_event_get_type(nativeEvent)));
    }
}

void UbuntuInput::postEvent(UbuntuWindow *platformWindow, const MirEvent *event)
{
    QWindow *window = platformWindow->window();

    const auto eventType = mir_event_get_type(event);
    if (mir_event_type_surface == eventType) {
        auto surfaceEvent = mir_event_get_surface_event(event);
        if (mir_surface_attrib_focus == mir_surface_event_get_attribute(surfaceEvent)) {
            const bool focused = mir_surface_event_get_attribute_value(surfaceEvent) == mir_surface_focused;
            if (focused) {
                mPendingFocusGainedEvents++;
            }
        }
    }

    QCoreApplication::postEvent(this, new UbuntuEvent(
            platformWindow, event, mEventType));

    if ((window->flags().testFlag(Qt::WindowTransparentForInput)) && window->parent()) {
        QCoreApplication::postEvent(this, new UbuntuEvent(
                    static_cast<UbuntuWindow*>(platformWindow->QPlatformWindow::parent()),
                    event, mEventType));
    }
}

void UbuntuInput::dispatchInputEvent(UbuntuWindow *window, const MirInputEvent *ev)
{
    switch (mir_input_event_get_type(ev))
    {
    case mir_input_event_type_key:
        dispatchKeyEvent(window, ev);
        break;
    case mir_input_event_type_touch:
        dispatchTouchEvent(window, ev);
        break;
    case mir_input_event_type_pointer:
        dispatchPointerEvent(window, ev);
        break;
    default:
        break;
    }
}

void UbuntuInput::dispatchTouchEvent(UbuntuWindow *window, const MirInputEvent *ev)
{
    const MirTouchEvent *tev = mir_input_event_get_touch_event(ev);

    // FIXME(loicm) Max pressure is device specific. That one is for the Samsung Galaxy Nexus. That
    //     needs to be fixed as soon as the compat input lib adds query support.
    const float kMaxPressure = 1.28;
    const QRect kWindowGeometry = window->geometry();
    QList<QWindowSystemInterface::TouchPoint> touchPoints;


    // TODO: Is it worth setting the Qt::TouchPointStationary ones? Currently they are left
    //       as Qt::TouchPointMoved
    const unsigned int kPointerCount = mir_touch_event_point_count(tev);
    for (unsigned int i = 0; i < kPointerCount; ++i) {
        QWindowSystemInterface::TouchPoint touchPoint;

        const float kX = mir_touch_event_axis_value(tev, i, mir_touch_axis_x) + kWindowGeometry.x();
        const float kY = mir_touch_event_axis_value(tev, i, mir_touch_axis_y) + kWindowGeometry.y(); // see bug lp:1346633 workaround comments elsewhere
        const float kW = mir_touch_event_axis_value(tev, i, mir_touch_axis_touch_major);
        const float kH = mir_touch_event_axis_value(tev, i, mir_touch_axis_touch_minor);
        const float kP = mir_touch_event_axis_value(tev, i, mir_touch_axis_pressure);
        touchPoint.id = mir_touch_event_id(tev, i);
        touchPoint.normalPosition = QPointF(kX / kWindowGeometry.width(), kY / kWindowGeometry.height());
        touchPoint.area = QRectF(kX - (kW / 2.0), kY - (kH / 2.0), kW, kH);
        touchPoint.pressure = kP / kMaxPressure;

        MirTouchAction touch_action = mir_touch_event_action(tev, i);
        switch (touch_action)
        {
        case mir_touch_action_down:
            mLastFocusedWindow = window;
            touchPoint.state = Qt::TouchPointPressed;
            break;
        case mir_touch_action_up:
            touchPoint.state = Qt::TouchPointReleased;
            break;
        case mir_touch_action_change:
        default:
            touchPoint.state = Qt::TouchPointMoved;
        }

        touchPoints.append(touchPoint);
    }

    ulong timestamp = mir_input_event_get_event_time(ev) / 1000000;
    QWindowSystemInterface::handleTouchEvent(window->window(), timestamp,
            mTouchDevice, touchPoints);
}

static uint32_t translateKeysym(uint32_t sym, const QString &text) {
    int code = 0;

    QTextCodec *systemCodec = QTextCodec::codecForLocale();
    if (sym < 128 || (sym < 256 && systemCodec->mibEnum() == 4)) {
        // upper-case key, if known
        code = isprint((int)sym) ? toupper((int)sym) : 0;
    } else if (sym >= XKB_KEY_F1 && sym <= XKB_KEY_F35) {
        return Qt::Key_F1 + (int(sym) - XKB_KEY_F1);
    } else if (text.length() == 1 && text.unicode()->unicode() > 0x1f
               && text.unicode()->unicode() != 0x7f
               && !(sym >= XKB_KEY_dead_grave && sym <= XKB_KEY_dead_currency)) {
        code = text.unicode()->toUpper().unicode();
    } else {
        for (int i = 0; KeyTable[i]; i += 2)
            if (sym == KeyTable[i])
                code = KeyTable[i + 1];
    }

    return code;
}

namespace
{
Qt::KeyboardModifiers qt_modifiers_from_mir(MirInputEventModifiers modifiers)
{
    Qt::KeyboardModifiers q_modifiers = Qt::NoModifier;
    if (modifiers & mir_input_event_modifier_shift) {
        q_modifiers |= Qt::ShiftModifier;
    }
    if (modifiers & mir_input_event_modifier_ctrl) {
        q_modifiers |= Qt::ControlModifier;
    }
    if (modifiers & mir_input_event_modifier_alt_left) {
        q_modifiers |= Qt::AltModifier;
    }
    if (modifiers & mir_input_event_modifier_meta) {
        q_modifiers |= Qt::MetaModifier;
    }
    if (modifiers & mir_input_event_modifier_alt_right) {
        q_modifiers |= Qt::GroupSwitchModifier;
    }
    return q_modifiers;
}
}

void UbuntuInput::dispatchKeyEvent(UbuntuWindow *window, const MirInputEvent *event)
{
    const MirKeyboardEvent *key_event = mir_input_event_get_keyboard_event(event);

    ulong timestamp = mir_input_event_get_event_time(event) / 1000000;
    xkb_keysym_t xk_sym = mir_keyboard_event_key_code(key_event);
    quint32 scan_code = mir_keyboard_event_scan_code(key_event);
    quint32 native_modifiers = mir_keyboard_event_modifiers(key_event);

    // Key modifier and unicode index mapping.
    auto modifiers = qt_modifiers_from_mir(native_modifiers);

    MirKeyboardAction action = mir_keyboard_event_action(key_event);
    QEvent::Type keyType = action == mir_keyboard_action_up
        ? QEvent::KeyRelease : QEvent::KeyPress;

    if (action == mir_keyboard_action_down)
        mLastFocusedWindow = window;

    QString text;
    QVarLengthArray<char, 32> chars(32);
    {
        int result = xkb_keysym_to_utf8(xk_sym, chars.data(), chars.size());

        if (result > 0) {
            text = QString::fromUtf8(chars.constData());
        }
    }
    int sym = translateKeysym(xk_sym, text);

    bool is_auto_rep = action == mir_keyboard_action_repeat;

    QPlatformInputContext *context = QGuiApplicationPrivate::platformIntegration()->inputContext();
    if (context) {
        QKeyEvent qKeyEvent(keyType, sym, modifiers, scan_code, xk_sym, native_modifiers, text, is_auto_rep);
        qKeyEvent.setTimestamp(timestamp);
        if (context->filterEvent(&qKeyEvent)) {
            qCDebug(ubuntumirclient, "key event filtered out by input context");
            return;
        }
    }

    QWindowSystemInterface::handleExtendedKeyEvent(window->window(), timestamp, keyType, sym, modifiers, scan_code, xk_sym, native_modifiers, text, is_auto_rep);
}

namespace
{
Qt::MouseButtons extract_buttons(const MirPointerEvent *pev)
{
    Qt::MouseButtons buttons = Qt::NoButton;
    if (mir_pointer_event_button_state(pev, mir_pointer_button_primary))
        buttons |= Qt::LeftButton;
    if (mir_pointer_event_button_state(pev, mir_pointer_button_secondary))
        buttons |= Qt::RightButton;
    if (mir_pointer_event_button_state(pev, mir_pointer_button_tertiary))
        buttons |= Qt::MiddleButton;
    if (mir_pointer_event_button_state(pev, mir_pointer_button_back))
        buttons |= Qt::BackButton;
    if (mir_pointer_event_button_state(pev, mir_pointer_button_forward))
        buttons |= Qt::ForwardButton;

    return buttons;
}
}

void UbuntuInput::dispatchPointerEvent(UbuntuWindow *platformWindow, const MirInputEvent *ev)
{
    const auto window = platformWindow->window();
    const auto timestamp = mir_input_event_get_event_time(ev) / 1000000;

    const auto pev = mir_input_event_get_pointer_event(ev);
    const auto action = mir_pointer_event_action(pev);

    const auto modifiers = qt_modifiers_from_mir(mir_pointer_event_modifiers(pev));
    const auto localPoint = QPointF(mir_pointer_event_axis_value(pev, mir_pointer_axis_x),
                                    mir_pointer_event_axis_value(pev, mir_pointer_axis_y));

    switch (action) {
    case mir_pointer_action_button_up:
    case mir_pointer_action_button_down:
    case mir_pointer_action_motion:
    {
        const float hDelta = mir_pointer_event_axis_value(pev, mir_pointer_axis_hscroll);
        const float vDelta = mir_pointer_event_axis_value(pev, mir_pointer_axis_vscroll);

        if (hDelta != 0 || vDelta != 0) {
            // QWheelEvent::DefaultDeltasPerStep = 120 but doesn't exist on vivid
            const QPoint angleDelta(120 * hDelta, 120 * vDelta);
            QWindowSystemInterface::handleWheelEvent(window, timestamp, localPoint, window->position() + localPoint,
                                                     QPoint(), angleDelta, modifiers, Qt::ScrollUpdate);
        }
        auto buttons = extract_buttons(pev);
        QWindowSystemInterface::handleMouseEvent(window, timestamp, localPoint, window->position() + localPoint /* Should we omit global point instead? */,
                                                 buttons, modifiers);
        break;
    }
    case mir_pointer_action_enter:
        QWindowSystemInterface::handleEnterEvent(window, localPoint, window->position() + localPoint);
        break;
    case mir_pointer_action_leave:
        QWindowSystemInterface::handleLeaveEvent(window);
        break;
    default:
        qCDebug(ubuntumirclient, "Unrecognized pointer event");
    }
}

static const char* nativeOrientationDirectionToStr(MirOrientation orientation)
{
    switch (orientation) {
    case mir_orientation_normal:
        return "Normal";
        break;
    case mir_orientation_left:
        return "Left";
        break;
    case mir_orientation_inverted:
        return "Inverted";
        break;
    case mir_orientation_right:
        return "Right";
        break;
    default:
        return "INVALID!";
    }
}

void UbuntuInput::dispatchOrientationEvent(QWindow *window, const MirOrientationEvent *event)
{
    MirOrientation mir_orientation = mir_orientation_event_get_direction(event);
    qCDebug(ubuntumirclientInput, "orientation direction: %s", nativeOrientationDirectionToStr(mir_orientation));

    if (!window->screen()) {
        qCDebug(ubuntumirclient, "Window has no associated screen, dropping orientation event");
        return;
    }

    OrientationChangeEvent::Orientation orientation;
    switch (mir_orientation) {
    case mir_orientation_normal:
        orientation = OrientationChangeEvent::TopUp;
        break;
    case mir_orientation_left:
        orientation = OrientationChangeEvent::LeftUp;
        break;
    case mir_orientation_inverted:
        orientation = OrientationChangeEvent::TopDown;
        break;
    case mir_orientation_right:
        orientation = OrientationChangeEvent::RightUp;
        break;
    default:
        qCDebug(ubuntumirclient, "No such orientation %d", mir_orientation);
        return;
    }

    // Dispatch orientation event to [Platform]Screen, as that is where Qt reads it. Screen will handle
    // notifying Qt of the actual orientation change - done to prevent multiple Windows each creating
    // an identical orientation change event and passing it directly to Qt.
    // [Platform]Screen can also factor in the native orientation.
    QCoreApplication::postEvent(static_cast<UbuntuScreen*>(window->screen()->handle()),
                                new OrientationChangeEvent(OrientationChangeEvent::mType, orientation));
}

void UbuntuInput::handleSurfaceEvent(const QPointer<UbuntuWindow> &window, const MirSurfaceEvent *event)
{
    auto surfaceEventAttribute = mir_surface_event_get_attribute(event);

    switch (surfaceEventAttribute) {
    case mir_surface_attrib_focus: {
        const bool focused = mir_surface_event_get_attribute_value(event) == mir_surface_focused;
        // Mir may have sent a pair of focus lost/gained events, so we need to "peek" into the queue
        // so that we don't deactivate windows prematurely.
        if (focused) {
            mPendingFocusGainedEvents--;
            QWindowSystemInterface::handleWindowActivated(window->window(), Qt::ActiveWindowFocusReason);

            // NB: Since processing of system events is queued, never check qGuiApp->applicationState()
            //     as it might be outdated. Always call handleApplicationStateChanged() with the latest
            //     state regardless.
            QWindowSystemInterface::handleApplicationStateChanged(Qt::ApplicationActive);

        } else if(!mPendingFocusGainedEvents) {
            qCDebug(ubuntumirclient, "No windows have focus");
            QWindowSystemInterface::handleWindowActivated(nullptr, Qt::ActiveWindowFocusReason);
            QWindowSystemInterface::handleApplicationStateChanged(Qt::ApplicationInactive);
        }
        break;
    }
    case mir_surface_attrib_visibility:
        window->handleSurfaceExposeChange(
                    mir_surface_event_get_attribute_value(event) == mir_surface_visibility_exposed);
        break;
    // Remaining attributes are ones client sets for server, and server should not override them
    case mir_surface_attrib_state: {
        MirSurfaceState state = static_cast<MirSurfaceState>(mir_surface_event_get_attribute_value(event));

        if (state == mir_surface_state_hidden) {
            window->handleSurfaceVisibilityChanged(false);
        } else {
            // it's visible!
            window->handleSurfaceVisibilityChanged(true);
            window->handleSurfaceStateChanged(mirSurfaceStateToWindowState(state));
        }
        break;
    }
    case mir_surface_attrib_type:
    case mir_surface_attrib_swapinterval:
    case mir_surface_attrib_dpi:
    case mir_surface_attrib_preferred_orientation:
    case mir_surface_attribs:
        break;
    }
}

void UbuntuInput::handleSurfaceOutputEvent(const QPointer<UbuntuWindow> &window, const MirSurfaceOutputEvent *event)
{
    const uint32_t outputId = mir_surface_output_event_get_output_id(event);
    const int dpi = mir_surface_output_event_get_dpi(event);
    const MirFormFactor formFactor = mir_surface_output_event_get_form_factor(event);
    const float scale = mir_surface_output_event_get_scale(event);

    const auto screenObserver = mIntegration->screenObserver();
    UbuntuScreen *screen = screenObserver->findScreenWithId(outputId);
    if (!screen) {
        qCWarning(ubuntumirclient) << "Mir notified window" << window->window() << "on an unknown screen with id" << outputId;
        return;
    }

    screenObserver->handleScreenPropertiesChange(screen, dpi, formFactor, scale);
    window->handleScreenPropertiesChange(formFactor, scale);

    if (window->screen() != screen) {
        QWindowSystemInterface::handleWindowScreenChanged(window->window(), screen->screen());
    }
}
