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
 */
/**
 * pm_context_menu_init - initialize the right click menu for playlists
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include "display_playlists.h"
#include "playlist_display_context_menu.h"
#include "playlist_display_spl.h"
#include "libgtkpod/gp_itdb.h"
#include "libgtkpod/context_menus.h"
#include "libgtkpod/misc_playlist.h"
#include "libgtkpod/misc_track.h"
#include "libgtkpod/misc.h"
#include "libgtkpod/prefs.h"
#include "libgtkpod/syncdir.h"

static void context_menu_delete_playlist_head(GtkMenuItem *mi, gpointer data) {
    DeleteAction deleteaction = GPOINTER_TO_INT (data);
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

void context_menu_delete_track_head(GtkMenuItem *mi, gpointer data) {
    DeleteAction deleteaction = GPOINTER_TO_INT (data);
    GList *playlists = pm_get_selected_playlists();
    if (! playlists) {
        message_sb_no_playlist_selected();
        return;
    }

    while (playlists) {
        Playlist *pl = playlists->data;
        if (pl) {
            gtkpod_set_current_playlist(pl);
            delete_track_head(deleteaction);
        }
        playlists = playlists->next;
    }
}

static GtkWidget *add_delete_all_tracks_from_ipod(GtkWidget *menu) {
    GtkWidget *mi;
    GtkWidget *sub;

    mi = hookup_menu_item(menu, _("Remove All Tracks from iPod"), GTK_STOCK_DELETE, NULL, NULL);
    sub = gtk_menu_new();
    gtk_widget_show(sub);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM (mi), sub);
    hookup_menu_item(sub, _("I'm sure"), NULL, G_CALLBACK (context_menu_delete_track_head), GINT_TO_POINTER (DELETE_ACTION_IPOD));
    return mi;
}

static GtkWidget *add_delete_all_tracks_from_database(GtkWidget *menu) {
    GtkWidget *mi;
    GtkWidget *sub;

    mi = hookup_menu_item(menu, _("Remove All Tracks from Database"), GTK_STOCK_DELETE, NULL, NULL);
    sub = gtk_menu_new();
    gtk_widget_show(sub);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM (mi), sub);
    hookup_menu_item(sub, _("I'm sure"), NULL, G_CALLBACK (context_menu_delete_track_head), GINT_TO_POINTER (DELETE_ACTION_DATABASE));
    return mi;
}

static GtkWidget *add_delete_all_podcasts_from_ipod(GtkWidget *menu) {
    GtkWidget *mi;
    GtkWidget *sub;

    mi = hookup_menu_item(menu, _("Remove All Podcasts from iPod"), GTK_STOCK_DELETE, NULL, NULL);
    sub = gtk_menu_new();
    gtk_widget_show(sub);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM (mi), sub);
    hookup_menu_item(sub, _("I'm sure"), NULL, G_CALLBACK (context_menu_delete_track_head), GINT_TO_POINTER (DELETE_ACTION_IPOD));
    return mi;
}

static GtkWidget *add_delete_playlist_including_tracks_ipod(GtkWidget *menu) {
    return hookup_menu_item(menu, _("Delete Including Tracks"), GTK_STOCK_DELETE, G_CALLBACK (context_menu_delete_playlist_head), GINT_TO_POINTER (DELETE_ACTION_IPOD));
}

static GtkWidget *add_delete_playlist_including_tracks_database(GtkWidget *menu) {
    return hookup_menu_item(menu, _("Delete Including Tracks (Database)"), GTK_STOCK_DELETE, G_CALLBACK (context_menu_delete_playlist_head), GINT_TO_POINTER (DELETE_ACTION_DATABASE));
}

static GtkWidget *add_delete_playlist_including_tracks_harddisk(GtkWidget *menu) {
    return hookup_menu_item(menu, _("Delete Including Tracks (Harddisk)"), GTK_STOCK_DELETE, G_CALLBACK (context_menu_delete_playlist_head), GINT_TO_POINTER (DELETE_ACTION_LOCAL));
}

static GtkWidget *add_delete_playlist_but_keep_tracks(GtkWidget *menu) {
    return hookup_menu_item(menu, _("Delete But Keep Tracks"), GTK_STOCK_DELETE, G_CALLBACK (context_menu_delete_playlist_head), GINT_TO_POINTER (DELETE_ACTION_PLAYLIST));
}

static void copy_selected_playlists_to_target_itdb(GtkMenuItem *mi, gpointer *userdata) {
    iTunesDB *t_itdb = *userdata;
    g_return_if_fail (t_itdb);

    GList *playlists = pm_get_selected_playlists();
    while(playlists) {
        Playlist *pl = playlists->data;
        copy_playlist_to_target_itdb(pl, t_itdb);
        playlists = playlists->next;
    }
}



static void copy_selected_playlists_to_target_playlist(GtkMenuItem *mi, gpointer *userdata) {
    Playlist *t_pl = *userdata;
    g_return_if_fail (t_pl);

    GList *playlists = pm_get_selected_playlists();
    while(playlists) {
        Playlist *pl = playlists->data;
        copy_playlist_to_target_playlist(pl, t_pl);
        playlists = playlists->next;
    }
}

static GtkWidget *add_copy_selected_playlists_to_target_itdb(GtkWidget *menu, const gchar *title) {
    GtkWidget *mi;
    GtkWidget *sub;
    GtkWidget *pl_mi;
    GtkWidget *pl_sub;
    GList *itdbs;
    GList *db;
    struct itdbs_head *itdbs_head;
    iTunesDB *itdb;
    const gchar *stock_id = NULL;
    Playlist *pl;

    itdbs_head = gp_get_itdbs_head();

    mi = hookup_menu_item(menu, title, GTK_STOCK_COPY, NULL, NULL);
    sub = gtk_menu_new();
    gtk_widget_show(sub);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM (mi), sub);

    for (itdbs = itdbs_head->itdbs; itdbs; itdbs = itdbs->next) {
        itdb = itdbs->data;
        ExtraiTunesDBData *eitdb = itdb->userdata;
        if (itdb->usertype & GP_ITDB_TYPE_LOCAL) {
            stock_id = GTK_STOCK_HARDDISK;
        }
        else {
            if (eitdb->itdb_imported) {
                stock_id = GTK_STOCK_CONNECT;
            }
            else {
                stock_id = GTK_STOCK_DISCONNECT;
            }
        }
        pl_mi = hookup_menu_item(sub, _(itdb_playlist_mpl(itdb)->name), stock_id, NULL, NULL);
        pl_sub = gtk_menu_new();
        gtk_widget_show(pl_sub);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM (pl_mi), pl_sub);
        hookup_menu_item(pl_sub, _(itdb_playlist_mpl(itdb)->name), stock_id, G_CALLBACK(copy_selected_playlists_to_target_itdb), &itdbs->data);
        add_separator(pl_sub);
        for (db = itdb->playlists; db; db = db->next) {
            pl = db->data;
            if (!itdb_playlist_is_mpl(pl)) {
                if (pl->is_spl)
                    stock_id = GTK_STOCK_PROPERTIES;
                else
                    stock_id = GTK_STOCK_JUSTIFY_LEFT;
                hookup_menu_item(pl_sub, _(pl->name), stock_id, G_CALLBACK(copy_selected_playlists_to_target_playlist), &db->data);
            }
        }
    }
    return mi;
}

/* Edit selected smart playlist */
static void edit_spl(GtkMenuItem *mi, gpointer data) {
    Playlist *pl = gtkpod_get_current_playlist();

    if (pl)
        spl_edit(pl);
}

