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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <math.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include "charset.h"
#include "gp_itdb.h"
#include "sha1.h"
#include "file.h"
#include "file_convert.h"
#include "misc.h"
#include "misc_track.h"
#include "prefs.h"
#include "syncdir.h"
#include "autodetection.h"
#include "clientserver.h"
#include "gtkpod_app_iface.h"

/* A struct containing a list with available iTunesDBs. A pointer to
 this struct is stored in gtkpod_app as itdbs_head */
static struct itdbs_head *itdbs_head = NULL;

/* for convenience */
struct itdbs_head *gp_get_itdbs_head() {
    g_return_val_if_fail(gtkpod_app, NULL);
    return g_object_get_data(G_OBJECT (gtkpod_app), "itdbs_head");
}

void gp_itdb_extra_destroy(ExtraiTunesDBData *eitdb) {
    if (eitdb) {
        sha1_free_eitdb(eitdb);
        gp_itdb_pc_path_hash_destroy(eitdb);
        g_free(eitdb->offline_filename);
        itdb_photodb_free(eitdb->photodb);
        g_free(eitdb);
    }
}

ExtraiTunesDBData *gp_itdb_extra_duplicate(ExtraiTunesDBData *eitdb) {
    ExtraiTunesDBData *eitdb_dup = NULL;
    if (eitdb) {
        /* FIXME: not yet implemented */
        g_return_val_if_reached (NULL);
    }
    return eitdb_dup;
}

void gp_playlist_extra_destroy(ExtraPlaylistData *epl) {
    if (epl) {
        g_free(epl);
    }
}

ExtraPlaylistData *gp_playlist_extra_duplicate(ExtraPlaylistData *epl) {
    ExtraPlaylistData *epl_dup = NULL;

    if (epl) {
        epl_dup = g_new (ExtraPlaylistData, 1);
        memcpy(epl_dup, epl, sizeof(ExtraPlaylistData));
    }
    return epl_dup;
}

void gp_track_extra_destroy(ExtraTrackData *etrack) {
    if (etrack) {
        g_free(etrack->year_str);
        g_free(etrack->pc_path_locale);
        g_free(etrack->pc_path_utf8);
        g_free(etrack->converted_file);
        g_free(etrack->thumb_path_locale);
        g_free(etrack->thumb_path_utf8);
        g_free(etrack->hostname);
        g_free(etrack->sha1_hash);
        g_free(etrack->charset);
        g_free(etrack->lyrics);
        g_free(etrack);
    }
}

ExtraTrackData *gp_track_extra_duplicate(ExtraTrackData *etr) {
    ExtraTrackData *etr_dup = NULL;

    if (etr) {
        etr_dup = g_new (ExtraTrackData, 1);
        memcpy(etr_dup, etr, sizeof(ExtraTrackData));
        /* copy strings */
        etr_dup->year_str = g_strdup(etr->year_str);
        etr_dup->pc_path_locale = g_strdup(etr->pc_path_locale);
        etr_dup->pc_path_utf8 = g_strdup(etr->pc_path_utf8);
        etr_dup->converted_file = g_strdup(etr->converted_file);
        etr_dup->thumb_path_locale = g_strdup(etr->thumb_path_locale);
        etr_dup->thumb_path_utf8 = g_strdup(etr->thumb_path_utf8);
        etr_dup->hostname = g_strdup(etr->hostname);
        etr_dup->sha1_hash = g_strdup(etr->sha1_hash);
        etr_dup->charset = g_strdup(etr->charset);
        etr_dup->lyrics = g_strdup(etr->lyrics);
        /* clear the pc_path_hashed flag */
        etr_dup->pc_path_hashed = FALSE;
    }
    return etr_dup;
}

/* Create a new itdb struct including ExtraiTunesDBData */
iTunesDB *gp_itdb_new(void) {
    iTunesDB *itdb = itdb_new();
    gp_itdb_add_extra(itdb);
    return itdb;
}

/* Free itdb and take care of dependencies */
void gp_itdb_free(iTunesDB *itdb) {
    /* cancel all pending conversions */
    file_convert_cancel_itdb (itdb);
    itdb_free(itdb);
}

/* Add and initialize ExtraiTunesDBData if missing */
void gp_itdb_add_extra(iTunesDB *itdb) {
    g_return_if_fail (itdb);

    if (!itdb->userdata) {
        ExtraiTunesDBData *eitdb = g_new0 (ExtraiTunesDBData, 1);
        itdb->userdata = eitdb;
        itdb->userdata_destroy = (ItdbUserDataDestroyFunc) gp_itdb_extra_destroy;
        itdb->userdata_duplicate = (ItdbUserDataDuplicateFunc) gp_itdb_extra_duplicate;
        eitdb->data_changed = FALSE;
        eitdb->itdb_imported = FALSE;
        gp_itdb_pc_path_hash_init(eitdb);
    }
}

