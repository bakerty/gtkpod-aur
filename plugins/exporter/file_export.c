/*
 |  Copyright (C) 2002 Corey Donohoe <atmos at atmos.org>
 |  Copyright (C) 2002-2005 Jorg Schuler <jcsjcs at users sourceforge net>
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

#include "plugin.h"
#include "file_export.h"
#include "libgtkpod/charset.h"
#include "libgtkpod/file.h"
#include "libgtkpod/sha1.h"
#include "libgtkpod/misc.h"
#include "libgtkpod/misc_track.h"
#include "libgtkpod/prefs.h"
#include "libgtkpod/directories.h"
#include "libgtkpod/gp_private.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Structure to keep all necessary information */
struct fcd {
    GList *tracks; /* tracks to be written */
    GList **filenames; /* pointer to GList to append the filenames used */
    GtkBuilder *builder; /* GtkBuilder reference */
    Track *track; /* current track to export */
    gchar *filename; /* filename for the current track to export */
    GString *errors; /* Errors generated during the export */
};

/*------------------------------------------------------------------

 Code to export a playlist file

 ------------------------------------------------------------------*/

/* Options for the export playlist file file chooser */
typedef enum {
    EXPORT_PLAYLIST_FILE_TYPE_M3U = 0, EXPORT_PLAYLIST_FILE_TYPE_PLS
} ExportPlaylistFileType;

/* Strings for prefs settings */
/* use original charset or specified one? */
const gchar *EXPORT_FILES_SPECIAL_CHARSET = "export_files_special_charset";
/* whether we check for existing files on export or not */
const gchar *EXPORT_FILES_CHECK_EXISTING = "export_files_check_existing";
const gchar *EXPORT_FILES_PATH = "export_files_path";
const gchar *EXPORT_FILES_TPL = "export_files_template";
/* Default prefs settings */
const gchar *EXPORT_FILES_TPL_DFLT = "%o;%a - %t.m4a;%a - %t.mp3;%t.wav";

/* Strings for the widgets involved */
const gchar *ExportPlaylistFileTypeW[] = { "type_m3u", "type_pls", NULL };
/* order must be identical to 'FileSource' enum in file.h */
const gchar *ExportPlaylistFileSourceW[] = { "source_prefer_local", "source_local", "source_ipod", NULL };

/* Strings for prefs settings */
const gchar *EXPORT_PLAYLIST_FILE_TYPE = "export_playlist_file_type";
const gchar *EXPORT_PLAYLIST_FILE_SOURCE = "export_playlist_file_source";
const gchar *EXPORT_PLAYLIST_FILE_PATH = "export_playlist_file_path";
const gchar *EXPORT_PLAYLIST_FILE_TPL = "export_playlist_file_template";

/* Default prefs settings */
const gchar *EXPORT_PLAYLIST_FILE_TPL_DFLT = "%a - %t";

/*------------------------------------------------------------------

 Code to export tracks

 ------------------------------------------------------------------*/

/**
 * READ_WRITE_BLOCKSIZE - how many bytes we read per fread/fwrite call
 */
#define READ_WRITE_BLOCKSIZE 65536

#ifdef G_THREADS_ENABLED
/* Thread specific */
static GMutex mutex; /* shared lock */
static GCond cond;
static gboolean mutex_data = FALSE;
#endif

static void _lock_mutex() {
    g_mutex_lock (&mutex);
}

static void _unlock_mutex() {
    g_mutex_unlock (&mutex);
}

static void _wait_cond(gint64 gtime) {
    g_cond_wait_until (&cond, &mutex, gtime);
}

static void _cond_signal() {
    g_cond_signal (&cond);
}

/******************************************************************
 export_fcd_cleanup - free memory taken up by the fcd
 structure.
 ******************************************************************/
static void export_fcd_cleanup(struct fcd *fcd) {
    g_return_if_fail (fcd);
    g_list_free(fcd->tracks);
    g_string_free(fcd->errors, TRUE);

    if (fcd->filename)
        g_free(fcd->filename);

    g_free(fcd);
    release_widgets();
}

/**
 * copy_file_fd_sync - given two open file descriptors, read from one
 * 	and write the data to the other, fsync() before returning.
 * @from - the file descriptor we're reading from
 * @to - the file descriptor we're writing to
 * Returns TRUE on write success, FALSE on write failure
 */
