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

#include "gp_itdb.h"
#include "sha1.h"
#include "prefs.h"
#include "misc.h"
#include "misc_track.h"
#include "charset.h"
#include "exporter_iface.h"
#include "filetype_iface.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>

/* ------------------------------------------------------------ *\
|                                                                |
 |         functions for sha1 checksums                            |
 |                                                                |
 \* ------------------------------------------------------------ */

/**
 * Register all tracks in the sha1 hash and remove duplicates (while
 * preserving playlists)
 */
void gp_sha1_hash_tracks_itdb(iTunesDB *itdb) {
    gint ns, count;
    GList *gl;

    g_return_if_fail (itdb);

    if (!prefs_get_int("sha1"))
        return;
    ns = itdb_tracks_number(itdb); /* number of tracks */
    if (ns == 0)
        return;

    block_widgets(); /* block widgets -- this might take a while,
     so we'll do refreshs */
    sha1_free(itdb); /* release sha1 hash */
    count = 0;
    /* populate the hash table */
    gl = itdb->tracks;
    while (gl) {
        Track *track = gl->data;
        Track *oldtrack = sha1_track_exists_insert(itdb, track);

        /* need to get next track now because it might be a duplicate and
         thus be removed when we call gp_duplicate_remove() */
        gl = gl->next;

        if (oldtrack) {
            gp_duplicate_remove(oldtrack, track);
        }

        ++count;
        if (((count % 20) == 1) || (count == ns)) { /* update for count == 1, 21, 41 ... and for count == n */
            gtkpod_statusbar_message(ngettext ("Hashed %d of %d track.",
                    "Hashed %d of %d tracks.", ns), count, ns);
            while (widgets_blocked && gtk_events_pending())
                gtk_main_iteration();
        }
    }
    gp_duplicate_remove(NULL, NULL); /* show info dialogue */
    release_widgets(); /* release widgets again */
}

/**
 * Call gp_hash_tracks_itdb() for each itdb.
 *
 */
void gp_sha1_hash_tracks(void) {
    GList *gl;
    struct itdbs_head *itdbs_head;

    itdbs_head = gp_get_itdbs_head();
    g_return_if_fail (itdbs_head);

    block_widgets();
    for (gl = itdbs_head->itdbs; gl; gl = gl->next) {
        gp_sha1_hash_tracks_itdb(gl->data);
    }
    release_widgets();
}

static void rm_sha1(gpointer track, gpointer user_data) {
    ExtraTrackData *etr;
    g_return_if_fail (track);
    etr = ((Track *) track)->userdata;
    g_return_if_fail (etr);
    C_FREE (etr->sha1_hash);
}

/**
 * Call sha1_free() for each itdb and delete sha1 checksums in all tracks.
 *
 */
void gp_sha1_free_hash(void) {
    GList *gl;
    struct itdbs_head *itdbs_head;

    itdbs_head = NULL;
    g_return_if_fail (gtkpod_app);
    itdbs_head = gp_get_itdbs_head();
    g_return_if_fail (itdbs_head);

    for (gl = itdbs_head->itdbs; gl; gl = gl->next) {
        iTunesDB *itdb = gl->data;
        g_return_if_fail (itdb);
        sha1_free(itdb);
        g_list_foreach(itdb->tracks, rm_sha1, NULL);
    }
}

/* This function removes a duplicate track "track" from memory while
 * preserving the playlists.
 *
 * The sha1 hash is not modified.
 *
 * The playcount/recent_playcount are modified to show the cumulative
 * playcounts for that track.
 *
 * The star rating is set to the average of both star ratings if both
 * ratings are not 0, or the higher rating if one of the ratings is 0
 * (it is assumed that a rating of "0" means that no rating has been
 * set).
 *
 * The "added" timestamp is set to the older entry (unless that one
 * is 0).
 *
 * The "modified" and "last played" timestamps are set to the more
 * recent entry.
 *
 * You should call "gp_duplicate_remove (NULL, NULL)" to pop up the info
 * dialogue with the list of duplicate tracks afterwards. Call with
 * "NULL, (void *)-1" to just clean up without dialoge.
 *
 * If "track" does not exist in
 * the master play list, only a message is logged (to be displayed
 * later when called with "NULL, NULL" */
void gp_duplicate_remove(Track *oldtrack, Track *track) {
    gchar *buf, *buf2;
    static gint deltrack_nr = 0;
    static gboolean removed = FALSE;
    static GString *str = NULL;

    /*   printf ("%p, %p, '%s'\n", oldtrack, track, str?str->str:"empty");*/

    if (prefs_get_int("show_duplicates") && !oldtrack && !track && str) {
        if (str->len) { /* Some tracks have been deleted. Print a notice */
            if (removed) {
                buf = g_strdup_printf(ngettext ("The following duplicate track has been removed.",
                        "The following %d duplicate tracks have been removed.",
                        deltrack_nr), deltrack_nr);
            }
            else {
                buf
                        = g_strdup_printf(ngettext ("The following duplicate track has not been added to the master play list.",
                                "The following %d duplicate tracks have not been added to the master play list.",
                                deltrack_nr), deltrack_nr);
            }
            gtkpod_confirmation
                (-1,                      /* gint id, */
                FALSE,                   /* gboolean modal, */
                _("Duplicate detection"),/* title */
                buf,                     /* label */
                str->str,                /* scrolled text */
                NULL, 0, NULL,      /* option 1 */
                NULL, 0, NULL,      /* option 2 */
                TRUE,               /* gboolean confirm_again, */
                "show_duplicates",
                /* ConfHandlerCA confirm_again_handler,*/
                CONF_NULL_HANDLER,  /* ConfHandler ok_handler,*/
                NULL,               /* don't show "Apply" button */
                NULL,               /* don't show "Cancel" button */
                NULL,               /* gpointer user_data1,*/
                NULL);              /* gpointer user_data2,*/
            g_free(buf);
        }
    }

    if (oldtrack == NULL) { /* clean up */
        if (str)
            g_string_free(str, TRUE);
        str = NULL;
        removed = FALSE;
        deltrack_nr = 0;
        gtkpod_tracks_statusbar_update();
    }

    if (oldtrack && track) {
        ExtraTrackData *oldetr = oldtrack->userdata;
        ExtraTrackData *etr = track->userdata;
        iTunesDB *itdb = oldtrack->itdb;
        g_return_if_fail (itdb);
        g_return_if_fail (oldetr);
        g_return_if_fail (etr);

        if (prefs_get_int("show_duplicates")) {
            /* add info about it to str */
            buf = get_track_info(track, TRUE);
            buf2 = get_track_info(oldtrack, TRUE);
            if (!str) {
                deltrack_nr = 0;
                str = g_string_sized_new(2000); /* used to keep record
                 * of duplicate
                 * tracks */
            }
            g_string_append_printf(str, "'%s': identical to '%s'\n", buf, buf2);
            g_free(buf);
            g_free(buf2);
        }
        /* Set playcount */
        oldtrack->playcount += track->playcount;
        oldtrack->recent_playcount += track->recent_playcount;
        /* Set rating */
        if (oldtrack->rating && track->rating)
            oldtrack->rating = floor((double) (oldtrack->rating + track->rating + ITDB_RATING_STEP) / (2
                    * ITDB_RATING_STEP)) * ITDB_RATING_STEP;
        else
            oldtrack->rating = MAX (oldtrack->rating, track->rating);
        /* Set 'modified' timestamp */
        oldtrack->time_modified = MAX (oldtrack->time_modified,
                track->time_modified);
        /* Set 'played' timestamp */
        oldtrack->time_played = MAX (oldtrack->time_played, track->time_played);
        /* Set 'added' timestamp */
        oldtrack->time_added = MIN (oldtrack->time_added, track->time_added);

        /* Update filename if new track has filename set (should be
         always!?) and old filename is not available or no longer
         valid */
        if (etr->pc_path_locale) {
            if (!oldetr->pc_path_locale || !g_file_test(oldetr->pc_path_locale, G_FILE_TEST_IS_REGULAR)) {
                g_free(oldetr->pc_path_locale);
                g_free(oldetr->pc_path_utf8);
                oldetr->pc_path_locale = g_strdup(etr->pc_path_locale);
                oldetr->pc_path_utf8 = g_strdup(etr->pc_path_utf8);
            }
        }
        if (itdb_playlist_contains_track(itdb_playlist_mpl(itdb), track)) { /* track is already added to memory -> replace with "oldtrack" */
            /* check for "track" in all playlists (except for MPL) */
            GList *gl;
            gl = g_list_nth(itdb->playlists, 1);
            while (gl) {
                Playlist *pl = gl->data;
                g_return_if_fail (pl);
                /* if "track" is in playlist pl, we remove it and add
                 the "oldtrack" instead (this way round we don't have
                 to worry about changing sha1 hash entries */
                if (itdb_playlist_contains_track(pl, track)) {
                    gp_playlist_remove_track(pl, track, DELETE_ACTION_PLAYLIST);
                    if (!itdb_playlist_contains_track(pl, oldtrack))
                        gp_playlist_add_track(pl, oldtrack, TRUE);
                }
                gl = gl->next;
            }
            /* remove track from MPL, i.e. from the ipod (or the local
             * database */
            if (itdb->usertype & GP_ITDB_TYPE_IPOD) {
                gp_playlist_remove_track(NULL, track, DELETE_ACTION_IPOD);
            }
            if (itdb->usertype & GP_ITDB_TYPE_LOCAL) {
                gp_playlist_remove_track(NULL, track, DELETE_ACTION_DATABASE);
            }
            removed = TRUE;
        }
        ++deltrack_nr; /* count duplicate tracks */
        data_changed(itdb);
    }
}

