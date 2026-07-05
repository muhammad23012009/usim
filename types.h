/*
 * This file is part of USim (https://github.com/muhammad23012009/USim)
 * Copyright (c) 2026 Muhammad Asif  <thevancedgamer@mentallysanemainliners.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TYPES_H
#define TYPES_H

#include <QObject>

namespace USimNamespace {
    Q_NAMESPACE         // Tells moc to look at this namespace

    enum LpacState {
        IDLE,
        ENABLING,
        DISABLING,
        REMOVING,
        // Indication for client to start busy loop/etc
        STARTING,
        GETTING_CHALLENGE,
        INIT_AUTH,
        AUTH_SERVER,
        AUTH_CLIENT,
        METADATA_PARSING,
        PREPARE_DOWNLOAD,
        GET_BOUND_PACKAGE,
        // Downloads and loads the eSIM onto the eUICC
        DOWNLOAD_PACKAGE,
        // Power-cycles the eUICC and processes pending notifications
        PROCESS_AND_FINISH,
        // End the busy wait
        DONE
    };
    Q_ENUM_NS(LpacState)
};

struct eSIMInfo {
    Q_GADGET

    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString providerName MEMBER providerName)
    Q_PROPERTY(QString iccid MEMBER iccid)
    Q_PROPERTY(bool enabled MEMBER enabled)

public:
    QString name;
    QString providerName;
    QString iccid;
    bool enabled;

    bool operator==(const eSIMInfo& other) const {
        return name == other.name &&
               providerName == other.providerName &&
               iccid == other.iccid &&
               enabled == other.enabled;
    }
};

#endif