static gboolean copy_file_fd_sync(FILE *from, FILE *to) {
    gboolean result = FALSE;
    gchar data[READ_WRITE_BLOCKSIZE];

    if ((from) && (to)) {
        int read_bytes = 0, write_bytes = 0;

        do {
            if ((read_bytes = fread(data, sizeof(char), READ_WRITE_BLOCKSIZE, from))) {
                if ((write_bytes = fwrite(data, sizeof(char), read_bytes, to))) {
                    result = TRUE;
                }
                else {
                    if ((ferror(to) != 0)) {
                        /*			fprintf(stderr, "%d is errno writing\n", errno);*/
                        result = FALSE;
                    }
                    break;
                }
            }
            else {
                if (!feof(from))
                    result = FALSE;
                else
                    result = TRUE;
                break;

            }

        }
        while (!(feof(from)));
    }
    if (!result)
        result = fsync(fileno(to));
    return (result);
}

/* Return TRUE if the file @dest exists and is of same size as @from */
static gboolean file_is_ok(gchar *from, gchar *dest) {
    struct stat st_from, st_dest;

    if (stat(dest, &st_dest) == -1)
        return FALSE;
    if (stat(from, &st_from) == -1)
        return FALSE;

    if (st_from.st_size == st_dest.st_size)
        return TRUE;

    return FALSE;
}

/**
 * copy_file - copy the filename on disk named file, to
 * the destination file dest.  Both names are FULL pathnames to the file
 * @file - the filename to copy
 * @dest - the filename we copy to
 * Returns TRUE on successful copying
 */
static gboolean copy_file(gchar *file, gchar *dest, GError **error) {
    gboolean result = FALSE;
    FILE *from = NULL, *to = NULL;
    gboolean check_existing;
    gchar *buf = NULL;

    prefs_get_int_value(EXPORT_FILES_CHECK_EXISTING, &check_existing);

    if (check_existing && file_is_ok(file, dest)) {
        buf = g_strdup_printf(_("Skipping existing file with same length: '%s'\n"), dest);
        gtkpod_log_error(error, buf);
        g_free(buf);
        return TRUE;
    }

    if (g_file_test(dest, G_FILE_TEST_EXISTS)) {
        buf = g_strdup_printf(_("Overwriting existing file: '%s'\n"), dest);
        gtkpod_log_error(error, buf);
        g_free(buf);
        buf = NULL;
    }

    if ((from = fopen(file, "r"))) {
        if ((to = fopen(dest, "w"))) {
            result = copy_file_fd_sync(from, to);
            fclose(to);
        }
        else {
            switch (errno) {
            case EPERM:
                buf = g_strdup_printf(_("Error copying '%s' to '%s': Permission Error (%s)\n"), file, dest, g_strerror(errno));
                break;
            default:
                buf = g_strdup_printf(_("Error copying '%s' to '%s' (%s)\n"), file, dest, g_strerror(errno));
            }
        }
        fclose(from);
    }
    else {
        buf = g_strdup_printf(_("Could not open '%s' for reading.\n"), file);
    }

    if (buf) {
        gtkpod_log_error(error, buf);
        g_free(buf);
    }

    return (result);
}

/**
 * get_preferred_filename - useful for generating the preferred
 * @param Track the track
 * @return the file filename (including directories) for this Track
 * based on the users preferences.  The returned char* must be freed
 * by the caller.
 */
static gchar *
track_get_export_filename(Track *track, GError **error) {
    gchar *res_utf8, *res_cs = NULL;
    gchar *template;
    gboolean special_charset;

    g_return_val_if_fail (track, NULL);

    prefs_get_string_value(EXPORT_FILES_TPL, &template);

    res_utf8 = get_string_from_full_template(track, template, TRUE, error);
    C_FREE (template);

    prefs_get_int_value(EXPORT_FILES_SPECIAL_CHARSET, &special_charset);

    /* convert it to the charset */
    if (special_charset) { /* use the specified charset */
        res_cs = charset_from_utf8(res_utf8);
    }
    else { /* use the charset stored in track->charset */
        res_cs = charset_track_charset_from_utf8(track, res_utf8);
    }
    g_free(res_utf8);
    return res_cs;
}

/**
 * write_track_to_dir - copy the Track* to the desired output file
 * @s - The Track reference we're manipulating
 * Returns - TRUE on success, FALSE on failure
 */
