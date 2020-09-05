/* Time-stamp: <2003-06-12 23:57:56 jcs>
|
|  Copyright (C) 2002-2003 Jorg Schuler <jcsjcs at users.sourceforge.net>
|  Part of the gtkpod project.
|
|  URL: http://gtkpod.sourceforge.net/
|
|  This program is free software; you can redistribute it and/or modify
|  it under the terms of the GNU General Public License as published by
|  the Free Software Foundation; either version 2 of the License, or
|  (at your option) any later version.
|
|  This program is distributed in the hope that it will be useful,
|  but WITHOUT ANY WARRANTY; without even the implied warranty of
|  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
|  GNU General Public License for more details.
|
|  You should have received a copy of the GNU General Public License
|  along with this program; if not, write to the Free Software
|  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
|
|  iTunes and iPod are trademarks of Apple
|
|  This product is not supported/written/published by Apple!
|
|  $Id$
*/

#ifndef __DATE_PARSER_H__
#define __DATE_PARSER_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <time.h>

/* Set to "1" if debugging output is desired */
#define DP_DEBUG 0

/* struct for time info (added, played, modified) */
typedef struct {
    gchar *int_str;   /* copy of string specified in the sort tab */
    gboolean valid;   /* is current string valid? */
    time_t lower;     /* timestamp for lower limit */
    time_t upper;     /* timestamp for upper limit ("-1": no limit) */
    GtkWidget *entry; /* pointer to GtkEntry in sort tab */
    GtkWidget *active;/* pointer to toggle button */
} TimeInfo;

gboolean dp_parse (gchar *dp_str, time_t *result,
		   gboolean lower_margin, gboolean strict);
void dp2_parse (TimeInfo *ti);
#endif
