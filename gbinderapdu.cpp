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

#include <iostream>
#include <string>
#include <format>
#include <future>
#include <QMutex>
#include <QThread>

#include <gbinder.h>
#include "gbinderapdu.hpp"

extern "C" {
#include <euicc/hexutil.h>
}

constexpr const char* HIDL_SERVICE_DEVICE = "/dev/hwbinder";
constexpr const char* HIDL_SERVICE_IFACE = "android.hardware.radio@1.0::IRadio";
constexpr const char* HIDL_SERVICE_IFACE_CALLBACK = "android.hardware.radio@1.0::IRadioResponse";
constexpr const char* HIDL_SERVICE_IFACE_INDICATIONS = "android.hardware.radio@1.0::IRadioIndication";

constexpr const char* AIDL_SIM_IFACE       = "android.hardware.radio.sim.IRadioSim";
constexpr const char* AIDL_SIM_RESPONSE    = "android.hardware.radio.sim.IRadioSimResponse";
constexpr const char* AIDL_SIM_INDICATION  = "android.hardware.radio.sim.IRadioSimIndication";

static const GBinderWriterField sim_apdu_f[] = {GBINDER_WRITER_FIELD_HIDL_STRING(struct sim_apdu, data),
                                                GBINDER_WRITER_FIELD_END()};

static const GBinderWriterType sim_apdu_t = {GBINDER_WRITER_STRUCT_NAME_AND_SIZE(struct sim_apdu), sim_apdu_f};

static GMainContext* g_binderContext = nullptr;
static GMainLoop* g_binderLoop = nullptr;

static int g_channelId = -1;
static icc_io_result g_lastIccIoResult = {0};

struct radio_response_info {
    int32_t type;
    int32_t serial;
    int32_t error;
};

struct sim_refresh_result {
    int32_t type;
    int efId;
    GBinderHidlString aid;
};

gsize binder_read_parcelable_size(GBinderReader* reader) {
    guint32 non_null = 0, payload_size = 0;
    if (gbinder_reader_read_uint32(reader, &non_null) && non_null &&
        gbinder_reader_read_uint32(reader, &payload_size) &&
        payload_size >= sizeof(payload_size)) {
        return payload_size - sizeof(payload_size);
    }
    return 0;
}

