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

#include "usim.h"

#include <QThread>
#include <QDebug>

USim::USim(QObject *parent):
  QObject(parent)
{
    m_lpacThread = new QThread(this);
    m_lpacWorker = new LpacWorker();
    m_lpacWorker->moveToThread(m_lpacThread);

    connect(m_lpacThread, &QThread::finished, m_lpacWorker, &QObject::deleteLater);
    connect(this, &USim::destroyed, m_lpacThread, &QThread::quit);

    connect(this, &USim::processLpa, m_lpacWorker, &LpacWorker::processLpa);
    connect(this, &USim::removeEsim, m_lpacWorker, &LpacWorker::removeEsim);
    connect(this, &USim::enableEsim, m_lpacWorker, &LpacWorker::enableEsim);
    connect(this, &USim::disableEsim, m_lpacWorker, &LpacWorker::disableEsim);
    connect(this, &USim::destroyEuicc, m_lpacWorker, &LpacWorker::destroyEuicc);
    connect(this, &USim::getInstalledEsims, m_lpacWorker, &LpacWorker::getInstalledEsims);

    connect(m_lpacWorker, &LpacWorker::esimsChanged, this, [this](QList<eSIMInfo> esims) {
        qDebug() << "USim received updated eSIMs:" << esims.size();
        for (const auto& esim : esims) {
            qDebug() << "eSIM Info - Name:" << esim.name
                     << ", Provider:" << esim.providerName
                     << ", ICCID:" << esim.iccid
                     << ", Enabled:" << esim.enabled;
        }
        m_esims = esims;
        emit esimsChanged(m_esims);
    });

    connect(m_lpacWorker, &LpacWorker::stateChanged, this, [this](USimNamespace::LpacState state) {
        if (state == USimNamespace::LpacState::STARTING) {
            m_busy = true;
            emit busyChanged(m_busy);
        } else if (state == USimNamespace::LpacState::DONE) {
            m_busy = false;
            emit busyChanged(m_busy);
        }

        m_state = state;
        emit stateChanged(m_state);
    });

    connect(m_lpacWorker, &LpacWorker::askForUserConfirmation, this, [this](QString simName, QString providerName, QString iccid) {
        eSIMInfo info;
        info.name = simName;
        info.providerName = providerName;
        info.iccid = iccid;
        m_installingEsim = info;
        emit installingEsimChanged(m_installingEsim);
    });

    connect(this, &USim::confirmEsimInstall, this, [this](bool confirmed) {
        m_lpacWorker->m_userConfirmed = confirmed;
        m_lpacWorker->m_userConfirmation.wakeAll();
    });

    m_lpacThread->start();
    qDebug() << "This thread is: " << QThread::currentThread() << " and the lpac thread is: " << m_lpacThread;
}

