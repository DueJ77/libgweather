/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* $Id: weather-met.c 10124 2007-01-05 14:02:46Z kmaraas $ */

/*
 *  Papadimitriou Spiros <spapadim+@cs.cmu.edu>
 *
 *  This code released under the GNU GPL.
 *  Read the file COPYING for more information.
 *
 *  Weather server functions (MET)
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include <libgweather/weather.h>
#include "weather-priv.h"

static char *
met_reprocess (char *x, int len)
{
    char *p = x;
    char *o;
    int spacing = 0;
    static gchar *buf;
    static gint buflen = 0;
    gchar *lastspace = NULL;
    int count = 0;

    if (buflen < len)
    {
	if (buf)
	    g_free (buf);
	buf = g_malloc (len + 1);
	buflen = len;
    }

    o = buf;
    x += len;       /* End mark */

    while (*p && p < x) {
	if (g_ascii_isspace (*p)) {
	    if (!spacing) {
		spacing = 1;
		lastspace = o;
		count++;
		*o++ = ' ';
	    }
	    p++;
	    continue;
	}
	spacing = 0;
	if (count > 75 && lastspace) {
	    count = o - lastspace - 1;
	    *lastspace = '\n';
	    lastspace = NULL;
	}

	if (*p == '&') {
	    if (g_ascii_strncasecmp (p, "&amp;", 5) == 0) {
		*o++ = '&';
		count++;
		p += 5;
		continue;
	    }
	    if (g_ascii_strncasecmp (p, "&lt;", 4) == 0) {
		*o++ = '<';
		count++;
		p += 4;
		continue;
	    }
	    if (g_ascii_strncasecmp (p, "&gt;", 4) == 0) {
		*o++ = '>';
		count++;
		p += 4;
		continue;
	    }
	}
	if (*p == '<') {
	    if (g_ascii_strncasecmp (p, "<BR>", 4) == 0) {
		*o++ = '\n';
		count = 0;
	    }
	    if (g_ascii_strncasecmp (p, "<B>", 3) == 0) {
		*o++ = '\n';
		*o++ = '\n';
		count = 0;
	    }
	    p++;
	    while (*p && *p != '>')
		p++;
	    if (*p)
		p++;
	    continue;
	}
	*o++ = *p++;
	count++;
    }
    *o = 0;
    return buf;
}


/*
 * Parse the metoffice forecast info.
 * For gnome 3.0 we want to just embed an HTML bonobo component and
 * be done with this ;)
 */

static gchar *
met_parse (const gchar *meto)
{
    gchar *p;
    gchar *rp;
    gchar *r = g_strdup ("Met Office Forecast\n");
    gchar *t;

    g_return_val_if_fail (meto != NULL, r);

    p = strstr (meto, "Summary: </b>");
    g_return_val_if_fail (p != NULL, r);

    rp = strstr (p, "Text issued at:");
    g_return_val_if_fail (rp != NULL, r);

    p += 13;
    /* p to rp is the text block we want but in HTML malformat */
    t = g_strconcat (r, met_reprocess (p, rp - p), NULL);
    g_free (r);

    return t;
}

static void
met_finish (SoupSession *session, SoupMessage *msg, gpointer data)
{
    WeatherInfo *info = (WeatherInfo *)data;

    g_return_if_fail (info != NULL);

    if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
	g_warning ("Failed to get Met Office forecast data: %d %s.\n",
		   msg->status_code, msg->reason_phrase);
        request_done (info, FALSE);
        return;
    }

    info->forecast = met_parse (msg->response_body->data);
    request_done (info, TRUE);
}

void
metoffice_start_open (WeatherInfo *info)
{
    gchar *url;
    SoupMessage *msg;
    WeatherLocation *loc;

    loc = info->location;
    url = g_strdup_printf ("http://www.metoffice.gov.uk/weather/europe/uk/%s.html", loc->zone + 1);

    msg = soup_message_new ("GET", url);
    soup_session_queue_message (info->session, msg, met_finish, info);
    g_free (url);
}
