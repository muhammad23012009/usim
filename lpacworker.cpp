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

#include "lpacworker.h"
#include "gbinderapdu.hpp"

#include <euicc/es8p.h>
#include <euicc/es9p.h>
#include <euicc/es10a.h>
#include <euicc/es10b.h>

#include <QThread>
#include <QDebug>

int http_interface_transmit(struct euicc_ctx *ctx, const char *url, uint32_t *rcode, uint8_t **rx,
                                   uint32_t *rx_len, const uint8_t *tx, uint32_t tx_len, const char **h);
int _init_libcurl(void);

LpacWorker::LpacWorker(QObject *parent):
  QObject(parent)
{
    _init_libcurl();

    euicc_http_interface *http_interface = new euicc_http_interface();
    memset(http_interface, 0, sizeof(euicc_http_interface));

    euicc_apdu_interface *apdu_interface = new euicc_apdu_interface();
    memset(apdu_interface, 0, sizeof(euicc_apdu_interface));

    http_interface->transmit = http_interface_transmit;

    apdu_interface->connect = [](struct euicc_ctx *ctx) -> int {
        return GbinderApduInterface::instance()->connect(ctx);
    };
    apdu_interface->disconnect = [](struct euicc_ctx *ctx) {
        GbinderApduInterface::instance()->disconnect(ctx);
    };
    apdu_interface->logic_channel_open = [](struct euicc_ctx *ctx, const uint8_t *aid, uint8_t aid_len) -> int {
        return GbinderApduInterface::instance()->logic_channel_open(ctx, aid, aid_len);
    };
    apdu_interface->logic_channel_close = [](struct euicc_ctx *ctx, uint8_t channel) {
        GbinderApduInterface::instance()->logic_channel_close(ctx, channel);
    };
    apdu_interface->transmit = [](struct euicc_ctx *ctx, uint8_t **rx, uint32_t *rx_len, const uint8_t *tx, uint32_t tx_len) -> int {
        return GbinderApduInterface::instance()->transmit(ctx, rx, rx_len, tx, tx_len);
    };

    memset(&m_ctx, 0, sizeof(m_ctx));
    // Setup the eUICC context first
    m_ctx.aid = nullptr;
    m_ctx.aid_len = 0;
    m_ctx.apdu.interface = apdu_interface;
    m_ctx.http.interface = http_interface;
    m_ctx.http.log_fp = stdout;
    m_ctx.apdu.log_fp = stdout;
}

void LpacWorker::processLpa(const QString& lpaString)
{
    EuiccContextGuard guard(&m_ctx, &m_mutex);

    qDebug() << "Processing LPA string:" << lpaString;
    if (lpaString.left(4) != "LPA:") {
        qWarning() << "Invalid LPA string format. Expected to start with 'LPA:'.";
        return;
    }

    QString string = lpaString.mid(4); // Remove the "LPA:" prefix

    QStringList components = string.split('$');

    qDebug() << "version:" << components[0];
    qDebug() << "smdp:" << components[1];
    qDebug() << "activation code:" << components[2];
    qDebug() << "confirmation code:" << components.value(4, "N/A");

    installProfile(components[1], components[2], components.value(4, ""));
}

void LpacWorker::removeEsim(const QString& iccid)
{
    EuiccContextGuard guard(&m_ctx, &m_mutex);
    QByteArray iccidBytes = iccid.toUtf8();
    int ret = 0;

    qDebug() << "Removing eSIM with ICCID:" << iccid;

    if (!m_esims.contains(iccid)) {
        qWarning() << "eSIM with ICCID:" << iccid << "not found.";
        return;
    }

    eSIMInfo esim = m_esims.take(iccid);
    if (esim.enabled) {
        qWarning() << "eSIM with ICCID:" << iccid << "is enabled. Disable in a new context before removing.";
        return;
    }

    ret = es10c_delete_profile(&m_ctx, iccidBytes.constData());
    if (ret != 0) {
        qWarning() << "Failed to delete eSIM with ICCID:" << iccid << "Error code:" << ret;
        return;
    }
    qDebug() << "Deleted eSIM with ICCID:" << iccid;

    processNotifications();

    emit esimsChanged(m_esims.values());
}