/**
 * Register all tracks in the sha1 hash and remove duplicates (while
 * preserving playlists).
 * Call  gp_duplicate_remove (NULL, NULL); to show an info dialogue
 */
void gp_itdb_hash(iTunesDB *itdb) {
    gint ns, track_nr;
    Track *track, *oldtrack;

    g_return_if_fail (itdb);

    if (!prefs_get_int("sha1"))
        return;

    ns = itdb_tracks_number(itdb);
    if (ns == 0)
        return;

    block_widgets(); /* block widgets -- this might take a while,
     so we'll do refreshs */
    sha1_free(itdb); /* release sha1 hash */
    track_nr = 0;
    /* populate the hash table */
    while ((track = g_list_nth_data(itdb->tracks, track_nr))) {
        oldtrack = sha1_track_exists_insert(itdb, track);
        /*        printf("%d:%d:%p:%p\n", count, track_nr, track, oldtrack); */
        if (oldtrack) {
            gp_duplicate_remove(oldtrack, track);
        }
        else { /* if we removed a track (above), we don't need to increment
         the track_nr here */
            ++track_nr;
        }
    }
    release_widgets(); /* release widgets again */
}

/* ------------------------------------------------------------ *\
|                                                                |
 |         functions to locate tracks                             |
 |                                                                |
 \* ------------------------------------------------------------ */

/* Returns the track with the filename @name or NULL, if none can be
 * found. This function also works if @filename is on the iPod. */
Track *gp_track_by_filename(iTunesDB *itdb, gchar *filename) {
    gchar *musicdir = NULL;
    Track *result = NULL;

    g_return_val_if_fail (itdb, NULL);
    g_return_val_if_fail (filename, NULL);

    if (itdb->usertype & GP_ITDB_TYPE_IPOD) {
        gchar *mountpoint = get_itdb_prefs_string(itdb, KEY_MOUNTPOINT);
        g_return_val_if_fail (mountpoint, NULL);
        musicdir = itdb_get_music_dir(mountpoint);
        if (!musicdir) {
            /* FIXME: guess */
            musicdir = g_build_filename(mountpoint, "iPod_Control", "Music", NULL);
        }
        g_free(mountpoint);
    }
    if ((itdb->usertype & GP_ITDB_TYPE_IPOD) && (musicdir != NULL) && (strncmp(filename, musicdir, strlen(musicdir))
            == 0)) { /* handle track on iPod (in music dir) */
        GList *gl;
        for (gl = itdb->tracks; gl && !result; gl = gl->next) {
            Track *track = gl->data;
            gchar *ipod_path;
            g_return_val_if_fail (track, NULL);
            ipod_path = itdb_filename_on_ipod(track);
            if (ipod_path) {
                if (strcasecmp(ipod_path, filename) == 0) {
                    result = track;
                }
                g_free(ipod_path);
            }
        }
    }
    else { /* handle track on local filesystem */
        GList *gl;
        for (gl = itdb->tracks; gl && !result; gl = gl->next) {
            Track *track = gl->data;
            ExtraTrackData *etr;
            g_return_val_if_fail (track, NULL);
            etr = track->userdata;
            g_return_val_if_fail (etr, NULL);
            if (etr->pc_path_locale) {
                if (strcmp(etr->pc_path_locale, filename) == 0)
                    result = track;
            }
        }
    }
    g_free(musicdir);
    return result;
}

/* Find @track in repository @itdb by the following methods and return
 the matches:

 1) DND origin data
 2) filename matches
 3) SHA1 match

 If DND origin data is available and valid only one track is
 returned. Otherwise all tracks matching the filename and the SHA1
 are returned.

 If DND origin data is found to be invalid it is deleted.

 Return value: a GList with matching tracks. You must call
 g_list_free() on the list when it is no longer needed.
 */
GList *gp_itdb_find_same_tracks(iTunesDB *itdb, Track *track) {
    ExtraTrackData *etr;
    Track *itr;
    GList *tracks = NULL;

    g_return_val_if_fail (itdb, NULL);
    g_return_val_if_fail (track, NULL);

    etr = track->userdata;
    g_return_val_if_fail (etr, NULL);

    if (itdb->id == etr->local_itdb_id) { /* we can probably find the original track from the DND data */
        GList *gl;
        for (gl = itdb->tracks; gl; gl = gl->next) {
            itr = gl->data;
            g_return_val_if_fail (itr, NULL);
            if (itr->dbid == etr->local_track_dbid) { /* found track */
                tracks = g_list_prepend(tracks, itr);
                return tracks;
            }
        }
        /* DND origin data is no longer valid */
        etr->local_itdb_id = 0;
        etr->local_track_dbid = 0;
    }

    /* No luck so far -- let's get filename matches */
    tracks = gp_itdb_pc_path_hash_find_tracks(itdb, etr->pc_path_utf8);

    /* And also try SHA1 match */
    itr = sha1_sha1_exists(itdb, etr->sha1_hash);

    if (itr) { /* insert into tracks list if not already present */
        if (!g_list_find(tracks, itr)) {
            tracks = g_list_prepend(tracks, itr);
        }
    }

    return tracks;
}

/* Find @track in all local repositories and return a list.

 This function calls gp_itdb_find_same_tracks() for each local
 repository and concatenates the results into one list.

 Return value: a GList with matching tracks. You must call
 g_list_free() on the list when it is no longer needed.
 */
GList *gp_itdb_find_same_tracks_in_local_itdbs(Track *track) {
    GList *gl, *tracks = NULL;
    struct itdbs_head *ih = gp_get_itdbs_head();

    g_return_val_if_fail (ih, NULL);
    g_return_val_if_fail (track, NULL);

    for (gl = ih->itdbs; gl; gl = gl->next) {
        iTunesDB *itdb = gl->data;
        g_return_val_if_fail (itdb, tracks);
        if (itdb->usertype & GP_ITDB_TYPE_LOCAL) {
            GList *addtracks = gp_itdb_find_same_tracks(itdb, track);
            tracks = g_list_concat(tracks, addtracks);
        }
    }
    return tracks;
}

/* Find @track in all repositories (local and iPod) and return a list.

 This function calls gp_itdb_find_same_tracks() for each
 repository and concatenates the results into one list.

 Return value: a GList with matching tracks. You must call
 g_list_free() on the list when it is no longer needed.
 */
GList *gp_itdb_find_same_tracks_in_itdbs(Track *track) {
    GList *gl, *tracks = NULL;
    struct itdbs_head *ih = gp_get_itdbs_head();

    g_return_val_if_fail (ih, NULL);
    g_return_val_if_fail (track, NULL);

    for (gl = ih->itdbs; gl; gl = gl->next) {
        GList *addtracks;
        iTunesDB *itdb = gl->data;
        g_return_val_if_fail (itdb, tracks);
        addtracks = gp_itdb_find_same_tracks(itdb, track);
        tracks = g_list_concat(tracks, addtracks);
    }
    return tracks;
}

/* ------------------------------------------------------------ *\
|                                                                |
 |         functions for local path hashtable                     |
 |                                                                |
 \* ------------------------------------------------------------ */

/* set up hash table for local filenames */
void gp_itdb_pc_path_hash_init(ExtraiTunesDBData *eitdb) {
    g_return_if_fail (eitdb);

    if (!eitdb->pc_path_hash) {
        eitdb->pc_path_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    }
}

/* function for destroying hash value, used when destroying the hash
 * table in gp_itdb_local_path_hash_destroy() below. */
static void pc_path_hash_free_value(gpointer key, gpointer value, gpointer userdata) {
    g_list_free(value);
}

/* free all memory associated with the local_path_hash */
void gp_itdb_pc_path_hash_destroy(ExtraiTunesDBData *eitdb) {
    g_return_if_fail (eitdb);

    if (eitdb->pc_path_hash) {
        g_hash_table_foreach(eitdb->pc_path_hash, pc_path_hash_free_value, NULL);
        g_hash_table_destroy(eitdb->pc_path_hash);
        eitdb->pc_path_hash = NULL;
    }
}

/* Add track to filehash. This function must only be called once for
 * each track. */
