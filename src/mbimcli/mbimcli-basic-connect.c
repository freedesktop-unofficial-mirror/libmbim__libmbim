/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * mbimcli -- Command line interface to control MBIM devices
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2013 - 2014 Aleksander Morgado <aleksander@aleksander.es>
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>

#include <libmbim-glib.h>

#include "mbimcli.h"

/* Context */
typedef struct {
    MbimDevice *device;
    GCancellable *cancellable;
} Context;
static Context *ctx;

/* Options */
static gboolean  query_device_caps_flag;
static gboolean  query_subscriber_ready_status_flag;
static gboolean  query_radio_state_flag;
static gchar    *set_radio_state_str;
static gboolean  query_device_services_flag;
static gboolean  query_pin_flag;
static gchar    *set_pin_enter_str;
static gchar    *set_pin_change_str;
static gchar    *set_pin_enable_str;
static gchar    *set_pin_disable_str;
static gchar    *set_pin_enter_puk_str;
static gboolean  query_home_provider_flag;
static gboolean  query_preferred_providers_flag;
static gboolean  query_visible_providers_flag;
static gboolean  query_register_state_flag;
static gboolean  set_register_state_automatic_flag;
static gboolean  query_signal_state_flag;
static gboolean  query_packet_service_flag;
static gboolean  set_packet_service_attach_flag;
static gboolean  set_packet_service_detach_flag;
static gboolean  query_connect_flag;
static gchar    *set_connect_activate_str;
static gboolean  set_connect_deactivate_flag;
static gboolean  query_packet_statistics_flag;

static GOptionEntry entries[] = {
    { "query-device-caps", 0, 0, G_OPTION_ARG_NONE, &query_device_caps_flag,
      "Query device capabilities",
      NULL
    },
    { "query-subscriber-ready-status", 0, 0, G_OPTION_ARG_NONE, &query_subscriber_ready_status_flag,
      "Query subscriber ready status",
      NULL
    },
    { "query-radio-state", 0, 0, G_OPTION_ARG_NONE, &query_radio_state_flag,
      "Query radio state",
      NULL
    },
    { "set-radio-state", 0, 0, G_OPTION_ARG_STRING, &set_radio_state_str,
      "Set radio state",
      "[(on|off)]"
    },
    { "query-device-services", 0, 0, G_OPTION_ARG_NONE, &query_device_services_flag,
      "Query device services",
      NULL
    },
    { "query-pin-state", 0, 0, G_OPTION_ARG_NONE, &query_pin_flag,
      "Query PIN state",
      NULL
    },
    { "enter-pin", 0, 0, G_OPTION_ARG_STRING, &set_pin_enter_str,
      "Enter PIN",
      "[(current PIN)]"
    },
    { "change-pin", 0, 0, G_OPTION_ARG_STRING, &set_pin_change_str,
      "Change PIN",
      "[(current PIN),(new PIN)]"
    },
    { "enable-pin", 0, 0, G_OPTION_ARG_STRING, &set_pin_enable_str,
      "Enable PIN",
      "[(current PIN)]"
    },
    { "disable-pin", 0, 0, G_OPTION_ARG_STRING, &set_pin_disable_str,
      "Disable PIN",
      "[(current PIN)]"
    },
    { "enter-puk", 0, 0, G_OPTION_ARG_STRING, &set_pin_enter_puk_str,
      "Enter PUK",
      "[(PUK),(new PIN)]"
    },
    { "query-home-provider", 0, 0, G_OPTION_ARG_NONE, &query_home_provider_flag,
      "Query home provider",
      NULL
    },
    { "query-preferred-providers", 0, 0, G_OPTION_ARG_NONE, &query_preferred_providers_flag,
      "Query preferred providers",
      NULL
    },
    { "query-visible-providers", 0, 0, G_OPTION_ARG_NONE, &query_visible_providers_flag,
      "Query visible providers",
      NULL
    },
    { "query-registration-state", 0, 0, G_OPTION_ARG_NONE, &query_register_state_flag,
      "Query registration state",
      NULL
    },
    { "register-automatic", 0, 0, G_OPTION_ARG_NONE, &set_register_state_automatic_flag,
      "Launch automatic registration",
      NULL
    },
    { "query-signal-state", 0, 0, G_OPTION_ARG_NONE, &query_signal_state_flag,
      "Query signal state",
      NULL
    },
    { "query-packet-service-state", 0, 0, G_OPTION_ARG_NONE, &query_packet_service_flag,
      "Query packet service state",
      NULL
    },
    { "attach-packet-service", 0, 0, G_OPTION_ARG_NONE, &set_packet_service_attach_flag,
      "Attach to the packet service",
      NULL
    },
    { "detach-packet-service", 0, 0, G_OPTION_ARG_NONE, &set_packet_service_detach_flag,
      "Detach from the packet service",
      NULL
    },
    { "query-connection-state", 0, 0, G_OPTION_ARG_NONE, &query_connect_flag,
      "Query connection state",
      NULL
    },
    { "connect", 0, 0, G_OPTION_ARG_STRING, &set_connect_activate_str,
      "Connect (Authentication, Username and Password are optional)",
      "[(APN),(PAP|CHAP|MSCHAPV2),(Username),(Password)]"
    },
    { "disconnect", 0, 0, G_OPTION_ARG_NONE, &set_connect_deactivate_flag,
      "Disconnect",
      NULL
    },
    { "query-packet-statistics", 0, 0, G_OPTION_ARG_NONE, &query_packet_statistics_flag,
      "Query packet statistics",
      NULL
    },
    { NULL }
};

GOptionGroup *
mbimcli_basic_connect_get_option_group (void)
{
    GOptionGroup *group;

    group = g_option_group_new ("basic-connect",
                                "Basic Connect options",
                                "Show Basic Connect Service options",
                                NULL,
                                NULL);
    g_option_group_add_entries (group, entries);

    return group;
}

