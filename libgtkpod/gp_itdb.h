/*
|  Copyright (C) 2002-2007 Jorg Schuler <jcsjcs at users sourceforge net>
|  Part of the gtkpod project.
|
|  URL: http://www.gtkpod.org/
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

#ifndef __DISPLAY_ITDB_H__
#define __DISPLAY_ITDB_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include "itdb.h"
#include "file_convert_info.h"
#include "gtkpod_app_iface.h"

struct itdbs_head
{
    GList *itdbs;
};

typedef struct
{
    struct itdbs_head *itdbs_head; /* pointer to the master itdbs_head     */
    GHashTable *sha1hash;          /* sha1 hash information                */
    GHashTable *pc_path_hash;      /* hash with local filenames            */
    GList *pending_deletion;       /* tracks marked for removal from
				      media                                */
    gchar *offline_filename;       /* filename for offline database
				      (only for GP_ITDP_TYPE_IPOD)         */
    gboolean offline;              /* offline mode?                        */
    gboolean data_changed;         /* data changed since import?           */
    gboolean photo_data_changed;	/* photo data changed since import?		*/
    gboolean itdb_imported;        /* has in iTunesDB been imported?       */
    gboolean ipod_ejected;         /* if iPod was ejected                  */
    PhotoDB *photodb;            /* Photo DB reference used if the ipod supports photos */
} ExtraiTunesDBData;

typedef struct
{
    glong size;
} ExtraPlaylistData;

typedef struct
{
  gchar   *year_str;        /* year as string -- always identical to year  */
  gchar   *pc_path_locale;  /* path on PC (local encoding)                 */
  gchar   *pc_path_utf8;    /* PC filename in utf8 encoding                */
  time_t  mtime;            /* modification date of PC file                */
  gboolean pc_path_hashed;  /* for programming error detection (see
			       gp_itdb_local_path_hash_add_track()         */
  gchar   *converted_file;  /* if converted file exists: name in utf8      */
  gint32  orig_filesize;    /* size of original file (if converted)        */
  FileConvertStatus conversion_status; /* current status of conversion     */
  gchar   *thumb_path_locale;/* same for thumbnail                         */
  gchar   *thumb_path_utf8;  /* same for thumbnail                         */
  gchar   *hostname;        /* name of host this file has been imported on */
  gchar   *sha1_hash;       /* sha1 hash of file (or NULL)                 */
  gchar   *charset;         /* charset used for ID3 tags                   */
  gint32  sortindex;        /* used for stable sorting (current order)     */
  gboolean tchanged;        /* temporary use, e.g. in detail.c             */
  gboolean tartwork_changed;			/* temporary use for artwork, eg. in detail.c          */
  guint64 local_itdb_id;    /* when using DND from local to iPod:
			       original itdb                               */
  guint64 local_track_dbid; /* when using DND from local to iPod:
			       original track                              */
  gchar   *lyrics;          /* Lyrics information as read from file or as
			       updated in the program                      */
} ExtraTrackData;

/* types for iTunesDB */
typedef enum
{
    GP_ITDB_TYPE_LOCAL = 1<<0,    /* local browsing, normal music */
    GP_ITDB_TYPE_IPOD  = 1<<1,    /* iPod                         */
    GP_ITDB_TYPE_PODCASTS = 1<<2, /* local browsing, podcasts     */
    GP_ITDB_TYPE_AUTOMATIC = 1<<3,/* repository was automounted   */
} GpItdbType;

/* Delete actions */
typedef enum
{
    /* remove from playlist only -- cannot be used on MPL */
    DELETE_ACTION_PLAYLIST = 0,
    /* remove from iPod (implicates removing from database) */
    DELETE_ACTION_IPOD,
    /* remove from local harddisk (implicates removing from database) */
    DELETE_ACTION_LOCAL,
    /* remove from database only */
    DELETE_ACTION_DATABASE
} DeleteAction;

struct DeleteData
{
    iTunesDB *itdb;
    Playlist *pl;
    GList *tracks;
    DeleteAction deleteaction;
};

gboolean gp_init_itdbs(gpointer data);
iTunesDB *setup_itdb_n (gint i);

struct itdbs_head *gp_get_itdbs_head ();

iTunesDB *gp_itdb_new (void);
void gp_itdb_add (iTunesDB *itdb, gint pos);
void gp_itdb_remove (iTunesDB *itdb);
void gp_itdb_free (iTunesDB *itdb);
void gp_replace_itdb (iTunesDB *old_itdb, iTunesDB *new_itdb);
void gp_itdb_add_extra (iTunesDB *itdb);
void gp_itdb_add_extra_full (iTunesDB *itdb);

Track *gp_track_new (void);
#define gp_track_free itdb_track_free
Track *gp_track_add (iTunesDB *itdb, Track *track);
void gp_track_remove (Track *track);
void gp_track_unlink (Track *track);
void gp_track_add_extra (Track *track);
void gp_track_validate_entries (Track *track);
gboolean gp_track_set_thumbnails (Track *track, const gchar *filename);
gboolean gp_track_set_thumbnails_from_data (Track *track,
					    const guchar *image_data,
					    gsize image_data_len);

gboolean gp_track_remove_thumbnails (Track *track);
void gp_track_cleanup_empty_strings (Track *track);

Playlist *gp_playlist_new (const gchar *title, gboolean spl);
void gp_playlist_add (iTunesDB *itdb, Playlist *pl, gint32 pos);
void gp_playlist_remove (Playlist *pl);
guint gp_playlist_remove_by_name (iTunesDB *itdb, gchar *pl_name);
Playlist *gp_playlist_add_new (iTunesDB *itdb, gchar *name,
			       gboolean spl, gint32 pos);
Playlist *gp_playlist_by_name_or_add (iTunesDB *itdb, gchar *pl_name,
				      gboolean spl);
void gp_playlist_remove_track (Playlist *plitem, Track *track,
			       DeleteAction deleteaction);
void gp_playlist_add_track (Playlist *pl, Track *track, gboolean display);

void gp_playlist_add_extra (Playlist *pl);

gboolean gp_increase_playcount (gchar *sha1, gchar *file, gint num);
iTunesDB *gp_get_selected_itdb (void);
iTunesDB *gp_get_ipod_itdb (void);
iTunesDB *gp_get_podcast_itdb (void);
#endif
