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

#ifndef UBUNTU_INPUT_H
#define UBUNTU_INPUT_H

// Qt
#include <qpa/qwindowsysteminterface.h>
#include <QAtomicInt>
#include <QLoggingCategory>

#include <mir_toolkit/mir_client_library.h>

class UbuntuClientIntegration;
class UbuntuWindow;

class UbuntuInput : public QObject
{
    Q_OBJECT

public:
    UbuntuInput(UbuntuClientIntegration* integration);
    virtual ~UbuntuInput();

    // QObject methods.
    void customEvent(QEvent* event) override;

    void postEvent(UbuntuWindow* window, const MirEvent *event);
    UbuntuClientIntegration* integration() const { return mIntegration; }
    UbuntuWindow *lastFocusedWindow() const {return mLastFocusedWindow; }

protected:
    void dispatchKeyEvent(UbuntuWindow *window, const MirInputEvent *event);
    void dispatchPointerEvent(UbuntuWindow *window, const MirInputEvent *event);
    void dispatchTouchEvent(UbuntuWindow *window, const MirInputEvent *event);
    void dispatchInputEvent(UbuntuWindow *window, const MirInputEvent *event);

    void dispatchOrientationEvent(QWindow* window, const MirOrientationEvent *event);
    void handleSurfaceEvent(const QPointer<UbuntuWindow> &window, const MirSurfaceEvent *event);
    void handleSurfaceOutputEvent(const QPointer<UbuntuWindow> &window, const MirSurfaceOutputEvent *event);

private:
    UbuntuClientIntegration* mIntegration;
    QTouchDevice* mTouchDevice;
    const QByteArray mEventFilterType;
    const QEvent::Type mEventType;

    UbuntuWindow *mLastFocusedWindow;
    QAtomicInt mPendingFocusGainedEvents;
};

#endif // UBUNTU_INPUT_H