gboolean
mbimcli_basic_connect_options_enabled (void)
{
    static guint n_actions = 0;
    static gboolean checked = FALSE;

    if (checked)
        return !!n_actions;

    n_actions = (query_device_caps_flag +
                 query_subscriber_ready_status_flag +
                 query_radio_state_flag +
                 !!set_radio_state_str +
                 query_device_services_flag +
                 query_pin_flag +
                 !!set_pin_enter_str +
                 !!set_pin_change_str +
                 !!set_pin_enable_str +
                 !!set_pin_disable_str +
                 !!set_pin_enter_puk_str +
                 query_register_state_flag +
                 query_home_provider_flag +
                 query_preferred_providers_flag +
                 query_visible_providers_flag +
                 set_register_state_automatic_flag +
                 query_signal_state_flag +
                 query_packet_service_flag +
                 set_packet_service_attach_flag +
                 set_packet_service_detach_flag +
                 query_connect_flag +
                 !!set_connect_activate_str +
                 set_connect_deactivate_flag +
                 query_packet_statistics_flag);

    if (n_actions > 1) {
        g_printerr ("error: too many Basic Connect actions requested\n");
        exit (EXIT_FAILURE);
    }

    checked = TRUE;
    return !!n_actions;
}

static void
context_free (Context *context)
{
    if (!context)
        return;

    if (context->cancellable)
        g_object_unref (context->cancellable);
    if (context->device)
        g_object_unref (context->device);
    g_slice_free (Context, context);
}

static void
shutdown (gboolean operation_status)
{
    /* Cleanup context and finish async operation */
    context_free (ctx);
    mbimcli_async_operation_done (operation_status);
}

static void
query_device_caps_ready (MbimDevice   *device,
                         GAsyncResult *res)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimDeviceType device_type;
    const gchar *device_type_str;
    MbimCellularClass cellular_class;
    gchar *cellular_class_str;
    MbimVoiceClass voice_class;
    const gchar *voice_class_str;
    MbimSimClass sim_class;
    gchar *sim_class_str;
    MbimDataClass data_class;
    gchar *data_class_str;
    MbimSmsCaps sms_caps;
    gchar *sms_caps_str;
    MbimCtrlCaps ctrl_caps;
    gchar *ctrl_caps_str;
    guint32 max_sessions;
    gchar *custom_data_class;
    gchar *device_id;
    gchar *firmware_info;
    gchar *hardware_info;

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        if (response)
            mbim_message_unref (response);
        shutdown (FALSE);
        return;
    }

    if (!mbim_message_device_caps_response_parse (
            response,
            &device_type,
            &cellular_class,
            &voice_class,
            &sim_class,
            &data_class,
            &sms_caps,
            &ctrl_caps,
            &max_sessions,
            &custom_data_class,
            &device_id,
            &firmware_info,
            &hardware_info,
            &error)) {
        g_printerr ("error: couldn't parse response message: %s\n", error->message);
        g_error_free (error);
        shutdown (FALSE);
        return;
    }

    device_type_str = mbim_device_type_get_string (device_type);
    cellular_class_str = mbim_cellular_class_build_string_from_mask (cellular_class);
    voice_class_str = mbim_voice_class_get_string (voice_class);
    sim_class_str = mbim_sim_class_build_string_from_mask (sim_class);
    data_class_str = mbim_data_class_build_string_from_mask (data_class);
    sms_caps_str = mbim_sms_caps_build_string_from_mask (sms_caps);
    ctrl_caps_str = mbim_ctrl_caps_build_string_from_mask (ctrl_caps);

    g_print ("[%s] Device capabilities retrieved:\n"
             "\t      Device type: '%s'\n"
             "\t   Cellular class: '%s'\n"
             "\t      Voice class: '%s'\n"
             "\t        Sim class: '%s'\n"
             "\t       Data class: '%s'\n"
             "\t         SMS caps: '%s'\n"
             "\t        Ctrl caps: '%s'\n"
             "\t     Max sessions: '%u'\n"
             "\tCustom data class: '%s'\n"
             "\t        Device ID: '%s'\n"
             "\t    Firmware info: '%s'\n"
             "\t    Hardware info: '%s'\n",
             mbim_device_get_path_display (device),
             VALIDATE_UNKNOWN (device_type_str),
             VALIDATE_UNKNOWN (cellular_class_str),
             VALIDATE_UNKNOWN (voice_class_str),
             VALIDATE_UNKNOWN (sim_class_str),
             VALIDATE_UNKNOWN (data_class_str),
             VALIDATE_UNKNOWN (sms_caps_str),
             VALIDATE_UNKNOWN (ctrl_caps_str),
             max_sessions,
             VALIDATE_UNKNOWN (custom_data_class),
             VALIDATE_UNKNOWN (device_id),
             VALIDATE_UNKNOWN (firmware_info),
             VALIDATE_UNKNOWN (hardware_info));

    g_free (cellular_class_str);
    g_free (sim_class_str);
    g_free (data_class_str);
    g_free (sms_caps_str);
    g_free (ctrl_caps_str);
    g_free (custom_data_class);
    g_free (device_id);
    g_free (firmware_info);
    g_free (hardware_info);

    mbim_message_unref (response);
    shutdown (TRUE);
}

static void
query_subscriber_ready_status_ready (MbimDevice   *device,
                                     GAsyncResult *res)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimSubscriberReadyState ready_state;
    const gchar *ready_state_str;
    gchar *subscriber_id;
    gchar *sim_iccid;
    MbimReadyInfoFlag ready_info;
    gchar *ready_info_str;
    guint32 telephone_numbers_count;
    gchar **telephone_numbers;
    gchar *telephone_numbers_str;

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        if (response)
            mbim_message_unref (response);
        shutdown (FALSE);
        return;
    }

    if (!mbim_message_subscriber_ready_status_response_parse (
            response,
            &ready_state,
            &subscriber_id,
            &sim_iccid,
            &ready_info,
            &telephone_numbers_count,
            &telephone_numbers,
            &error)) {
        g_printerr ("error: couldn't parse response message: %s\n", error->message);
        g_error_free (error);
        shutdown (FALSE);
        return;
    }

    telephone_numbers_str = (telephone_numbers ? g_strjoinv (", ", telephone_numbers) : NULL);
    ready_state_str = mbim_subscriber_ready_state_get_string (ready_state);
    ready_info_str = mbim_ready_info_flag_build_string_from_mask (ready_info);

    g_print ("[%s] Subscriber ready status retrieved:\n"
             "\t      Ready state: '%s'\n"
             "\t    Subscriber ID: '%s'\n"
             "\t        SIM ICCID: '%s'\n"
             "\t       Ready info: '%s'\n"
             "\tTelephone numbers: (%u) '%s'\n",
             mbim_device_get_path_display (device),
             VALIDATE_UNKNOWN (ready_state_str),
             VALIDATE_UNKNOWN (subscriber_id),
             VALIDATE_UNKNOWN (sim_iccid),
             VALIDATE_UNKNOWN (ready_info_str),
             telephone_numbers_count, VALIDATE_UNKNOWN (telephone_numbers_str));

    g_free (subscriber_id);
    g_free (sim_iccid);
    g_free (ready_info_str);
    g_strfreev (telephone_numbers);
    g_free (telephone_numbers_str);

    mbim_message_unref (response);
    shutdown (TRUE);
}