GBinderLocalReply *GBinderWorker::radioResponseHandler(GBinderLocalObject *obj, GBinderRemoteRequest *req, guint code,
                                                          guint flags, int *status, void *user_data)
{
    GBinderWorker *self = static_cast<GBinderWorker *>(user_data);
    GBinderReader reader;
    int error, serial, type;
    gbinder_remote_request_init_reader(req, &reader);

    if (self->m_aidl) {
        binder_read_parcelable_size(&reader);
        gbinder_reader_read_int32(&reader, &type);
        gbinder_reader_read_int32(&reader, &serial);
        gbinder_reader_read_int32(&reader, &error);
    } else {
        const struct radio_response_info *resp = gbinder_reader_read_hidl_struct(&reader, struct radio_response_info);
        error = resp->error;
        serial = resp->serial;
        type = resp->type;
        std::cout << "Received radio response. Type: " << type << ", Serial: " << serial << ", Error: " << error << std::endl;
        std::cout << "Transaction code: " << code << ", Flags: " << flags << std::endl;
    }

    if (error != 0) {
        std::cerr << "Error in radio response. Error code: " << error << std::endl;
    }

    if (code == HIDL_SERVICE_ICC_OPEN_LOGICAL_CHANNEL_CALLBACK || code == AIDL_SERVICE_ICC_OPEN_LOGICAL_CHANNEL_CALLBACK) {
        std::cout << "Received response for IRadio::iccOpenLogicalChannel1" << std::endl;
        gbinder_reader_read_int32(&reader, &g_channelId);
        self->m_openReady = true;
        if (error != 0)
            g_channelId = -1;

    } else if (code == HIDL_SERVICE_ICC_TRANSMIT_APDU_LOGICAL_CHANNEL_CALLBACK || code == AIDL_SERVICE_ICC_TRANSMIT_APDU_LOGICAL_CHANNEL_CALLBACK) {
        if (self->m_aidl) {
            binder_read_parcelable_size(&reader);
            int sw1, sw2;
            gbinder_reader_read_int32(&reader, &sw1);
            gbinder_reader_read_int32(&reader, &sw2);
            char* hex = gbinder_reader_read_string16(&reader);
            g_lastIccIoResult.sw1 = sw1;
            g_lastIccIoResult.sw2 = sw2;
            g_lastIccIoResult.simResponse.data.str = hex;
            g_lastIccIoResult.simResponse.len = strlen(hex);
            g_lastIccIoResult.simResponse.owns_buffer = TRUE;
            self->m_transmitResponseReady = true;
            std::cout << "Received response for Radio::iccTransmitApduLogicalChannel, hex: " << g_lastIccIoResult.simResponse.data.str << std::endl;
        } else {
            const icc_io_result *icc_io_res = gbinder_reader_read_hidl_struct(&reader, struct icc_io_result);
            g_lastIccIoResult.sw1 = icc_io_res->sw1;
            g_lastIccIoResult.sw2 = icc_io_res->sw2;
            g_lastIccIoResult.simResponse.data.str = strndup(icc_io_res->simResponse.data.str, icc_io_res->simResponse.len);
            g_lastIccIoResult.simResponse.len = icc_io_res->simResponse.len;
            g_lastIccIoResult.simResponse.owns_buffer = TRUE;
            self->m_transmitResponseReady = true;
            std::cout << "Received response for IRadio::iccTransmitApduLogicalChannel, hex: " << g_lastIccIoResult.simResponse.data.str << std::endl;
        }

    } else if (code == HIDL_SERVICE_ICC_CLOSE_LOGICAL_CHANNEL_CALLBACK || code == AIDL_SERVICE_ICC_CLOSE_LOGICAL_CHANNEL_CALLBACK) {
        std::cout << "Received response for IRadio::iccCloseLogicalChannel" << std::endl;
    } else if (code == HIDL_SERVICE_SET_SIM_POWER_CALLBACK) {
        std::cout << "Received response for IRadio::setSimPower" << std::endl;
        self->m_simPowerReady = true;
    } else if (code == HIDL_SERVICE_GET_ICC_CARD_STATUS_CALLBACK || code == AIDL_SERVICE_GET_ICC_CARD_STATUS_CALLBACK) {
        int cardState = 0;
        std::cout << "Received response for IRadio::getIccCardStatus" << std::endl;
        if (self->m_aidl) {
            binder_read_parcelable_size(&reader);
            gbinder_reader_read_int32(&reader, &cardState);
        } else {
            const card_status* cardStatus = gbinder_reader_read_hidl_struct(&reader, struct card_status);
            cardState = cardStatus->cardState;
        }

        self->m_cardStatusReceived = true;

        if (cardState == 1) {
            std::cout << "Card state is present. Card state: " << cardState << std::endl;
            self->m_cardStatusReady = true;
            // Doesn't hurt to do this
            self->m_refreshReceived = true;
        } else {
            std::cout << "Card state is not present. Card state: " << cardState << std::endl;
        }

    } else {
        std::cerr << "Received unknown radio response code: " << code << std::endl;
        return nullptr;
    }

    self->m_wait.wakeAll();
    return nullptr;
}