static gboolean write_track(struct fcd *fcd) {
    gboolean result = FALSE;

    g_return_val_if_fail (fcd, FALSE);
    g_return_val_if_fail (fcd->track, FALSE);
    g_return_val_if_fail (fcd->filename, FALSE);
    g_return_val_if_fail (fcd->track->itdb, FALSE);

    if (fcd->filename) {
        gchar *from_file = NULL;
        if (fcd->track->itdb->usertype & GP_ITDB_TYPE_IPOD) {
            from_file = get_file_name_from_source(fcd->track, SOURCE_IPOD);
        }
        else if (fcd->track->itdb->usertype & GP_ITDB_TYPE_LOCAL) {
            from_file = get_file_name_from_source(fcd->track, SOURCE_LOCAL);
        }
        else {
            g_return_val_if_reached (FALSE);
        }
        if (from_file) {
            gchar *filename, *dest_dir;
            prefs_get_string_value(EXPORT_FILES_PATH, &dest_dir);
            filename = g_build_filename(dest_dir, fcd->filename, NULL);

            if (mkdirhierfile(filename)) {
                GError *error = NULL;
                if (copy_file(from_file, filename, &error)) {
                    result = TRUE;
                    if (fcd->filenames) { /* append filename to list */
                        *fcd->filenames = g_list_append(*fcd->filenames, filename);
                        filename = NULL;
                    }

                    if (error) {
                        /* File may have been skipped so need to log message */
                        fcd->errors = g_string_append(fcd->errors, g_strdup_printf(_("'%s'\n"), error->message));
                        g_error_free(error);
                    }
                }
                else {
                    /* Failed to copy correctly */
                    gchar *buf;
                    if (error) {
                        buf = g_strdup_printf(_("'%s'\n"), error->message);
                    }
                    else {
                        buf = g_strdup_printf(_("Failed to copy file %s. No error reported."), from_file);
                    }

                    fcd->errors = g_string_append(fcd->errors, buf);
                    g_error_free(error);
                    g_free(buf);
                }
            }
            g_free(from_file);
            g_free(filename);
        }
        else {
            gchar *buf = get_track_info(fcd->track, FALSE);
            fcd->errors = g_string_append(fcd->errors, g_strdup_printf(_("Could not find file for '%s' on the iPod\n"), buf));
            g_free(buf);
        }
    }
    return (result);
}

static void export_report_errors(GString *errors) {
    if (errors && errors->len > 0) {
        gtkpod_confirmation(-1, /* gint id, */
                TRUE, /* gboolean modal, */
                _("Export Errors"), /* title */
                _("Errors created by export"), /* label */
                errors->str, /* scrolled text */
                NULL, 0, NULL, /* option 1 */
                NULL, 0, NULL, /* option 2 */
                TRUE, /* gboolean confirm_again, */
                "show_export_errors",/* confirm_again_key,*/
                CONF_NULL_HANDLER, /* ConfHandler ok_handler,*/
                NULL, /* don't show "Apply" button */
                NULL, /* cancel_handler,*/
                NULL, /* gpointer user_data1,*/
                NULL); /* gpointer user_data2,*/
    }
}

#ifdef G_THREADS_ENABLED
/* Threaded write_track */
static gpointer th_write_track(gpointer fcd) {
    gboolean result = FALSE;

    result = write_track(fcd);

    _lock_mutex();
    mutex_data = TRUE; /* signal that thread will end */
    _cond_signal();
    _unlock_mutex();

    return GINT_TO_POINTER(result);
}
#endif

/******************************************************************
 export_files_write - copy the specified tracks to the selected
 directory.
 ******************************************************************/
