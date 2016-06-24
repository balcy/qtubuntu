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

#ifndef UBUNTU_NATIVE_INTERFACE_H
#define UBUNTU_NATIVE_INTERFACE_H

#include <qpa/qplatformnativeinterface.h>

#include "integration.h"

class QPlatformScreen;

class UbuntuNativeInterface : public QPlatformNativeInterface {
    Q_OBJECT
public:
    enum ResourceType { EglDisplay, EglContext, NativeOrientation, Display, MirConnection, Scale, FormFactor };

    UbuntuNativeInterface(const UbuntuClientIntegration *integration);
    ~UbuntuNativeInterface();

    // QPlatformNativeInterface methods.
    void* nativeResourceForIntegration(const QByteArray &resource) override;
    void* nativeResourceForContext(const QByteArray& resourceString,
                                   QOpenGLContext* context) override;
    void* nativeResourceForWindow(const QByteArray& resourceString,
                                  QWindow* window) override;
    void* nativeResourceForScreen(const QByteArray& resourceString,
                                  QScreen* screen) override;

    QVariantMap windowProperties(QPlatformWindow *window) const override;
    QVariant windowProperty(QPlatformWindow *window, const QString &name) const override;
    QVariant windowProperty(QPlatformWindow *window, const QString &name, const QVariant &defaultValue) const override;

    // New methods.
    const QByteArray& genericEventFilterType() const { return mGenericEventFilterType; }

Q_SIGNALS: // New signals
    void screenPropertyChanged(QPlatformScreen *screen, const QString &propertyName);

private:
    const UbuntuClientIntegration *mIntegration;
    const QByteArray mGenericEventFilterType;
    Qt::ScreenOrientation* mNativeOrientation;
};

#endif // UBUNTU_NATIVE_INTERFACE_H