void gp_itdb_pc_path_hash_add_track(Track *track) {
    iTunesDB *itdb;
    ExtraTrackData *etr;
    ExtraiTunesDBData *eitdb;

    g_return_if_fail (track);
    etr = track->userdata;
    g_return_if_fail (etr);

    itdb = track->itdb;
    g_return_if_fail (itdb);

    eitdb = itdb->userdata;
    g_return_if_fail (eitdb);
    g_return_if_fail (eitdb->pc_path_hash);

    /* This is only to detect programming errors */
    g_return_if_fail (!etr->pc_path_hashed);

    if (etr->pc_path_utf8 && *etr->pc_path_utf8) { /* add to hash table */
        GList *tracks;
        tracks = g_hash_table_lookup(eitdb->pc_path_hash, etr->pc_path_utf8);
        tracks = g_list_prepend(tracks, track);
        g_hash_table_replace(eitdb->pc_path_hash, g_strdup(etr->pc_path_utf8), tracks);
        /* This is only to detect programming errors */
        etr->pc_path_hashed = TRUE;
    }
}

/* used in the next two functions */
struct pc_path_hash_find_track_data {
    Track *track;
    gchar *key;
};

/* Used in the next function. Return TRUE and the current key if
 td->track is contained in the list for key */
static gboolean pc_path_hash_find_track(gpointer key, gpointer value, gpointer user_data) {
    GList *tracks = value;
    struct pc_path_hash_find_track_data *td = user_data;

    if (g_list_find(tracks, td->track)) {
        td->key = g_strdup(key);
        return TRUE;
    }
    return FALSE;
}

/* remove track from filehash */
void gp_itdb_pc_path_hash_remove_track(Track *track) {
    struct pc_path_hash_find_track_data td;
    ExtraTrackData *etr;
    iTunesDB *itdb;
    ExtraiTunesDBData *eitdb;
    GList *tracks;

    g_return_if_fail (track);
    etr = track->userdata;
    g_return_if_fail (etr);

    itdb = track->itdb;
    g_return_if_fail (itdb);

    eitdb = itdb->userdata;
    g_return_if_fail (eitdb);
    g_return_if_fail (eitdb->pc_path_hash);

    if (!etr->pc_path_hashed)
        return;

    if (etr->pc_path_utf8 && *etr->pc_path_utf8) { /* try lookup with filename */
        GList *tracks;
        tracks = g_hash_table_lookup(eitdb->pc_path_hash, etr->pc_path_utf8);
        if (tracks) { /* filename exists */
            GList *link = g_list_find(tracks, track);
            if (link) { /* track found */
                tracks = g_list_remove_link(tracks, link);
                if (tracks) { /* still tracks left under this filename */
                    g_hash_table_replace(eitdb->pc_path_hash, g_strdup(etr->pc_path_utf8), tracks);
                }
                else { /* no more tracks left under this filename */
                    g_hash_table_remove(eitdb->pc_path_hash, etr->pc_path_utf8);
                }
                etr->pc_path_hashed = FALSE;
                return;
            }
        }
    }

    /* We didn't find the track by filename, or now filename is
     * available any more) -> search through the list */
    td.track = track;
    td.key = NULL;
    tracks = g_hash_table_find(eitdb->pc_path_hash, pc_path_hash_find_track, &td);
    if (tracks) {
        tracks = g_list_remove(tracks, track);
        if (tracks) { /* still tracks left under this filename */
            g_hash_table_replace(eitdb->pc_path_hash, g_strdup(td.key), tracks);
        }
        else { /* no more tracks left under this filename */
            g_hash_table_remove(eitdb->pc_path_hash, td.key);
        }
        etr->pc_path_hashed = FALSE;
        g_free(td.key);
    }
}

/* Return all tracks with @filename (@filename in UTF8).
 You must g_list_free the returned list when it is not longer needed. */
GList *gp_itdb_pc_path_hash_find_tracks(iTunesDB *itdb, const gchar *filename) {
    ExtraiTunesDBData *eitdb;
    GList *tracks = NULL;

    g_return_val_if_fail (itdb, NULL);
    eitdb = itdb->userdata;
    g_return_val_if_fail (eitdb, NULL);
    g_return_val_if_fail (eitdb->pc_path_hash, NULL);

    if (filename && *filename)
        tracks = g_hash_table_lookup(eitdb->pc_path_hash, filename);

    return g_list_copy(tracks);
}

/* ------------------------------------------------------------ *\
|                                                                |
 |         functions to retrieve information from tracks          |
 |                                                                |
 \* ------------------------------------------------------------ */

/* return the address of the UTF8 field @t_item. @t_item is one of
 * (the applicable) T_* defined in track.h */
gchar **track_get_item_pointer(Track *track, T_item t_item) {
    gchar **result = NULL;
    ExtraTrackData *etr;

    g_return_val_if_fail (track, NULL);
    etr = track->userdata;
    g_return_val_if_fail (etr, NULL);

    switch (t_item) {
    case T_ALBUM:
        result = &track->album;
        break;
    case T_ARTIST:
        result = &track->artist;
        break;
    case T_TITLE:
        result = &track->title;
        break;
    case T_GENRE:
        result = &track->genre;
        break;
    case T_COMMENT:
        result = &track->comment;
        break;
    case T_COMPOSER:
        result = &track->composer;
        break;
    case T_FILETYPE:
        result = &track->filetype;
        break;
    case T_IPOD_PATH:
        result = &track->ipod_path;
        break;
    case T_PC_PATH:
        result = &etr->pc_path_utf8;
        break;
    case T_YEAR:
        result = &etr->year_str;
        break;
    case T_GROUPING:
        result = &track->grouping;
        break;
    case T_CATEGORY:
        result = &track->category;
        break;
    case T_DESCRIPTION:
        result = &track->description;
        break;
    case T_PODCASTURL:
        result = &track->podcasturl;
        break;
    case T_PODCASTRSS:
        result = &track->podcastrss;
        break;
    case T_SUBTITLE:
        result = &track->subtitle;
        break;
    case T_THUMB_PATH:
        result = &etr->thumb_path_utf8;
        break;
    case T_TV_SHOW:
        result = &track->tvshow;
        break;
    case T_TV_EPISODE:
        result = &track->tvepisode;
        break;
    case T_TV_NETWORK:
        result = &track->tvnetwork;
        break;
    case T_ALBUMARTIST:
        result = &track->albumartist;
        break;
    case T_SORT_ARTIST:
        result = &track->sort_artist;
        break;
    case T_SORT_TITLE:
        result = &track->sort_title;
        break;
    case T_SORT_ALBUM:
        result = &track->sort_album;
        break;
    case T_SORT_ALBUMARTIST:
        result = &track->sort_albumartist;
        break;
    case T_SORT_COMPOSER:
        result = &track->sort_composer;
        break;
    case T_SORT_TVSHOW:
        result = &track->sort_tvshow;
        break;
    case T_LYRICS:
        result = &etr->lyrics;
        break;
    case T_ALL:
    case T_IPOD_ID:
    case T_TRACK_NR:
    case T_TRANSFERRED:
    case T_SIZE:
    case T_TRACKLEN:
    case T_STARTTIME:
    case T_STOPTIME:
    case T_BITRATE:
    case T_SAMPLERATE:
    case T_BPM:
    case T_PLAYCOUNT:
    case T_RATING:
    case T_TIME_ADDED:
    case T_TIME_PLAYED:
    case T_TIME_MODIFIED:
    case T_TIME_RELEASED:
    case T_VOLUME:
    case T_SOUNDCHECK:
    case T_CD_NR:
    case T_COMPILATION:
    case T_REMEMBER_PLAYBACK_POSITION:
    case T_SKIP_WHEN_SHUFFLING:
    case T_CHECKED:
    case T_MEDIA_TYPE:
    case T_SEASON_NR:
    case T_EPISODE_NR:
    case T_GAPLESS_TRACK_FLAG:
    case T_ITEM_NUM:
        g_return_val_if_reached (NULL);
    }
    return result;
}

/* return the UTF8 item @t_item. @t_item is one of
 (the applicable) T_* defined in track.h */
const gchar *track_get_item(Track *track, T_item t_item) {
    gchar **ptr;

    g_return_val_if_fail (track, NULL);

    ptr = track_get_item_pointer(track, t_item);

    if (ptr)
        return *ptr;
    else
        return NULL;
}

/* Copy item @item from @frtrack to @totrack.
 Return value:
 TRUE: @totrack was changed
 FALSE: @totrack is unchanged
 */