static void
query_radio_state_ready (MbimDevice   *device,
                         GAsyncResult *res)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimRadioSwitchState hardware_radio_state;
    const gchar *hardware_radio_state_str;
    MbimRadioSwitchState software_radio_state;
    const gchar *software_radio_state_str;

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        if (response)
            mbim_message_unref (response);
        shutdown (FALSE);
        return;
    }

    if (!mbim_message_radio_state_response_parse (
            response,
            &hardware_radio_state,
            &software_radio_state,
            &error)) {
        g_printerr ("error: couldn't parse response message: %s\n", error->message);
        g_error_free (error);
        shutdown (FALSE);
        return;
    }

    hardware_radio_state_str = mbim_radio_switch_state_get_string (hardware_radio_state);
    software_radio_state_str = mbim_radio_switch_state_get_string (software_radio_state);

    g_print ("[%s] Radio state retrieved:\n"
             "\t     Hardware Radio State: '%s'\n"
             "\t     Software Radio State: '%s'\n",
             mbim_device_get_path_display (device),
             VALIDATE_UNKNOWN (hardware_radio_state_str),
             VALIDATE_UNKNOWN (software_radio_state_str));

    mbim_message_unref (response);
    shutdown (TRUE);
}

static void
query_device_services_ready (MbimDevice   *device,
                             GAsyncResult *res)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimDeviceServiceElement **device_services;
    guint32 device_services_count;
    guint32 max_dss_sessions;

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        if (response)
            mbim_message_unref (response);
        shutdown (FALSE);
        return;
    }

    if (!mbim_message_device_services_response_parse (
            response,
            &device_services_count,
            &max_dss_sessions,
            &device_services,
            &error)) {
        g_printerr ("error: couldn't parse response message: %s\n", error->message);
        g_error_free (error);
        shutdown (FALSE);
        return;
    }

    g_print ("[%s] Device services retrieved:\n"
             "\tMax DSS sessions: '%u'\n",
             mbim_device_get_path_display (device),
             max_dss_sessions);
    if (device_services_count == 0)
        g_print ("\t        Services: None\n");
    else {
        guint32 i;

        g_print ("\t        Services: (%u)\n", device_services_count);
        for (i = 0; i < device_services_count; i++) {
            MbimService service;
            gchar *uuid_str;
            GString *cids;
            guint32 j;

            service = mbim_uuid_to_service (&device_services[i]->device_service_id);
            uuid_str = mbim_uuid_get_printable (&device_services[i]->device_service_id);

            cids = g_string_new ("");
            for (j = 0; j < device_services[i]->cids_count; j++) {
                if (service == MBIM_SERVICE_INVALID) {
                    g_string_append_printf (cids, "%u", device_services[i]->cids[j]);
                    if (j < device_services[i]->cids_count - 1)
                        g_string_append (cids, ", ");
                } else {
                    g_string_append_printf (cids, "%s%s (%u)",
                                            j == 0 ? "" : "\t\t                   ",
                                            mbim_cid_get_printable (service, device_services[i]->cids[j]),
                                            device_services[i]->cids[j]);
                    if (j < device_services[i]->cids_count - 1)
                        g_string_append (cids, ",\n");
                }
            }

            g_print ("\n"
                     "\t\t          Service: '%s'\n"
                     "\t\t             UUID: [%s]:\n"
                     "\t\t      DSS payload: %u\n"
                     "\t\tMax DSS instances: %u\n"
                     "\t\t             CIDs: %s\n",
                     service == MBIM_SERVICE_INVALID ? "unknown" : mbim_service_get_string (service),
                     uuid_str,
                     device_services[i]->dss_payload,
                     device_services[i]->max_dss_instances,
                     cids->str);

            g_string_free (cids, TRUE);
            g_free (uuid_str);
        }
    }

    mbim_device_service_element_array_free (device_services);

    mbim_message_unref (response);
    shutdown (TRUE);
}

static void
pin_ready (MbimDevice   *device,
           GAsyncResult *res,
           gpointer user_data)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimPinType pin_type;
    MbimPinState pin_state;
    const gchar *pin_state_str;
    guint32 remaining_attempts;

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        if (response)
            mbim_message_unref (response);
        shutdown (FALSE);
        return;
    }

    if (!mbim_message_pin_response_parse (
            response,
            &pin_type,
            &pin_state,
            &remaining_attempts,
            &error)) {
        g_printerr ("error: couldn't parse response message: %s\n", error->message);
        g_error_free (error);
        shutdown (FALSE);
        return;
    }

    if (GPOINTER_TO_UINT (user_data))
        g_print ("[%s] PIN operation successful\n\n",
                 mbim_device_get_path_display (device));

    pin_state_str = mbim_pin_state_get_string (pin_state);

    g_print ("[%s] Pin Info:\n"
             "\t         Pin State: '%s'\n",
             mbim_device_get_path_display (device),
             VALIDATE_UNKNOWN (pin_state_str));
    if (pin_type != MBIM_PIN_TYPE_UNKNOWN) {
        const gchar *pin_type_str;

        pin_type_str = mbim_pin_type_get_string (pin_type);
        g_print ("\t           PinType: '%s'\n"
                 "\tRemaining attempts: '%u'\n",
                 VALIDATE_UNKNOWN (pin_type_str),
                 remaining_attempts);
    }

    mbim_message_unref (response);
    shutdown (TRUE);
}

