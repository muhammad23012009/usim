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

#ifndef LPAC_WORKER_H
#define LPAC_WORKER_H

#include <QObject>
#include <QDebug>
#include <QMutex>
#include <QWaitCondition>

extern "C" {
#include <euicc/euicc.h>
#include <euicc/interface.h>
}

#include "types.h"

// Runs in a separate thread (because why not i am a man of free will)
class LpacWorker : public QObject
{
    Q_OBJECT

public:
    explicit LpacWorker(QObject *parent = nullptr);
    ~LpacWorker() = default;

    QMutex m_userConfirmationMutex;
    QWaitCondition m_userConfirmation;
    bool m_userConfirmed = false;

signals:
    void esimsChanged(QList<eSIMInfo> esims);
    void stateChanged(USimNamespace::LpacState state);
    void askForUserConfirmation(QString simName, QString providerName, QString iccid);

public slots:
    void processLpa(const QString& lpaString);
    void removeEsim(const QString& iccid);
    void enableEsim(const QString& iccid);
    void disableEsim(const QString& iccid);
    void destroyEuicc();
    void getInstalledEsims();

private:
    void installProfile(const QString& smdp, const QString& activationCode, const QString& confirmationCode);

    void processNotifications();

    class EuiccContextGuard {
    public:
        explicit EuiccContextGuard(euicc_ctx *ctx, QMutex *mutex) : m_ctx(ctx), m_mutex(mutex) {
            qDebug() << "Initializing eUICC context";
            m_mutex->lock();

            if (euicc_init(m_ctx) != 0) {
                qWarning() << "Failed to initialize eUICC context";
            }
        }

        ~EuiccContextGuard() {
            qDebug() << "Finalizing eUICC context";
            euicc_fini(m_ctx);
            m_mutex->unlock();
        }
    private:
        euicc_ctx *m_ctx;
        QMutex *m_mutex;
    };

    // ICCID <-> eSIMInfo map
    QMap<QString, eSIMInfo> m_esims;

    bool m_esimsParsed = false;
    bool m_euiccInitialized = false;
    euicc_ctx m_ctx;
    QMutex m_mutex;
};

#endif