static void export_files_write(struct fcd *fcd) {
    GList *l = NULL;
    gint n;
    gdouble total = 0;

#ifdef G_THREADS_ENABLED
    GThread *thread = NULL;
    gint64 gtime;
#endif

    g_return_if_fail (fcd);

    block_widgets();

    n = g_list_length(fcd->tracks);
    /* calculate total length to be copied */
    for (l = fcd->tracks; l; l = l->next) {
        Track *s = (Track*) l->data;
        total += s->size;
    }

    if (n != 0) {
        gint count = 0; /* number of tracks copied */
        gdouble copied = 0; /* number of bytes copied */
        gdouble fraction; /* fraction copied (copied/total) */
        gdouble old_fraction = 0;
        gboolean result = TRUE;
        gchar *progtext;
        time_t diff, start, fullsecs, hrs, mins, secs;

        gtkpod_statusbar_reset_progress(100);
        start = time(NULL);
        for (l = fcd->tracks; l; l = l->next) {
            gboolean resultWrite = TRUE;
            Track *tr = (Track*) l->data;
            fcd->track = tr;

            GError *error = NULL;
            fcd->filename = track_get_export_filename(fcd->track, &error);
            if (error != NULL) {
                fcd->errors = g_string_append(fcd->errors, g_strdup(error->message));
                resultWrite = FALSE;
                result &= resultWrite;
                g_error_free(error);
            } else {
                copied += tr->size;
#ifdef G_THREADS_ENABLED
                mutex_data = FALSE;
                thread = g_thread_new ("export-thread", th_write_track, fcd);
                if (thread) {
                    _lock_mutex();
                    do {
                        while (widgets_blocked && gtk_events_pending())
                        gtk_main_iteration();
                        /* wait a maximum of 20 ms */
                        gtime = g_get_monotonic_time () + 20000 * G_TIME_SPAN_SECOND;
                        _wait_cond(gtime);
                    }
                    while (!mutex_data);
                    _unlock_mutex();
                    resultWrite = (gboolean) GPOINTER_TO_INT(g_thread_join (thread));
                    result &= resultWrite;
                }
                else {
                    g_warning ("Thread creation failed, falling back to default.\n");
                    resultWrite = write_track(fcd);
                    result &= resultWrite;
                }
#else
                resultWrite = write_track (fcd);
                result &= resultWrite;
                while (widgets_blocked && gtk_events_pending ())
                    gtk_main_iteration ();
#endif

                if (fcd->filename) {
                    g_free(fcd->filename);
                    fcd->filename = 0;
                }
            }

            if (!resultWrite) {
                gchar *msg = g_strdup_printf(_("Failed to write '%s-%s'\n\n"), tr->artist, tr->title);
                fcd->errors = g_string_append(fcd->errors, msg);
                g_free(msg);
            }

            ++count;
            fraction = copied / total;

            diff = time(NULL) - start;
            fullsecs = (diff / fraction) - diff + 5;
            hrs = fullsecs / 3600;
            mins = (fullsecs % 3600) / 60;
            secs = ((fullsecs % 60) / 5) * 5;
            /* don't bounce up too quickly (>10% change only) */
            /*	      left = ((mins < left) || (100*mins >= 110*left)) ? mins : left;*/

            progtext
                    = g_strdup_printf(_("%d%% (%d:%02d:%02d left)"), (int) (100 * fraction), (int) hrs, (int) mins, (int) secs);
            gdouble ticks = fraction - old_fraction;
            gtkpod_statusbar_increment_progress_ticks(ticks * 100, progtext);

            old_fraction = fraction;
            g_free(progtext);

            if (fraction == 1) {
                gtkpod_statusbar_reset_progress(100);
                gtkpod_statusbar_message(ngettext ("Exported %d of %d track.", "Exported %d of %d tracks.", n), count, n);
            }

            while (widgets_blocked && gtk_events_pending())
                gtk_main_iteration();
        }
        if (!result) {
            export_report_errors(fcd->errors);
            gtkpod_statusbar_message(_("Some tracks were not exported."));
        }
    }
    release_widgets();
}

static gboolean export_files_write_cb(gpointer data) {
    struct fcd *fcd = data;
    export_files_write(fcd);
    export_fcd_cleanup(fcd);
    return FALSE;
}

/******************************************************************
 export_files_retrieve_options - retrieve options and store
 them in the prefs.
 ******************************************************************/
static void export_files_store_option_settings(struct fcd *fcd, GtkFileChooser *fc) {
    g_return_if_fail (fcd && fcd->builder && fc);

    option_get_toggle_button(fcd->builder, EXPORT_FILES_SPECIAL_CHARSET);
    option_get_toggle_button(fcd->builder, EXPORT_FILES_CHECK_EXISTING);
    option_get_string(fcd->builder, EXPORT_FILES_TPL, NULL);
    option_get_filename(fc, EXPORT_FILES_PATH, NULL);
}

/******************************************************************
 export_tracks_as_files - Export files off of your ipod to an arbitrary
 directory, specified by the file chooser dialog

 @tracks    - GList with data of type (Track*) we want to write
 @filenames - a pointer to a GList where to store the filenames used
 to write the tracks (or NULL)
 @display   - TRUE: display a list of tracks to be exported
 @message   - message to be displayed above the display of tracks

 ******************************************************************/