/* Validate a complete @itdb (including tracks and playlists),
 * i.e. add the Extra*Data */
void gp_itdb_add_extra_full(iTunesDB *itdb) {
    GList *gl;

    g_return_if_fail (itdb);

    /* Add and initialize ExtraiTunesDBData if missing */
    gp_itdb_add_extra(itdb);

    /* validate tracks */
    for (gl = itdb->tracks; gl; gl = gl->next) {
        Track *track = gl->data;
        g_return_if_fail (track);
        gp_track_add_extra(track);
    }

    /* validate playlists */
    for (gl = itdb->playlists; gl; gl = gl->next) {
        Playlist *pl = gl->data;
        g_return_if_fail (pl);
        gp_playlist_add_extra(pl);
    }
}

Playlist *gp_playlist_new(const gchar *title, gboolean spl) {
    Playlist *pl = itdb_playlist_new(title, spl);
    pl->userdata = g_new0 (ExtraPlaylistData, 1);
    pl->userdata_destroy = (ItdbUserDataDestroyFunc) gp_playlist_extra_destroy;
    pl->userdata_duplicate = (ItdbUserDataDuplicateFunc) gp_playlist_extra_duplicate;
    return pl;
}

/* Add and initialize the ExtraPlaylistData if missing */
void gp_playlist_add_extra(Playlist *pl) {
    g_return_if_fail (pl);

    if (!pl->userdata) {
        ExtraPlaylistData *epl = g_new0 (ExtraPlaylistData, 1);
        pl->userdata = epl;
        pl->userdata_destroy = (ItdbUserDataDestroyFunc) gp_playlist_extra_destroy;
        pl->userdata_duplicate = (ItdbUserDataDuplicateFunc) gp_playlist_extra_duplicate;
    }
}

Track *gp_track_new(void) {
    Track *track = itdb_track_new();
    /* Add ExtraTrackData */
    gp_track_add_extra(track);
    gp_track_set_flags_default(track);
    return track;
}

/* Add and initialize the ExtraTrackData if missing */
void gp_track_add_extra(Track *track) {
    g_return_if_fail (track);

    if (!track->userdata) {
        ExtraTrackData *etr = g_new0 (ExtraTrackData, 1);
        track->userdata = etr;
        etr->lyrics = NULL;
        track->userdata_destroy = (ItdbUserDataDestroyFunc) gp_track_extra_destroy;
        track->userdata_duplicate = (ItdbUserDataDuplicateFunc) gp_track_extra_duplicate;
    }
}

/* Append track to the track list of @itdb */
/* Note: the track will also have to be added to the playlists */
/* Returns: pointer to the added track -- may be different in the case
 of duplicates. In that case a pointer to the already existing track
 is returned. */
Track *gp_track_add(iTunesDB *itdb, Track *track) {
    Track *result = NULL;
    Track *oldtrack = sha1_track_exists_insert(itdb, track);

    if (oldtrack) {
        gp_duplicate_remove(oldtrack, track);
        itdb_track_free(track);
        result = oldtrack;
    }
    else {
        /* Make sure all strings are initialised -- that way we don't
         have to worry about it when we are handling the strings */
        /* exception: sha1_hash, hostname, charset: these may be NULL. */
        gp_track_validate_entries(track);
        itdb_track_add(itdb, track, -1);
        /* add to filename hash */
        gp_itdb_pc_path_hash_add_track(track);
        /* add to background conversion if necessary */
        if (! file_convert_add_track (track)) {
            g_idle_add((GSourceFunc) gp_remove_track_cb, track);
            return NULL;
        }
        result = track;
        data_changed(itdb);
    }
    return result;
}

/* Remove track and notify all windows of the change
 NOTE: you need to notify the main display via pm_remove_track()
 before when you make sure @track is no longer referenced in any
 playlist -- see gp_playlist_remove_track for details */
void gp_track_remove(Track *track) {
    /* call gp_track_unlink() and itdb_track_free() instead of
     itdb_track_remove() so we don't have to maintain two remove
     functions separately. If something needs to be done before
     removing the track do it in gp_track_unlink */
    gp_track_unlink(track);
    itdb_track_free(track);
}

/* Unlink track and notify all windows of the change
 NOTE: you need to notify the main display via pm_remove_track()
 before when you make sure @track is no longer referenced in any
 playlist -- see gp_playlist_remove_track for details */
