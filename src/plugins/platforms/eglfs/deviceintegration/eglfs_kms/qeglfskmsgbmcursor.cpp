// Copyright (C) 2015 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
// Copyright (C) 2016 The Qt Company Ltd.
// Copyright (C) 2016 Pelagicore AG
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qeglfskmsgbmcursor_p.h"
#include "qeglfskmsgbmscreen_p.h"
#include "qeglfskmsgbmdevice_p.h"

#include <private/qeglfskmsintegration_p.h>

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QLoggingCategory>
#include <QtGui/QPainter>
#include <QtGui/private/qguiapplication_p.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#ifndef DRM_CAP_CURSOR_WIDTH
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#ifndef DRM_CAP_CURSOR_HEIGHT
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

QEglFSKmsGbmCursor::QEglFSKmsGbmCursor(QEglFSKmsGbmScreen *screen)
    : m_screen(screen)
    , m_cursorSize(64, 64) // 64x64 is the old standard size, we now try to query the real size below
    , m_bo(nullptr)
    , m_cursorImage(nullptr, nullptr, 0, 0, 0, 0)
    , m_state(CursorPendingVisible)
    , m_deviceListener(nullptr)
{
    QByteArray hideCursorVal = qgetenv("QT_QPA_EGLFS_HIDECURSOR");
    if (!hideCursorVal.isEmpty() && hideCursorVal.toInt()) {
        m_state = CursorDisabled;
        return;
    }

    uint64_t width, height;
    if ((drmGetCap(m_screen->device()->fd(), DRM_CAP_CURSOR_WIDTH, &width) == 0)
        && (drmGetCap(m_screen->device()->fd(), DRM_CAP_CURSOR_HEIGHT, &height) == 0)) {
        m_cursorSize.setWidth(width);
        m_cursorSize.setHeight(height);
    }

    m_bo = gbm_bo_create(static_cast<QEglFSKmsGbmDevice *>(m_screen->device())->gbmDevice(), m_cursorSize.width(), m_cursorSize.height(),
                         GBM_FORMAT_ARGB8888, GBM_BO_USE_CURSOR_64X64 | GBM_BO_USE_WRITE);
    if (!m_bo) {
        qWarning("Could not create buffer for cursor!");
    } else {
        initCursorAtlas();
    }

    m_deviceListener = new QEglFSKmsGbmCursorDeviceListener(this);
    connect(QGuiApplicationPrivate::inputDeviceManager(), &QInputDeviceManager::deviceListChanged,
            m_deviceListener, &QEglFSKmsGbmCursorDeviceListener::onDeviceListChanged);
    if (!m_deviceListener->hasMouse())
        m_state = CursorPendingHidden;

#ifndef QT_NO_CURSOR
    QCursor cursor(Qt::ArrowCursor);
    changeCursor(&cursor, nullptr);
#endif
    setPos(QPoint(0, 0));
}

QEglFSKmsGbmCursor::~QEglFSKmsGbmCursor()
{
    delete m_deviceListener;

    for (QPlatformScreen *screen : m_screen->virtualSiblings()) {
        QEglFSKmsScreen *kmsScreen = static_cast<QEglFSKmsScreen *>(screen);
        drmModeSetCursor(kmsScreen->device()->fd(), kmsScreen->output().crtc_id, 0, 0, 0);
        drmModeMoveCursor(kmsScreen->device()->fd(), kmsScreen->output().crtc_id, 0, 0);
    }

    if (m_bo) {
        gbm_bo_destroy(m_bo);
        m_bo = nullptr;
    }
}

void QEglFSKmsGbmCursor::updateMouseStatus()
{
    const bool wasVisible = m_state == CursorVisible;
    const bool visible = m_deviceListener->hasMouse();
    if (visible == wasVisible)
        return;

    m_state = visible ? CursorPendingVisible : CursorPendingHidden;

#ifndef QT_NO_CURSOR
    changeCursor(nullptr, m_screen->topLevelAt(pos()));
#endif
}

