/*
 * Copyright (C) 2014-2015 Canonical, Ltd.
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

#ifndef UBUNTU_SCREEN_H
#define UBUNTU_SCREEN_H

#include <qpa/qplatformscreen.h>
#include <QSurfaceFormat>
#include <EGL/egl.h>

#include "cursor.h"

struct MirConnection;

class UbuntuScreen : public QObject, public QPlatformScreen
{
    Q_OBJECT
public:
    UbuntuScreen(MirConnection *connection);
    virtual ~UbuntuScreen();

    // QPlatformScreen methods.
    QImage::Format format() const override { return mFormat; }
    int depth() const override { return mDepth; }
    QRect geometry() const override { return mGeometry; }
    QRect availableGeometry() const override { return mGeometry; }
    QSizeF physicalSize() const override { return mPhysicalSize; }
    Qt::ScreenOrientation nativeOrientation() const override { return mNativeOrientation; }
    Qt::ScreenOrientation orientation() const override { return mNativeOrientation; }
    QPlatformCursor *cursor() const override { return const_cast<UbuntuCursor*>(&mCursor); }

    // New methods.
    QSurfaceFormat surfaceFormat() const { return mSurfaceFormat; }
    EGLDisplay eglDisplay() const { return mEglDisplay; }
    EGLConfig eglConfig() const { return mEglConfig; }
    EGLNativeDisplayType eglNativeDisplay() const { return mEglNativeDisplay; }
    void handleWindowSurfaceResize(int width, int height);
    uint32_t mirOutputId() const { return mOutputId; }

    // QObject methods.
    void customEvent(QEvent* event) override;

private:
    QRect mGeometry;
    QSizeF mPhysicalSize;
    Qt::ScreenOrientation mNativeOrientation;
    Qt::ScreenOrientation mCurrentOrientation;
    QImage::Format mFormat;
    int mDepth;
    uint32_t mOutputId;
    QSurfaceFormat mSurfaceFormat;
    EGLDisplay mEglDisplay;
    EGLConfig mEglConfig;
    EGLNativeDisplayType mEglNativeDisplay;
    UbuntuCursor mCursor;
};

#endif // UBUNTU_SCREEN_H