void gp_track_unlink(Track *track) {
    gtkpod_track_removed(track);
    /* cancel pending conversions */
    file_convert_cancel_track(track);
    /* remove from SHA1 hash */
    sha1_track_remove(track);
    /* remove from pc_path_hash */
    gp_itdb_pc_path_hash_remove_track(track);
    /* remove from database */
    itdb_track_unlink(track);
}

/* Set a thumbnail and update ExtraTrackData (e.g. filename) */
static gboolean gp_track_set_thumbnails_internal(Track *track, const gchar *filename, const guchar *image_data, gsize image_data_len) {
    gboolean result = FALSE;
    ExtraTrackData *etr;

    g_return_val_if_fail (track, FALSE);

    etr = track->userdata;
    g_return_val_if_fail (etr, FALSE);

    if (filename) {
        result = itdb_track_set_thumbnails(track, filename);
    }
    else if (image_data) {
        result = itdb_track_set_thumbnails_from_data(track, image_data, image_data_len);
    }

    g_free(etr->thumb_path_locale);
    g_free(etr->thumb_path_utf8);

    if (filename && (result == TRUE)) {
        etr->thumb_path_locale = g_strdup(filename);
        etr->thumb_path_utf8 = charset_to_utf8(filename);
    }
    else {
        etr->thumb_path_locale = g_strdup("");
        etr->thumb_path_utf8 = g_strdup("");
    }

    return result;
}

/* Set a thumbnail and update data in ExtraTrackData */
gboolean gp_track_set_thumbnails_from_data(Track *track, const guchar *image_data, gsize image_data_len) {
    g_return_val_if_fail (track, FALSE);
    g_return_val_if_fail (image_data, FALSE);

    return gp_track_set_thumbnails_internal(track, NULL, image_data, image_data_len);
}

/* Set a thumbnail and store the filename in ExtraTrackData */
gboolean gp_track_set_thumbnails(Track *track, const gchar *filename) {
    g_return_val_if_fail (track, FALSE);
    g_return_val_if_fail (filename, FALSE);

    return gp_track_set_thumbnails_internal(track, filename, NULL, 0);
}

/* Remove a thumbnail and remove the filename in ExtraTrackData */
/* Return value:
 FALSE: track did not have any thumbnails, so no change was done
 TRUE: track did have thumbnails which were removed */
gboolean gp_track_remove_thumbnails(Track *track) {
    gboolean changed = FALSE;

    ExtraTrackData *etr;
    g_return_val_if_fail (track, FALSE);
    etr = track->userdata;
    g_return_val_if_fail (etr, FALSE);

    if (itdb_track_has_thumbnails(track))
        changed = TRUE;

    itdb_track_remove_thumbnails(track);
    g_free(etr->thumb_path_locale);
    g_free(etr->thumb_path_utf8);
    etr->thumb_path_locale = g_strdup("");
    etr->thumb_path_utf8 = g_strdup("");

    return changed;
}

/* add itdb to itdbs (and add to display) */
void gp_itdb_add(iTunesDB *itdb, gint pos) {
    ExtraiTunesDBData *eitdb;

    g_return_if_fail (itdbs_head);
    g_return_if_fail (itdb);
    eitdb = itdb->userdata;
    g_return_if_fail (eitdb);

    eitdb->itdbs_head = itdbs_head;
    itdbs_head->itdbs = g_list_insert(itdbs_head->itdbs, itdb, pos);
    g_signal_emit(gtkpod_app, gtkpod_app_signals[ITDB_ADDED], 0, itdb, pos);
}

/* Remove itdb to itdbs (and remove from display). Call
 * itdb_free() to free the memory of the itdb. */
void gp_itdb_remove(iTunesDB *itdb) {
    g_return_if_fail (itdbs_head);
    g_return_if_fail (itdb);

    g_signal_emit(gtkpod_app, gtkpod_app_signals[ITDB_REMOVED], 0, itdb);
    itdbs_head->itdbs = g_list_remove(itdbs_head->itdbs, itdb);
}

/* Also replaces @old_itdb in the itdbs GList and take care that the
 * displayed itdb gets replaced as well */