void export_tracks_as_files(GList *tracks, GList **filenames, gboolean display, gchar *message) {
    gint response;
    GtkWidget *win, *options, *message_box;
    struct fcd *fcd;
    GtkWidget *fc;
    GtkBuilder *export_files_xml;
    iTunesDB *itdb = NULL;

    if (tracks) {
        Track *tr = tracks->data;
        g_return_if_fail (tr);
        itdb = tr->itdb;
        g_return_if_fail (itdb);
    }

    /* no export possible if in offline mode */
    if (tracks && get_offline(itdb)) {
        Track *tr = tracks->data;
        g_return_if_fail (tr);
        if (itdb->usertype & GP_ITDB_TYPE_IPOD) {
            GtkWidget
                    *dialog =
                            gtk_message_dialog_new(GTK_WINDOW (gtkpod_app), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, _("Export from iPod database not possible in offline mode."));
            gtk_dialog_run(GTK_DIALOG (dialog));
            gtk_widget_destroy(dialog);
            return;
        }
    }

    fc
            = gtk_file_chooser_dialog_new(_("Select Export Destination Directory"), GTK_WINDOW (gtkpod_app), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);

    gchar *glade_path = g_build_filename(get_glade_dir(), "exporter.xml", NULL);

    export_files_xml = gtkpod_builder_xml_new(glade_path);
    win = gtkpod_builder_xml_get_widget(export_files_xml, "export_files_options");
    options = gtkpod_builder_xml_get_widget(export_files_xml, "ef_options_frame");
    message_box = gtkpod_builder_xml_get_widget(export_files_xml, "ef_message_box");
    g_free(glade_path);

    /* Information needed to clean up later */
    fcd = g_malloc0(sizeof(struct fcd));
    fcd->tracks = g_list_copy(tracks);
    fcd->builder = export_files_xml;
    fcd->filenames = filenames;
    fcd->errors = g_string_new("");

    /* according to GTK FAQ: move a widget to a new parent */
    g_object_ref(options);
    gtk_container_remove(GTK_CONTAINER (win), options);
    /* set extra options */
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER (fc), options);
    g_object_unref(options);

    gtk_widget_destroy(win);

    /* set message text */
    if (display) {
        GList *gl;
        GtkWidget *label = gtkpod_builder_xml_get_widget(export_files_xml, "ef_message");
        GtkWidget *tv = gtkpod_builder_xml_get_widget(export_files_xml, "ef_textview");
        GtkTextBuffer *tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
        if (message)
            gtk_label_set_text(GTK_LABEL (label), message);
        else
            gtk_widget_hide(label);

        if (!tb) { /* set up textbuffer */
            tb = gtk_text_buffer_new(NULL);
            gtk_text_view_set_buffer(GTK_TEXT_VIEW (tv), tb);
            gtk_text_view_set_editable(GTK_TEXT_VIEW (tv), FALSE);
            gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tv), FALSE);
        }

        for (gl = tracks; gl; gl = gl->next) {
            GtkTextIter ti;
            gchar *text;
            Track *tr = gl->data;
            g_return_if_fail (tr);

            text = get_track_info(tr, FALSE);
            /* append info to the end, incl. newline */
            gtk_text_buffer_get_end_iter(tb, &ti);
            gtk_text_buffer_insert(tb, &ti, text, -1);
            gtk_text_buffer_get_end_iter(tb, &ti);
            gtk_text_buffer_insert(tb, &ti, "\n", -1);
            g_free(text);
        }
    }
    else {
        gtk_widget_hide(message_box);
    }

    /* set last folder */
    option_set_folder(GTK_FILE_CHOOSER (fc), EXPORT_FILES_PATH);
    /* set toggle button "charset" */
    option_set_toggle_button(export_files_xml, EXPORT_FILES_SPECIAL_CHARSET, FALSE);
    /* set toggle button "check for existing files" */
    option_set_toggle_button(export_files_xml, EXPORT_FILES_CHECK_EXISTING, TRUE);

    /* set last template */
    option_set_string(export_files_xml, EXPORT_FILES_TPL, EXPORT_FILES_TPL_DFLT);

    response = gtk_dialog_run(GTK_DIALOG (fc));

    switch (response) {
    case GTK_RESPONSE_ACCEPT:
        export_files_store_option_settings(fcd, GTK_FILE_CHOOSER(fc));
        /* Let the dialog close before exporting in the background */
        gdk_threads_add_idle((GSourceFunc) export_files_write_cb, fcd);
        break;
    case GTK_RESPONSE_CANCEL:
        break;
    default:
        break;
    }
    gtk_widget_destroy(GTK_WIDGET (fc));
}

/*------------------------------------------------------------------

 Code for DND: export when dragging from the iPod to the local
 database.

 ------------------------------------------------------------------*/

/* If tracks are dragged from the iPod to the local database, the
 tracks need to be copied from the iPod to the harddisk. This
 function will ask where to copy them to, and add the tracks to the
 MPL of @itdb_d.
 A list of tracks that needs to be processed by the drag is
 returned.

 If tracks are not dragged from the iPod to the local database, a
 copy of @tracks is returned.

 The returned GList must be g_list_free()'ed after it is no longer
 used. */