GBinderLocalReply *GBinderWorker::radioIndicationHandler(GBinderLocalObject *obj, GBinderRemoteRequest *req, guint code,
                                                         guint flags, int *status, void *user_data)
{
    GBinderWorker *self = static_cast<GBinderWorker *>(user_data);
    GBinderReader reader;
    gbinder_remote_request_init_reader(req, &reader);

    std::cout << "Received radio indication. Code: " << code << ", Flags: " << flags << std::endl;

    // SIM state change indication
    // code 19 is simStateChanged on HIDL, code 6 is simStateChanged on AIDL
    if (code == 19 || code == 6) {
        std::cout << "sim state changed" << std::endl;
        // If we're still waiting on simRefresh, just wake up and let it query the card status instead
        self->m_refreshReceived = true;
        self->m_wait.wakeAll();
    }

    // SIM refresh indication
    // code 17 is simRefresh on HIDL, code 5 is simRefresh on AIDL
    if (code == 17 || code == 5) {
        int indicationType = 0;
        gbinder_reader_read_int32(&reader, &indicationType);
        const sim_refresh_result* refreshResult = gbinder_reader_read_hidl_struct(&reader, struct sim_refresh_result);
        // SIM_INIT == 1 / SIM_RESET == 2
        if (refreshResult->type == 1 || refreshResult->type == 2) {
            // Wake up the waiting thread and let it query the card status
            self->m_refreshReceived = true;
            std::cout << "SIM refresh indication received. Type: " << refreshResult->type << ", EF ID: " << refreshResult->efId << std::endl;
            self->m_wait.wakeAll();
            // We will wake up the wait_for_refresh() function in sim state change signal
        }
    }

    return nullptr;
}

GBinderWorker::GBinderWorker()
{
    // Figure out if we need AIDL or HIDL
    std::string aidlSerrviceName = "android.hardware.radio.sim.IRadioSim/slot2";
    auto sm = gbinder_servicemanager_new("/dev/binder");
    int status;

    auto remoteObj = gbinder_servicemanager_get_service_sync(sm, aidlSerrviceName.c_str(), &status);
    if (remoteObj) {
        std::cout << "Using AIDL interface for IRadioSim" << std::endl;
        gbinder_servicemanager_unref(sm);
        m_aidl = true;
    }
}

void GBinderWorker::onLogicChannelOpen(uint8_t *aid, uint8_t aid_len)
{
    std::string serviceDevice = m_aidl ? "/dev/binder" : HIDL_SERVICE_DEVICE;
    std::string serviceIface = m_aidl ? AIDL_SIM_IFACE : HIDL_SERVICE_IFACE;
    std::string serviceIfaceCallback = m_aidl ? AIDL_SIM_RESPONSE : HIDL_SERVICE_IFACE_CALLBACK;
    std::string serviceIfaceIndications = m_aidl ? AIDL_SIM_INDICATION : HIDL_SERVICE_IFACE_INDICATIONS;
    std::string fqname = m_aidl ? AIDL_SIM_IFACE : HIDL_SERVICE_IFACE;
    fqname += "/slot2";

    std::cout << "Using serviceDevice: " << serviceDevice << ", serviceIface: " << serviceIface << std::endl;
    std::cout << "fqname: " << fqname << std::endl;

    m_sm = gbinder_servicemanager_new(serviceDevice.c_str());
    if (!m_sm) {
        // Handle error
        return;
    }

    int status = 0;
    m_remote = gbinder_remote_object_ref(gbinder_servicemanager_get_service_sync(m_sm, fqname.c_str(), &status));
    if (!m_remote) {
        std::cout << "failed to get remote service while opening logic channel" << std::endl;
        // Handle error
        gbinder_servicemanager_unref(m_sm);
        return;
    }

    m_client = gbinder_client_new(m_remote, serviceIface.c_str());
    if (!m_client) {
        // Handle error
        gbinder_remote_object_unref(m_remote);
        gbinder_servicemanager_unref(m_sm);
        return;
    }

    auto responseCallback = gbinder_servicemanager_new_local_object(m_sm, serviceIfaceCallback.c_str(), radioResponseHandler, this);
    auto indicationCallback = gbinder_servicemanager_new_local_object(m_sm, serviceIfaceIndications.c_str(), radioIndicationHandler, this);
    if (m_aidl) {
        gbinder_local_object_set_stability(responseCallback, GBINDER_STABILITY_VINTF);
        gbinder_local_object_set_stability(indicationCallback, GBINDER_STABILITY_VINTF);
    }

    auto request = gbinder_client_new_request(m_client);
    GBinderWriter writer;
    gbinder_local_request_init_writer(request, &writer);
    gbinder_writer_append_local_object(&writer, responseCallback);
    gbinder_writer_append_local_object(&writer, indicationCallback);
    gbinder_client_transact_sync_reply(m_client, m_aidl ? AIDL_SERVICE_SET_RESPONSE_FUNCTIONS : HIDL_SERVICE_SET_RESPONSE_FUNCTIONS, request, &status);
    gbinder_local_request_unref(request);

    if (status < 0) {
        // Handle error
        gbinder_client_unref(m_client);
        gbinder_remote_object_unref(m_remote);
        gbinder_servicemanager_unref(m_sm);
        return;
    }

    // Now, try to open the AID
    uint8_t aid_hex[255];
    euicc_hexutil_bin2hex((char *)aid_hex, 255, aid, aid_len);

    request = gbinder_client_new_request(m_client);
    gbinder_local_request_init_writer(request, &writer);
    gbinder_writer_append_int32(&writer, 1000);

    if (m_aidl) {
        gbinder_writer_append_string16(&writer, (char *)aid_hex);
    } else {
        gbinder_writer_append_hidl_string_copy(&writer, (char *)aid_hex);
    }

    gbinder_writer_append_int32(&writer, 0);
    status = gbinder_client_transact_sync_oneway(m_client, m_aidl ? AIDL_SERVICE_ICC_OPEN_LOGICAL_CHANNEL : HIDL_SERVICE_ICC_OPEN_LOGICAL_CHANNEL, request);
    gbinder_local_request_unref(request);

    if (status < 0) {
        std::cerr << "Failed to call IRadio::iccOpenLogicalChannel: " << status << std::endl;
        gbinder_client_unref(m_client);
        gbinder_remote_object_unref(m_remote);
        gbinder_servicemanager_unref(m_sm);
        return;
    }

    std::cout << "Waiting for response from IRadio::iccOpenLogicalChannel..." << std::endl;
    free((void*)aid);
}