void LpacWorker::enableEsim(const QString& iccid)
{
    EuiccContextGuard guard(&m_ctx, &m_mutex);
    QByteArray iccidBytes = iccid.toUtf8();
    int ret = 0;

    qDebug() << "Enabling eSIM with ICCID:" << iccid;

    if (!m_esims.contains(iccid)) {
        qWarning() << "eSIM with ICCID:" << iccid << "not found.";
        return;
    }

    if (m_esims[iccid].enabled) {
        qWarning() << "eSIM with ICCID:" << iccid << "is already enabled.";
        return;
    }

    // Since we're refreshing, the eUICC can act up, so we should probably wait until it has settled
    ret = es10c_enable_profile(&m_ctx, iccidBytes.constData(), 0);
    if (ret != 0) {
        qWarning() << "Failed to enable eSIM with ICCID:" << iccid << "Error code:" << ret;
        return;
    }
    qDebug() << "Enabled eSIM with ICCID:" << iccid;

    m_esims[iccid].enabled = true;
    emit esimsChanged(m_esims.values());

    int maxAttempts = 15;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        qDebug() << "Attempt" << attempt << "to process notifications.";

        char *eidValue = nullptr;
        if (es10c_get_eid(&m_ctx, &eidValue) == 0) {
            qDebug() << "Successfully retrieved EID:" << eidValue;
            free(eidValue);
            break;
        }
        qDebug() << "Waiting for the modem to settle the fuck down";
        QThread::sleep(1);
    }

    processNotifications();
}

void LpacWorker::disableEsim(const QString& iccid)
{
    EuiccContextGuard guard(&m_ctx, &m_mutex);
    QByteArray iccidBytes = iccid.toUtf8();
    int ret = 0;

    qDebug() << "Disabling eSIM with ICCID:" << iccid;

    if (!m_esims.contains(iccid)) {
        qWarning() << "eSIM with ICCID:" << iccid << "not found.";
        return;
    }

    if (!m_esims[iccid].enabled) {
        qWarning() << "eSIM with ICCID:" << iccid << "is already disabled.";
        return;
    }

    // ugh, disabling is still so fucked up, immediately removing an esim after it's installed
    // does not work, and the modem keeps replying with error 5, how the hell do i even fix this
    ret = es10c_disable_profile(&m_ctx, iccidBytes.constData(), 0);
    if (ret != 0) {
        qWarning() << "Failed to disable eSIM with ICCID:" << iccid << "Error code:" << ret;
        return;
    }
    qDebug() << "Disabled eSIM with ICCID:" << iccid;

    m_esims[iccid].enabled = false;
    emit esimsChanged(m_esims.values());

    processNotifications();
}

void LpacWorker::getInstalledEsims()
{
    EuiccContextGuard guard(&m_ctx, &m_mutex);

    if (!m_esimsParsed) {
        es10c_profile_info_list *profileList = nullptr;
        es10c_get_profiles_info(&m_ctx, &profileList);

        for (auto i = profileList; i != nullptr; i = i->next) {
            eSIMInfo info;
            info.iccid = QString::fromUtf8(i->iccid);
            info.name = QString::fromUtf8(i->profileName);
            info.providerName = QString::fromUtf8(i->serviceProviderName);
            info.enabled = i->profileState == ES10C_PROFILE_STATE_ENABLED;
            m_esims.insert(info.iccid, info);
        }
    }

    m_esimsParsed = true;
    emit esimsChanged(m_esims.values());
}