static GtkWidget *add_edit_smart_playlist(GtkWidget *menu) {
    return hookup_menu_item(menu, _("Edit Smart Playlist"), GTK_STOCK_PROPERTIES, G_CALLBACK (edit_spl), NULL);
}

/* Display repository options */
static void edit_properties(GtkMenuItem *mi, gpointer data) {
    g_return_if_fail (gtkpod_get_current_playlist());

    gtkpod_edit_repository(gtkpod_get_current_playlist()->itdb, gtkpod_get_current_playlist());
}

/* Open photo editor */
static void open_photo_editor(GtkMenuItem *mi, gpointer data) {
    g_return_if_fail(gtkpod_get_current_itdb());

    gtkpod_edit_photos(gtkpod_get_current_itdb());
}

/* Save Changes */
static void save_changes(GtkMenuItem *mi, gpointer data) {
    g_return_if_fail (gtkpod_get_current_playlist());

    GList *playlists = pm_get_selected_playlists();
    while (playlists) {
        Playlist *pl = playlists->data;
        gp_save_itdb(pl->itdb);
        playlists = playlists->next;
    }
}

/* Load an itdb */
static void load_ipod(GtkMenuItem *mi, gpointer data) {
    g_return_if_fail (gtkpod_get_current_playlist());
    gp_load_ipod(gtkpod_get_current_playlist()->itdb);
}