GList *transfer_track_glist_between_itdbs(iTunesDB *itdb_s, iTunesDB *itdb_d, GList *tracks) {
    GList *gl;
    GList *existing_tracks = NULL;
    GList *new_tracks = NULL;
    GList *added_tracks = NULL;

    g_return_val_if_fail (itdb_s, NULL);
    g_return_val_if_fail (itdb_d, NULL);

    if (!((itdb_s->usertype & GP_ITDB_TYPE_IPOD) && (itdb_d->usertype & GP_ITDB_TYPE_LOCAL))) { /* drag is not from iPod to local database -> return copy of
     * @tracks */
        return g_list_copy(tracks);
    }

    /* drag is from iPod to local database */

    /* no drag possible if in offline mode */
    if (get_offline(itdb_s)) {
        GtkWidget
                *dialog =
                        gtk_message_dialog_new(GTK_WINDOW (gtkpod_app), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, _("Drag from iPod database not possible in offline mode."));
        gtk_dialog_run(GTK_DIALOG (dialog));
        gtk_widget_destroy(dialog);
        return NULL;
    }

    /* make a list of tracks that already exist in itdb_d and of those
     that do not yet exist */
    for (gl = tracks; gl; gl = gl->next) {
        Track *otr;
        Track *tr = gl->data;
        g_return_val_if_fail (tr, NULL);

        otr = sha1_track_exists(itdb_d, tr);

        if (otr) {
            existing_tracks = g_list_append(existing_tracks, otr);
        }
        else {
            new_tracks = g_list_append(new_tracks, tr);
        }
    }

    /* if new tracks exist, copy them from the iPod to the harddisk */
    if (new_tracks) {
        GList *filenames = NULL;
        Playlist *mpl = itdb_playlist_mpl(itdb_d);
        g_return_val_if_fail (mpl, NULL);

        export_tracks_as_files(new_tracks, &filenames, TRUE, _("The following tracks have to be copied to your harddisk"));
        /* add copied tracks to MPL of @itdb_d */
        while (new_tracks && filenames) {
            Track *dtr, *added_track;
            ExtraTrackData *edtr;
            Track *tr = new_tracks->data;
            gchar *filename = filenames->data;
            g_return_val_if_fail (tr, NULL);
            g_return_val_if_fail (filename, NULL);

            /* duplicate track */
            dtr = itdb_track_duplicate(tr);
            edtr = dtr->userdata;
            g_return_val_if_fail (edtr, NULL);

            /* set filename */
            g_free(edtr->pc_path_utf8);
            g_free(edtr->pc_path_locale);
            edtr->pc_path_utf8 = charset_to_utf8(filename);
            edtr->pc_path_locale = filename;

            /* free ipod path */
            g_free(dtr->ipod_path);
            dtr->ipod_path = g_strdup("");

            /* add track to itdb_d */
            added_track = gp_track_add(itdb_d, dtr);
            /* this cannot happen because we checked that the track is
             not in itdb_d before! */
            g_return_val_if_fail (added_track == dtr, NULL);
            /* add track to MPL */
            gp_playlist_add_track(mpl, dtr, TRUE);

            /* add track to added_tracks */
            added_tracks = g_list_append(added_tracks, dtr);

            /* remove the links from the GLists */
            new_tracks = g_list_delete_link(new_tracks, new_tracks);
            filenames = g_list_delete_link(filenames, filenames);
        }

        if (filenames) {
            GList *gl;
            gtkpod_warning(_("Some tracks were not copied to your harddisk. Only the copied tracks will be included in the current drag and drop operation.\n\n"));
            for (gl = filenames; gl; gl = gl->next) {
                g_free(gl->data);
            }
            g_list_free(filenames);
            filenames = NULL;
        }
        /* new_tracks must always be shorter than filenames! */
        g_return_val_if_fail (!new_tracks, NULL);
    }

    /* return a list containing the existing tracks and the added
     tracks */
    return g_list_concat(existing_tracks, added_tracks);
}

/* same as transfer_track_glist_between_itdbs() but the tracks are
 represented as pointers in ASCII format. This function parses the
 tracks in @data and calls transfer_track_glist_between_itdbs() */
GList *transfer_track_names_between_itdbs(iTunesDB *itdb_s, iTunesDB *itdb_d, gchar *data) {
    GList *result;
    Track *tr;
    GList *tracks = NULL;
    gchar *datap = data;

    g_return_val_if_fail (itdb_s, NULL);
    g_return_val_if_fail (itdb_d, NULL);
    g_return_val_if_fail (data, NULL);

    /* parse tracks and create GList */
    while (parse_tracks_from_string(&datap, &tr)) {
        tracks = g_list_append(tracks, tr);
    }

    result = transfer_track_glist_between_itdbs(itdb_s, itdb_d, tracks);

    g_list_free(tracks);

    return result;
}