gboolean track_copy_item(Track *frtrack, Track *totrack, T_item item) {
    gboolean changed = FALSE;
    const gchar *fritem;
    gchar **toitem_ptr;
    ExtraTrackData *efrtr, *etotr;

    g_return_val_if_fail (frtrack, FALSE);
    g_return_val_if_fail (totrack, FALSE);
    g_return_val_if_fail ((item > 0) && (item < T_ITEM_NUM), FALSE);

    efrtr = frtrack->userdata;
    etotr = totrack->userdata;
    g_return_val_if_fail (efrtr, FALSE);
    g_return_val_if_fail (etotr, FALSE);

    if (frtrack == totrack)
        return FALSE;

    switch (item) {
    case T_ALBUM:
    case T_ARTIST:
    case T_TITLE:
    case T_GENRE:
    case T_COMMENT:
    case T_COMPOSER:
    case T_FILETYPE:
    case T_IPOD_PATH:
    case T_PC_PATH:
    case T_YEAR:
    case T_GROUPING:
    case T_CATEGORY:
    case T_DESCRIPTION:
    case T_PODCASTURL:
    case T_PODCASTRSS:
    case T_SUBTITLE:
    case T_THUMB_PATH:
    case T_TV_SHOW:
    case T_TV_EPISODE:
    case T_TV_NETWORK:
    case T_ALBUMARTIST:
    case T_SORT_ARTIST:
    case T_SORT_TITLE:
    case T_SORT_ALBUM:
    case T_SORT_ALBUMARTIST:
    case T_SORT_COMPOSER:
    case T_SORT_TVSHOW:
    case T_LYRICS:
        fritem = track_get_item(frtrack, item);
        toitem_ptr = track_get_item_pointer(totrack, item);
        g_return_val_if_fail (fritem, FALSE);
        g_return_val_if_fail (toitem_ptr, FALSE);
        if ((*toitem_ptr == NULL) || (strcmp(fritem, *toitem_ptr) != 0)) {
            g_free(*toitem_ptr);
            *toitem_ptr = g_strdup(fritem);
            changed = TRUE;
        }
        if (item == T_YEAR) {
            if (totrack->year != frtrack->year) {
                totrack->year = frtrack->year;
                changed = TRUE;
            }
        }
        else if ((changed) && (item == T_LYRICS)) {
            write_lyrics_to_file(totrack);
        }
        /* handle items that have two entries */
        if (item == T_PC_PATH) {
            if ((etotr->pc_path_locale == NULL) || (strcmp(efrtr->pc_path_locale, etotr->pc_path_locale) != 0)) {
                g_free(etotr->pc_path_locale);
                etotr->pc_path_locale = g_strdup(efrtr->pc_path_locale);
                changed = TRUE;
            }
        }
        if (item == T_THUMB_PATH) {
            if ((etotr->thumb_path_locale == NULL) || (strcmp(efrtr->thumb_path_locale, etotr->thumb_path_locale) != 0)) {
                g_free(etotr->thumb_path_locale);
                etotr->thumb_path_locale = g_strdup(efrtr->thumb_path_locale);
                changed = TRUE;
            }
        }
        break;
    case T_IPOD_ID:
        if (frtrack->id != totrack->id) {
            totrack->id = frtrack->id;
            changed = TRUE;
        }
        break;
    case T_TRACK_NR:
        if (frtrack->track_nr != totrack->track_nr) {
            totrack->track_nr = frtrack->track_nr;
            changed = TRUE;
        }
        if (frtrack->tracks != totrack->tracks) {
            totrack->tracks = frtrack->tracks;
            changed = TRUE;
        }
        break;
    case T_TRANSFERRED:
        if (frtrack->transferred != totrack->transferred) {
            totrack->transferred = frtrack->transferred;
            changed = TRUE;
        }
        break;
    case T_SIZE:
        if (frtrack->size != totrack->size) {
            totrack->size = frtrack->size;
            changed = TRUE;
        }
        break;
    case T_TRACKLEN:
        if (frtrack->tracklen != totrack->tracklen) {
            totrack->tracklen = frtrack->tracklen;
            changed = TRUE;
        }
        break;
    case T_STARTTIME:
        if (frtrack->starttime != totrack->starttime) {
            totrack->starttime = frtrack->starttime;
            changed = TRUE;
        }
        break;
    case T_STOPTIME:
        if (frtrack->stoptime != totrack->stoptime) {
            totrack->stoptime = frtrack->stoptime;
            changed = TRUE;
        }
        break;
    case T_BITRATE:
        if (frtrack->bitrate != totrack->bitrate) {
            totrack->bitrate = frtrack->bitrate;
            changed = TRUE;
        }
        break;
    case T_SAMPLERATE:
        if (frtrack->samplerate != totrack->samplerate) {
            totrack->samplerate = frtrack->samplerate;
            changed = TRUE;
        }
        break;
    case T_BPM:
        if (frtrack->BPM != totrack->BPM) {
            totrack->BPM = frtrack->BPM;
            changed = TRUE;
        }
        break;
    case T_PLAYCOUNT:
        if (frtrack->playcount != totrack->playcount) {
            totrack->playcount = frtrack->playcount;
            changed = TRUE;
        }
        break;
    case T_RATING:
        if (frtrack->rating != totrack->rating) {
            totrack->rating = frtrack->rating;
            changed = TRUE;
        }
        break;
    case T_TIME_ADDED:
    case T_TIME_PLAYED:
    case T_TIME_MODIFIED:
    case T_TIME_RELEASED:
        if (time_get_time(frtrack, item) != time_get_time(totrack, item)) {
            time_set_time(totrack, time_get_time(frtrack, item), item);
            changed = TRUE;
        }
        break;
    case T_VOLUME:
        if (frtrack->volume != totrack->volume) {
            totrack->volume = frtrack->volume;
            changed = TRUE;
        }
        break;
    case T_SOUNDCHECK:
        if (frtrack->soundcheck != totrack->soundcheck) {
            totrack->soundcheck = frtrack->soundcheck;
            changed = TRUE;
        }
        break;
    case T_CD_NR:
        if (frtrack->cd_nr != totrack->cd_nr) {
            totrack->cd_nr = frtrack->cd_nr;
            changed = TRUE;
        }
        if (frtrack->cds != totrack->cds) {
            totrack->cds = frtrack->cds;
            changed = TRUE;
        }
        break;
    case T_COMPILATION:
        if (frtrack->compilation != totrack->compilation) {
            totrack->compilation = frtrack->compilation;
            changed = TRUE;
        }
        break;
    case T_REMEMBER_PLAYBACK_POSITION:
        if (frtrack->remember_playback_position != totrack->remember_playback_position) {
            totrack->remember_playback_position = frtrack->remember_playback_position;
            changed = TRUE;
        }
        break;
    case T_SKIP_WHEN_SHUFFLING:
        if (frtrack->skip_when_shuffling != totrack->skip_when_shuffling) {
            totrack->skip_when_shuffling = frtrack->skip_when_shuffling;
            changed = TRUE;
        }
        break;
    case T_CHECKED:
        if (frtrack->checked != totrack->checked) {
            totrack->checked = frtrack->checked;
            changed = TRUE;
        }
        break;
    case T_MEDIA_TYPE:
        if (frtrack->mediatype != totrack->mediatype) {
            totrack->mediatype = frtrack->mediatype;
            changed = TRUE;
        }
        break;
    case T_SEASON_NR:
        if (frtrack->season_nr != totrack->season_nr) {
            totrack->season_nr = frtrack->season_nr;
            changed = TRUE;
        }
        break;
    case T_EPISODE_NR:
        if (frtrack->episode_nr != totrack->episode_nr) {
            totrack->episode_nr = frtrack->episode_nr;
            changed = TRUE;
        }
        break;
    case T_GAPLESS_TRACK_FLAG:
        if (frtrack->gapless_track_flag != totrack->gapless_track_flag) {
            totrack->gapless_track_flag = frtrack->gapless_track_flag;
            changed = TRUE;
        }
        break;
    case T_ITEM_NUM:
    case T_ALL:
        g_return_val_if_reached (FALSE);

    }
    return changed;
}

/* return a pointer to the specified timestamp. @t_item is one of (the
 applicable) T_* defined in track.h.  If the parameters are illegal,
 "0" is returned. */
time_t *track_get_timestamp_ptr(Track *track, T_item t_item) {
    g_return_val_if_fail (track, NULL);

    switch (t_item) {
    case T_TIME_PLAYED:
        return &track->time_played;
    case T_TIME_MODIFIED:
        return &track->time_modified;
    case T_TIME_RELEASED:
        return &track->time_released;
    case T_TIME_ADDED:
        return &track->time_added;
    default:
        g_return_val_if_reached (0);
    }
}

/* return the specified timestamp. @t_item is one of
 (the * applicable) T_* defined in track.h. If the parameters are
 illegal, "0" is returned. */
time_t track_get_timestamp(Track *track, T_item t_item) {
    time_t *ptr;
    g_return_val_if_fail (track, 0);

    ptr = track_get_timestamp_ptr(track, t_item);
    if (ptr)
        return *ptr;
    else
        return 0;
}

/* unified format for TRACKLEN, STARTTIME, STOPTIME */
static gchar *track_get_length_string(gint32 length) {
    /* Translators: this is minutes:seconds.thousandths */
    return g_strdup_printf(_("%d:%06.3f"), length / 60000, ((float) (length % 60000)) / 1000);
}

