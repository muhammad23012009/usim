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
#include <euicc/euicc.h>
#include <euicc/interface.h>

// Runs in a separate thread (because why not i am a man of free will)
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

class LpacWorker : public QObject
{
    Q_OBJECT

public:
    explicit LpacWorker(QObject *parent = nullptr);
    ~LpacWorker() = default;

signals:
    void esimsChanged(QList<eSIMInfo> esims);

public slots:
    void processLpa(const QString& lpaString);
    void removeEsim(const QString& iccid);
    void enableEsim(const QString& iccid);
    void disableEsim(const QString& iccid);
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