void gp_replace_itdb(iTunesDB *old_itdb, iTunesDB *new_itdb) {
    ExtraiTunesDBData *new_eitdb;
    Playlist *old_pl, *mpl;
    GList *old_link;
    gchar *old_pl_name = NULL;

    g_return_if_fail (old_itdb);
    g_return_if_fail (new_itdb);
    g_return_if_fail (itdbs_head);

    new_eitdb = new_itdb->userdata;
    g_return_if_fail (new_eitdb);

    old_link = g_list_find(itdbs_head->itdbs, old_itdb);
    g_return_if_fail (old_link);

    /* remember old selection */
    old_pl = gtkpod_get_current_playlist();
    if (old_pl) { /* remember name of formerly selected playlist if it's in the same itdb */
        if (old_pl->itdb == old_itdb)
            old_pl_name = g_strdup(old_pl->name);
    }

    /* replace old_itdb with new_itdb */
    new_eitdb->itdbs_head = itdbs_head;
    old_link->data = new_itdb;

    /* Set prefs system with name of MPL */
    mpl = itdb_playlist_mpl(new_itdb);
    set_itdb_prefs_string(new_itdb, "name", mpl->name);

    /* update ui */
    g_signal_emit(gtkpod_app, gtkpod_app_signals[ITDB_UPDATED], 0, old_itdb, new_itdb);

    /* reselect old playlist if still available */
    if (old_pl_name) {
        Playlist *pl = itdb_playlist_by_name(new_itdb, old_pl_name);
        if (pl)
            gtkpod_set_current_playlist(pl);
    }

    /* Clean up */
    /* free old_itdb */
    gp_itdb_free(old_itdb);
    g_free(old_pl_name);
}

/* add playlist to itdb and to display */
void gp_playlist_add(iTunesDB *itdb, Playlist *pl, gint32 pos) {
    g_return_if_fail (itdb);
    g_return_if_fail (pl);

    itdb_playlist_add(itdb, pl, pos);
    gtkpod_playlist_added(itdb, pl, pos);
    data_changed(itdb);
}

/* create new playlist with title @name and add to @itdb and to
 * display at position @pos */
Playlist *gp_playlist_add_new(iTunesDB *itdb, gchar *name, gboolean spl, gint32 pos) {
    Playlist *pl;

    g_return_val_if_fail (itdb, NULL);
    g_return_val_if_fail (name, NULL);

    pl = gp_playlist_new(name, spl);
    gp_playlist_add(itdb, pl, pos);
    return pl;
}

/** If playlist @pl_name doesn't exist, then it will be created
 * and added to the tail of playlists, otherwise pointer to an existing
 * playlist will be returned
 */
Playlist *gp_playlist_by_name_or_add(iTunesDB *itdb, gchar *pl_name, gboolean spl) {
    Playlist *pl = NULL;

    g_return_val_if_fail (itdb, pl);
    g_return_val_if_fail (pl_name, pl);
    pl = itdb_playlist_by_name(itdb, pl_name);
    if (pl) { /* check if it's the same type (spl or normal) */
        if (pl->is_spl == spl)
            return pl;
    }
    /* Create a new playlist */
    pl = gp_playlist_add_new(itdb, pl_name, spl, -1);
    return pl;
}

/* Remove a playlist from the itdb and from the display */
void gp_playlist_remove(Playlist *pl) {
    g_return_if_fail (pl);
    g_return_if_fail (pl->itdb);
    g_signal_emit(gtkpod_app, gtkpod_app_signals[PLAYLIST_REMOVED], 0, pl);
    data_changed(pl->itdb);
    itdb_playlist_remove(pl);
    // In case interested parties do not properly reset current playlist, ensure it happens here
    if (pl && pl == gtkpod_get_current_playlist())
        gtkpod_set_current_playlist(NULL);
}

/* FIXME: this is a bit dangerous. . . we delete all
 * playlists with titles @pl_name and return how many
 * pl have been removed.
 ***/
guint gp_playlist_remove_by_name(iTunesDB *itdb, gchar *pl_name) {
    guint i;
    guint pl_removed = 0;

    g_return_val_if_fail (itdb, pl_removed);

    for (i = 1; i < itdb_playlists_number(itdb); i++) {
        Playlist *pl = itdb_playlist_by_nr(itdb, i);
        g_return_val_if_fail (pl, pl_removed);
        g_return_val_if_fail (pl->name, pl_removed);
        if (strcmp(pl->name, pl_name) == 0) {
            gp_playlist_remove(pl);
            /* we just deleted the ith element of playlists, so
             * we must examine the new ith element. */
            pl_removed++;
            i--;
        }
    }
    return pl_removed;
}

/* This function removes the track "track" from the
 playlist "plitem" and also adjusts the display.
 No action is taken if "track" is not in the playlist.
 If "plitem" == NULL, remove from master playlist.
 If the track is removed from the MPL, it's also removed
 from memory completely (i.e. from the tracklist and sha1 hash).
 Depending on @deleteaction, the track is either marked for deletion
 on the ipod/hard disk or just removed from the database
 */