void GBinderWorker::onTransmit(uint8_t* tx, uint32_t tx_len)
{
    auto request = gbinder_client_new_request(m_client);
    GBinderWriter writer;
    gbinder_local_request_init_writer(request, &writer);
    gbinder_writer_append_int32(&writer, 1000);

    uint8_t tx_hex[4096] = {0};
    euicc_hexutil_bin2hex((char *)tx_hex, 4096, &tx[5], tx_len - 5);

    if (m_aidl) {
        // AIDL parcelable
        gbinder_writer_append_int32(&writer, 1); // nonnull
        auto start = gbinder_writer_bytes_written(&writer);
        gbinder_writer_append_int32(&writer, -1); // placeholder for size
        gbinder_writer_append_int32(&writer, g_channelId);   // sessionId
        gbinder_writer_append_int32(&writer, tx[0]);         // cla
        gbinder_writer_append_int32(&writer, tx[1]);         // instruction
        gbinder_writer_append_int32(&writer, tx[2]);         // p1
        gbinder_writer_append_int32(&writer, tx[3]);         // p2
        gbinder_writer_append_int32(&writer, tx[4]);         // p3
        gbinder_writer_append_string16(&writer, (char*)tx_hex);     // data (String16)
        gbinder_writer_append_bool(&writer, FALSE);          // isEs10 / trailing field

        // fill the parcelable size field
        gbinder_writer_overwrite_int32(&writer, start, gbinder_writer_bytes_written(&writer) - start);
    } else {
        sim_apdu apdu = {
            .sessionId = g_channelId,
            .cla = tx[0],
            .instruction = tx[1],
            .p1 = tx[2],
            .p2 = tx[3],
            .p3 = tx[4],
            .data =
            {
                .data = {.str = (char *)tx_hex },
                .len = static_cast<guint32>(strlen((char *)tx_hex) + 1),
                .owns_buffer = FALSE,
            },
        };

        gbinder_writer_append_struct(&writer, &apdu, &sim_apdu_t, NULL);
    }

    int status = gbinder_client_transact_sync_oneway(m_client, m_aidl ? AIDL_SERVICE_ICC_TRANSMIT_APDU_LOGICAL_CHANNEL : HIDL_SERVICE_ICC_TRANSMIT_APDU_LOGICAL_CHANNEL, request);
    gbinder_local_request_unref(request);

    std::cout << "Waiting for response from IRadio::iccTransmitApduLogicalChannel..." << std::endl;

    free((void*)tx);
}