/* Return text for display. g_free() after use. */
gchar *track_get_text(Track *track, T_item item) {
    gchar *text = NULL;
    ExtraTrackData *etr;
    iTunesDB *itdb;

    g_return_val_if_fail ((item > 0) && (item < T_ITEM_NUM), NULL);
    g_return_val_if_fail (track, NULL);
    etr = track->userdata;
    g_return_val_if_fail (etr, NULL);
    itdb = track->itdb;
    g_return_val_if_fail (itdb, NULL);

    switch (item) {
    case T_TITLE:
        text = g_strdup(track->title);
        break;
    case T_ARTIST:
        text = g_strdup(track->artist);
        break;
    case T_ALBUM:
        text = g_strdup(track->album);
        break;
    case T_GENRE:
        text = g_strdup(track->genre);
        break;
    case T_COMPOSER:
        text = g_strdup(track->composer);
        break;
    case T_COMMENT:
        text = g_strdup(track->comment);
        break;
    case T_FILETYPE:
        text = g_strdup(track->filetype);
        break;
    case T_GROUPING:
        text = g_strdup(track->grouping);
        break;
    case T_CATEGORY:
        text = g_strdup(track->category);
        break;
    case T_DESCRIPTION:
        text = g_strdup(track->description);
        break;
    case T_PODCASTURL:
        text = g_strdup(track->podcasturl);
        break;
    case T_PODCASTRSS:
        text = g_strdup(track->podcastrss);
        break;
    case T_SUBTITLE:
        text = g_strdup(track->subtitle);
        break;
    case T_TV_SHOW:
        text = g_strdup(track->tvshow);
        break;
    case T_TV_EPISODE:
        text = g_strdup(track->tvepisode);
        break;
    case T_TV_NETWORK:
        text = g_strdup(track->tvnetwork);
        break;
    case T_ALBUMARTIST:
        text = g_strdup(track->albumartist);
        break;
    case T_SORT_ARTIST:
        text = g_strdup(track->sort_artist);
        break;
    case T_SORT_TITLE:
        text = g_strdup(track->sort_title);
        break;
    case T_SORT_ALBUM:
        text = g_strdup(track->sort_album);
        break;
    case T_SORT_ALBUMARTIST:
        text = g_strdup(track->sort_albumartist);
        break;
    case T_SORT_COMPOSER:
        text = g_strdup(track->sort_composer);
        break;
    case T_SORT_TVSHOW:
        text = g_strdup(track->sort_tvshow);
        break;
    case T_TRACK_NR:
        if (track->tracks == 0)
            text = g_strdup_printf("%d", track->track_nr);
        else
            text = g_strdup_printf(_("%d/%d"), track->track_nr, track->tracks);
        break;
    case T_CD_NR:
        if (track->cds == 0)
            text = g_strdup_printf("%d", track->cd_nr);
        else
            text = g_strdup_printf(_("%d/%d"), track->cd_nr, track->cds);
        break;
    case T_IPOD_ID:
        if (track->id != -1)
            text = g_strdup_printf("%d", track->id);
        else
            text = g_strdup(_("n/a"));
        break;
    case T_PC_PATH:
        text = g_strdup(etr->pc_path_utf8);
        break;
    case T_IPOD_PATH:
        if (itdb->usertype & GP_ITDB_TYPE_IPOD) {
            text = g_strdup(track->ipod_path);
        }
        if (itdb->usertype & GP_ITDB_TYPE_LOCAL) {
            text = g_strdup(_("Local Database"));
        }
        break;
    case T_THUMB_PATH:
        text = g_strdup(etr->thumb_path_utf8);
        if (!text || (strlen(text) == 0)) { /* no path set */
            g_free(text);
            text = NULL;
            if (itdb_track_has_thumbnails(track)) { /* artwork is set */
                text = g_strdup(_("Embedded or filename was lost"));
            }
            else {
                text = g_strdup(_("Artwork not set"));
            }
        }
        break;
    case T_SIZE:
        text = g_strdup_printf("%d", track->size);
        break;
    case T_TRACKLEN:
        text = track_get_length_string(track->tracklen);
        break;
    case T_STARTTIME:
        text = track_get_length_string(track->starttime);
        break;
    case T_STOPTIME:
        if (track->stoptime == 0)
            text = track_get_length_string(track->tracklen);
        else
            text = track_get_length_string(track->stoptime);
        break;
    case T_BITRATE:
        text = g_strdup_printf("%dk", track->bitrate);
        break;
    case T_SAMPLERATE:
        text = g_strdup_printf("%d", track->samplerate);
        break;
    case T_BPM:
        text = g_strdup_printf("%d", track->BPM);
        break;
    case T_PLAYCOUNT:
        text = g_strdup_printf("%d", track->playcount);
        break;
    case T_YEAR:
        text = g_strdup_printf("%d", track->year);
        break;
    case T_RATING:
        text = g_strdup_printf("%d", track->rating / ITDB_RATING_STEP);
        break;
    case T_TIME_PLAYED:
    case T_TIME_MODIFIED:
    case T_TIME_ADDED:
    case T_TIME_RELEASED:
        text = time_field_to_string(track, item);
        break;
    case T_VOLUME:
        text = g_strdup_printf("%d", track->volume);
        break;
    case T_SOUNDCHECK:
        text = g_strdup_printf("%0.2f", soundcheck_to_replaygain(track->soundcheck));
        break;
    case T_SEASON_NR:
        text = g_strdup_printf("%d", track->season_nr);
        break;
    case T_EPISODE_NR:
        text = g_strdup_printf("%d", track->episode_nr);
        break;
    case T_MEDIA_TYPE:
        text = g_strdup_printf("%#.8x", track->mediatype);
        break;
    case T_TRANSFERRED:
    case T_COMPILATION:
    case T_REMEMBER_PLAYBACK_POSITION:
    case T_SKIP_WHEN_SHUFFLING:
    case T_ALL:
    case T_CHECKED:
    case T_ITEM_NUM:
    case T_GAPLESS_TRACK_FLAG:
        break;
    case T_LYRICS:
        read_lyrics_from_file(track, &text);
        break;
    }
    return text;
}

/* unified scanner for TRACKLEN, STARTTIME, STOPTIME */
static gint32 track_scan_length(const gchar *new_text) {
    gint32 nr;
    const gchar *str;

    g_return_val_if_fail (new_text, 0);

    str = strrchr(new_text, ':');
    if (str) { /* MM:SS */
        /* A simple cast to gint32 can sometimes produce a number
         that's "1" too small (14.9999999999 -> 14 instead of 15) ->
         add 0.1 */
        nr = 1000 * (((gdouble) (60 * atoi(new_text))) + atof(str + 1)) + 0.1;
    }
    else { /* SS */
        nr = 1000 * atof(new_text) + 0.1;
    }

    return nr;
}

/* Set track data according to @new_text

 Return value: TRUE, if the track data was modified, FALSE otherwise
 */
