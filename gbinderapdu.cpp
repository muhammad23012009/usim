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

#include <gbinder.h>
#include "gbinderapdu.hpp"
#include <euicc/hexutil.h>

constexpr const char* HIDL_SERVICE_DEVICE = "/dev/hwbinder";
constexpr const char* HIDL_SERVICE_IFACE = "android.hardware.radio@1.0::IRadio";
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

GBinderLocalReply *GBinderWorker::radioResponseHandler(GBinderLocalObject *obj, GBinderRemoteRequest *req, guint code,
                                                          guint flags, int *status, void *user_data)
{
    GBinderWorker *self = static_cast<GBinderWorker *>(user_data);
    GBinderReader reader;
    gbinder_remote_request_init_reader(req, &reader);

    const struct radio_response_info *resp = gbinder_reader_read_hidl_struct(&reader, struct radio_response_info);
    std::cout << "Received radio response. Type: " << resp->type << ", Serial: " << resp->serial << ", Error: " << resp->error << std::endl;
    std::cout << "Transaction code: " << code << ", Flags: " << flags << std::endl;

    if (code == HIDL_SERVICE_ICC_OPEN_LOGICAL_CHANNEL_CALLBACK) {
        std::cout << "Received response for IRadio::iccOpenLogicalChannel1" << std::endl;
        gbinder_reader_read_int32(&reader, &g_channelId);
        self->m_wait.wakeAll();
    } else if (code == HIDL_SERVICE_ICC_TRANSMIT_APDU_LOGICAL_CHANNEL_CALLBACK) {
        const icc_io_result *icc_io_res = gbinder_reader_read_hidl_struct(&reader, struct icc_io_result);
        g_lastIccIoResult.sw1 = icc_io_res->sw1;
        g_lastIccIoResult.sw2 = icc_io_res->sw2;
        g_lastIccIoResult.simResponse.data.str = strndup(icc_io_res->simResponse.data.str, icc_io_res->simResponse.len);
        g_lastIccIoResult.simResponse.len = icc_io_res->simResponse.len;
        g_lastIccIoResult.simResponse.owns_buffer = TRUE;
        self->m_wait.wakeAll();
    } else if (code == HIDL_SERVICE_ICC_CLOSE_LOGICAL_CHANNEL_CALLBACK) {
        std::cout << "Received response for IRadio::iccCloseLogicalChannel" << std::endl;
        self->m_wait.wakeAll();
    } else {
        std::cerr << "Received unknown radio response code: " << code << std::endl;
    }

    return nullptr;
}

void GBinderWorker::onLogicChannelOpen(uint8_t *aid, uint8_t aid_len)
{
    std::string fqname = HIDL_SERVICE_IFACE;
    fqname += "/slot2";

    m_sm = gbinder_servicemanager_new(HIDL_SERVICE_DEVICE);
    if (!m_sm) {
        // Handle error
        return;
    }

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

    auto responseCallback = gbinder_servicemanager_new_local_object(m_sm, HIDL_SERVICE_IFACE, radioResponseHandler, this);
    auto request = gbinder_client_new_request(m_client);
    GBinderWriter writer;
    gbinder_local_request_init_writer(request, &writer);
    gbinder_writer_append_local_object(&writer, responseCallback);
    gbinder_writer_append_local_object(&writer, nullptr);
    gbinder_client_transact_sync_reply(m_client, HIDL_SERVICE_SET_RESPONSE_FUNCTIONS, request, &status);
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
    gbinder_writer_append_hidl_string_copy(&writer, (char *)aid_hex);
    gbinder_writer_append_int32(&writer, 0);
    status = gbinder_client_transact_sync_oneway(m_client, HIDL_SERVICE_ICC_OPEN_LOGICAL_CHANNEL, request);
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

    std::cout << "APDU req: " << tx_hex << std::endl;

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
    int status = gbinder_client_transact_sync_oneway(m_client, HIDL_SERVICE_ICC_TRANSMIT_APDU_LOGICAL_CHANNEL, request);
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
        gbinder_client_transact_sync_oneway(m_client, HIDL_SERVICE_ICC_CLOSE_LOGICAL_CHANNEL, req);
        gbinder_local_request_unref(req);

        g_channelId = -1;
    }
}

GbinderApduInterface::GbinderApduInterface()
{
    m_worker = new GBinderWorker();
    m_workerThread = new QThread();

    QObject::connect(this, &GbinderApduInterface::logicChannelOpen, m_worker, &GBinderWorker::onLogicChannelOpen, Qt::QueuedConnection);
    QObject::connect(this, QOverload<uint8_t*, uint32_t>::of(&GbinderApduInterface::transmit), m_worker, &GBinderWorker::onTransmit, Qt::QueuedConnection);
    QObject::connect(this, &GbinderApduInterface::cleanup, m_worker, &GBinderWorker::onCleanup, Qt::QueuedConnection);

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
    emit cleanup();
}

int GbinderApduInterface::logic_channel_open(struct euicc_ctx *ctx, const uint8_t *aid, uint8_t aid_len)
{
    m_worker->m_mutex.lock();

    uint8_t *aid_copy = (uint8_t *)malloc(aid_len);
    memcpy(aid_copy, aid, aid_len);

    // Will be freed by the worker
    emit logicChannelOpen(aid_copy, aid_len);

    m_worker->m_wait.wait(&m_worker->m_mutex);
    m_worker->m_mutex.unlock();

    return g_channelId;
}

void GbinderApduInterface::logic_channel_close(struct euicc_ctx *ctx, uint8_t channel)
{
    m_worker->m_mutex.lock();

    emit cleanup();

    m_worker->m_wait.wait(&m_worker->m_mutex);
    m_worker->m_mutex.unlock();

    gbinder_client_unref(m_worker->m_client);
    gbinder_remote_object_unref(m_worker->m_remote);
    gbinder_servicemanager_unref(m_worker->m_sm);
}

int GbinderApduInterface::transmit(struct euicc_ctx *ctx, uint8_t **rx, uint32_t *rx_len, const uint8_t *tx, uint32_t tx_len)
{
    m_worker->m_mutex.lock();

    uint8_t *tx_copy = (uint8_t *)malloc(tx_len);
    memcpy(tx_copy, tx, tx_len);

    // tx_copy will be freed by the worker
    emit transmit(tx_copy, tx_len);

    m_worker->m_wait.wait(&m_worker->m_mutex);
    m_worker->m_mutex.unlock();

    *rx_len = g_lastIccIoResult.simResponse.len / 2 + 2;
    *rx = (uint8_t *)calloc(*rx_len, sizeof(uint8_t));
    ::euicc_hexutil_hex2bin_r(*rx, *rx_len, g_lastIccIoResult.simResponse.data.str, g_lastIccIoResult.simResponse.len);
    (*rx)[*rx_len - 2] = g_lastIccIoResult.sw1;
    (*rx)[*rx_len - 1] = g_lastIccIoResult.sw2;

    // see radio_response_transact -- this is our buffer.
    free((void *)g_lastIccIoResult.simResponse.data.str);

    return 0;
}
