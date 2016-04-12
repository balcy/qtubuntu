/*
 * Copyright (C) 2016 Canonical, Ltd.
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

#ifndef UBUNTU_OFFSCREEN_SURFACE_H
#define UBUNTU_OFFSCREEN_SURFACE_H

#include <qpa/qplatformoffscreensurface.h>
#include <QSurfaceFormat>

class QOpenGLFramebufferObject;

class UbuntuOffscreenSurface : public QPlatformOffscreenSurface
{
public:
    UbuntuOffscreenSurface(QOffscreenSurface *offscreenSurface);

    QSurfaceFormat format() const override;
    bool isValid() const override;

    QOpenGLFramebufferObject* buffer() const;
    void setBuffer(QOpenGLFramebufferObject *buffer);

private:
    QOpenGLFramebufferObject *m_buffer;
    QSurfaceFormat m_format;
};

#endif // UBUNTU_OFFSCREEN_SURFACE_H
