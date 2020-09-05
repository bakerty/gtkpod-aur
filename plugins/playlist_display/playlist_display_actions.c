/*
 |  Copyright (C) 2002-2010 Jorg Schuler <jcsjcs at users sourceforge net>
 |                                          Paul Richardson <phantom_sf at users.sourceforge.net>
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

#include "playlist_display_actions.h"
#include "display_playlists.h"
#include "playlist_display_spl.h"
#include "libgtkpod/file.h"
#include "libgtkpod/gtkpod_app_iface.h"
#include "libgtkpod/gp_itdb.h"
#include "libgtkpod/prefs.h"
#include "libgtkpod/misc.h"
#include "libgtkpod/misc_track.h"
#include "libgtkpod/misc_playlist.h"
#include "libgtkpod/file.h"
#include "libgtkpod/syncdir.h"
#include "libgtkpod/tools.h"
#include <gdk/gdk.h>

/* Callback after directories to add have been selected */
static void add_selected_dirs(GSList *names, Playlist *db_active_pl) {
    gboolean result = TRUE;
    GString *errors = g_string_new("");
    GError *error = NULL;

    g_return_if_fail (names);
    g_return_if_fail (db_active_pl);

    if (names) {
        gtkpod_statusbar_busy_push();

        GSList* currentnode;
        for (currentnode = names; currentnode; currentnode = currentnode->next) {
            result
                    &= add_directory_by_name(db_active_pl->itdb, currentnode->data, db_active_pl, prefs_get_int("add_recursively"), NULL, NULL, &error);
            if (error) {
                gchar *buf = g_strdup_printf(_("%s\n"), error->message);
                g_string_append(errors, buf);
                g_free(buf);
                g_error_free(error);
                error = NULL;
            }
        }

        /* Final save of itdb */
        gp_save_itdb(db_active_pl->itdb);
        /* clear log of non-updated tracks */
        display_non_updated((void *) -1, NULL);
        /* display log of updated tracks */
        display_updated(NULL, NULL);
        /* display log of detected duplicates */
        gp_duplicate_remove(NULL, NULL);

        gtkpod_set_current_playlist(db_active_pl);
        gtkpod_statusbar_busy_pop();

        /* Were all files successfully added? */
        if (result == FALSE) {
            if (errors->len > 0) {
                gtkpod_confirmation(-1, /* gint id, */
                        TRUE, /* gboolean modal, */
                        _("Directory Addition Errors"), /* title */
                        _(" Some directories were not added successfully"), /* label */
                        errors->str, /* scrolled text */
                        NULL, 0, NULL, /* option 1 */
                        NULL, 0, NULL, /* option 2 */
                        TRUE, /* gboolean confirm_again, */
                        "show_file_addition_errors",/* confirm_again_key,*/
                        CONF_NULL_HANDLER, /* ConfHandler ok_handler,*/
                        NULL, /* don't show "Apply" button */
                        NULL, /* cancel_handler,*/
                        NULL, /* gpointer user_data1,*/
                        NULL); /* gpointer user_data2,*/
            }
            else {
                gtkpod_warning(_("Some directories failed to be added but no errors were reported."));
            }
        }

        g_string_free(errors, TRUE);
    }
}

static gboolean add_selected_dirs_cb(gpointer data) {
    GSList *names = data;
    Playlist *pl = gtkpod_get_current_playlist();
    add_selected_dirs(names, pl);

    g_slist_foreach(names, (GFunc) g_free, NULL);
    g_slist_free(names);
    return FALSE;
}

static void create_add_directories_dialog(Playlist *pl) {
    GSList* names = NULL; /* List of selected items */
    GtkWidget *dialog;

    if (!pl) {
        gtkpod_warning_simple(_("Please select a playlist or repository before adding tracks."));
        return;
    }

    dialog
            = gtk_file_chooser_dialog_new(_("Add Folder"), GTK_WINDOW (gtkpod_app), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_ADD, GTK_RESPONSE_ACCEPT, NULL);

    /* Allow multiple selection of directories. */
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);

    /* Set same directory as the last browsed directory. */
    gchar *last_dir = prefs_get_string("last_dir_browsed");
    if (last_dir) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (dialog), last_dir);
        g_free(last_dir);
    }

    if (gtk_dialog_run(GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        names = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER (dialog));
        prefs_set_string("last_dir_browsed", gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER (dialog)));
    }

    gtk_widget_destroy(dialog);

    if (names) {
        gdk_threads_add_idle((GSourceFunc) add_selected_dirs_cb, names);
    }
}