gboolean track_set_text(Track *track, const gchar *new_text, T_item item) {
    gboolean changed = FALSE;
    gchar **itemp_utf8;
    const gchar *str;
    gchar *tempstr;
    ExtraTrackData *etr;
    gint32 nr;
    time_t t;

    g_return_val_if_fail (track, FALSE);
    g_return_val_if_fail (new_text, FALSE);

    etr = track->userdata;
    g_return_val_if_fail (etr, FALSE);

    switch (item) {
    case T_TITLE:
    case T_ALBUM:
    case T_ARTIST:
    case T_GENRE:
    case T_COMPOSER:
    case T_COMMENT:
    case T_FILETYPE:
    case T_GROUPING:
    case T_CATEGORY:
    case T_DESCRIPTION:
    case T_PODCASTURL:
    case T_PODCASTRSS:
    case T_SUBTITLE:
    case T_TV_SHOW:
    case T_TV_EPISODE:
    case T_TV_NETWORK:
    case T_ALBUMARTIST:
    case T_SORT_ARTIST:
    case T_SORT_TITLE:
    case T_SORT_ALBUM:
    case T_SORT_ALBUMARTIST:
    case T_SORT_COMPOSER:
    case T_SORT_TVSHOW:
        itemp_utf8 = track_get_item_pointer(track, item);
        if (g_utf8_collate(*itemp_utf8, new_text) != 0) {
            g_free(*itemp_utf8);
            *itemp_utf8 = g_strdup(new_text);
            changed = TRUE;
        }
        break;
    case T_LYRICS:
        /*
         * If lyrics string starts with an 'Error' then the track type is unsuitable
         * or the path could not be understood. Either way, not a reason to flag
         * the track as changed
         */
        if (!g_str_has_prefix(new_text, "Error") && !g_str_equal(etr->lyrics, new_text)) {
            g_free(etr->lyrics);
            etr->lyrics = g_strdup(new_text);
            changed = TRUE;
        }
        break;
    case T_TRACK_NR:
        nr = atoi(new_text);
        if ((nr >= 0) && (nr != track->track_nr)) {
            track->track_nr = nr;
            changed = TRUE;
        }
        str = strrchr(new_text, '/');
        if (str) {
            nr = atoi(str + 1);
            if ((nr >= 0) && (nr != track->tracks)) {
                track->tracks = nr;
                changed = TRUE;
            }
        }
        break;
    case T_CD_NR:
        nr = atoi(new_text);
        if ((nr >= 0) && (nr != track->cd_nr)) {
            track->cd_nr = nr;
            changed = TRUE;
        }
        str = strrchr(new_text, '/');
        if (str) {
            nr = atoi(str + 1);
            if ((nr >= 0) && (nr != track->cds)) {
                track->cds = nr;
                changed = TRUE;
            }
        }
        break;
    case T_YEAR:
        nr = atoi(new_text);
        if ((nr >= 0) && (nr != track->year)) {
            g_free(etr->year_str);
            etr->year_str = g_strdup_printf("%d", nr);
            track->year = nr;
            changed = TRUE;
        }
        break;
    case T_PLAYCOUNT:
        nr = atoi(new_text);
        if ((nr >= 0) && (nr != track->playcount)) {
            track->playcount = nr;
            changed = TRUE;
        }
        break;
    case T_RATING:
        nr = atoi(new_text);
        if ((nr >= 0) && (nr <= 5) && (nr != track->rating)) {
            track->rating = nr * ITDB_RATING_STEP;
            changed = TRUE;
        }
        break;
    case T_TIME_ADDED:
    case T_TIME_PLAYED:
    case T_TIME_MODIFIED:
    case T_TIME_RELEASED:
        t = time_string_to_time(new_text);
        tempstr = time_field_to_string(track, item);
        /*
         * Cannot compare time_t values directly since the conversion
         * to text is only accurate to seconds while time_t has millisecond
         * accuracy.
         */
        if ((t != -1) && (!g_str_equal(new_text, tempstr))) {
            time_set_time(track, t, item);
            changed = TRUE;
        }
        g_free(tempstr);
        break;
    case T_VOLUME:
        nr = atoi(new_text);
        if (nr != track->volume) {
            track->volume = nr;
            changed = TRUE;
        }
        break;
    case T_SOUNDCHECK:
        nr = replaygain_to_soundcheck(atof(new_text));
        /* 	printf("%d : %f\n", nr, atof (new_text)); */
        if (nr != track->soundcheck) {
            track->soundcheck = nr;
            changed = TRUE;
        }
        break;
    case T_SIZE:
        nr = atoi(new_text);
        if (nr != track->size) {
            track->size = nr;
            changed = TRUE;
        }
        break;
    case T_BITRATE:
        nr = atoi(new_text);
        if (nr != track->bitrate) {
            track->bitrate = nr;
            changed = TRUE;
        }
        break;
    case T_SAMPLERATE:
        nr = atoi(new_text);
        if (nr != track->samplerate) {
            track->samplerate = nr;
            changed = TRUE;
        }
        break;
    case T_BPM:
        nr = atoi(new_text);
        if (nr != track->BPM) {
            track->BPM = nr;
            changed = TRUE;
        }
        break;
    case T_TRACKLEN:
        tempstr = track_get_length_string(track->tracklen);
        if (!g_str_equal(new_text, tempstr)) {
            nr = track_scan_length(new_text);
            track->tracklen = nr;
            changed = TRUE;
        }
        g_free(tempstr);
        break;
    case T_STARTTIME:
        tempstr = track_get_length_string(track->starttime);
        if (!g_str_equal(new_text, tempstr)) {
            nr = track_scan_length(new_text);
            track->starttime = nr;
            changed = TRUE;
            /* Set stoptime to 0 if stoptime is the same as tracklen */
            if (track->stoptime == track->tracklen)
                track->stoptime = 0;
        }
        g_free(tempstr);
        break;
    case T_STOPTIME:
        if (track->stoptime == 0)
            tempstr = track_get_length_string(track->tracklen);
        else
            tempstr = track_get_length_string(track->stoptime);

        if (! g_str_equal(new_text, tempstr)) {
            track->stoptime = track_scan_length(new_text);
            changed = TRUE;
        }
        g_free(tempstr);
        break;
    case T_SEASON_NR:
        nr = atoi(new_text);
        if ((nr >= 0) && (nr != track->season_nr)) {
            track->season_nr = nr;
            changed = TRUE;
        }
        break;
    case T_EPISODE_NR:
        nr = atoi(new_text);
        if ((nr >= 0) && (nr != track->episode_nr)) {
            track->episode_nr = nr;
            changed = TRUE;
        }
        break;
    case T_MEDIA_TYPE:
    case T_PC_PATH:
    case T_IPOD_PATH:
    case T_IPOD_ID:
    case T_TRANSFERRED:
    case T_COMPILATION:
    case T_REMEMBER_PLAYBACK_POSITION:
    case T_SKIP_WHEN_SHUFFLING:
    case T_CHECKED:
    case T_ALL:
    case T_GAPLESS_TRACK_FLAG:
    case T_ITEM_NUM:
    case T_THUMB_PATH: // TODO: this should in fact be settable
        gtkpod_warning("Programming error: track_set_text() called with illegal argument (item: %d)\n", item);
        break;
    }

    return changed;
}

/* Fills @size with the size and @num with the number of
 non-transferred tracks. The size is in Bytes, minus the space taken
 by tracks that will be overwritten. */
/* @size and @num may be NULL */
void gp_info_nontransferred_tracks(iTunesDB *itdb, gdouble *size, guint32 *num) {
    GList *gl;

    if (size)
        *size = 0;
    if (num)
        *num = 0;
    g_return_if_fail (itdb);

    for (gl = itdb->tracks; gl; gl = gl->next) {
        Track *tr = gl->data;
        ExtraTrackData *etr;
        g_return_if_fail (tr);
        etr = tr->userdata;
        g_return_if_fail (etr);
        if (!tr->transferred) {
            if (size)
                *size += tr->size;
            if (num)
                *num += 1;
        }
    }
}

static void intern_add_track(Playlist *pl, Track *track) {
    iTunesDB *from_itdb, *to_itdb;
    Playlist *to_mpl;
    from_itdb = track->itdb;
    g_return_if_fail (from_itdb);
    to_itdb = pl->itdb;
    to_mpl = itdb_playlist_mpl(to_itdb);

    /* 	    printf ("add tr %p to pl: %p\n", track, pl); */
    if (from_itdb == to_itdb) { /* DND within the same itdb */

        /* set flags to 'podcast' if adding to podcast list */
        if (itdb_playlist_is_podcasts(pl))
            gp_track_set_flags_podcast(track);
#if 0 /* initially iTunes didn't add podcasts to the MPL */
        if (!itdb_playlist_contains_track (to_mpl, track))
        { /* add to MPL if not already present (will happen
         * if dragged from the podcasts playlist */
            gp_playlist_add_track (to_mpl, track, TRUE);
        }
#endif
        if (!itdb_playlist_is_mpl(pl)) {
            /* add to designated playlist -- unless adding
             * to podcasts list and track already exists there */
            if (itdb_playlist_is_podcasts(pl) && g_list_find(pl->members, track)) {
                gchar *buf = get_track_info(track, FALSE);
                gtkpod_warning(_("Podcast already present: '%s'\n\n"), buf);
                g_free(buf);
            }
            else {
                gp_playlist_add_track(pl, track, TRUE);
            }
        }
    }
    else { /* DND between different itdbs -- need to duplicate the
     track before inserting */
        Track *duptr, *addtr;
        ExtraTrackData *eduptr;
        /* duplicate track */
        duptr = itdb_track_duplicate(track);
        eduptr = duptr->userdata;
        g_return_if_fail (eduptr);

        duptr->transferred = FALSE;

        /* check if adding to iPod and track is on different iPod */
        if ((from_itdb->usertype & GP_ITDB_TYPE_IPOD) && (to_itdb->usertype & GP_ITDB_TYPE_IPOD)) {
            /* Check if track exists locally */
            if (!(eduptr->pc_path_locale && g_file_test(eduptr->pc_path_locale, G_FILE_TEST_EXISTS))) { /* No. Use iPod path as source */
                g_free(eduptr->pc_path_locale);
                g_free(eduptr->pc_path_utf8);
                eduptr->pc_path_locale = itdb_filename_on_ipod(track);
                eduptr->pc_path_utf8 = charset_to_utf8(eduptr->pc_path_locale);
            }
            /* Remove old reference to iPod path */
            g_free(duptr->ipod_path);
            duptr->ipod_path = g_strdup("");
        }

        if (!eduptr->pc_path_locale) {
            gchar *buf;
            buf = get_track_info(track, FALSE);
            gtkpod_warning(_("Could not find source file for '%s'. Track not copied."));
            g_free(buf);
            itdb_track_free(duptr);
            return;
        }

        if ((from_itdb->usertype & GP_ITDB_TYPE_LOCAL) && (to_itdb->usertype & GP_ITDB_TYPE_IPOD)) { /* make sure the DND origin data is set correctly */
            eduptr->local_itdb_id = from_itdb->id;
            eduptr->local_track_dbid = track->dbid;
        }

        /* add to database -- if duplicate detection is on and the
         same track already exists in the database, the already
         existing track is returned and @duptr is freed */
        addtr = gp_track_add(to_itdb, duptr);
        if (!addtr) {
            /* Track was rendered invalid, possibly by the conversion routine */
            return;
        }

        /* set flags to 'podcast' if adding to podcast list */
        if (itdb_playlist_is_podcasts(pl))
            gp_track_set_flags_podcast(addtr);

        if (addtr == duptr) { /* no duplicate */
#if 0 /* initially iTunes didn't add podcasts to the MPL */
            /* we need to add to the MPL if the track is no
             duplicate and will not be added to the podcasts
             playlist */
            if (!itdb_playlist_is_podcasts (pl))
            { /* don't add to mpl if we add to the podcasts
             playlist */
                gp_playlist_add_track (to_mpl, addtr, TRUE);
            }
#else
            /* we need to add to the MPL if the track is no
             duplicate */
            gp_playlist_add_track(to_mpl, addtr, TRUE);
#endif
        }
#if 0 /* initially iTunes didn't add podcasts to the MPL */
        else
        { /* duplicate */
            /* we also need to add to the MPL if the track is a
             duplicate, does not yet exist in the MPL and will
             not be added to a podcast list (this happens if
             it's already in the podcast list) */
            if ((!itdb_playlist_contains_track (to_mpl, addtr)) &&
                    (!itdb_playlist_is_podcasts (pl)))
            {
                gp_playlist_add_track (to_mpl, addtr, TRUE);
            }
        }
#endif
        /* add to designated playlist (if not mpl) -- unless
         * adding to podcasts list and track already * exists
         * there */
        if (!itdb_playlist_is_mpl(pl)) {
            if (itdb_playlist_is_podcasts(pl) && g_list_find(pl->members, addtr)) {
                gchar *buf = get_track_info(addtr, FALSE);
                gtkpod_warning(_("Podcast already present: '%s'\n\n"), buf);
                g_free(buf);
            }
            else {
                gp_playlist_add_track(pl, addtr, TRUE);
            }
        }
    }
}

