/*
 * Copyright (C) 2014 Canonical, Ltd.
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

// Qt
#include <QCoreApplication>
#include <QtCore/qmath.h>
#include <QScreen>
#include <QThread>
#include <qpa/qwindowsysteminterface.h>
#include <QtPlatformSupport/private/qeglconvenience_p.h>

// Qt sensors
#include <QtSensors/QOrientationSensor>
#include <QtSensors/QOrientationReading>

// local
#include "screen.h"
#include "logging.h"

// platform-api
#include <ubuntu/application/ui/display.h>
#include <ubuntu/application/ui/options.h>

static const int kSwapInterval = 1;

#if !defined(QT_NO_DEBUG)
static void printEglConfig(EGLDisplay display, EGLConfig config) {
  DASSERT(display != EGL_NO_DISPLAY);
  DASSERT(config != nullptr);
  static const struct { const EGLint attrib; const char* name; } kAttribs[] = {
    { EGL_BUFFER_SIZE, "EGL_BUFFER_SIZE" },
    { EGL_ALPHA_SIZE, "EGL_ALPHA_SIZE" },
    { EGL_BLUE_SIZE, "EGL_BLUE_SIZE" },
    { EGL_GREEN_SIZE, "EGL_GREEN_SIZE" },
    { EGL_RED_SIZE, "EGL_RED_SIZE" },
    { EGL_DEPTH_SIZE, "EGL_DEPTH_SIZE" },
    { EGL_STENCIL_SIZE, "EGL_STENCIL_SIZE" },
    { EGL_CONFIG_CAVEAT, "EGL_CONFIG_CAVEAT" },
    { EGL_CONFIG_ID, "EGL_CONFIG_ID" },
    { EGL_LEVEL, "EGL_LEVEL" },
    { EGL_MAX_PBUFFER_HEIGHT, "EGL_MAX_PBUFFER_HEIGHT" },
    { EGL_MAX_PBUFFER_PIXELS, "EGL_MAX_PBUFFER_PIXELS" },
    { EGL_MAX_PBUFFER_WIDTH, "EGL_MAX_PBUFFER_WIDTH" },
    { EGL_NATIVE_RENDERABLE, "EGL_NATIVE_RENDERABLE" },
    { EGL_NATIVE_VISUAL_ID, "EGL_NATIVE_VISUAL_ID" },
    { EGL_NATIVE_VISUAL_TYPE, "EGL_NATIVE_VISUAL_TYPE" },
    { EGL_SAMPLES, "EGL_SAMPLES" },
    { EGL_SAMPLE_BUFFERS, "EGL_SAMPLE_BUFFERS" },
    { EGL_SURFACE_TYPE, "EGL_SURFACE_TYPE" },
    { EGL_TRANSPARENT_TYPE, "EGL_TRANSPARENT_TYPE" },
    { EGL_TRANSPARENT_BLUE_VALUE, "EGL_TRANSPARENT_BLUE_VALUE" },
    { EGL_TRANSPARENT_GREEN_VALUE, "EGL_TRANSPARENT_GREEN_VALUE" },
    { EGL_TRANSPARENT_RED_VALUE, "EGL_TRANSPARENT_RED_VALUE" },
    { EGL_BIND_TO_TEXTURE_RGB, "EGL_BIND_TO_TEXTURE_RGB" },
    { EGL_BIND_TO_TEXTURE_RGBA, "EGL_BIND_TO_TEXTURE_RGBA" },
    { EGL_MIN_SWAP_INTERVAL, "EGL_MIN_SWAP_INTERVAL" },
    { EGL_MAX_SWAP_INTERVAL, "EGL_MAX_SWAP_INTERVAL" },
    { -1, NULL }
  };
  const char* string = eglQueryString(display, EGL_VENDOR);
  LOG("EGL vendor: %s", string);
  string = eglQueryString(display, EGL_VERSION);
  LOG("EGL version: %s", string);
  string = eglQueryString(display, EGL_EXTENSIONS);
  LOG("EGL extensions: %s", string);
  LOG("EGL configuration attibutes:");
  for (int index = 0; kAttribs[index].attrib != -1; index++) {
    EGLint value;
    if (eglGetConfigAttrib(display, config, kAttribs[index].attrib, &value))
      LOG("  %s: %d", kAttribs[index].name, static_cast<int>(value));
  }
}
#endif

class OrientationReadingEvent : public QEvent {
public:
    OrientationReadingEvent(QEvent::Type type, QOrientationReading::Orientation orientation)
        : QEvent(type)
        , mOrientation(orientation) {
        DLOG("OrientationReadingEvent::OrientationReadingEvent()");
    }
    ~OrientationReadingEvent() {
        DLOG("OrientationReadingEvent::~OrientationReadingEvent()");
    }
    static const QEvent::Type mType;
    QOrientationReading::Orientation mOrientation;
};

const QEvent::Type OrientationReadingEvent::mType =
        static_cast<QEvent::Type>(QEvent::registerEventType());


UbuntuScreen::UbuntuScreen()
    : mFormat(QImage::Format_RGB32)
    , mDepth(32)
    , mSurfaceFormat()
    , mEglDisplay(EGL_NO_DISPLAY)
    , mEglConfig(nullptr)
{
    // Initialize EGL.
    ASSERT(eglBindAPI(EGL_OPENGL_ES_API) == EGL_TRUE);

    UAUiDisplay* u_display = ua_ui_display_new_with_index(0);
    mEglNativeDisplay = ua_ui_display_get_native_type(u_display);
    ASSERT((mEglDisplay = eglGetDisplay(mEglNativeDisplay)) != EGL_NO_DISPLAY);
    ua_ui_display_destroy(u_display);
    ASSERT(eglInitialize(mEglDisplay, nullptr, nullptr) == EGL_TRUE);

    // Configure EGL buffers format.
    mSurfaceFormat.setRedBufferSize(8);
    mSurfaceFormat.setGreenBufferSize(8);
    mSurfaceFormat.setBlueBufferSize(8);
    mSurfaceFormat.setAlphaBufferSize(8);
    mSurfaceFormat.setDepthBufferSize(24);
    mSurfaceFormat.setStencilBufferSize(8);
    if (!qEnvironmentVariableIsEmpty("QTUBUNTU_MULTISAMPLE")) {
        mSurfaceFormat.setSamples(4);
        DLOG("ubuntumirclient: setting MSAA to 4 samples");
    }
#ifdef QTUBUNTU_USE_OPENGL
    mSurfaceFormat.setRenderableType(QSurfaceFormat::OpenGL);
#else
    mSurfaceFormat.setRenderableType(QSurfaceFormat::OpenGLES);
#endif
    mEglConfig = q_configFromGLFormat(mEglDisplay, mSurfaceFormat, true);

    #if !defined(QT_NO_DEBUG)
    printEglConfig(mEglDisplay, mEglConfig);
    #endif

    // Set vblank swap interval.
    int swapInterval = kSwapInterval;
    QByteArray swapIntervalString = qgetenv("QTUBUNTU_SWAPINTERVAL");
    if (!swapIntervalString.isEmpty()) {
        bool ok;
        swapInterval = swapIntervalString.toInt(&ok);
        if (!ok)
            swapInterval = kSwapInterval;
    }
    DLOG("ubuntumirclient: setting swap interval to %d", swapInterval);
    eglSwapInterval(mEglDisplay, swapInterval);

    // Get screen resolution.
    UAUiDisplay* display = ua_ui_display_new_with_index(0);
    const int kScreenWidth = ua_ui_display_query_horizontal_res(display);
    const int kScreenHeight = ua_ui_display_query_vertical_res(display);
    DASSERT(kScreenWidth > 0 && kScreenHeight > 0);
    DLOG("ubuntumirclient: screen resolution: %dx%d", kScreenWidth, kScreenHeight);
    ua_ui_display_destroy(display);

    mGeometry = QRect(0, 0, kScreenWidth, kScreenHeight);
    mAvailableGeometry = QRect(0, 0, kScreenWidth, kScreenHeight);

    DLOG("QUbuntuScreen::QUbuntuScreen (this=%p)", this);

    // Set the default orientation based on the initial screen dimmensions.
    mNativeOrientation = (mAvailableGeometry.width() >= mAvailableGeometry.height()) ? Qt::LandscapeOrientation : Qt::PortraitOrientation;

    // If it's a landscape device (i.e. some tablets), start in landscape, otherwise portrait
    mCurrentOrientation = (mNativeOrientation == Qt::LandscapeOrientation) ? Qt::LandscapeOrientation : Qt::PortraitOrientation;

    mOrientationSensor = new QOrientationSensor();
    QObject::connect(mOrientationSensor, SIGNAL(readingChanged()), this, SLOT(onOrientationReadingChanged()));
    mOrientationSensor->start();
}

UbuntuScreen::~UbuntuScreen()
{
    eglTerminate(mEglDisplay);
    delete mOrientationSensor;
}

void UbuntuScreen::toggleSensors(const bool enable) const {
    DLOG("UbuntuScreen::toggleSensors (this=%p, enable=%d)", this, enable);
    if (enable)
        mOrientationSensor->start();
    else
        mOrientationSensor->stop();
}

void UbuntuScreen::customEvent(QEvent* event) {
    DLOG("UbuntuScreen::customEvent (event: %p)", event);
    DASSERT(QThread::currentThread() == thread());

    OrientationReadingEvent* oReadingEvent = static_cast<OrientationReadingEvent*>(event);
    switch (oReadingEvent->mOrientation) {
        case QOrientationReading::LeftUp: {
            mCurrentOrientation = (mNativeOrientation == Qt::LandscapeOrientation) ?
                        Qt::InvertedPortraitOrientation : Qt::LandscapeOrientation;
            break;
        }
        case QOrientationReading::TopUp: {
            mCurrentOrientation = (mNativeOrientation == Qt::LandscapeOrientation) ?
                        Qt::LandscapeOrientation : Qt::PortraitOrientation;
            break;
        }
        case QOrientationReading::RightUp: {
            mCurrentOrientation = (mNativeOrientation == Qt::LandscapeOrientation) ?
                        Qt::PortraitOrientation : Qt::InvertedLandscapeOrientation;
            break;
        }
        case QOrientationReading::TopDown: {
            mCurrentOrientation = (mNativeOrientation == Qt::LandscapeOrientation) ?
                        Qt::InvertedLandscapeOrientation : Qt::InvertedPortraitOrientation;
            break;
        }
        default: {
            DLOG("Unknown orientation.");
        }
    }

    // Raise the event signal so that client apps know the orientation changed
    QWindowSystemInterface::handleScreenOrientationChange(screen(), mCurrentOrientation);
}

void UbuntuScreen::onOrientationReadingChanged() {
    DLOG("UbuntuScreen::onOrientationReadingChanged");
    DASSERT(mOrientationSensor != NULL);

    // Make sure to switch to the main Qt thread context
    QCoreApplication::postEvent(this, new OrientationReadingEvent(
                                    OrientationReadingEvent::mType, mOrientationSensor->reading()->orientation()));
}