bool QEglFSKmsGbmCursorDeviceListener::hasMouse() const
{
    return QGuiApplicationPrivate::inputDeviceManager()->deviceCount(QInputDeviceManager::DeviceTypePointer) > 0;
}

void QEglFSKmsGbmCursorDeviceListener::onDeviceListChanged(QInputDeviceManager::DeviceType type)
{
    if (type == QInputDeviceManager::DeviceTypePointer)
        m_cursor->updateMouseStatus();
}

void QEglFSKmsGbmCursor::pointerEvent(const QMouseEvent &event)
{
    setPos(event.globalPosition().toPoint());
}

#ifndef QT_NO_CURSOR
void QEglFSKmsGbmCursor::changeCursor(QCursor *windowCursor, QWindow *window)
{
    Q_UNUSED(window);

    if (!m_bo)
        return;

    if (m_state == CursorPendingHidden) {
        m_state = CursorHidden;
        for (QPlatformScreen *screen : m_screen->virtualSiblings()) {
            QEglFSKmsScreen *kmsScreen = static_cast<QEglFSKmsScreen *>(screen);
            drmModeSetCursor(kmsScreen->device()->fd(), kmsScreen->output().crtc_id, 0, 0, 0);
        }
    }

    if (m_state == CursorHidden || m_state == CursorDisabled)
        return;

    const Qt::CursorShape newShape = windowCursor ? windowCursor->shape() : Qt::ArrowCursor;
    if (newShape == Qt::BitmapCursor) {
        m_cursorImage.set(windowCursor->pixmap().toImage(),
                          windowCursor->hotSpot().x(),
                          windowCursor->hotSpot().y());
    } else {
        // Standard cursor, look up in atlas
        const int width = m_cursorAtlas.cursorWidth;
        const int height = m_cursorAtlas.cursorHeight;
        const qreal ws = (qreal)m_cursorAtlas.cursorWidth / m_cursorAtlas.width;
        const qreal hs = (qreal)m_cursorAtlas.cursorHeight / m_cursorAtlas.height;

        QRect textureRect(ws * (newShape % m_cursorAtlas.cursorsPerRow) * m_cursorAtlas.width,
                          hs * (newShape / m_cursorAtlas.cursorsPerRow) * m_cursorAtlas.height,
                          width,
                          height);
        QPoint hotSpot = m_cursorAtlas.hotSpots[newShape];
        m_cursorImage.set(m_cursorAtlas.image.copy(textureRect),
                          hotSpot.x(),
                          hotSpot.y());
    }

    if (m_cursorImage.image()->width() > m_cursorSize.width() || m_cursorImage.image()->height() > m_cursorSize.height())
        qWarning("Cursor larger than %dx%d, cursor will be clipped.", m_cursorSize.width(), m_cursorSize.height());

    QImage cursorImage(m_cursorSize, QImage::Format_ARGB32);
    cursorImage.fill(Qt::transparent);

    QPainter painter;
    painter.begin(&cursorImage);
    painter.drawImage(0, 0, *m_cursorImage.image());
    painter.end();

    gbm_bo_write(m_bo, cursorImage.constBits(), cursorImage.sizeInBytes());

    uint32_t handle = gbm_bo_get_handle(m_bo).u32;

    if (m_state == CursorPendingVisible)
        m_state = CursorVisible;

    for (QPlatformScreen *screen : m_screen->virtualSiblings()) {
        QEglFSKmsScreen *kmsScreen = static_cast<QEglFSKmsScreen *>(screen);
        if (kmsScreen->isCursorOutOfRange())
            continue;
        int status = drmModeSetCursor(kmsScreen->device()->fd(), kmsScreen->output().crtc_id, handle,
                                      m_cursorSize.width(), m_cursorSize.height());
        if (status != 0)
            qWarning("Could not set cursor on screen %s: %d", kmsScreen->name().toLatin1().constData(), status);
    }
}
#endif // QT_NO_CURSOR

QPoint QEglFSKmsGbmCursor::pos() const
{
    return m_pos;
}

