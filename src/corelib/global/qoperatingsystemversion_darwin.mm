// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qoperatingsystemversion_p.h"

#include <QtCore/qfile.h>
#include <QtCore/qversionnumber.h>
#include <QtCore/qstring.h>
#include <QtCore/qdebug.h>

#ifdef Q_OS_MACOS
#  include <CoreServices/CoreServices.h>
#endif

QT_BEGIN_NAMESPACE

QOperatingSystemVersionBase QOperatingSystemVersionBase::current_impl()
{
    int major = 10;
    int minor = 0;
    int micro = 0;

#if defined(Q_OS_MACOS)
    // Use Gestalt for pre-10.10
    SInt32 gestaltVersion = 0;
    if (Gestalt(gestaltSystemVersion, &gestaltVersion) == noErr) {
        // gestaltSystemVersion is 0xMMmmbb (MM=major, mm=minor, bb=bugfix)
        major = ((gestaltVersion & 0xFF0000) >> 16);
        minor = ((gestaltVersion & 0x00FF00) >> 8);
        micro = (gestaltVersion & 0x0000FF);

        // On 10.4+, gestaltSystemVersion is reliable
        // But for versions >= 10.10, it may return 10.9 for compatibility,
        // so check SystemVersion.plist as fallback
        if (major == 10 && minor == 9) {
            QFile vfile(QStringLiteral("/System/Library/CoreServices/SystemVersion.plist"));
            if (vfile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QByteArray data = vfile.readAll();
                vfile.close();
                // Look for <key>ProductVersion</key><string>X.Y.Z</string>
                QByteArray key = "<key>ProductVersion</key>";
                int idx = data.indexOf(key);
                if (idx >= 0) {
                    int sidx = data.indexOf("<string>", idx);
                    int eidx = data.indexOf("</string>", sidx);
                    if (sidx > 0 && eidx > sidx) {
                        QByteArray version = data.mid(sidx + 8, eidx - sidx - 8).trimmed();
                        QList<QByteArray> parts = version.split('.');
                        if (parts.size() > 1) {
                            major = parts.at(0).toInt();
                            minor = parts.at(1).toInt();
                            micro = parts.size() > 2 ? parts.at(2).toInt() : 0;
                        }
                    }
                }
            }
        }
    }
#endif

    QOperatingSystemVersionBase operatingSystemVersion;
    operatingSystemVersion.m_os = currentType();
    operatingSystemVersion.m_major = major;
    operatingSystemVersion.m_minor = minor;
    operatingSystemVersion.m_micro = micro;
    return operatingSystemVersion;
}
