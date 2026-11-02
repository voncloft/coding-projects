// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 voncloft-casino contributors
#pragma once
#include <QSettings>

namespace casino {
inline bool migrateLegacySettings(QSettings &current,QSettings &legacy) {
    // Once the new profile exists, it is authoritative. Keep the old file intact.
    if(!current.allKeys().isEmpty())return true;
    for(const auto &key:legacy.allKeys())current.setValue(key,legacy.value(key));
    current.sync();
    return current.status()==QSettings::NoError;
}
}