static gboolean
set_pin_input_parse (guint         n_expected,
                     const gchar  *str,
                     gchar       **pin,
                     gchar       **new_pin)
{
    gchar **split;

    g_assert (n_expected == 1 || n_expected == 2);
    g_assert (pin != NULL);
    g_assert (new_pin != NULL);

    /* Format of the string is:
     *    "[(current PIN)]"
     * or:
     *    "[(current PIN),(new PIN)]"
     */
    split = g_strsplit (str, ",", -1);

    if (g_strv_length (split) > n_expected) {
        g_printerr ("error: couldn't parse input string, too many arguments\n");
        g_strfreev (split);
        return FALSE;
    }

    if (g_strv_length (split) < n_expected) {
        g_printerr ("error: couldn't parse input string, missing arguments\n");
        g_strfreev (split);
        return FALSE;
    }

    *pin = g_strdup (split[0]);
    *new_pin = g_strdup (split[1]);

    g_strfreev (split);
    return TRUE;
}

enum {
    CONNECTION_STATUS,
    CONNECT,
    DISCONNECT
};

static void
connect_ready (MbimDevice   *device,
               GAsyncResult *res,
               gpointer user_data)
{
    MbimMessage *response;
    GError *error = NULL;
    guint32 session_id;
    MbimActivationState activation_state;
    MbimVoiceCallState voice_call_state;
    MbimContextIpType ip_type;
    const MbimUuid *context_type;
    guint32 nw_error;

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        if (response)
            mbim_message_unref (response);
        shutdown (FALSE);
        return;
    }

    if (!mbim_message_connect_response_parse (
            response,
            &session_id,
            &activation_state,
            &voice_call_state,
            &ip_type,
            &context_type,
            &nw_error,
            &error)) {
        g_printerr ("error: couldn't parse response message: %s\n", error->message);
        g_error_free (error);
        shutdown (FALSE);
        return;
    }

    switch (GPOINTER_TO_UINT (user_data)) {
    case CONNECT:
        g_print ("[%s] Successfully connected\n\n",
                 mbim_device_get_path_display (device));
        break;
    case DISCONNECT:
        g_print ("[%s] Successfully disconnected\n\n",
                 mbim_device_get_path_display (device));
        break;
    default:
        break;
    }

    g_print ("[%s] Connection status:\n"
             "\t      Session ID: '%u'\n"
             "\tActivation state: '%s'\n"
             "\tVoice call state: '%s'\n"
             "\t         IP type: '%s'\n"
             "\t    Context type: '%s'\n"
             "\t   Network error: '%s'\n",
             mbim_device_get_path_display (device),
             session_id,
             VALIDATE_UNKNOWN (mbim_activation_state_get_string (activation_state)),
             VALIDATE_UNKNOWN (mbim_voice_call_state_get_string (voice_call_state)),
             VALIDATE_UNKNOWN (mbim_context_ip_type_get_string (ip_type)),
             VALIDATE_UNKNOWN (mbim_context_type_get_string (mbim_uuid_to_context_type (context_type))),
             VALIDATE_UNKNOWN (mbim_nw_error_get_string (nw_error)));

    mbim_message_unref (response);
    shutdown (TRUE);
}

static gboolean
set_connect_activate_parse (const gchar       *str,
                            gchar            **apn,
                            MbimAuthProtocol  *auth_protocol,
                            gchar            **username,
                            gchar            **password)
{
    gchar **split;

    g_assert (apn != NULL);
    g_assert (auth_protocol != NULL);
    g_assert (username != NULL);
    g_assert (password != NULL);

    /* Format of the string is:
     *    "[(APN),(PAP|CHAP|MSCHAPV2),(Username),(Password)]"
     */
    split = g_strsplit (str, ",", -1);

    if (g_strv_length (split) > 4) {
        g_printerr ("error: couldn't parse input string, too many arguments\n");
        g_strfreev (split);
        return FALSE;
    }

    if (g_strv_length (split) < 1) {
        g_printerr ("error: couldn't parse input string, missing arguments\n");
        g_strfreev (split);
        return FALSE;
    }

    /* APN */
    *apn = g_strdup (split[0]);

    /* Some defaults */
    *auth_protocol = MBIM_AUTH_PROTOCOL_NONE;
    *username = NULL;
    *password = NULL;

    /* Use authentication method */
    if (split[1]) {
        if (g_ascii_strcasecmp (split[1], "PAP") == 0)
            *auth_protocol = MBIM_AUTH_PROTOCOL_PAP;
        else if (g_ascii_strcasecmp (split[1], "CHAP") == 0)
            *auth_protocol = MBIM_AUTH_PROTOCOL_CHAP;
        else if (g_ascii_strcasecmp (split[1], "MSCHAPV2") == 0)
            *auth_protocol = MBIM_AUTH_PROTOCOL_MSCHAPV2;
        else
            *auth_protocol = MBIM_AUTH_PROTOCOL_NONE;

        /* Username */
        if (split[2]) {
            *username = g_strdup (split[2]);

            /* Password */
            *password = g_strdup (split[3]);
        }
    }

    g_strfreev (split);
    return TRUE;
}

static void
home_provider_ready (MbimDevice   *device,
                     GAsyncResult *res,
                     gpointer user_data)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimProvider *provider;
    gchar *provider_state_str;
    gchar *cellular_class_str;

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        if (response)
            mbim_message_unref (response);
        shutdown (FALSE);
        return;
    }

    if (!mbim_message_home_provider_response_parse (response,
                                                    &provider,
                                                    &error)) {
        g_printerr ("error: couldn't parse response message: %s\n", error->message);
        g_error_free (error);
        shutdown (FALSE);
        return;
    }

    provider_state_str = mbim_provider_state_build_string_from_mask (provider->provider_state);
    cellular_class_str = mbim_cellular_class_build_string_from_mask (provider->cellular_class);

    g_print ("[%s] Home provider:\n"
             "\t   Provider ID: '%s'\n"
             "\t Provider Name: '%s'\n"
             "\t         State: '%s'\n"
             "\tCellular class: '%s'\n"
             "\t          RSSI: '%u'\n"
             "\t    Error rate: '%u'\n",
             mbim_device_get_path_display (device),
             VALIDATE_UNKNOWN (provider->provider_id),
             VALIDATE_UNKNOWN (provider->provider_name),
             VALIDATE_UNKNOWN (provider_state_str),
             VALIDATE_UNKNOWN (cellular_class_str),
             provider->rssi,
             provider->error_rate);

    g_free (cellular_class_str);
    g_free (provider_state_str);

    mbim_provider_free (provider);
    mbim_message_unref (response);
    shutdown (TRUE);
}