/*------------------------------------------------------------------

 Code to export a playlist file

 ------------------------------------------------------------------*/

/******************************************************************
 export_playlist_file_retrieve_options - retrieve options and store
 them in the prefs.
 ******************************************************************/
static void export_playlist_file_retrieve_options(struct fcd *fcd, GtkFileChooser *fc) {
    g_return_if_fail (fcd && fc);

    option_get_radio_button(fcd->builder, EXPORT_PLAYLIST_FILE_TYPE, ExportPlaylistFileTypeW);
    option_get_radio_button(fcd->builder, EXPORT_PLAYLIST_FILE_SOURCE, ExportPlaylistFileSourceW);
    option_get_string(fcd->builder, EXPORT_PLAYLIST_FILE_TPL, NULL);
    option_get_folder(fc, EXPORT_PLAYLIST_FILE_PATH, NULL);
}

/******************************************************************
 export_playlist_file_write - write out a playlist file (filename is
 retrieved from the file chooser dialog, options are retrieved from
 the prefs system
 ******************************************************************/
static void export_playlist_file_write(struct fcd *fcd) {
    guint num;
    gint type, source;
    gchar *template;
    FILE *file;

    g_return_if_fail (fcd);

    num = g_list_length(fcd->tracks);
    type = prefs_get_int(EXPORT_PLAYLIST_FILE_TYPE);
    source = prefs_get_int(EXPORT_PLAYLIST_FILE_SOURCE);
    template = prefs_get_string(EXPORT_PLAYLIST_FILE_TPL);
    if (!template)
        template = g_strdup(EXPORT_PLAYLIST_FILE_TPL_DFLT);

    file = fopen(fcd->filename, "w");

    if (file) {
        guint i, n;

        switch (type) {
        case EXPORT_PLAYLIST_FILE_TYPE_M3U:
            fprintf(file, "#EXTM3U\n");
            break;
        case EXPORT_PLAYLIST_FILE_TYPE_PLS:
            fprintf(file, "[playlist]\n");
            break;
        }

        for (n = 0, i = 0; i < num; ++i) {
            Track *track = g_list_nth_data(fcd->tracks, i);
            GError *error = NULL;
            gchar *infotext_utf8 = get_string_from_full_template(track, template, FALSE, &error);
            if (error != NULL) {
                fcd->errors = g_string_append(fcd->errors, g_strdup(error->message));
                g_error_free(error);
                continue;
            }

            gchar *filename = get_file_name_from_source(track, source);
            gchar *infotext;

            if (infotext_utf8) {
                infotext = charset_from_utf8(infotext_utf8);
                C_FREE (infotext_utf8);
            }
            else {
                infotext = g_strdup("");
            }

            if (filename) {
                ++n; /* number of verified tracks */

                switch (type) {
                case EXPORT_PLAYLIST_FILE_TYPE_M3U:
                    fprintf(file, "#EXTINF:%d,%s\n", (track->tracklen + 990) / 1000, infotext);
                    fprintf(file, "%s\n", filename);
                    break;
                case EXPORT_PLAYLIST_FILE_TYPE_PLS:
                    fprintf(file, "File%d=%s\n", n, filename);
                    fprintf(file, "Title%d=%s\n", n, infotext);
                    fprintf(file, "Length%d=%d\n", n, (track->tracklen + 990) / 1000);
                    break;
                }
            }
            else {
                fcd->errors = g_string_append(fcd->errors, g_strdup_printf(_("No valid filename for: %s\n\n"), infotext));
            }
            g_free(infotext);
            g_free(filename);
        }
        switch (type) {
        case EXPORT_PLAYLIST_FILE_TYPE_M3U:
            break;
        case EXPORT_PLAYLIST_FILE_TYPE_PLS:
            fprintf(file, "NumberOfEntries=%d\n", n);
            fprintf(file, "Version=2\n");
            break;
        }
        fclose(file);
        gtkpod_statusbar_message(ngettext ("Created playlist with one track.",
                "Created playlist with %d tracks.", n), n);
    }
    else {
        fcd->errors = g_string_append(fcd->errors, g_strdup_printf(_("Could not open '%s' for writing (%s).\n\n"), fcd->filename, g_strerror(errno)));
    }

    export_report_errors(fcd->errors);
    g_free(template);
}

static gboolean export_playlist_file_write_cb(gpointer data) {
    struct fcd *fcd = data;
    export_playlist_file_write(fcd);
    export_fcd_cleanup(fcd);
    return FALSE;
}

/******************************************************************
 export_playlist_file_response - handle the response codes
 accordingly.
 ******************************************************************/