/* OK Button */
static void fileselection_add_playlists(GSList* names, iTunesDB *itdb) {
    GSList* gsl;
    GString *errors = g_string_new("");

    /* Get the names of the playlist(s) and add them */

    g_return_if_fail (itdb);

    block_widgets();

    gtkpod_statusbar_busy_push();
    for (gsl = names; gsl; gsl = gsl->next) {
        GError *error = NULL;
        add_playlist_by_filename(itdb, gsl->data, NULL, -1, NULL, NULL, &error);
        if (error) {
            gchar *buf = g_strdup_printf(_("'%s'\n"), error->message);
            g_string_append(errors, buf);
            g_free(buf);
            g_error_free(error);
            error = NULL;
        }
    }

    release_widgets();

    /* clear log of non-updated tracks */
    display_non_updated((void *) -1, NULL);

    /* display log of updated tracks */
    display_updated(NULL, NULL);

    /* display log of detected duplicates */
    gp_duplicate_remove(NULL, NULL);
    gtkpod_statusbar_busy_pop();
    gtkpod_tracks_statusbar_update();
    gtkpod_set_current_playlist(itdb_playlist_mpl(itdb));

    if (errors->len > 0) {
        gtkpod_confirmation(-1, /* gint id, */
                TRUE, /* gboolean modal, */
                _("Playlist Addition Errors"), /* title */
                _("Some tracks in the playlist were not added successfully"), /* label */
                errors->str, /* scrolled text */
                NULL, 0, NULL, /* option 1 */
                NULL, 0, NULL, /* option 2 */
                TRUE, /* gboolean confirm_again, */
                "show_playlist_addition_errors",/* confirm_again_key,*/
                CONF_NULL_HANDLER, /* ConfHandler ok_handler,*/
                NULL, /* don't show "Apply" button */
                NULL, /* cancel_handler,*/
                NULL, /* gpointer user_data1,*/
                NULL); /* gpointer user_data2,*/
    }
    else {
        gtkpod_warning(_("Some tracks failed to be added but no errors were reported."));
    }

    g_string_free(errors, TRUE);
}

/* Open a modal file selection dialog with multiple selction enabled */
static GSList* fileselection_get_files(const gchar *title) {
    GtkWidget* fc; /* The file chooser dialog */
    gint response; /* The response of the filechooser */
    gchar *last_dir, *new_dir;
    GSList * files = NULL;

    fc
            = gtk_file_chooser_dialog_new(title, GTK_WINDOW (gtkpod_app), GTK_FILE_CHOOSER_ACTION_OPEN,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

    /* allow multiple selection of files */
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER (fc), TRUE);

    /* set same directory as last time */
    last_dir = prefs_get_string("last_dir_browsed");
    if (last_dir) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (fc), last_dir);
        g_free(last_dir);
    }

    /* Run the dialog */
    response = gtk_dialog_run(GTK_DIALOG(fc));

    /* Handle the response */
    switch (response) {
    case GTK_RESPONSE_ACCEPT:
        new_dir = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER (fc));
        prefs_set_string("last_dir_browsed", new_dir);
        g_free(new_dir);
        files = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER (fc));
        break;
    case GTK_RESPONSE_CANCEL:
        break;
    default: /* Fall through */
        break;
    }
    gtk_widget_destroy(fc);

    return files;
}

/* Open a modal file selection dialog for adding playlist files */
static void create_add_playlists_dialog(iTunesDB *itdb) {
    gchar *str;
    GSList *names;
    ExtraiTunesDBData *eitdb;
    Playlist *mpl;

    if (!itdb) {
        gtkpod_warning_simple(_("Please select a playlist or repository before adding tracks."));
        return;
    }

    eitdb = itdb->userdata;
    g_return_if_fail (eitdb);

    if (!eitdb->itdb_imported) {
        gtkpod_warning_simple(_("Please load the iPod before adding tracks."));
        return;
    }

    mpl = itdb_playlist_mpl(itdb);
    g_return_if_fail (mpl);

    /* Create window title */
    str = g_strdup_printf(_("Add playlist files to '%s'"), mpl->name);

    names = fileselection_get_files(str);
    g_free(str);

    if (!names)
        return;

    fileselection_add_playlists(names, itdb);

    g_slist_foreach(names, (GFunc) g_free, NULL);
    g_slist_free(names);
}