static void eject_ipod(GtkMenuItem *mi, gpointer data) {
    iTunesDB *itdb;
    ExtraiTunesDBData *eitdb;

    /* all of the checks below indicate a programming error -> give a
     warning through the g_..._fail macros */
    g_return_if_fail (gtkpod_get_current_playlist());
    itdb = gtkpod_get_current_playlist()->itdb;
    g_return_if_fail (itdb);
    g_return_if_fail (itdb->usertype & GP_ITDB_TYPE_IPOD);
    eitdb = itdb->userdata;
    g_return_if_fail (eitdb);
    g_return_if_fail (eitdb->itdb_imported == TRUE);

    gp_eject_ipod(itdb);
}

static GtkWidget *add_edit_ipod_properties(GtkWidget *menu) {
    if (!gtkpod_has_repository_editor())
        return menu;

    return hookup_menu_item(menu, _("Edit iPod Properties"), GTK_STOCK_PREFERENCES, G_CALLBACK (edit_properties), NULL);
}

static GtkWidget *add_edit_repository_properties(GtkWidget *menu) {
    if (!gtkpod_has_repository_editor())
            return menu;

    return hookup_menu_item(menu, _("Edit Repository Properties"), GTK_STOCK_PREFERENCES, G_CALLBACK (edit_properties), NULL);
}

static GtkWidget *add_open_photo_editor(GtkWidget *menu) {
    iTunesDB *itdb = gtkpod_get_current_itdb();
    if (!itdb)
        return menu;

    if (! itdb_device_supports_photo(itdb->device))
        return menu;

    if (!gtkpod_has_photo_editor())
        return menu;

    return hookup_menu_item(menu, _("Open Photo Editor"), GTK_STOCK_SELECT_COLOR, G_CALLBACK (open_photo_editor), NULL);
}

static GtkWidget *add_edit_playlist_properties(GtkWidget *menu) {
    if (!gtkpod_has_repository_editor())
            return menu;

    return hookup_menu_item(menu, _("Edit Playlist Properties"), GTK_STOCK_PREFERENCES, G_CALLBACK (edit_properties), NULL);
}

static GtkWidget *add_load_ipod(GtkWidget *menu) {
    return hookup_menu_item(menu, _("Load iPod"), GTK_STOCK_CONNECT, G_CALLBACK (load_ipod), NULL);
}

static GtkWidget *add_save_changes(GtkWidget *menu) {
    return hookup_menu_item(menu, _("Save Changes"), GTK_STOCK_SAVE, G_CALLBACK (save_changes), NULL);
}

static GtkWidget *add_eject_ipod(GtkWidget *menu) {
    return hookup_menu_item(menu, _("Eject iPod"), GTK_STOCK_DISCONNECT, G_CALLBACK (eject_ipod), NULL);
}

/*
 * sync_dirs_ entries - sync the directories of the selected playlist
 *
 * @mi - the menu item selected
 * @data - Ignored, should be NULL
 */
static void sync_dirs(GtkMenuItem *mi, gpointer data) {
    GList *playlists = pm_get_selected_playlists();
    while (playlists) {
        Playlist *pl = playlists->data;
        sync_playlist(pl, NULL, KEY_SYNC_CONFIRM_DIRS, 0, KEY_SYNC_DELETE_TRACKS, 0, KEY_SYNC_CONFIRM_DELETE, 0, KEY_SYNC_SHOW_SUMMARY, 0);
        playlists = playlists->next;
    }
}

static GtkWidget *add_sync_playlist_with_dirs(GtkWidget *menu) {
    return hookup_menu_item(menu, _("Sync Playlist with Dir(s)"), GTK_STOCK_REFRESH, G_CALLBACK (sync_dirs), NULL);
}

static void update_multi_tracks_from_file(GtkMenuItem *mi, gpointer data) {
    GList *playlists = pm_get_selected_playlists();
    while (playlists) {
        Playlist *pl = playlists->data;
        update_tracks(pl->members);
        playlists = playlists->next;
    }
}

GtkWidget *add_multi_update_tracks_from_file(GtkWidget *menu) {
    return hookup_menu_item(menu, _("Update Tracks from File"), GTK_STOCK_REFRESH, G_CALLBACK (update_multi_tracks_from_file), NULL);
}

