/*
 * Copyright (C) 2014,2016 Canonical, Ltd.
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
#include "nativeinterface.h"
#include "screen.h"
#include "glcontext.h"
#include "window.h"

// Qt
#include <private/qguiapplication_p.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qscreen.h>
#include <QtCore/QMap>

class UbuntuResourceMap : public QMap<QByteArray, UbuntuNativeInterface::ResourceType>
{
public:
    UbuntuResourceMap()
        : QMap<QByteArray, UbuntuNativeInterface::ResourceType>() {
        insert("egldisplay", UbuntuNativeInterface::EglDisplay);
        insert("eglcontext", UbuntuNativeInterface::EglContext);
        insert("nativeorientation", UbuntuNativeInterface::NativeOrientation);
        insert("display", UbuntuNativeInterface::Display);
        insert("mirconnection", UbuntuNativeInterface::MirConnection);
        insert("mirsurface", UbuntuNativeInterface::MirSurface);
        insert("scale", UbuntuNativeInterface::Scale);
        insert("formfactor", UbuntuNativeInterface::FormFactor);
    }
};

Q_GLOBAL_STATIC(UbuntuResourceMap, ubuntuResourceMap)

UbuntuNativeInterface::UbuntuNativeInterface(const UbuntuClientIntegration *integration)
    : mIntegration(integration)
    , mGenericEventFilterType(QByteArrayLiteral("Event"))
    , mNativeOrientation(nullptr)
{
}

UbuntuNativeInterface::~UbuntuNativeInterface()
{
    delete mNativeOrientation;
    mNativeOrientation = nullptr;
}

void* UbuntuNativeInterface::nativeResourceForIntegration(const QByteArray &resourceString)
{
    const QByteArray lowerCaseResource = resourceString.toLower();

    if (!ubuntuResourceMap()->contains(lowerCaseResource)) {
        return nullptr;
    }

    const ResourceType resourceType = ubuntuResourceMap()->value(lowerCaseResource);

    if (resourceType == UbuntuNativeInterface::MirConnection) {
        return mIntegration->mirConnection();
    } else {
        return nullptr;
    }
}

void* UbuntuNativeInterface::nativeResourceForContext(
    const QByteArray& resourceString, QOpenGLContext* context)
{
    if (!context)
        return nullptr;

    const QByteArray kLowerCaseResource = resourceString.toLower();

    if (!ubuntuResourceMap()->contains(kLowerCaseResource))
        return nullptr;

    const ResourceType kResourceType = ubuntuResourceMap()->value(kLowerCaseResource);

    if (kResourceType == UbuntuNativeInterface::EglContext)
        return static_cast<UbuntuOpenGLContext*>(context->handle())->eglContext();
    else
        return nullptr;
}

void* UbuntuNativeInterface::nativeResourceForWindow(const QByteArray& resourceString, QWindow* window)
{
    const QByteArray kLowerCaseResource = resourceString.toLower();
    if (!ubuntuResourceMap()->contains(kLowerCaseResource))
        return NULL;
    const ResourceType kResourceType = ubuntuResourceMap()->value(kLowerCaseResource);

    switch (kResourceType) {
    case EglDisplay:
        return mIntegration->eglDisplay();
    case NativeOrientation:
        // Return the device's native screen orientation.
        if (window) {
            UbuntuScreen *ubuntuScreen = static_cast<UbuntuScreen*>(window->screen()->handle());
            mNativeOrientation = new Qt::ScreenOrientation(ubuntuScreen->nativeOrientation());
        } else {
            QPlatformScreen *platformScreen = QGuiApplication::primaryScreen()->handle();
            mNativeOrientation = new Qt::ScreenOrientation(platformScreen->nativeOrientation());
        }
        return mNativeOrientation;
    case MirSurface:
        if (window) {
            auto ubuntuWindow = static_cast<UbuntuWindow*>(window->handle());
            if (ubuntuWindow) {
                return ubuntuWindow->mirSurface();
            } else {
                return nullptr;
            }
        } else {
            return nullptr;
        }
    default:
        return nullptr;
    }
}

void* UbuntuNativeInterface::nativeResourceForScreen(const QByteArray& resourceString, QScreen* screen)
{
    const QByteArray kLowerCaseResource = resourceString.toLower();
    if (!ubuntuResourceMap()->contains(kLowerCaseResource))
        return NULL;
    const ResourceType kResourceType = ubuntuResourceMap()->value(kLowerCaseResource);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    auto ubuntuScreen = static_cast<UbuntuScreen*>(screen->handle());
    if (kResourceType == UbuntuNativeInterface::Display) {
        return mIntegration->eglNativeDisplay();
    // Changes to the following properties are emitted via the UbuntuNativeInterface::screenPropertyChanged
    // signal fired by UbuntuScreen. Connect to this signal for these properties updates.
    // WARNING: code highly thread unsafe!
    } else if (kResourceType == UbuntuNativeInterface::Scale) {
        // In application code, read with:
        //    float scale = *reinterpret_cast<float*>(nativeResourceForScreen("scale", screen()));
        return &ubuntuScreen->mScale;
    } else if (kResourceType == UbuntuNativeInterface::FormFactor) {
        return &ubuntuScreen->mFormFactor;
    } else
        return NULL;
}

// Changes to these properties are emitted via the UbuntuNativeInterface::windowPropertyChanged
// signal fired by UbuntuWindow. Connect to this signal for these properties updates.
QVariantMap UbuntuNativeInterface::windowProperties(QPlatformWindow *window) const
{
    QVariantMap propertyMap;
    auto w = static_cast<UbuntuWindow*>(window);
    if (w) {
        propertyMap.insert("scale", w->scale());
        propertyMap.insert("formFactor", w->formFactor());
    }
    return propertyMap;
}

QVariant UbuntuNativeInterface::windowProperty(QPlatformWindow *window, const QString &name) const
{
    auto w = static_cast<UbuntuWindow*>(window);
    if (!w) {
        return QVariant();
    }

    if (name == QStringLiteral("scale")) {
        return w->scale();
    } else if (name == QStringLiteral("formFactor")) {
        return w->formFactor();
    } else {
        return QVariant();
    }
}

QVariant UbuntuNativeInterface::windowProperty(QPlatformWindow *window, const QString &name, const QVariant &defaultValue) const
{
    QVariant returnVal = windowProperty(window, name);
    if (!returnVal.isValid()) {
        return defaultValue;
    } else {
        return returnVal;
    }
}