void gp_playlist_remove_track(Playlist *plitem, Track *track, DeleteAction deleteaction) {
    iTunesDB *itdb;
    Playlist *mpl;
    gboolean remove_track = FALSE;

    g_return_if_fail (track);
    itdb = track->itdb;
    g_return_if_fail (itdb);

    switch (deleteaction) {
    case DELETE_ACTION_IPOD:
    case DELETE_ACTION_LOCAL:
    case DELETE_ACTION_DATABASE:
        /* remove from MPL in these cases */
        plitem = NULL;
        break;
    case DELETE_ACTION_PLAYLIST:
        /* cannot remove from MPL */
        g_return_if_fail (plitem);
        break;
    }

    mpl = itdb_playlist_mpl(track->itdb);

    if (plitem == NULL)
        plitem = mpl;

    g_return_if_fail (plitem);

    /* remove track from playlist */
    itdb_playlist_remove_track(plitem, track);

#if 0
    /* podcasts are no longer treated differently from other playlists */
    /* if we removed a podcasts, remove it from memory as well, unless
     it's present in the MPL (this happens if this podcast was on
     the iPod as podcast as well as standard track) */
    if (itdb_playlist_is_podcasts (plitem))
    {
        /* just for safety: remove possible duplicates of @track in
         the podcast playlist before removing it from memory */
        while (g_list_find (plitem->members, track))
        {
            itdb_playlist_remove_track (plitem, track);
        }
        if (!itdb_playlist_contains_track (mpl, track))
        {
            remove_track = TRUE;
        }
        else
        { /* strip the podcast flags */
            gp_track_set_flags_default (track);
        }
    }
#endif

    if (itdb_playlist_is_mpl(plitem)) { /* if it's the MPL, we remove the track permanently */
        GList *gl = g_list_nth(itdb->playlists, 1);
        ExtraiTunesDBData *eitdb = itdb->userdata;
        g_return_if_fail (eitdb);

        while (gl) { /* first we remove the track from all other playlists (i=1) */
            Playlist *pl = gl->data;
            g_return_if_fail (pl);
            while (g_list_find(pl->members, track)) {
                itdb_playlist_remove_track(pl, track);
            }
            gl = gl->next;
        }
        remove_track = TRUE;
    }

    if (remove_track) {
        if (itdb->usertype & GP_ITDB_TYPE_IPOD) {
            switch (deleteaction) {
            case DELETE_ACTION_DATABASE:
                /* ATTENTION: this might create a dangling file on the
                 iPod! */
                gp_track_remove(track);
                break;
            case DELETE_ACTION_IPOD:
                if (track->transferred) {
                    gp_track_unlink(track);
                    mark_track_for_deletion(itdb, track);
                }
                else {
                    gp_track_remove(track);
                }
                break;
            case DELETE_ACTION_PLAYLIST:
            case DELETE_ACTION_LOCAL:
                break;
                /* not allowed -- programming error */
                g_return_if_reached ();
                break;
            }
        }
        if (itdb->usertype & GP_ITDB_TYPE_LOCAL) {
            switch (deleteaction) {
            case DELETE_ACTION_LOCAL:
                gp_track_unlink(track);
                mark_track_for_deletion(itdb, track);
                break;
            case DELETE_ACTION_DATABASE:
                gp_track_remove(track);
                break;
            case DELETE_ACTION_PLAYLIST:
            case DELETE_ACTION_IPOD:
                /* not allowed -- programming error */
                g_return_if_reached ();
                break;
            }
        }
    }

    data_changed(itdb);
}

/* This function appends the track "track" to the
 playlist @pl. It then lets the display model know.
 @display: if TRUE, track is added the display.  Otherwise it's only
 added to memory */
/* All tracks added to the podcast playlist will get the mark_unplayed
 flag get set */
void gp_playlist_add_track(Playlist *pl, Track *track, gboolean display) {
    iTunesDB *itdb;

    g_return_if_fail (track);
    g_return_if_fail (pl);
    itdb = pl->itdb;
    g_return_if_fail (itdb);

    itdb_playlist_add_track(pl, track, -1);
    if (itdb_playlist_is_podcasts(pl)) { /* have the iPod display a bullet in front of the track as it
     has been newly added */
        track->mark_unplayed = 0x02;
    }

    if (display)
        gtkpod_track_added(track);

    data_changed(itdb);
}

/* Make sure all strings are initialised -- that way we don't
 have to worry about it when we are handling the strings.
 exception: sha1_hash, hostname and charset: these may be NULL. */
