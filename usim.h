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

class USim : public QObject {
    Q_OBJECT

    Q_PROPERTY(QList<eSIMInfo> esims MEMBER m_esims NOTIFY esimsChanged)

public:
    explicit USim(QObject *parent = nullptr);
    ~USim() {
        m_lpacThread->quit();
        m_lpacThread->wait();
    }

signals:
    void processLpa(const QString& lpaString);
    void removeEsim(const QString& iccid);
    void enableEsim(const QString& iccid);
    void disableEsim(const QString& iccid);
    void getInstalledEsims();
    void esimsChanged(QList<eSIMInfo> esims);

private:
    QList<eSIMInfo> m_esims;

    QThread* m_lpacThread;
    LpacWorker* m_lpacWorker;
};

#endif