static void
preferred_providers_ready (MbimDevice   *device,
                           GAsyncResult *res,
                           gpointer user_data)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimProvider **providers;
    guint n_providers;
    guint i;

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        if (response)
            mbim_message_unref (response);
        shutdown (FALSE);
        return;
    }

    if (!mbim_message_preferred_providers_response_parse (response,
                                                          &n_providers,
                                                          &providers,
                                                          &error)) {
        g_printerr ("error: couldn't parse response message: %s\n", error->message);
        g_error_free (error);
        shutdown (FALSE);
        return;
    }

    if (!n_providers)
        g_print ("[%s] No preferred providers given\n",
                 mbim_device_get_path_display (device));
    else
        g_print ("[%s] Preferred providers (%u):\n",
                 mbim_device_get_path_display (device),
                 n_providers);

    for (i = 0; i < n_providers; i++) {
        gchar *provider_state_str;
        gchar *cellular_class_str;

        provider_state_str = mbim_provider_state_build_string_from_mask (providers[i]->provider_state);
        cellular_class_str = mbim_cellular_class_build_string_from_mask (providers[i]->cellular_class);

        g_print ("\tProvider [%u]:\n"
                 "\t\t    Provider ID: '%s'\n"
                 "\t\t  Provider Name: '%s'\n"
                 "\t\t          State: '%s'\n"
                 "\t\t Cellular class: '%s'\n"
                 "\t\t           RSSI: '%u'\n"
                 "\t\t     Error rate: '%u'\n",
                 i,
                 VALIDATE_UNKNOWN (providers[i]->provider_id),
                 VALIDATE_UNKNOWN (providers[i]->provider_name),
                 VALIDATE_UNKNOWN (provider_state_str),
                 VALIDATE_UNKNOWN (cellular_class_str),
                 providers[i]->rssi,
                 providers[i]->error_rate);

        g_free (cellular_class_str);
        g_free (provider_state_str);
    }

    mbim_provider_array_free (providers);
    mbim_message_unref (response);
    shutdown (TRUE);
}

static void
visible_providers_ready (MbimDevice   *device,
                           GAsyncResult *res,
                           gpointer user_data)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimProvider **providers;
    guint n_providers;
    guint i;

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        if (response)
            mbim_message_unref (response);
        shutdown (FALSE);
        return;
    }

    if (!mbim_message_visible_providers_response_parse (response,
                                                        &n_providers,
                                                        &providers,
                                                        &error)) {
        g_printerr ("error: couldn't parse response message: %s\n", error->message);
        g_error_free (error);
        shutdown (FALSE);
        return;
    }

    if (!n_providers)
        g_print ("[%s] No visible providers given\n",
                 mbim_device_get_path_display (device));
    else
        g_print ("[%s] Visible providers (%u):\n",
                 mbim_device_get_path_display (device),
                 n_providers);

    for (i = 0; i < n_providers; i++) {
        gchar *provider_state_str;
        gchar *cellular_class_str;

        provider_state_str = mbim_provider_state_build_string_from_mask (providers[i]->provider_state);
        cellular_class_str = mbim_cellular_class_build_string_from_mask (providers[i]->cellular_class);

        g_print ("\tProvider [%u]:\n"
                 "\t\t    Provider ID: '%s'\n"
                 "\t\t  Provider Name: '%s'\n"
                 "\t\t          State: '%s'\n"
                 "\t\t Cellular class: '%s'\n"
                 "\t\t           RSSI: '%u'\n"
                 "\t\t     Error rate: '%u'\n",
                 i,
                 VALIDATE_UNKNOWN (providers[i]->provider_id),
                 VALIDATE_UNKNOWN (providers[i]->provider_name),
                 VALIDATE_UNKNOWN (provider_state_str),
                 VALIDATE_UNKNOWN (cellular_class_str),
                 providers[i]->rssi,
                 providers[i]->error_rate);

        g_free (cellular_class_str);
        g_free (provider_state_str);
    }

    mbim_provider_array_free (providers);
    mbim_message_unref (response);
    shutdown (TRUE);
}

static void
register_state_ready (MbimDevice   *device,
                      GAsyncResult *res,
                      gpointer user_data)
{
    MbimMessage *response;
    GError *error = NULL;
    MbimNwError nw_error;
    MbimRegisterState register_state;
    MbimRegisterMode register_mode;
    MbimDataClass available_data_classes;
    gchar *available_data_classes_str;
    MbimCellularClass cellular_class;
    gchar *cellular_class_str;
    gchar *provider_id;
    gchar *provider_name;
    gchar *roaming_text;
    MbimRegistrationFlag registration_flag;
    gchar *registration_flag_str;

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        if (response)
            mbim_message_unref (response);
        shutdown (FALSE);
        return;
    }

    if (!mbim_message_register_state_response_parse (response,
                                                     &nw_error,
                                                     &register_state,
                                                     &register_mode,
                                                     &available_data_classes,
                                                     &cellular_class,
                                                     &provider_id,
                                                     &provider_name,
                                                     &roaming_text,
                                                     &registration_flag,
                                                     &error)) {
        g_printerr ("error: couldn't parse response message: %s\n", error->message);
        g_error_free (error);
        shutdown (FALSE);
        return;
    }

    if (GPOINTER_TO_UINT (user_data))
        g_print ("[%s] Successfully launched automatic registration\n\n",
                 mbim_device_get_path_display (device));

    available_data_classes_str = mbim_data_class_build_string_from_mask (available_data_classes);
    cellular_class_str = mbim_cellular_class_build_string_from_mask (cellular_class);
    registration_flag_str = mbim_registration_flag_build_string_from_mask (registration_flag);

    g_print ("[%s] Registration status:\n"
             "\t         Network error: '%s'\n"
             "\t        Register state: '%s'\n"
             "\t         Register mode: '%s'\n"
             "\tAvailable data classes: '%s'\n"
             "\tCurrent cellular class: '%s'\n"
             "\t           Provider ID: '%s'\n"
             "\t         Provider name: '%s'\n"
             "\t          Roaming text: '%s'\n"
             "\t    Registration flags: '%s'\n",
             mbim_device_get_path_display (device),
             VALIDATE_UNKNOWN (mbim_nw_error_get_string (nw_error)),
             VALIDATE_UNKNOWN (mbim_register_state_get_string (register_state)),
             VALIDATE_UNKNOWN (mbim_register_mode_get_string (register_mode)),
             VALIDATE_UNKNOWN (available_data_classes_str),
             VALIDATE_UNKNOWN (cellular_class_str),
             VALIDATE_UNKNOWN (provider_id),
             VALIDATE_UNKNOWN (provider_name),
             VALIDATE_UNKNOWN (roaming_text),
             VALIDATE_UNKNOWN (registration_flag_str));

    g_free (available_data_classes_str);
    g_free (cellular_class_str);
    g_free (registration_flag_str);
    g_free (provider_name);
    g_free (provider_id);
    g_free (roaming_text);

    mbim_message_unref (response);
    shutdown (TRUE);
}

