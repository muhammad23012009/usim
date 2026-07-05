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

#ifndef GBINDER_APDU_H
#define GBINDER_APDU_H

#include <QObject>
#include <QWaitCondition>
#include <QMutex>
#include <cstdint>
#include <thread>

#include <gbinder/gbinder_writer.h>

extern "C" {
#include <euicc/interface.h>
}

// ref: IRadio
#define HIDL_SERVICE_SET_RESPONSE_FUNCTIONS GBINDER_FIRST_CALL_TRANSACTION
#define HIDL_SERVICE_GET_ICC_CARD_STATUS (GBINDER_FIRST_CALL_TRANSACTION + 1)
#define HIDL_SERVICE_ICC_OPEN_LOGICAL_CHANNEL (GBINDER_FIRST_CALL_TRANSACTION + 105)
#define HIDL_SERVICE_ICC_CLOSE_LOGICAL_CHANNEL (GBINDER_FIRST_CALL_TRANSACTION + 106)
#define HIDL_SERVICE_ICC_TRANSMIT_APDU_LOGICAL_CHANNEL (GBINDER_FIRST_CALL_TRANSACTION + 107)
#define HIDL_SERVICE_SET_SIM_POWER (GBINDER_FIRST_CALL_TRANSACTION + 128)

// ref: IRadioResponse
#define HIDL_SERVICE_GET_ICC_CARD_STATUS_CALLBACK GBINDER_FIRST_CALL_TRANSACTION
#define HIDL_SERVICE_ICC_OPEN_LOGICAL_CHANNEL_CALLBACK (GBINDER_FIRST_CALL_TRANSACTION + 104)
#define HIDL_SERVICE_ICC_CLOSE_LOGICAL_CHANNEL_CALLBACK (GBINDER_FIRST_CALL_TRANSACTION + 105)
#define HIDL_SERVICE_ICC_TRANSMIT_APDU_LOGICAL_CHANNEL_CALLBACK (GBINDER_FIRST_CALL_TRANSACTION + 106)
#define HIDL_SERVICE_SET_SIM_POWER_CALLBACK (GBINDER_FIRST_CALL_TRANSACTION + 127)

struct icc_io_result {
    int32_t sw1;
    int32_t sw2;
    GBinderHidlString simResponse;
};

struct sim_apdu {
    int32_t sessionId;
    int32_t cla;
    int32_t instruction;
    int32_t p1;
    int32_t p2;
    int32_t p3;
    GBinderHidlString data;
};

struct app_status {
    int32_t appType;
    int32_t appState;
    int32_t persoSubstate;          // applicable only if app_state == SUBSCRIPTION_PERSO
    GBinderHidlString aidPtr;                        // e.g., from 0xA0, 0x00 -> 0x41,
                                          // 0x30, 0x30, 0x30
    GBinderHidlString appLabelPtr;
    int32_t pin1Replaced;                 // applicable to USIM, CSIM and ISIM
    int32_t pin1;
    int32_t pin2;
};

struct card_status {
    int32_t cardState;
    int32_t universalPinState;
    int32_t gsmUmtsSubscriptionAppIndex;
    int32_t cdmaSubscriptionAppIndex;
    int32_t imsSubscriptionAppIndex;
    // Vector of AppState
    GBinderHidlVec applications;
};

class GBinderWorker : public QObject
{
    Q_OBJECT
public slots:
    void onLogicChannelOpen(uint8_t *aid, uint8_t aid_len);
    void onTransmit(uint8_t *tx, uint32_t tx_len);
    void onCleanup();
    void onSimPowerOff();
    void onSimPowerOn();
    void onGetCardStatus();

public:
    QWaitCondition m_wait;
    QMutex m_mutex;

    GBinderServiceManager *m_sm = nullptr;
    GBinderRemoteObject *m_remote = nullptr;
    GBinderClient *m_client = nullptr;
    bool m_openReady = false;
    bool m_cardStatusReady = false;
    bool m_refreshReceived = false;
    bool m_transmitResponseReady = false;

private:
    static GBinderLocalReply *radioResponseHandler(GBinderLocalObject *obj, GBinderRemoteRequest *req, guint code,
                                                   guint flags, int *status, void *user_data);
    static GBinderLocalReply *radioIndicationHandler(GBinderLocalObject *obj, GBinderRemoteRequest *req, guint code,
                                                     guint flags, int *status, void *user_data);
};

class GbinderApduInterface : public QObject
{
    Q_OBJECT

public:
    explicit GbinderApduInterface();

    static GbinderApduInterface* instance()
    {
        static GbinderApduInterface instance;
        return &instance;
    }

    void init();

    // Methods called by the LpacWorker. These can and will block
    int connect(struct euicc_ctx *ctx);
    void disconnect(struct euicc_ctx *ctx);
    int logic_channel_open(struct euicc_ctx *ctx, const uint8_t *aid, uint8_t aid_len);
    void logic_channel_close(struct euicc_ctx *ctx, uint8_t channel);
    int transmit(struct euicc_ctx *ctx, uint8_t **rx, uint32_t *rx_len, const uint8_t *tx, uint32_t tx_len);
    void wait_for_refresh();

    void clean();
    void sim_power_off();
    void sim_power_on();

    void arm_refresh() {
        m_worker->m_refreshReceived = false;
        m_worker->m_cardStatusReady = false;
    }

// Signals used to dispatch work to the GBinderWorker thread
signals:
    void logicChannelOpen(uint8_t *aid, uint8_t aid_len);
    void transmit(uint8_t *tx, uint32_t tx_len);
    void simPowerOff();
    void simPowerOn();
    void getCardStatus();

    void cleanup();

private:
    GBinderWorker* m_worker = nullptr;
    QThread* m_workerThread = nullptr;
};

#endif // GBINDER_APDU_H