void gp_track_validate_entries(Track *track) {
    ExtraTrackData *etr;

    g_return_if_fail (track);
    etr = track->userdata;
    g_return_if_fail (etr);

    if (!track->title)
        track->title = g_strdup("");
    if (!track->artist)
        track->artist = g_strdup("");
    if (!track->album)
        track->album = g_strdup("");
    if (!track->genre)
        track->genre = g_strdup("");
    if (!track->composer)
        track->composer = g_strdup("");
    if (!track->comment)
        track->comment = g_strdup("");
    if (!track->filetype)
        track->filetype = g_strdup("");
    if (!track->grouping)
        track->grouping = g_strdup("");
    if (!track->category)
        track->category = g_strdup("");
    if (!track->description)
        track->description = g_strdup("");
    if (!track->podcasturl)
        track->podcasturl = g_strdup("");
    if (!track->podcastrss)
        track->podcastrss = g_strdup("");
    if (!track->subtitle)
        track->subtitle = g_strdup("");
    if (!track->ipod_path)
        track->ipod_path = g_strdup("");
    if (!track->tvshow)
        track->tvshow = g_strdup("");
    if (!track->tvepisode)
        track->tvepisode = g_strdup("");
    if (!track->tvnetwork)
        track->tvnetwork = g_strdup("");
    if (!track->albumartist)
        track->albumartist = g_strdup("");
    if (!track->sort_artist)
        track->sort_artist = g_strdup("");
    if (!track->sort_title)
        track->sort_title = g_strdup("");
    if (!track->sort_album)
        track->sort_album = g_strdup("");
    if (!track->sort_albumartist)
        track->sort_albumartist = g_strdup("");
    if (!track->sort_composer)
        track->sort_composer = g_strdup("");
    if (!track->sort_tvshow)
        track->sort_tvshow = g_strdup("");
    if (!etr->pc_path_utf8)
        etr->pc_path_utf8 = g_strdup("");
    if (!etr->pc_path_locale)
        etr->pc_path_locale = g_strdup("");
    if (!etr->thumb_path_utf8)
        etr->thumb_path_utf8 = g_strdup("");
    if (!etr->thumb_path_locale)
        etr->thumb_path_locale = g_strdup("");
    if (!etr->lyrics)
        etr->lyrics = g_strdup("");
    /* Make sure year_str is identical to year */
    g_free(etr->year_str);
    etr->year_str = g_strdup_printf("%d", track->year);
}


static void maybe_cleanup_string (char **str)
{
    if (((*str) != NULL) && ((*str)[0] == '\0')) {
        g_free (*str);
        *str = NULL;
    }
}

void gp_track_cleanup_empty_strings (Track *track)
{
    maybe_cleanup_string (&track->title);
    maybe_cleanup_string (&track->artist);
    maybe_cleanup_string (&track->album);
    maybe_cleanup_string (&track->genre);
    maybe_cleanup_string (&track->composer);
    maybe_cleanup_string (&track->comment);
    maybe_cleanup_string (&track->filetype);
    maybe_cleanup_string (&track->grouping);
    maybe_cleanup_string (&track->category);
    maybe_cleanup_string (&track->description);
    maybe_cleanup_string (&track->podcasturl);
    maybe_cleanup_string (&track->podcastrss);
    maybe_cleanup_string (&track->subtitle);
    maybe_cleanup_string (&track->ipod_path);
    maybe_cleanup_string (&track->tvshow);
    maybe_cleanup_string (&track->tvepisode);
    maybe_cleanup_string (&track->tvnetwork);
    maybe_cleanup_string (&track->albumartist);
    maybe_cleanup_string (&track->sort_artist);
    maybe_cleanup_string (&track->sort_title);
    maybe_cleanup_string (&track->sort_album);
    maybe_cleanup_string (&track->sort_albumartist);
    maybe_cleanup_string (&track->sort_composer);
    maybe_cleanup_string (&track->sort_tvshow);
}


/* Initialize the itdb data
 *
 * If itdb_n_type/filename/mountpoint... exist in the prefs, that data
 * is being used and local databases are read in directly if they
 * exist. Otherwise a simple two-itdb list is constructed consisting
 * of one local database and one ipod database.
 *
 */