static void
signal_state_ready (MbimDevice   *device,
                    GAsyncResult *res,
                    gpointer user_data)
{
    MbimMessage *response;
    GError *error = NULL;
    guint32 rssi;
    guint32 error_rate;
    guint32 signal_strength_interval;
    guint32 rssi_threshold;
    guint32 error_rate_threshold;

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        if (response)
            mbim_message_unref (response);
        shutdown (FALSE);
        return;
    }

    if (!mbim_message_signal_state_response_parse (response,
                                                   &rssi,
                                                   &error_rate,
                                                   &signal_strength_interval,
                                                   &rssi_threshold,
                                                   &error_rate_threshold,
                                                   &error)) {
        g_printerr ("error: couldn't parse response message: %s\n", error->message);
        g_error_free (error);
        shutdown (FALSE);
        return;
    }

    g_print ("[%s] Signal state:\n"
             "\t          RSSI [0-31,99]: '%u'\n"
             "\t     Error rate [0-7,99]: '%u'\n"
             "\tSignal strength interval: '%u'\n"
             "\t          RSSI threshold: '%u'\n",
             mbim_device_get_path_display (device),
             rssi,
             error_rate,
             signal_strength_interval,
             rssi_threshold);

    if (error_rate_threshold == 0xFFFFFFFF)
        g_print ("\t    Error rate threshold: 'unspecified'\n");
    else
        g_print ("\t    Error rate threshold: '%u'\n", error_rate_threshold);

    mbim_message_unref (response);
    shutdown (TRUE);
}

enum {
    PACKET_SERVICE_STATUS,
    PACKET_SERVICE_ATTACH,
    PACKET_SERVICE_DETACH
};

static void
packet_service_ready (MbimDevice   *device,
                      GAsyncResult *res,
                      gpointer user_data)
{
    MbimMessage *response;
    GError *error = NULL;
    guint32 nw_error;
    MbimPacketServiceState packet_service_state;
    MbimDataClass highest_available_data_class;
    gchar *highest_available_data_class_str;
    guint64 uplink_speed;
    guint64 downlink_speed;

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        if (response)
            mbim_message_unref (response);
        shutdown (FALSE);
        return;
    }

    if (!mbim_message_packet_service_response_parse (response,
                                                     &nw_error,
                                                     &packet_service_state,
                                                     &highest_available_data_class,
                                                     &uplink_speed,
                                                     &downlink_speed,
                                                     &error)) {
        g_printerr ("error: couldn't parse response message: %s\n", error->message);
        g_error_free (error);
        shutdown (FALSE);
        return;
    }

    switch (GPOINTER_TO_UINT (user_data)) {
    case PACKET_SERVICE_ATTACH:
        g_print ("[%s] Successfully attached to packet service\n\n",
                 mbim_device_get_path_display (device));
        break;
    case PACKET_SERVICE_DETACH:
        g_print ("[%s] Successfully detached from packet service\n\n",
                 mbim_device_get_path_display (device));
        break;
    default:
        break;
    }

    highest_available_data_class_str = mbim_data_class_build_string_from_mask (highest_available_data_class);

    g_print ("[%s] Packet service status:\n"
             "\t         Network error: '%s'\n"
             "\t  Packet service state: '%s'\n"
             "\tAvailable data classes: '%s'\n"
             "\t          Uplink speed: '%" G_GUINT64_FORMAT " bps'\n"
             "\t        Downlink speed: '%" G_GUINT64_FORMAT " bps'\n",
             mbim_device_get_path_display (device),
             VALIDATE_UNKNOWN (mbim_nw_error_get_string (nw_error)),
             VALIDATE_UNKNOWN (mbim_packet_service_state_get_string (packet_service_state)),
             VALIDATE_UNKNOWN (highest_available_data_class_str),
             uplink_speed,
             downlink_speed);

    g_free (highest_available_data_class_str);

    mbim_message_unref (response);
    shutdown (TRUE);
}

static void
packet_statistics_ready (MbimDevice   *device,
                         GAsyncResult *res)
{
    MbimMessage *response;
    GError *error = NULL;
    guint32 in_discards;
    guint32 in_errors;
    guint64 in_octets;
    guint64 in_packets;
    guint64 out_octets;
    guint64 out_packets;
    guint32 out_errors;
    guint32 out_discards;

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        g_printerr ("error: operation failed: %s\n", error->message);
        g_error_free (error);
        if (response)
            mbim_message_unref (response);
        shutdown (FALSE);
        return;
    }

    if (!mbim_message_packet_statistics_response_parse (response,
                                                        &in_discards,
                                                        &in_errors,
                                                        &in_octets,
                                                        &in_packets,
                                                        &out_octets,
                                                        &out_packets,
                                                        &out_errors,
                                                        &out_discards,
                                                        &error)) {
        g_printerr ("error: couldn't parse response message: %s\n", error->message);
        g_error_free (error);
        shutdown (FALSE);
        return;
    }

    g_print ("[%s] Packet statistics:\n"
             "\t   Octets (in): '%" G_GUINT64_FORMAT "'\n"
             "\t  Packets (in): '%" G_GUINT64_FORMAT "'\n"
             "\t Discards (in): '%u'\n"
             "\t   Errors (in): '%u'\n"
             "\t  Octets (out): '%" G_GUINT64_FORMAT "'\n"
             "\t Packets (out): '%" G_GUINT64_FORMAT "'\n"
             "\tDiscards (out): '%u'\n"
             "\t  Errors (out): '%u'\n",
             mbim_device_get_path_display (device),
             in_octets,
             in_packets,
             in_discards,
             in_errors,
             out_octets,
             out_packets,
             out_discards,
             out_errors);

    mbim_message_unref (response);
    shutdown (TRUE);
}