static void export_playlist_file_response(GtkDialog *fc, gint response, struct fcd *fcd) {
    /*     printf ("received response code: %d\n", response); */
    switch (response) {
    case RESPONSE_APPLY:
        export_playlist_file_retrieve_options(fcd, GTK_FILE_CHOOSER(fc));
        break;
    case GTK_RESPONSE_ACCEPT:
        export_playlist_file_retrieve_options(fcd, GTK_FILE_CHOOSER(fc));
        fcd->filename = g_strdup(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc)));
        gdk_threads_add_idle((GSourceFunc) export_playlist_file_write_cb, fcd);
        gtk_widget_destroy(GTK_WIDGET (fc));
        break;
    case GTK_RESPONSE_CANCEL:
        export_fcd_cleanup(fcd);
        gtk_widget_destroy(GTK_WIDGET (fc));
        break;
    case GTK_RESPONSE_DELETE_EVENT:
        export_fcd_cleanup(fcd);
        break;
    default:
        fprintf(stderr, "Programming error: export_playlist_file_response(): unknown response '%d'\n", response);
        break;
    }
}

/******************************************************************
 export_tracks_to_playlist_file - Create a playlist file to a location
 specified by the file selection dialog.
 @tracks: GList with tracks to be in playlist file.
 ******************************************************************/
void export_tracks_to_playlist_file(GList *tracks) {
    GtkWidget *win;
    GtkWidget *options;
    struct fcd *fcd = g_malloc0(sizeof(struct fcd));
    GtkWidget
            *fc =
                    gtk_file_chooser_dialog_new(_("Create Playlist File"), GTK_WINDOW (gtkpod_app), GTK_FILE_CHOOSER_ACTION_SAVE,
                                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_APPLY, RESPONSE_APPLY, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
    GtkBuilder *export_playlist_xml;

    gchar *glade_path = g_build_filename(get_glade_dir(), "exporter.xml", NULL);
    export_playlist_xml = gtkpod_builder_xml_new(glade_path);
    win = gtkpod_builder_xml_get_widget(export_playlist_xml, "export_playlist_file_options");
    g_free(glade_path);

    options = gtkpod_builder_xml_get_widget(export_playlist_xml, "ep_options_frame");

    fcd->builder = export_playlist_xml;

    /* Information needed to clean up later */
    fcd->tracks = g_list_copy(tracks);
    fcd->errors = g_string_new("");

    /* according to GTK FAQ: move a widget to a new parent */
    g_object_ref(options);
    gtk_container_remove(GTK_CONTAINER (win), options);
    /* set extra options */
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER (fc), options);
    g_object_unref(options);

    gtk_widget_destroy(win);

    /* set last folder */
    option_set_folder(GTK_FILE_CHOOSER (fc), EXPORT_PLAYLIST_FILE_PATH);
    /* set last type */
    option_set_radio_button(export_playlist_xml, EXPORT_PLAYLIST_FILE_TYPE, ExportPlaylistFileTypeW, 0);
    /* set last source */
    option_set_radio_button(export_playlist_xml, EXPORT_PLAYLIST_FILE_SOURCE, ExportPlaylistFileSourceW, 0);
    /* set last template */
    option_set_string(export_playlist_xml, EXPORT_PLAYLIST_FILE_TPL, EXPORT_PLAYLIST_FILE_TPL_DFLT);

    /* catch response codes */
    g_signal_connect (fc, "response",
            G_CALLBACK (export_playlist_file_response),
            fcd);

    gtk_widget_show(fc);
    block_widgets();
}

/*
 Playlists examples:

 Simple M3U playlist:


 C:\My Music\Pink Floyd\1979---The_Wall_CD1\1.In_The_Flesh.mp3
 C:\My Music\Pink Floyd\1979---The_Wall_CD1\10.One_Of_My_Turns.mp3


 Extended M3U playlist:


 #EXTM3U
 #EXTINF:199,Pink Floyd - In The Flesh
 R:\Music\Pink Floyd\1979---The_Wall_CD1\1.In_The_Flesh.mp3
 #EXTINF:217,Pink Floyd - One Of My Turns
 R:\Music\Pink Floyd\1979---The_Wall_CD1\10.One_Of_My_Turns.mp3


 PLS playlist:


 [playlist]
 File1=C:\My Music\Pink Floyd\1979---The_Wall_CD1\1.In_The_Flesh.mp3
 Title1=Pink Floyd - In The Flesh
 Length1=199
 File2=C:\My Music\Pink Floyd\1979---The_Wall_CD1\10.One_Of_My_Turns.mp3
 Title2=Pink Floyd - One Of My Turns
 Length2=217
 NumberOfEntries=2
 Version=2
 */