void GBinderWorker::onCleanup()
{
    if (m_client && g_channelId != -1) {
        GBinderLocalRequest *req = gbinder_client_new_request(m_client);
        GBinderWriter writer;
        gbinder_local_request_init_writer(req, &writer);
        gbinder_writer_append_int32(&writer, 1000);
        gbinder_writer_append_int32(&writer, g_channelId);
        gbinder_client_transact_sync_oneway(m_client, m_aidl ? AIDL_SERVICE_ICC_CLOSE_LOGICAL_CHANNEL : HIDL_SERVICE_ICC_CLOSE_LOGICAL_CHANNEL, req);
        gbinder_local_request_unref(req);

        g_channelId = -1;
    } else {
        m_wait.wakeAll();
    }
}

void GBinderWorker::onSimPowerOff()
{
    if (!m_client) {
        std::string fqname = HIDL_SERVICE_IFACE;
        fqname += "/slot2";
        m_sm = gbinder_servicemanager_new(HIDL_SERVICE_DEVICE);

        int status = 0;
        m_remote = gbinder_remote_object_ref(gbinder_servicemanager_get_service_sync(m_sm, fqname.c_str(), &status));
        if (!m_remote) {
            // Handle error
            gbinder_servicemanager_unref(m_sm);
            return;
        }

        m_client = gbinder_client_new(m_remote, HIDL_SERVICE_IFACE);
        if (!m_client) {
            // Handle error
            gbinder_remote_object_unref(m_remote);
            gbinder_servicemanager_unref(m_sm);
            return;
        }

        auto responseCallback = gbinder_servicemanager_new_local_object(m_sm, HIDL_SERVICE_IFACE_CALLBACK, radioResponseHandler, this);
        auto indicationCallback = gbinder_servicemanager_new_local_object(m_sm, HIDL_SERVICE_IFACE_INDICATIONS, radioIndicationHandler, this);
        auto request = gbinder_client_new_request(m_client);
        GBinderWriter writer;
        gbinder_local_request_init_writer(request, &writer);
        gbinder_writer_append_local_object(&writer, responseCallback);
        gbinder_writer_append_local_object(&writer, indicationCallback);
        gbinder_client_transact_sync_reply(m_client, HIDL_SERVICE_SET_RESPONSE_FUNCTIONS, request, &status);
        gbinder_local_request_unref(request);

        if (status < 0) {
            // Handle error
            gbinder_client_unref(m_client);
            gbinder_remote_object_unref(m_remote);
            gbinder_servicemanager_unref(m_sm);
            return;
        }
    }

    auto request = gbinder_client_new_request(m_client);
    GBinderWriter writer;
    gbinder_local_request_init_writer(request, &writer);
    gbinder_writer_append_int32(&writer, 1000);
    gbinder_writer_append_bool(&writer, 0); // 0 for power off
    gbinder_client_transact_sync_oneway(m_client, HIDL_SERVICE_SET_SIM_POWER, request);
    gbinder_local_request_unref(request);
}

void GBinderWorker::onSimPowerOn()
{
    auto request = gbinder_client_new_request(m_client);
    GBinderWriter writer;
    gbinder_local_request_init_writer(request, &writer);
    gbinder_writer_append_int32(&writer, 1000);
    gbinder_writer_append_bool(&writer, 1); // 1 for power on
    gbinder_client_transact_sync_oneway(m_client, HIDL_SERVICE_SET_SIM_POWER, request);
    gbinder_local_request_unref(request);
}

void GBinderWorker::onGetCardStatus()
{
    std::cout << "Requesting card status from thread " << QThread::currentThread() << " " << m_client << std::endl;
    auto request = gbinder_client_new_request(m_client);
    GBinderWriter writer;
    gbinder_local_request_init_writer(request, &writer);
    gbinder_writer_append_int32(&writer, 1000);
    gbinder_client_transact_sync_oneway(m_client, m_aidl ? AIDL_SERVICE_GET_ICC_CARD_STATUS : HIDL_SERVICE_GET_ICC_CARD_STATUS, request);
    gbinder_local_request_unref(request);
}