static void fileselection_add_files(GSList* names, Playlist *playlist) {
    GSList* gsl; /* Current node in list */
    gboolean result = TRUE; /* Result of file adding */
    GString *errors = g_string_new("");

    /* If we don't have a playlist to add to, don't add anything */
    g_return_if_fail (playlist);

    block_widgets();

    gtkpod_statusbar_busy_push();

    names = sort_tracknames_list(names);

    /* Get the filenames and add them */
    for (gsl = names; gsl; gsl = gsl->next) {
        GError *error = NULL;
        /* Might be useful to stick a GError on this to return a message if the
         * file fails to be added.
         */
        result
                &= add_track_by_filename(playlist->itdb, gsl->data, playlist, prefs_get_int("add_recursively"), NULL, NULL, &error);
        if (error) {
            gchar *buf = g_strdup_printf(_("%s\n"), error->message);
            g_string_append(errors, buf);
            g_free(buf);
            g_error_free(error);
            error = NULL;
        }
    }

    /* Final save of remaining added tracks */
    gp_save_itdb(playlist->itdb);
    /* clear log of non-updated tracks */
    display_non_updated((void *) -1, NULL);

    /* display log of updated tracks */
    display_updated(NULL, NULL);

    /* display log of detected duplicates */
    gp_duplicate_remove(NULL, NULL);

    gtkpod_statusbar_busy_pop();

    release_widgets();
    gtkpod_set_current_playlist(playlist);

    /* Were all files successfully added? */
    if (result == FALSE) {
        if (errors->len > 0) {
            gtkpod_confirmation(-1, /* gint id, */
                TRUE, /* gboolean modal, */
                _("File Addition Errors"), /* title */
                _("Some files were not added successfully"), /* label */
                errors->str, /* scrolled text */
                NULL, 0, NULL, /* option 1 */
                NULL, 0, NULL, /* option 2 */
                TRUE, /* gboolean confirm_again, */
                "show_file_addition_errors",/* confirm_again_key,*/
                CONF_NULL_HANDLER, /* ConfHandler ok_handler,*/
                NULL, /* don't show "Apply" button */
                NULL, /* cancel_handler,*/
                NULL, /* gpointer user_data1,*/
                NULL); /* gpointer user_data2,*/
        }
        else {
            gtkpod_warning(_("Some tracks failed to be added but no errors were reported."));
        }
    }

    g_string_free(errors, TRUE);
}

static gboolean fileselection_add_files_cb(gpointer data) {
    GSList *names = data;
    Playlist *pl = gtkpod_get_current_playlist();
    fileselection_add_files(names, pl);

    g_slist_foreach(names, (GFunc) g_free, NULL);
    g_slist_free(names);
    return FALSE;
}

/* Open a modal file selection dialog for adding individual files */
static void create_add_files_dialog(Playlist *pl) {
    gchar *str;
    GSList *names;
    iTunesDB *itdb;
    ExtraiTunesDBData *eitdb;
    Playlist *mpl;

    if (!pl) {
        gtkpod_warning_simple(_("Please select a playlist or repository before adding tracks."));
        return;
    }

    itdb = pl->itdb;
    g_return_if_fail (itdb);
    eitdb = itdb->userdata;
    g_return_if_fail (eitdb);

    if (!eitdb->itdb_imported) {
        gtkpod_warning_simple(_("Please load the iPod before adding tracks."));
        return;
    }

    mpl = itdb_playlist_mpl(itdb);
    g_return_if_fail (mpl);

    /* Create window title */
    if (mpl == pl) {
        str = g_strdup_printf(_("Add files to '%s'"), mpl->name);
    }
    else {
        str = g_strdup_printf(_("Add files to '%s/%s'"), mpl->name, pl->name);
    }

    names = fileselection_get_files(str);
    g_free(str);

    if (!names)
        return;

    // Let the dialog close first and add the tracks in the background
    gdk_threads_add_idle((GSourceFunc) fileselection_add_files_cb, names);
}

void on_load_ipod_mi(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();
    if (itdb != NULL) {
        gp_load_ipod(itdb);
    }
}

void on_load_ipods_mi(GtkAction* action, PlaylistDisplayPlugin* plugin) {
    gp_load_ipods();
}

void on_save_changes(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    handle_export();
}

void on_create_add_files(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    Playlist *pl;
    pl = gtkpod_get_current_playlist();
    create_add_files_dialog(pl);
}

void on_create_add_directory(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    Playlist *pl = gtkpod_get_current_playlist();
    create_add_directories_dialog(pl);
}

void on_create_add_playlists(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb;
    itdb = gtkpod_get_current_itdb();
    create_add_playlists_dialog(itdb);
}

/* callback for "add new playlist" button */
void on_new_playlist_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        add_new_pl_or_spl_user_name(itdb, NULL, -1);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

