/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 * libmbim-glib -- GLib/GIO based library to control MBIM devices
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2014 Aleksander Morgado <aleksander@aleksander.es>
 * Copyright (C) 2014 Smith Micro Software, Inc.
 */

#include <string.h>
#include "mbim-proxy-helpers.h"
#include "mbim-message-private.h"
#include "mbim-message.h"

MbimEventEntry **
mbim_proxy_helper_service_subscribe_request_parse (MbimMessage *message)
{
    MbimEventEntry **array = NULL;
    guint32 i;
    guint32 element_count;
    guint32 offset = 0;
    guint32 array_offset;
    MbimEventEntry *event;

    element_count = _mbim_message_read_guint32 (message, offset);
    if (element_count) {
        array = g_new (MbimEventEntry *, element_count + 1);

        offset += 4;
        for (i = 0; i < element_count; i++) {
            array_offset = _mbim_message_read_guint32 (message, offset);

            event = g_new (MbimEventEntry, 1);

            memcpy (&(event->device_service_id), _mbim_message_read_uuid (message, array_offset), 16);
            array_offset += 16;

            event->cids_count = _mbim_message_read_guint32 (message, array_offset);
            array_offset += 4;

#if 1
            if (event->cids_count)
                event->cids = _mbim_message_read_guint32_array (message, event->cids_count, array_offset);
            else
                event->cids = NULL;

            array[i] = event;
            offset += 8;
#else
            /* NOTE to Greg: shouldn't this be like this? */
            if (event->cids_count) {
                event->cids = _mbim_message_read_guint32_array (message, event->cids_count, array_offset);
                offset += (4 * event->cids_count);
            } else
                event->cids = NULL;

            array[i] = event;
#endif
        }

        array[element_count] = NULL;
    }

    return array;
}