GbinderApduInterface::GbinderApduInterface()
{
    m_worker = new GBinderWorker();
    m_workerThread = new QThread();

    QObject::connect(this, &GbinderApduInterface::logicChannelOpen, m_worker, &GBinderWorker::onLogicChannelOpen, Qt::QueuedConnection);
    QObject::connect(this, QOverload<uint8_t*, uint32_t>::of(&GbinderApduInterface::transmit), m_worker, &GBinderWorker::onTransmit, Qt::QueuedConnection);
    QObject::connect(this, &GbinderApduInterface::cleanup, m_worker, &GBinderWorker::onCleanup, Qt::QueuedConnection);

    QObject::connect(this, &GbinderApduInterface::simPowerOff, m_worker, &GBinderWorker::onSimPowerOff, Qt::QueuedConnection);
    QObject::connect(this, &GbinderApduInterface::simPowerOn, m_worker, &GBinderWorker::onSimPowerOn, Qt::QueuedConnection);

    QObject::connect(this, &GbinderApduInterface::getCardStatus, m_worker, &GBinderWorker::onGetCardStatus, Qt::QueuedConnection);

    m_worker->moveToThread(m_workerThread);
    m_workerThread->start();
}

void GbinderApduInterface::init()
{
}

int GbinderApduInterface::connect(struct euicc_ctx *ctx)
{
    return 0;
}

void GbinderApduInterface::disconnect(struct euicc_ctx *ctx)
{
    std::cout << "Disconnecting from GbinderApduInterface" << std::endl;
    m_worker->m_mutex.lock();

    emit cleanup();

    m_worker->m_wait.wait(&m_worker->m_mutex);
    m_worker->m_mutex.unlock();
}

int GbinderApduInterface::logic_channel_open(struct euicc_ctx *ctx, const uint8_t *aid, uint8_t aid_len)
{
    g_channelId = -1;

    // Before we do anything, check if the card is actually ready
    m_worker->m_mutex.lock();
    arm_refresh();

#if 0
    // TODO: Is this even a good idea? We should probably only check card status if we get a sim state changed indication
    std::cout << "Checking if card is ready from thread " << QThread::currentThread() << std::endl;
    while (!m_worker->m_cardStatusReady) {
        m_worker->m_cardStatusReceived = false;
        std::cout << "Card status not ready. Requesting card status..." << std::endl;
        emit getCardStatus();
        while (!m_worker->m_cardStatusReceived) {
            std::cout << "Waiting for card status response..." << std::endl;
            m_worker->m_wait.wait(&m_worker->m_mutex);
        }

        std::cout << "Card status received. Checking if ready..." << std::endl;
        QThread::sleep(1); // Sleep for a second before checking again
    }
#endif

    m_worker->m_mutex.unlock();

    m_worker->m_mutex.lock();

    uint8_t *aid_copy = (uint8_t *)malloc(aid_len);
    memcpy(aid_copy, aid, aid_len);

    m_worker->m_openReady = false;

    // Will be freed by the worker
    emit logicChannelOpen(aid_copy, aid_len);

    while (!m_worker->m_openReady)
        m_worker->m_wait.wait(&m_worker->m_mutex);

    m_worker->m_mutex.unlock();

    return g_channelId;
}

void GbinderApduInterface::logic_channel_close(struct euicc_ctx *ctx, uint8_t channel)
{
    std::cout << "Closing logic channel: " << (int)channel << std::endl;
    m_worker->m_mutex.lock();

    emit cleanup();

    m_worker->m_wait.wait(&m_worker->m_mutex);
    m_worker->m_mutex.unlock();

    gbinder_client_unref(m_worker->m_client);
    gbinder_remote_object_unref(m_worker->m_remote);
    gbinder_servicemanager_unref(m_worker->m_sm);

    m_worker->m_client = nullptr;
    m_worker->m_remote = nullptr;
    m_worker->m_sm = nullptr;
}