void QEglFSKmsGbmCursor::setPos(const QPoint &pos)
{
    for (QPlatformScreen *screen : m_screen->virtualSiblings()) {
        QEglFSKmsScreen *kmsScreen = static_cast<QEglFSKmsScreen *>(screen);
        const QRect screenGeom = kmsScreen->geometry();
        const QPoint origin = screenGeom.topLeft();
        const QPoint localPos = pos - origin;
        const QPoint adjustedLocalPos = localPos - m_cursorImage.hotspot();

        if (localPos.x() < 0 || localPos.y() < 0
            || localPos.x() >= screenGeom.width() || localPos.y() >= screenGeom.height())
        {
            if (!kmsScreen->isCursorOutOfRange()) {
                kmsScreen->setCursorOutOfRange(true);
                drmModeSetCursor(kmsScreen->device()->fd(), kmsScreen->output().crtc_id, 0, 0, 0);
            }
        } else {
            int ret;
            if (kmsScreen->isCursorOutOfRange() && m_bo) {
                kmsScreen->setCursorOutOfRange(false);
                uint32_t handle = gbm_bo_get_handle(m_bo).u32;
                ret = drmModeSetCursor(kmsScreen->device()->fd(), kmsScreen->output().crtc_id,
                                       handle, m_cursorSize.width(), m_cursorSize.height());
            } else {
                ret = drmModeMoveCursor(kmsScreen->device()->fd(), kmsScreen->output().crtc_id,
                                        adjustedLocalPos.x(), adjustedLocalPos.y());
            }
            if (ret == 0)
                m_pos = pos;
            else
                qWarning("Failed to move cursor on screen %s: %d", kmsScreen->name().toLatin1().constData(), ret);

            kmsScreen->handleCursorMove(pos);
        }
    }
}

void QEglFSKmsGbmCursor::initCursorAtlas()
{
    static QByteArray json = qgetenv("QT_QPA_EGLFS_CURSOR");
    if (json.isEmpty())
        json = ":/cursor.json";

    qCDebug(qLcEglfsKmsDebug) << "Initializing cursor atlas from" << json;

    QFile file(QString::fromUtf8(json));
    if (!file.open(QFile::ReadOnly)) {
        for (QPlatformScreen *screen : m_screen->virtualSiblings()) {
            QEglFSKmsScreen *kmsScreen = static_cast<QEglFSKmsScreen *>(screen);
            drmModeSetCursor(kmsScreen->device()->fd(), kmsScreen->output().crtc_id, 0, 0, 0);
            drmModeMoveCursor(kmsScreen->device()->fd(), kmsScreen->output().crtc_id, 0, 0);
        }
        m_state = CursorDisabled;
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject object = doc.object();

    QString atlas = object.value("image"_L1).toString();
    Q_ASSERT(!atlas.isEmpty());

    const int cursorsPerRow = object.value("cursorsPerRow"_L1).toDouble();
    Q_ASSERT(cursorsPerRow);
    m_cursorAtlas.cursorsPerRow = cursorsPerRow;

    const QJsonArray hotSpots = object.value("hotSpots"_L1).toArray();
    Q_ASSERT(hotSpots.count() == Qt::LastCursor + 1);
    for (int i = 0; i < hotSpots.count(); i++) {
        QPoint hotSpot(hotSpots[i].toArray()[0].toDouble(), hotSpots[i].toArray()[1].toDouble());
        m_cursorAtlas.hotSpots << hotSpot;
    }

    QImage image = QImage(atlas).convertToFormat(QImage::Format_ARGB32);
    m_cursorAtlas.cursorWidth = image.width() / m_cursorAtlas.cursorsPerRow;
    m_cursorAtlas.cursorHeight = image.height() / ((Qt::LastCursor + cursorsPerRow) / cursorsPerRow);
    m_cursorAtlas.width = image.width();
    m_cursorAtlas.height = image.height();
    m_cursorAtlas.image = image;
}

QT_END_NAMESPACE