/*------------------------------------------------------------------*\
 *                                                                  *
 *             DND to playlists                                     *
 *                                                                  *
 \*------------------------------------------------------------------*/

/* DND: add either a GList of tracks or an ASCII list of tracks to
 * Playlist @pl */
static void add_tracks_to_playlist(Playlist *pl, gchar *string, GList *tracks) {

    g_return_if_fail (!(string && tracks));
    g_return_if_fail (pl);
    g_return_if_fail (pl->itdb);
    g_return_if_fail (itdb_playlist_mpl (pl->itdb));
    if (!(string || tracks))
        return;

    if (string) {
        Track *track;
        gchar *str = string;
        while (parse_tracks_from_string(&str, &track)) {
            g_return_if_fail (track);
            intern_add_track(pl, track);
        }
    }
    if (tracks) {
        GList *gl;
        for (gl = tracks; gl; gl = gl->next) {
            Track *track = gl->data;
            g_return_if_fail (track);
            intern_add_track(pl, track);
        }
    }
}

/* DND: add a glist of tracks to Playlist @pl */
void add_trackglist_to_playlist(Playlist *pl, GList *tracks) {
    add_tracks_to_playlist(pl, NULL, tracks);
}

/* DND: add a list of tracks to Playlist @pl */
void add_tracklist_to_playlist(Playlist *pl, gchar *string) {
    add_tracks_to_playlist(pl, string, NULL);
}

/* DND: add a list of files to Playlist @pl.

 @pl: playlist to add to or NULL. If NULL, a "New Playlist" will be
 created and inserted at position @pl_pos for adding tracks and when
 adding a playlist file, a playlist with the name of the playlist
 file will be added.

 @pl_pos: position to add playlist file, ignored if @pl!=NULL.

 @trackaddfunc: passed on to add_track_by_filename() etc. */

/* Return value: playlist to where the tracks were added. Note: when
 adding playlist files, additional playlists may have been created */
Playlist *add_text_plain_to_playlist(iTunesDB *itdb, Playlist *pl, gchar *str, gint pl_pos, AddTrackFunc trackaddfunc, gpointer data) {
    gchar **files = NULL, **filesp = NULL;
    Playlist *pl_playlist = pl; /* playlist for playlist file */
    Playlist *pl_playlist_created = NULL;
    GError *error = NULL;

    g_return_val_if_fail (itdb, NULL);

    if (!str)
        return NULL;

    /*   printf("pl: %x, pl_pos: %d\n%s\n", pl, pl_pos, str);*/

    block_widgets();

    files = g_strsplit(str, "\n", -1);
    if (files) {
        filesp = files;
        while (*filesp) {
            gint file_len = -1;

            gchar *file = NULL;
            gchar *decoded_file = NULL;

            file = *filesp;
            /* file is in uri form (the ones we're looking for are
             file:///), file can include the \n or \r\n, which isn't
             a valid character of the filename and will cause the
             uri decode / file test to fail, so we'll cut it off if
             its there. */
            file_len = strlen(file);
            if (file_len && (file[file_len - 1] == '\n')) {
                file[file_len - 1] = 0;
                --file_len;
            }
            if (file_len && (file[file_len - 1] == '\r')) {
                file[file_len - 1] = 0;
                --file_len;
            }

            decoded_file = filename_from_uri(file, NULL, NULL);
            if (decoded_file != NULL) {
                if (g_file_test(decoded_file, G_FILE_TEST_IS_DIR)) { /* directory */
                    if (!pl) { /* no playlist yet -- create new one */
                        pl = add_new_pl_user_name(itdb, NULL, pl_pos);
                        if (!pl)
                            break; /* while (*filesp) */
                    }
                    add_directory_by_name(itdb, decoded_file, pl, prefs_get_int("add_recursively"), trackaddfunc, data, &error);
                }
                else if (g_file_test(decoded_file, G_FILE_TEST_IS_REGULAR)) { /* regular file */
                    FileType *filetype = determine_filetype(decoded_file);

                    if (filetype_is_video_filetype(filetype) || filetype_is_audio_filetype(filetype)) {
                        if (!pl) { /* no playlist yet -- create new one */
                            pl = add_new_pl_user_name(itdb, NULL, pl_pos);
                            if (!pl)
                                break; /* while (*filesp) */
                        }
                        add_track_by_filename(itdb, decoded_file, pl, prefs_get_int("add_recursively"), trackaddfunc, data, &error);
                    }
                    else if (filetype_is_playlist_filetype(filetype)) {
                        pl_playlist_created
                                = add_playlist_by_filename(itdb, decoded_file, pl_playlist, pl_pos, trackaddfunc, data, &error);
                    }
                }
                g_free(decoded_file);
            }
            if (error) {
                if (strlen(*filesp) != 0)
                    gtkpod_warning(_("drag and drop: ignored '%s'.\nreason: %s\n"), *filesp, error->message);
                g_error_free(error);
                error = NULL;
            }
            ++filesp;
        }
        g_strfreev(files);
    }
    /* display log of non-updated tracks */
    display_non_updated(NULL, NULL);
    /* display log updated tracks */
    display_updated(NULL, NULL);
    /* display log of detected duplicates */
    gp_duplicate_remove(NULL, NULL);

    release_widgets();

    if (pl)
        return pl;
    if (pl_playlist_created)
        return pl_playlist_created;
    return NULL;
}

/*------------------------------------------------------------------*\
 *                                                                  *
 * Functions setting default values on tracks                       *
 *                                                                  *
 \*------------------------------------------------------------------*/

/* set podcast-specific flags for @track */
void gp_track_set_flags_podcast(Track *track) {
    g_return_if_fail (track);
    track->skip_when_shuffling = 0x01; /* skip when shuffling */
    track->remember_playback_position = 0x01; /* remember playback
     * position */
    track->flag4 = 0x01; /* Show Title/Album on the 'Now Playing' page */
    track->mediatype = ITDB_MEDIATYPE_PODCAST; /* show up under Podcasts */
}

/* set podcast-specific flags for @track */
void gp_track_set_flags_default(Track *track) {
    g_return_if_fail (track);
    track->skip_when_shuffling = 0x00; /* do not skip when shuffling */
    track->remember_playback_position = 0x00; /* do not remember
     * playback position */
    track->flag4 = 0x00; /* Show Title/Album/Artist on the 'Now
     Playing' page */
}

/* return some sensible input about the "track". You must free the
 * return string after use. */
