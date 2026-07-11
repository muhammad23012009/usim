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

#ifndef USIM_H
#define USIM_H

#include <QObject>
#include <QThread>
#include "lpacworker.h"
#include "types.h"

class USim : public QObject {
    Q_OBJECT

    Q_PROPERTY(QList<eSIMInfo> esims MEMBER m_esims NOTIFY esimsChanged)
    Q_PROPERTY(USimNamespace::LpacState state MEMBER m_state NOTIFY stateChanged)
    Q_PROPERTY(bool busy MEMBER m_busy NOTIFY busyChanged)
    Q_PROPERTY(eSIMInfo installingEsim MEMBER m_installingEsim NOTIFY installingEsimChanged)

public:
    explicit USim(QObject *parent = nullptr);
    ~USim() {
        m_lpacThread->quit();
        m_lpacThread->wait();
    }

public slots:
    void openManualDialog();

signals:
    void processLpa(const QString& lpaString);
    void removeEsim(const QString& iccid);
    void enableEsim(const QString& iccid);
    void disableEsim(const QString& iccid);
    void renameEsim(const QString& iccid, const QString& nickname);
    void destroyEuicc();
    void getInstalledEsims();
    void esimsChanged(QList<eSIMInfo> esims);
    void stateChanged(USimNamespace::LpacState state);
    void busyChanged(bool busy);
    void installingEsimChanged(eSIMInfo installingEsim);
    void confirmEsimInstall(bool confirmed);
    void showManualSimDialog();

private:
    QList<eSIMInfo> m_esims;
    USimNamespace::LpacState m_state = USimNamespace::LpacState::IDLE;
    bool m_busy = false;
    eSIMInfo m_installingEsim;

    QThread* m_lpacThread;
    LpacWorker* m_lpacWorker;
};

#endif