static void _populate_single_playlist_menu(GtkWidget *menu) {

    Playlist *pl = pm_get_first_selected_playlist();
    g_return_if_fail(pl);

    // Ensure that all the tracks in the playlist are the current selected tracks
    gtkpod_set_selected_tracks(pl->members);

    ExtraiTunesDBData *eitdb;
    iTunesDB *itdb = pl->itdb;
    g_return_if_fail (itdb);
    eitdb = itdb->userdata;
    g_return_if_fail (eitdb);

    if (itdb->usertype & GP_ITDB_TYPE_IPOD) {
        if (eitdb->itdb_imported) {
            add_exec_commands(menu);

            add_separator(menu);
            if (itdb_playlist_is_mpl(pl)) {
                add_delete_all_tracks_from_ipod(menu);
            }
            else if (itdb_playlist_is_podcasts(pl)) {
                add_delete_all_podcasts_from_ipod(menu);
            }
            else {
                GtkWidget *delete_menu = add_sub_menu(menu, _("Delete"), GTK_STOCK_DELETE);
                add_delete_playlist_including_tracks_ipod(delete_menu);
                add_delete_playlist_but_keep_tracks(delete_menu);
            }
            add_separator(menu);
            add_copy_selected_playlists_to_target_itdb(menu, _("Copy selected playlist to..."));

            add_separator(menu);
            add_update_tracks_from_file(menu);
            if (!pl->is_spl) {
                add_sync_playlist_with_dirs(menu);
            }

            add_separator(menu);
            add_edit_track_details(menu);
            if (pl->is_spl) {
                add_edit_smart_playlist(menu);
            }
            if (itdb_playlist_is_mpl(pl)) {
                add_edit_ipod_properties(menu);
            }
            else {
                add_edit_playlist_properties(menu);
            }
            add_open_photo_editor(menu);
            add_eject_ipod(menu);
        }
        else { /* not imported */
            add_edit_ipod_properties(menu);
            add_separator(menu);
            add_load_ipod(menu);
        }
    }
    else if (itdb->usertype & GP_ITDB_TYPE_LOCAL) {
        add_exec_commands(menu);
        add_separator(menu);

        if (itdb_playlist_is_mpl(pl)) {
            add_delete_all_tracks_from_database(menu);
        }
        else {
            GtkWidget *delete_menu = add_sub_menu(menu, _("Delete"), GTK_STOCK_DELETE);
            add_delete_playlist_including_tracks_database(delete_menu);
            add_delete_playlist_including_tracks_harddisk(delete_menu);
            add_delete_playlist_but_keep_tracks(delete_menu);
        }
        add_copy_selected_playlists_to_target_itdb(menu, _("Copy selected playlist to..."));
        add_separator(menu);
        add_update_tracks_from_file(menu);
        if (!pl->is_spl) {
            add_sync_playlist_with_dirs(menu);
        }

        add_separator(menu);
        add_edit_track_details(menu);
        if (pl->is_spl) {
            add_edit_smart_playlist(menu);
        }
        if (itdb_playlist_is_mpl(pl)) {
            add_edit_repository_properties(menu);
        }
        else {
            add_edit_playlist_properties(menu);
        }
    }

    if (eitdb->data_changed) {
        add_save_changes(menu);
    }
}

static void _populate_multi_playlist_menu(GtkWidget *menu) {

    GtkWidget *delete_menu = add_sub_menu(menu, _("Delete"), GTK_STOCK_DELETE);
    add_delete_playlist_including_tracks_ipod(delete_menu);
    add_delete_playlist_but_keep_tracks(delete_menu);

    add_separator(menu);
    add_copy_selected_playlists_to_target_itdb(menu, _("Copy selected playlist to..."));

    add_separator(menu);
    add_multi_update_tracks_from_file(menu);

    add_sync_playlist_with_dirs(menu);

    add_save_changes(menu);
}

void pm_context_menu_init(void) {
    GtkWidget *menu = NULL;

    if (widgets_blocked)
        return;

    pm_stop_editing(TRUE);

    if (!pm_is_playlist_selected())
        return;

    menu = gtk_menu_new();

    if (pm_get_selected_playlist_count() == 1) {
        _populate_single_playlist_menu(menu);
    } else {
        _populate_multi_playlist_menu(menu);
    }

    /*
     * button should be button 0 as per the docs because we're calling
     * from a button release event
     */
    if (menu) {
        gtk_menu_popup(GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
    }
}
