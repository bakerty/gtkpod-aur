/*
|  File conversion started by Simon Naunton <snaunton gmail.com> in 2007
|
|  Copyright (C) 2002-2007 Jorg Schuler <jcsjcs at users.sourceforge.net>
|  Part of the gtkpod project.
|
|  URL: http://gtkpod.sourceforge.net/
|  URL: http://www.gtkpod.org
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


#ifndef __FILE_CONVERT_H_
#define __FILE_CONVERT_H_

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "itdb.h"
#include "file_convert_info.h"

extern const gchar *FILE_CONVERT_CACHEDIR;
extern const gchar *FILE_CONVERT_MAXDIRSIZE;
extern const gchar *FILE_CONVERT_TEMPLATE;
extern const gchar *FILE_CONVERT_MAX_THREADS_NUM;
extern const gchar *FILE_CONVERT_DISPLAY_LOG;
extern const gchar *FILE_CONVERT_BACKGROUND_TRANSFER;

void file_convert_init (void);
void file_convert_shutdown (void);
void file_convert_prefs_changed (void);
void file_convert_update_default_sizes (void);
gboolean file_convert_add_track (Track *track);
void file_convert_itdb_first (iTunesDB *itdb);
void file_convert_continue (void);
void file_convert_cancel_itdb (iTunesDB *itdb);
void file_convert_cancel_track (Track *track);

GList *file_transfer_get_failed_tracks (iTunesDB *itdb);
FileTransferStatus file_transfer_get_status (iTunesDB *itdb,
					     gint *to_convert_num,
					     gint *converting_num,
					     gint *to_transfer_num,
					     gint *transferred_num,
					     gint *failed_num);
void file_transfer_ack_itdb (iTunesDB *itdb);
void file_transfer_continue (iTunesDB *itdb);
void file_transfer_activate (iTunesDB *itdb, gboolean active);
void file_transfer_reset (iTunesDB *itdb);
void file_transfer_reschedule (iTunesDB *itdb);
#endif