gchar *get_track_info(Track *track, gboolean prefer_filename) {
    ExtraTrackData *etr;

    g_return_val_if_fail (track, NULL);
    etr = track->userdata;
    g_return_val_if_fail (etr, NULL);

    if (prefer_filename) {
        if (etr->pc_path_utf8 && strlen(etr->pc_path_utf8))
            return g_path_get_basename(etr->pc_path_utf8);
    }
    if ((track->title && strlen(track->title)))
        return g_strdup(track->title);
    if ((track->album && strlen(track->album)))
        return g_strdup(track->album);
    if ((track->artist && strlen(track->artist)))
        return g_strdup(track->artist);
    if ((track->composer && strlen(track->composer)))
        return g_strdup(track->composer);
    if (!prefer_filename) {
        if (etr->pc_path_utf8 && strlen(etr->pc_path_utf8))
            return g_path_get_basename(etr->pc_path_utf8);
    }

    return g_strdup_printf("iPod ID: %d", track->id);
}

/*------------------------------------------------------------------*\
 *                                                                  *
 *             Delete Track                                      *
 *                                                                  *
 \*------------------------------------------------------------------*/

/**
 *
 * Callback that could be used with g_idle_add method for
 * removing a track.
 */
gboolean gp_remove_track_cb(gpointer data) {
    Track *track = data;
    gp_playlist_remove_track(NULL, track, DELETE_ACTION_DATABASE);
    return FALSE;
}

/* cancel handler for delete track */
/* @user_data1 the selected playlist, @user_data2 are the selected tracks */
void delete_track_cancel(struct DeleteData *dd) {
    g_return_if_fail (dd);

    g_list_free(dd->tracks);
    g_free(dd);
}

/* ok handler for delete track */
/* @user_data1 the selected playlist, @user_data2 are the selected tracks */
void delete_track_ok(struct DeleteData *dd) {
    gint n;
    GList *l;

    g_return_if_fail (dd);
    g_return_if_fail (dd->pl);
    g_return_if_fail (dd->itdb);

    /* should never happen */
    if (!dd->tracks)
        delete_track_cancel(dd);

    /* nr of tracks to be deleted */
    n = g_list_length(dd->tracks);

    gtkpod_statusbar_reset_progress(n);

    if (dd->itdb->usertype & GP_ITDB_TYPE_IPOD) {
        switch (dd->deleteaction) {
        case DELETE_ACTION_IPOD:
            gtkpod_statusbar_message(ngettext ("Deleting one track completely from iPod",
                    "Deleting %d tracks completely from iPod",
                    n), n);
            break;
        case DELETE_ACTION_PLAYLIST:
            gtkpod_statusbar_message(ngettext ("Deleting %d track from playlist '%s'",
                    "Deleting %d tracks from playlist '%s'",
                    n), n, dd->pl->name);
            break;
        case DELETE_ACTION_LOCAL:
        case DELETE_ACTION_DATABASE:
        default:
            /* not allowed -- programming error */
            g_return_if_reached ();
            break;
        }
    }
    if (dd->itdb->usertype & GP_ITDB_TYPE_LOCAL) {
        switch (dd->deleteaction) {
        case DELETE_ACTION_LOCAL:
            gtkpod_statusbar_message(ngettext ("Deleting one track from harddisk",
                    "Deleting %d tracks from harddisk",
                    n), n);
            break;
        case DELETE_ACTION_PLAYLIST:
            gtkpod_statusbar_message(ngettext ("Deleting %d track from playlist '%s'",
                    "Deleting %d tracks from playlist '%s'",
                    n), n, dd->pl->name);
            break;
        case DELETE_ACTION_DATABASE:
            gtkpod_statusbar_message(ngettext ("Deleting one track from local database",
                    "Deleting %d tracks from local database",
                    n), n);
            break;
        case DELETE_ACTION_IPOD:
        default:
            /* not allowed -- programming error */
            g_return_if_reached ();
            break;
        }
    }

    int i = 1;
    for (l = dd->tracks; l; l = l->next) {
        Track *track = l->data;
        gchar *buf = g_strdup_printf(_("Deleting Track %d/%d ..."), i, n);
        gtkpod_statusbar_increment_progress_ticks(1, buf);
        g_free(buf);

        gp_playlist_remove_track(dd->pl, track, dd->deleteaction);
        i++;
    }
    g_list_free(dd->tracks);
    g_free(dd);

    gtkpod_statusbar_message(_("Completed deletion"));
    gtkpod_tracks_statusbar_update();
}

/* Deletes selected tracks from current playlist.
 @deleteaction: on of the DeleteActions defined in misc.h */
void delete_track_head(DeleteAction deleteaction) {
    Playlist *pl;
    GList *selected_tracks;
    GString *str;
    gchar *label, *title;
    gboolean confirm_again;
    struct DeleteData *dd;
    iTunesDB *itdb;
    GtkResponseType response;
    gchar *confirm_again_key;

    pl = gtkpod_get_current_playlist();
    if (pl == NULL) { /* no playlist??? Cannot happen, but... */
        message_sb_no_playlist_selected();
        return;
    }
    itdb = pl->itdb;
    g_return_if_fail (itdb);

    selected_tracks = gtkpod_get_selected_tracks();
    if (selected_tracks == NULL) { /* no tracks selected */
        message_sb_no_tracks_selected();
        return;
    }

    dd = g_malloc0(sizeof(struct DeleteData));
    dd->deleteaction = deleteaction;
    dd->tracks = selected_tracks;
    dd->pl = pl;
    dd->itdb = itdb;

    delete_populate_settings(dd, &label, &title, &confirm_again, &confirm_again_key, &str);
    /* open window */
    response = gtkpod_confirmation(-1, /* gint id, */
    TRUE, /* gboolean modal, */
    title, /* title */
    label, /* label */
    str->str, /* scrolled text */
    NULL, 0, NULL, /* option 1 */
    NULL, 0, NULL, /* option 2 */
    confirm_again, /* gboolean confirm_again, */
    confirm_again_key,/* ConfHandlerOpt confirm_again_key,*/
    CONF_NULL_HANDLER, /* ConfHandler ok_handler,*/
    NULL, /* don't show "Apply" button */
    CONF_NULL_HANDLER, /* cancel_handler,*/
    NULL, /* gpointer user_data1,*/
    NULL); /* gpointer user_data2,*/

    switch (response) {
    case GTK_RESPONSE_OK:
        /* Delete the tracks */
        delete_track_ok(dd);
        break;
    default:
        delete_track_cancel(dd);
        break;
    }

    g_free(label);
    g_free(title);
    g_free(confirm_again_key);
    g_string_free(str, TRUE);
}

void copy_tracks_to_target_playlist(GList *tracks, Playlist *t_pl) {
    GList *addtracks = NULL;
    Track *first;
    Playlist *mpl;
    gint n;
    Exporter *exporter;

    g_return_if_fail (tracks);
    g_return_if_fail (t_pl);
    g_return_if_fail (t_pl->itdb);

    mpl = itdb_playlist_mpl(t_pl->itdb);
    g_return_if_fail(mpl);

    exporter = gtkpod_get_exporter();
    g_return_if_fail(exporter);

    if (tracks) {
        first = tracks->data;
        g_return_if_fail (first);
        addtracks = exporter_transfer_track_glist_between_itdbs(exporter, first->itdb, t_pl->itdb, tracks);
        add_trackglist_to_playlist(t_pl, addtracks);
    }
    n = g_list_length(addtracks);
    gtkpod_statusbar_message(ngettext ("Copied %d track to '%s' in '%s'",
            "Copied %d tracks to %s in '%s'", n), n, t_pl->name, mpl->name);
    g_list_free(addtracks);
    addtracks = NULL;
}

/*
 * Copy selected tracks to a specified itdb.
 */
void copy_tracks_to_target_itdb(GList *tracks, iTunesDB *t_itdb) {
    GList *addtracks = NULL;
    Track *first = tracks->data;
    Playlist *mpl;
    gint n;
    Exporter *exporter;

    g_return_if_fail(tracks);
    g_return_if_fail(t_itdb);

    mpl = itdb_playlist_mpl(t_itdb);
    g_return_if_fail(mpl);

    exporter = gtkpod_get_exporter();
    g_return_if_fail(exporter);

    addtracks = exporter_transfer_track_glist_between_itdbs(exporter, first->itdb, t_itdb, tracks);

    if (addtracks) {
        add_trackglist_to_playlist(mpl, addtracks);
        n = g_list_length(addtracks);
        gtkpod_statusbar_message(ngettext ("Copied %d track to '%s'",
                "Copied %d tracks to '%s'", n), n, mpl->name);
        g_list_free(addtracks);
        addtracks = NULL;
    }
}

/*------------------------------------------------------------------*\
 *                                                                  *
 *              Frequently used error messages *
 *                                                                  *
 \*------------------------------------------------------------------*/

void message_sb_no_tracks_selected() {
    gtkpod_statusbar_message(_("No tracks selected"));
}