int GbinderApduInterface::transmit(struct euicc_ctx *ctx, uint8_t **rx, uint32_t *rx_len, const uint8_t *tx, uint32_t tx_len)
{
    m_worker->m_mutex.lock();

    uint8_t *tx_copy = (uint8_t *)malloc(tx_len);
    memcpy(tx_copy, tx, tx_len);

    // tx_copy will be freed by the worker
    emit transmit(tx_copy, tx_len);

    m_worker->m_transmitResponseReady = false;

    while (!m_worker->m_transmitResponseReady)
        m_worker->m_wait.wait(&m_worker->m_mutex);

    m_worker->m_mutex.unlock();

    *rx_len = g_lastIccIoResult.simResponse.len / 2 + 2;
    *rx = (uint8_t *)calloc(*rx_len, sizeof(uint8_t));
    ::euicc_hexutil_hex2bin_r(*rx, *rx_len, g_lastIccIoResult.simResponse.data.str, g_lastIccIoResult.simResponse.len);
    (*rx)[*rx_len - 2] = g_lastIccIoResult.sw1;
    (*rx)[*rx_len - 1] = g_lastIccIoResult.sw2;

    // see radio_response_transact -- this is our buffer.
    free((void *)g_lastIccIoResult.simResponse.data.str);
    g_lastIccIoResult.simResponse.data.str = nullptr;

    return 0;
}

void GbinderApduInterface::clean()
{
    m_worker->m_mutex.lock();

    emit cleanup();

    m_worker->m_wait.wait(&m_worker->m_mutex);
    m_worker->m_mutex.unlock();

    gbinder_client_unref(m_worker->m_client);
    m_worker->m_client = nullptr;
    gbinder_remote_object_unref(m_worker->m_remote);
    m_worker->m_remote = nullptr;
    gbinder_servicemanager_unref(m_worker->m_sm);
    m_worker->m_sm = nullptr;
}

void GbinderApduInterface::wait_for_refresh()
{
    m_worker->m_mutex.lock();

    std::cout << "Waiting for SIM refresh indication..." << std::endl;

    // Armed in arm_refresh()
    while (!m_worker->m_refreshReceived)
        m_worker->m_wait.wait(&m_worker->m_mutex);

    m_worker->m_mutex.unlock();

    std::cout << "SIM refresh indication received and handled." << std::endl;
    std::cout << "Querying card status..." << std::endl;

    m_worker->m_mutex.lock();

    std::cout << "Checking if card is ready..." << std::endl;
    while (!m_worker->m_cardStatusReady) {
        m_worker->m_cardStatusReceived = false;
        std::cout << "Card status not ready. Requesting card status..." << std::endl;
        emit getCardStatus();
        while (!m_worker->m_cardStatusReceived) {
            std::cout << "Waiting for card status response..." << std::endl;
            m_worker->m_wait.wait(&m_worker->m_mutex);
        }

        std::cout << "Card status received. Checking if ready..." << std::endl;
        QThread::sleep(1); // Sleep for a second before checking again
    }

    m_worker->m_mutex.unlock();

    std::cout << "Card status received and handled." << std::endl;
}

// This method will only be called after logic_channel_close is called, so we are responsible for creating our own objects
void GbinderApduInterface::sim_power_off()
{
    std::cout << "Sim power off requested" << std::endl;

    m_worker->m_mutex.lock();

    m_worker->m_simPowerReady = false;

    emit simPowerOff();

    while (!m_worker->m_simPowerReady)
        m_worker->m_wait.wait(&m_worker->m_mutex);

    m_worker->m_mutex.unlock();
}

void GbinderApduInterface::sim_power_on()
{
    std::cout << "Sim power on requested" << std::endl;

    m_worker->m_mutex.lock();

    emit simPowerOn();

    m_worker->m_simPowerReady = false;
    emit simPowerOn();

    while (!m_worker->m_simPowerReady)
        m_worker->m_wait.wait(&m_worker->m_mutex);

    m_worker->m_mutex.unlock();

    // Since we allocated all the service managers and requests, free them
    clean();
}