void LpacWorker::installProfile(const QString& smdp, const QString& activationCode, const QString& confirmationCode)
{
    qDebug() << "Installing profile with SMDP:" << smdp
             << "Activation Code:" << activationCode
             << "Confirmation Code:" << confirmationCode;

    QByteArray serverAddress = smdp.toUtf8();
    QByteArray confirmationCodeBytes = confirmationCode.toUtf8();
    int ret = 0;
    const QByteArray imei = "123456789012345";
    es10a_euicc_configured_addresses smdp_addresses = {0};
    eSIMInfo info;

    m_ctx.http.server_address = serverAddress.constData();
    qDebug() << "Setting SMDP server address to:" << m_ctx.http.server_address;

    es10b_load_bound_profile_package_result result = {0};
    es8p_metadata *metadata = nullptr;

    qDebug() << "Getting authentication info";
    ret = es10b_get_euicc_challenge_and_info(&m_ctx);
    if (ret != 0) {
        qWarning() << "Failed to get authentication info." << ret;
        return;
    }

    qDebug() << "initiating auth";
    if (es9p_initiate_authentication(&m_ctx)) {
        qWarning() << "Failed to initiate authentication.";
        return;
    }

    qDebug() << "authenticating server";
    if (es10b_authenticate_server(&m_ctx, activationCode.toUtf8().constData(), imei)) {
        qWarning() << "Failed to authenticate server.";
        return;
    }

    qDebug() << "Authenticating client";
    if (es9p_authenticate_client(&m_ctx)) {
        qWarning() << "Failed to authenticate client.";
        return;
    }

    if (m_ctx.http._internal.prepare_download_param->b64_profileMetadata) {
        if (es8p_metadata_parse(&metadata,
                m_ctx.http._internal.prepare_download_param->b64_profileMetadata) == 0) {
            qDebug() << "Parsed metadata. Profile Name:" << metadata->profileName
                     << "Service Provider Name:" << metadata->serviceProviderName
                     << "ICCID:" << metadata->iccid;
            // if you gate on user confirmation, block the worker here on a
            // QWaitCondition / std::condition_variable until the GUI answers.
        }
    }

    if (m_esims.contains(QString::fromUtf8(metadata->iccid))) {
        qWarning() << "Profile with ICCID:" << QString::fromUtf8(metadata->iccid) << "already exists.";
        goto err;
    }

    if (es10b_prepare_download(&m_ctx, confirmationCode.length() > 0 ? confirmationCode.toUtf8().constData() : nullptr) != 0) {
        qWarning() << "Failed to prepare download.";
        goto err;
    }

    if (es9p_get_bound_profile_package(&m_ctx) != 0) {
        qWarning() << "Failed to get bound profile package.";
        goto err;
    }

    if (es10b_load_bound_profile_package(&m_ctx, &result) != 0) {
        qWarning() << "Failed to load bound profile package.";
        goto err;
    }

    qDebug() << "Successfully installed profile. ICCID:" << result.iccid
             << "Sequence Number:" << result.seqNumber
             << "BPP Command ID:" << result.bppCommandId
             << "Error Reason:" << result.errorReason;

    euicc_http_cleanup(&m_ctx);
    info.iccid = QString::fromUtf8(result.iccid);
    info.name = QString::fromUtf8(metadata->profileName);
    info.providerName = QString::fromUtf8(metadata->serviceProviderName);
    info.enabled = false; // Newly installed profile isn't enabled
    m_esims.insert(info.iccid, info);
    emit esimsChanged(m_esims.values());

    processNotifications();

    es8p_metadata_free(&metadata);

    return;

err:
    es10b_cancel_session(&m_ctx, ES10B_CANCEL_SESSION_REASON_ENDUSERREJECTION);
    es9p_cancel_session(&m_ctx);
    euicc_http_cleanup(&m_ctx);
    es8p_metadata_free(&metadata);
    qWarning() << "Profile installation failed. Session canceled.";
}

// Must be called with EuiccContextGuard held
void LpacWorker::processNotifications()
{
    struct es10b_notification_metadata_list *notifs = nullptr;

    int maxAttempts = 10;
    while (es10b_list_notification(&m_ctx, &notifs) != 0 && maxAttempts-- > 0) {
        qWarning() << "Failed to list notifications";
        QThread::sleep(1);
    }

    for (auto *n = notifs; n != nullptr; n = n->next) {
        struct es10b_pending_notification pending = {0};

        if (es10b_retrieve_notifications_list(&m_ctx, &pending, n->seqNumber) != 0) {
            qWarning() << "Failed to retrieve notification seq" << n->seqNumber;
            continue;
        }

        m_ctx.http.server_address = pending.notificationAddress;
        int ret = es9p_handle_notification(&m_ctx, pending.b64_PendingNotification);
        es10b_pending_notification_free(&pending);

        if (ret == 0) {
            // only remove from the chip once the server has accepted it
            es10b_remove_notification_from_list(&m_ctx, n->seqNumber);
            qDebug() << "Sent + cleared notification seq" << n->seqNumber
                     << "op" << n->profileManagementOperation;
        } else {
            qWarning() << "Server rejected notification seq" << n->seqNumber
                       << "- leaving it on chip for retry";
        }
    }

    es10b_notification_metadata_list_free_all(notifs);
    //euicc_http_cleanup(&m_ctx);
}