/* callback */
void on_smart_playlist_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        spl_edit_new(itdb, NULL, -1);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

void on_random_playlist_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        generate_random_playlist(itdb);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

void on_pl_containing_displayed_tracks_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    generate_displayed_playlist();
}

void on_pl_containing_selected_tracks_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    generate_selected_playlist();
}

void on_most_rated_tracks_playlist_s1_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        most_rated_pl(itdb);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

void on_most_listened_tracks1_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        most_listened_pl(itdb);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

void on_most_recent_played_tracks_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        last_listened_pl(itdb);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

void on_played_since_last_time1_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        since_last_pl(itdb);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

void on_all_tracks_never_listened_to1_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        never_listened_pl(itdb);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

void on_all_tracks_not_listed_in_any_playlist1_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        generate_not_listed_playlist(itdb);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

void on_pl_for_each_artist_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        generate_category_playlists(itdb, T_ARTIST);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

void on_pl_for_each_album_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        generate_category_playlists(itdb, T_ALBUM);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

void on_pl_for_each_genre_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        generate_category_playlists(itdb, T_GENRE);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

void on_pl_for_each_composer_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        generate_category_playlists(itdb, T_COMPOSER);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

void on_pl_for_each_year_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        generate_category_playlists(itdb, T_YEAR);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

void on_pl_for_each_rating_activate(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    iTunesDB *itdb = gtkpod_get_current_itdb();

    if (itdb) {
        each_rating_pl(itdb);
    }
    else {
        message_sb_no_itdb_selected();
    }
}

static void delete_selected_playlists(DeleteAction deleteaction) {
    GList *playlists = pm_get_selected_playlists();

    if (! playlists) {
        message_sb_no_playlist_selected();
        return;
    }

    while (playlists) {
        Playlist *pl = playlists->data;
        if (pl) {
            gtkpod_set_current_playlist(pl);
            delete_playlist_head(deleteaction);
        }
        playlists = playlists->next;
    }
}

void on_delete_selected_playlists (GtkAction *action, PlaylistDisplayPlugin* plugin)
{
    delete_selected_playlists (DELETE_ACTION_PLAYLIST);
}

void on_delete_selected_playlists_including_tracks_from_harddisk (GtkAction *action, PlaylistDisplayPlugin* plugin)
{
    delete_selected_playlists (DELETE_ACTION_LOCAL);
}

void on_delete_selected_playlists_including_tracks_from_ipod (GtkAction *action, PlaylistDisplayPlugin* plugin)
{
    delete_selected_playlists (DELETE_ACTION_IPOD);
}

void on_delete_selected_playlists_including_tracks_from_database (GtkAction *action, PlaylistDisplayPlugin* plugin)
{
    delete_selected_playlists (DELETE_ACTION_DATABASE);
}

void on_delete_selected_playlists_including_tracks_from_device(GtkAction *action, PlaylistDisplayPlugin* plugin)
{
    iTunesDB *itdb = gtkpod_get_current_itdb();
    if (!itdb)
        return;

    if (itdb->usertype & GP_ITDB_TYPE_IPOD) {
        on_delete_selected_playlists_including_tracks_from_ipod(action, plugin);
    } else if (itdb->usertype & GP_ITDB_TYPE_LOCAL) {
        on_delete_selected_playlists_including_tracks_from_harddisk(action, plugin);
    }
}

void on_update_selected_playlists (GtkAction *action, PlaylistDisplayPlugin* plugin) {

    GList *playlists = pm_get_selected_playlists();

    while (playlists) {
        Playlist *pl = playlists->data;
        if (pl) {
            update_tracks(pl->members);
        }
        playlists = playlists->next;
    }
}

void on_sync_playlists_with_dirs(GtkAction *action, PlaylistDisplayPlugin* plugin) {
    GList *playlists = pm_get_selected_playlists();

    while (playlists) {
        Playlist *pl = playlists->data;
        if (pl) {
            sync_playlist(pl, NULL, KEY_SYNC_CONFIRM_DIRS, 0, KEY_SYNC_DELETE_TRACKS, 0, KEY_SYNC_CONFIRM_DELETE, 0, KEY_SYNC_SHOW_SUMMARY, 0);
        }
        playlists = playlists->next;
    }
}

void on_normalize_selected_playlist (GtkMenuItem *menuitem, gpointer user_data) {
    GList *playlists = pm_get_selected_playlists();
    while (playlists) {
        Playlist *pl = playlists->data;
        if (pl) {
            nm_tracks_list (pl->members);
        }
        playlists = playlists->next;
    }
}