void
mbimcli_basic_connect_run (MbimDevice   *device,
                           GCancellable *cancellable)
{
    /* Initialize context */
    ctx = g_slice_new (Context);
    ctx->device = g_object_ref (device);
    if (cancellable)
        ctx->cancellable = g_object_ref (cancellable);

    /* Request to get capabilities? */
    if (query_device_caps_flag) {
        MbimMessage *request;

        g_debug ("Asynchronously querying device capabilities...");
        request = (mbim_message_device_caps_query_new (NULL));
        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)query_device_caps_ready,
                             NULL);
        mbim_message_unref (request);
        return;
    }

    /* Request to get subscriber ready status? */
    if (query_subscriber_ready_status_flag) {
        MbimMessage *request;

        g_debug ("Asynchronously querying subscriber ready status...");
        request = (mbim_message_subscriber_ready_status_query_new (NULL));
        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)query_subscriber_ready_status_ready,
                             NULL);
        mbim_message_unref (request);
        return;
    }

    /* Request to get radio state? */
    if (query_radio_state_flag) {
        MbimMessage *request;

        g_debug ("Asynchronously querying radio state...");
        request = (mbim_message_radio_state_query_new (NULL));
        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)query_radio_state_ready,
                             NULL);
        mbim_message_unref (request);
        return;
    }

    /* Request to set radio state? */
    if (set_radio_state_str) {
        MbimMessage *request;
        MbimRadioSwitchState radio_state;

        if (g_ascii_strcasecmp (set_radio_state_str, "on") == 0) {
            radio_state = MBIM_RADIO_SWITCH_STATE_ON;
        } else if (g_ascii_strcasecmp (set_radio_state_str, "off") == 0) {
            radio_state = MBIM_RADIO_SWITCH_STATE_OFF;
        } else {
            g_printerr ("error: invalid radio state: '%s'\n", set_radio_state_str);
            shutdown (FALSE);
            return;
        }

        g_debug ("Asynchronously setting radio state to %s...",
                 radio_state == MBIM_RADIO_SWITCH_STATE_ON ? "on" : "off");
        request = mbim_message_radio_state_set_new (radio_state, NULL);
        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)query_radio_state_ready,
                             NULL);
        mbim_message_unref (request);
        return;
    }

    /* Request to query device services? */
    if (query_device_services_flag) {
        MbimMessage *request;

        g_debug ("Asynchronously querying device services...");
        request = (mbim_message_device_services_query_new (NULL));
        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)query_device_services_ready,
                             NULL);
        mbim_message_unref (request);
        return;
    }

    /* Query PIN state? */
    if (query_pin_flag) {
        MbimMessage *request;

        g_debug ("Asynchronously querying PIN state...");
        request = (mbim_message_pin_query_new (NULL));
        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)pin_ready,
                             GUINT_TO_POINTER (FALSE));
        mbim_message_unref (request);
        return;
    }

    /* Set PIN? */
    if (set_pin_enter_str ||
        set_pin_change_str ||
        set_pin_enable_str ||
        set_pin_disable_str ||
        set_pin_enter_puk_str) {
        MbimMessage *request;
        guint n_expected;
        MbimPinType pin_type;
        MbimPinOperation pin_operation;
        gchar *pin;
        gchar *new_pin;
        const gchar *input = NULL;
        GError *error = NULL;

        if (set_pin_enter_puk_str) {
            g_debug ("Asynchronously entering PUK...");
            pin_type = MBIM_PIN_TYPE_PUK1;
            input = set_pin_enter_puk_str;
            n_expected = 2;
            pin_operation = MBIM_PIN_OPERATION_ENTER;
        } else {
            pin_type = MBIM_PIN_TYPE_PIN1;
            if (set_pin_change_str) {
                g_debug ("Asynchronously changing PIN...");
                input = set_pin_change_str;
                n_expected = 2;
                pin_operation = MBIM_PIN_OPERATION_CHANGE;
            } else if (set_pin_enable_str) {
                g_debug ("Asynchronously enabling PIN...");
                input = set_pin_enable_str;
                n_expected = 1;
                pin_operation = MBIM_PIN_OPERATION_ENABLE;
            } else if (set_pin_disable_str) {
                g_debug ("Asynchronously disabling PIN...");
                input = set_pin_disable_str;
                n_expected = 1;
                pin_operation = MBIM_PIN_OPERATION_DISABLE;
            } else if (set_pin_enter_str) {
                g_debug ("Asynchronously entering PIN...");
                input = set_pin_enter_str;
                n_expected = 1;
                pin_operation = MBIM_PIN_OPERATION_ENTER;
            } else
                g_assert_not_reached ();
        }

        if (!set_pin_input_parse (n_expected, input, &pin, &new_pin)) {
            shutdown (FALSE);
            return;
        }

        request = (mbim_message_pin_set_new (pin_type,
                                             pin_operation,
                                             pin,
                                             new_pin,
                                             &error));
        g_free (pin);
        g_free (new_pin);

        if (!request) {
            g_printerr ("error: couldn't create request: %s\n", error->message);
            g_error_free (error);
            shutdown (FALSE);
            return;
        }

        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)pin_ready,
                             GUINT_TO_POINTER (TRUE));
        mbim_message_unref (request);
        return;
    }

    /* Query home provider? */
    if (query_home_provider_flag) {
        MbimMessage *request;

        request = mbim_message_home_provider_query_new (NULL);
        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)home_provider_ready,
                             NULL);
        mbim_message_unref (request);
        return;
    }

    /* Query preferred providers? */
    if (query_preferred_providers_flag) {
        MbimMessage *request;

        request = mbim_message_preferred_providers_query_new (NULL);
        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)preferred_providers_ready,
                             NULL);
        mbim_message_unref (request);
        return;
    }

    /* Query visible providers? */
    if (query_visible_providers_flag) {
        MbimMessage *request;

        request = mbim_message_visible_providers_query_new (MBIM_VISIBLE_PROVIDERS_ACTION_FULL_SCAN, NULL);
        mbim_device_command (ctx->device,
                             request,
                             120, /* longer timeout, needs to scan */
                             ctx->cancellable,
                             (GAsyncReadyCallback)visible_providers_ready,
                             NULL);
        mbim_message_unref (request);
        return;
    }

    /* Query registration status? */
    if (query_register_state_flag) {
        MbimMessage *request;

        request = mbim_message_register_state_query_new (NULL);
        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)register_state_ready,
                             GUINT_TO_POINTER (FALSE));
        mbim_message_unref (request);
        return;
    }

    /* Launch automatic registration? */
    if (set_register_state_automatic_flag) {
        MbimMessage *request;
        GError *error = NULL;

        request = mbim_message_register_state_set_new (NULL,
                                                       MBIM_REGISTER_ACTION_AUTOMATIC,
                                                       0,
                                                       &error);
        if (!request) {
            g_printerr ("error: couldn't create request: %s\n", error->message);
            g_error_free (error);
            shutdown (FALSE);
            return;
        }

        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)register_state_ready,
                             GUINT_TO_POINTER (TRUE));
        mbim_message_unref (request);
        return;
    }

    /* Query signal status? */
    if (query_signal_state_flag) {
        MbimMessage *request;

        request = mbim_message_signal_state_query_new (NULL);
        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)signal_state_ready,
                             NULL);
        mbim_message_unref (request);
        return;
    }

    /* Query packet service status? */
    if (query_packet_service_flag) {
        MbimMessage *request;

        request = mbim_message_packet_service_query_new (NULL);
        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)packet_service_ready,
                             GUINT_TO_POINTER (PACKET_SERVICE_STATUS));
        mbim_message_unref (request);
        return;
    }

    /* Launch packet attach or detach? */
    if (set_packet_service_attach_flag ||
        set_packet_service_detach_flag) {
        MbimMessage *request;
        MbimPacketServiceAction action;
        GError *error = NULL;

        if (set_packet_service_attach_flag)
            action = MBIM_PACKET_SERVICE_ACTION_ATTACH;
        else if (set_packet_service_detach_flag)
            action = MBIM_PACKET_SERVICE_ACTION_DETACH;
        else
            g_assert_not_reached ();

        request = mbim_message_packet_service_set_new (action, &error);
        if (!request) {
            g_printerr ("error: couldn't create request: %s\n", error->message);
            g_error_free (error);
            shutdown (FALSE);
            return;
        }

        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)packet_service_ready,
                             GUINT_TO_POINTER (set_packet_service_attach_flag ?
                                               PACKET_SERVICE_ATTACH :
                                               PACKET_SERVICE_DETACH));
        mbim_message_unref (request);
        return;
    }

    /* Query connection status? */
    if (query_connect_flag) {
        MbimMessage *request;
        GError *error = NULL;

        request = mbim_message_connect_query_new (0,
                                                  MBIM_ACTIVATION_STATE_UNKNOWN,
                                                  MBIM_VOICE_CALL_STATE_NONE,
                                                  MBIM_CONTEXT_IP_TYPE_DEFAULT,
                                                  mbim_uuid_from_context_type (MBIM_CONTEXT_TYPE_INTERNET),
                                                  0,
                                                  &error);
        if (!request) {
            g_printerr ("error: couldn't create request: %s\n", error->message);
            g_error_free (error);
            shutdown (FALSE);
            return;
        }

        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)connect_ready,
                             GUINT_TO_POINTER (CONNECTION_STATUS));
        mbim_message_unref (request);
        return;
    }

    /* Connect? */
    if (set_connect_activate_str) {
        MbimMessage *request;
        GError *error = NULL;
        gchar *apn;
        MbimAuthProtocol auth_protocol;
        gchar *username = NULL;
        gchar *password = NULL;

        if (!set_connect_activate_parse (set_connect_activate_str,
                                         &apn,
                                         &auth_protocol,
                                         &username,
                                         &password)) {
            shutdown (FALSE);
            return;
        }

        request = mbim_message_connect_set_new (0,
                                                MBIM_ACTIVATION_COMMAND_ACTIVATE,
                                                apn,
                                                username,
                                                password,
                                                MBIM_COMPRESSION_NONE,
                                                auth_protocol,
                                                MBIM_CONTEXT_IP_TYPE_DEFAULT,
                                                mbim_uuid_from_context_type (MBIM_CONTEXT_TYPE_INTERNET),
                                                &error);
        g_free (apn);
        g_free (username);
        g_free (password);

        if (!request) {
            g_printerr ("error: couldn't create request: %s\n", error->message);
            g_error_free (error);
            shutdown (FALSE);
            return;
        }

        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)connect_ready,
                             GUINT_TO_POINTER (CONNECT));
        mbim_message_unref (request);
        return;
    }

    /* Disconnect? */
    if (set_connect_deactivate_flag) {
        MbimMessage *request;
        GError *error = NULL;

        request = mbim_message_connect_set_new (0,
                                                MBIM_ACTIVATION_COMMAND_DEACTIVATE,
                                                NULL,
                                                NULL,
                                                NULL,
                                                MBIM_COMPRESSION_NONE,
                                                MBIM_AUTH_PROTOCOL_NONE,
                                                MBIM_CONTEXT_IP_TYPE_DEFAULT,
                                                mbim_uuid_from_context_type (MBIM_CONTEXT_TYPE_INTERNET),
                                                &error);
        if (!request) {
            g_printerr ("error: couldn't create request: %s\n", error->message);
            g_error_free (error);
            shutdown (FALSE);
            return;
        }

        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)connect_ready,
                             GUINT_TO_POINTER (DISCONNECT));
        mbim_message_unref (request);
        return;
    }

    /* Packet statistics? */
    if (query_packet_statistics_flag) {
        MbimMessage *request;

        request = mbim_message_packet_statistics_query_new (NULL);
        mbim_device_command (ctx->device,
                             request,
                             10,
                             ctx->cancellable,
                             (GAsyncReadyCallback)packet_statistics_ready,
                             NULL);
        mbim_message_unref (request);
        return;
    }

    g_warn_if_reached ();
}