gboolean gp_init(gpointer data) {
    gchar *cfgdir;

    cfgdir = prefs_get_cfgdir();

    itdbs_head = g_new0 (struct itdbs_head, 1);

    g_object_set_data(G_OBJECT (gtkpod_app), "itdbs_head", itdbs_head);

    if (!prefs_get_int_value("itdb_0_type", NULL)) {
        /* databases have not been set up previously -- take care of
         this */
        gchar *filename;

        /* Local database */
        filename = g_build_filename(cfgdir, "local_0.itdb", NULL);
        prefs_set_int("itdb_0_type", GP_ITDB_TYPE_LOCAL);
        prefs_set_string("itdb_0_name", _("Music Library"));
        prefs_set_string("itdb_0_filename", filename);
        g_free(filename);

        /* Podcasts database */
        filename = g_build_filename(cfgdir, "podcasts.itdb", NULL);
        prefs_set_int("itdb_1_type", GP_ITDB_TYPE_PODCASTS | GP_ITDB_TYPE_LOCAL);
        prefs_set_string("itdb_1_name", _("Podcasts"));
        prefs_set_string("itdb_1_filename", filename);
        g_free(filename);
    }

    /* initiate client server */
    server_setup();

    g_free(cfgdir);

    return FALSE;
}

/**
 * Initialise all the itunes databases
 */
gboolean gp_init_itdbs(gpointer data) {
    gint i;

    for (i = 0;; ++i) {
        ExtraiTunesDBData *eitdb;
        iTunesDB *itdb = setup_itdb_n(i);

        if (itdb == NULL) {
            gtkpod_statusbar_message(_("Importing of ipods completed."));
            break;
        }

        /* add to the display */
        gp_itdb_add(itdb, -1);

        /* update/sync playlists according to options set */
        eitdb = itdb->userdata;
        g_return_val_if_fail (eitdb, FALSE);
        if (eitdb->itdb_imported) {
            /* take care of autosync... */
            sync_all_playlists(itdb);

            /* update all live SPLs */
            itdb_spl_update_live(itdb);
        }
    }

    /* Itdbs now ready. Initiate autodetection */
    autodetection_init();

    return FALSE;
}

/* Create an repository according to the settings in the preferences
 system. */
iTunesDB *setup_itdb_n(gint i) {
    iTunesDB *itdb = NULL;
    gchar *property = get_itdb_prefs_key(i, "type");
    gint type;
    gboolean valid = prefs_get_int_value(property, &type);
    g_free(property);
    if (valid) {
        gchar *cfgdir = prefs_get_cfgdir();
        Playlist *pl = NULL;
        ExtraiTunesDBData *eitdb;
        gchar *filename = NULL;
        gchar *mountpoint = NULL;
        gchar *offline_filename = NULL;

        if (type & GP_ITDB_TYPE_LOCAL) {
            gchar *fn = get_itdb_prefs_key(i, "filename");

            filename = prefs_get_string(fn);

            if (!filename) {
                gchar *local = g_strdup_printf("local%d.itdb", i);
                filename = g_build_filename(cfgdir, local, NULL);
                g_free(local);
            }
            g_free(fn);
            if (g_file_test(filename, G_FILE_TEST_EXISTS))
                itdb = gp_import_itdb(NULL, type, NULL, NULL, filename);
        }
        else if (type & GP_ITDB_TYPE_IPOD) {
            gchar *key;

            key = get_itdb_prefs_key(i, KEY_MOUNTPOINT);
            mountpoint = prefs_get_string(key);
            g_free(key);

            key = get_itdb_prefs_key(i, "filename");
            offline_filename = prefs_get_string(key);
            g_free(key);

            if (!offline_filename) {
                gchar *local = g_strdup_printf("gtkpod_%d.itdb", i);
                offline_filename = g_build_filename(cfgdir, local, NULL);
                g_free(local);
            }
        }
        else {
            g_return_val_if_reached (NULL);
        }

        if (!itdb) {
            gchar *nm, *name;

            itdb = gp_itdb_new();
            eitdb = itdb->userdata;
            g_return_val_if_fail (eitdb, NULL);
            itdb->usertype = type;
            itdb->filename = filename;
            itdb_set_mountpoint(itdb, mountpoint);
            eitdb->offline_filename = offline_filename;

            nm = g_strdup_printf("itdb_%d_name", i);
            if (!prefs_get_string_value(nm, &name)) {
                if (type & GP_ITDB_TYPE_PODCASTS)
                    name = g_strdup(_("Podcasts"));
                else if (type & GP_ITDB_TYPE_LOCAL)
                    name = g_strdup(_("Local"));
                else
                    name = g_strdup(_("iPod"));
            }
            pl = gp_playlist_new(name, FALSE);
            g_free(name);
            g_free(nm);
            itdb_playlist_set_mpl(pl);
            itdb_playlist_add(itdb, pl, -1);

            if ((type & GP_ITDB_TYPE_PODCASTS) || (type & GP_ITDB_TYPE_LOCAL)) {
                eitdb->data_changed = TRUE;
                eitdb->itdb_imported = TRUE;
            }
            else {
                eitdb->data_changed = FALSE;
                eitdb->itdb_imported = FALSE;
            }
        }
        else {
            g_free(filename);
            g_free(offline_filename);
        }
        g_free(mountpoint);

        /* Check if Podcast Playlist is present on IPOD itdb
         * and add if not. If Podcast Playlist is present on
         * local itdb remove it. */
        pl = itdb_playlist_podcasts(itdb);
        if ((type & GP_ITDB_TYPE_IPOD) && !pl) { /* add podcast playlist */
            pl = gp_playlist_new(_("Podcasts"), FALSE);
            itdb_playlist_set_podcasts(pl);
            itdb_playlist_add(itdb, pl, -1);
            eitdb = itdb->userdata;
            g_return_val_if_fail (eitdb, NULL);
            eitdb->data_changed = FALSE;
        }
        if ((type & GP_ITDB_TYPE_LOCAL) && pl) { /* Remove podcast playlist. Normally no playlist
         * should be present, except for a few people who
         * used the CVS version between September and
         * October 2005. */
            if (itdb_playlist_tracks_number(pl) == 0) {
                gp_playlist_remove(pl);
            }
            else { /* OK, let's be nice and just drop the
             'podcast' flag instead of removing */
                pl->podcastflag = 0;
            }
        }
        g_free(cfgdir);
    }
    return itdb;
}

/* Increase playcount of filename <file> by <num>. If sha1 is activated,
 use sha1 to find the track. Otherwise use the filename. If @sha1 is
 set, this value is used directly to look up the track in the
 database (instead of calculating it from the file).

 Return value:
 TRUE: OK (playcount increased in GP_ITDB_TYPE_IPOD)
 FALSE: file could not be found in the GP_ITDB_TYPE_IPOD */
gboolean gp_increase_playcount(gchar *sha1, gchar *file, gint num) {
    gboolean result = FALSE;
    Track *track = NULL;
    GList *gl;

    g_return_val_if_fail (itdbs_head, FALSE);

    for (gl = itdbs_head->itdbs; gl; gl = gl->next) {
        iTunesDB *itdb = gl->data;
        g_return_val_if_fail (itdb, FALSE);

        if (sha1)
            track = sha1_sha1_exists(itdb, sha1);
        else
            track = sha1_file_exists(itdb, file, TRUE);
        if (!track)
            track = gp_track_by_filename(itdb, file);
        if (track) {
            gchar *buf1;
            track->playcount += num;
            data_changed(itdb);
            gtkpod_track_updated(track);
            buf1 = get_track_info(track, TRUE);
            gtkpod_statusbar_message(_("Increased playcount for '%s'"), buf1);
            g_free(buf1);
            if (itdb->usertype & GP_ITDB_TYPE_IPOD)
                result = TRUE;
        }
    }
    return result;
}

/* get the currently selected itdb. NULL is
 * returned if no itdb is active. */
iTunesDB *gp_get_selected_itdb(void) {
    return gtkpod_get_current_itdb();
}

/* Get the "ipod" itdb. If only one iPod itdb exists, this itdb is
 * returned. If more than one iPod itdb exists, the currently selected
 * itdb is returned if it's an iPod itdb, otherwise NULL is returned.
 */
iTunesDB *gp_get_ipod_itdb(void) {
    struct itdbs_head *itdbs_head;
    iTunesDB *itdb;
    GList *gl;
    gint i;

    /* if an iPod itdb is selected, return this */
    itdb = gp_get_selected_itdb();
    if (itdb && (itdb->usertype & GP_ITDB_TYPE_IPOD))
        return itdb;

    itdb = NULL;

    g_return_val_if_fail (gtkpod_app, NULL);
    itdbs_head = gp_get_itdbs_head();

    if (itdbs_head == NULL)
        return NULL;

    i = 0;
    for (gl = itdbs_head->itdbs; gl; gl = gl->next) {
        iTunesDB *itdbgl = gl->data;
        g_return_val_if_fail (itdbgl, NULL);
        if (itdbgl->usertype & GP_ITDB_TYPE_IPOD) {
            itdb = itdbgl;
            ++i;
        }
    }
    /* return iPod itdb if only one was found */
    if (i == 1)
        return itdb;

    return NULL;
}

/* return the podcast itdb */
iTunesDB *gp_get_podcast_itdb() {
    GList *gl;

    g_return_val_if_fail (itdbs_head, NULL);

    for (gl = itdbs_head->itdbs; gl; gl = gl->next) {
        iTunesDB *itdb = gl->data;
        g_return_val_if_fail (itdb, NULL);

        if (itdb->usertype & GP_ITDB_TYPE_PODCASTS)
            return itdb;
    }
    return NULL;
}

