/*
 |  Copyright (C) 2007 Maia Kozheva <sikon at users sourceforge net>
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
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include "infoview.h"
#include "plugin.h"
#include "info.h"
#include "libgtkpod/misc.h"
#include "libgtkpod/prefs.h"
#include "libgtkpod/misc_track.h"
#include "libgtkpod/directories.h"

const gchar *glade_file_path;

enum info_view_columns {
    C_DESCRIPTION = 0, C_TOTAL_IPOD, C_TOTAL_LOCAL, C_SELECTED_PLAYLIST, C_DISPLAYED_TRACKS, C_SELECTED_TRACKS, C_COUNT
};

static const gchar *column_headers[] =
    {
        "", N_("Total\n(iPod)"), N_("Total\n(local)"), N_("Selected\nPlaylist"), N_("Displayed\nTracks"),
        N_("Selected\nTracks"), NULL };

enum info_view_rows {
    R_NUMBER_OF_TRACKS, R_PLAY_TIME, R_FILE_SIZE, R_NUMBER_OF_PLAYLISTS, R_DELETED_TRACKS, R_FILE_SIZE_DELETED, R_NON_TRANSFERRED_TRACKS, R_FILE_SIZE_NON_TRANSFERRED, R_EFFECTIVE_FREE_SPACE, R_COUNT
};

static const gchar *row_headers[] =
    {
        N_("Number of tracks"), N_("Play time"), N_("File size"), N_("Number of playlists"), N_("Deleted tracks"),
        N_("File size (deleted)"), N_("Non-transferred tracks"), N_("File size (non-transferred)"),
        N_("Effective free space"), NULL };

static InfoView *info_view = NULL;

static void info_view_set_text(gint row, gint column, const gchar *text) {
    GtkTreeIter iter;

    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL (info_view->store), &iter, NULL, row);
    gtk_list_store_set(info_view->store, &iter, column, text, -1);
}

static void info_view_set_uint(gint row, gint column, guint32 value) {
    gchar buf[10];
    sprintf(buf, "%u", value);
    info_view_set_text(row, column, buf);
}

static void info_view_set_time(gint row, gint column, guint32 secs) {
    gchar *str = g_strdup_printf("%u:%02u:%02u", secs / 3600, (secs % 3600) / 60, secs % 60);

    info_view_set_text(row, column, str);
    g_free(str);
}

static void info_view_set_size(gint row, gint column, gdouble size) {
    gchar *str = get_filesize_as_string(size);
    info_view_set_text(row, column, str);
    g_free(str);
}

static void update_view_generic(gint column, GList *list) {
    guint32 tracks, playtime; /* playtime in secs */
    gdouble filesize; /* in bytes */

    g_return_if_fail (info_view);
    fill_in_info(list, &tracks, &playtime, &filesize);
    info_view_set_uint(R_NUMBER_OF_TRACKS, column, tracks);
    info_view_set_time(R_PLAY_TIME, column, playtime);
    info_view_set_size(R_FILE_SIZE, column, filesize);
}

static void on_info_view_update_track_view() {
    update_view_generic(C_DISPLAYED_TRACKS, gtkpod_get_displayed_tracks());

    update_view_generic(C_SELECTED_TRACKS, gtkpod_get_selected_tracks());
}

static void on_info_view_update_playlist_view() {
    Playlist *pl = gtkpod_get_current_playlist();
    if (!pl)
        return;
    update_view_generic(C_SELECTED_PLAYLIST, pl->members);

}

static void on_info_view_update_totals_view() {
    Playlist *pl;
    iTunesDB *itdb;
    gdouble nt_filesize, del_filesize;
    guint32 nt_tracks, del_tracks;

    itdb = get_itdb_ipod();

    if (itdb) {
        pl = itdb_playlist_mpl(itdb);
        g_return_if_fail (pl);
        update_view_generic(C_TOTAL_IPOD, pl->members);

        info_view_set_uint(R_NUMBER_OF_PLAYLISTS, C_TOTAL_IPOD, itdb_playlists_number(itdb) - 1);

        gp_info_nontransferred_tracks(itdb, &nt_filesize, &nt_tracks);
        gp_info_deleted_tracks(itdb, &del_filesize, &del_tracks);

        info_view_set_uint(R_NON_TRANSFERRED_TRACKS, C_TOTAL_IPOD, nt_tracks);
        info_view_set_size(R_FILE_SIZE_NON_TRANSFERRED, C_TOTAL_IPOD, nt_filesize);
        info_view_set_uint(R_DELETED_TRACKS, C_TOTAL_IPOD, del_tracks);
        info_view_set_size(R_FILE_SIZE_DELETED, C_TOTAL_IPOD, del_filesize);

        if (!get_offline(itdb)) {
            if (ipod_connected()) {
                gdouble free_space = get_ipod_free_space() + del_filesize - nt_filesize;
                info_view_set_size(R_EFFECTIVE_FREE_SPACE, C_TOTAL_IPOD, free_space);
            }
            else
                info_view_set_text(R_EFFECTIVE_FREE_SPACE, C_TOTAL_IPOD, _("n/c"));
        }
        else
            info_view_set_text(R_EFFECTIVE_FREE_SPACE, C_TOTAL_IPOD, _("offline"));
    }

    itdb = get_itdb_local();

    if (itdb) {
        pl = itdb_playlist_mpl(itdb);
        g_return_if_fail (pl);
        update_view_generic(C_TOTAL_LOCAL, pl->members);

        info_view_set_uint(R_NUMBER_OF_PLAYLISTS, C_TOTAL_LOCAL, itdb_playlists_number(itdb) - 1);
    }
}

static void setup_info_view() {
    gint i;

    info_view->store
            = gtk_list_store_new(C_COUNT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    for (i = 0; column_headers[i]; i++) {
        const gchar *hdr = column_headers[i][0] ? _(column_headers[i]) : column_headers[i];
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

        g_object_set(renderer, "editable", i > 0, "foreground", i % 2 ? "#000000" : "#000000", NULL);

        gtk_tree_view_insert_column_with_attributes(info_view->tree, -1, hdr, renderer, "markup", i, NULL);
    }

    for (i = 0; row_headers[i]; i++) {
        GtkTreeIter iter;
        gchar *text = g_strdup_printf("<b>%s</b>", _(row_headers[i]));

        gtk_list_store_append(info_view->store, &iter);
        gtk_list_store_set(info_view->store, &iter, C_DESCRIPTION, text, -1);
        g_free(text);
    }

    gtk_tree_view_set_model(info_view->tree, GTK_TREE_MODEL (info_view->store));
    g_object_unref(info_view->store);

    register_info_update_track_view(on_info_view_update_track_view);
    register_info_update_playlist_view(on_info_view_update_playlist_view);
    register_info_update_totals_view(on_info_view_update_totals_view);
    info_update();
}

static void create_info_view() {
    info_view = g_malloc0(sizeof(InfoView));

    /* Add widget in Shell. */
    info_display_plugin->info_window = gtk_scrolled_window_new(NULL, NULL);
    g_object_ref(info_display_plugin->info_window);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (info_display_plugin->info_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (info_display_plugin->info_window), GTK_SHADOW_IN);
    info_view->window = info_display_plugin->info_window;

    info_view->tree = GTK_TREE_VIEW (gtk_tree_view_new());
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(info_view->window), GTK_WIDGET (info_view->tree));
    anjuta_shell_add_widget(ANJUTA_PLUGIN(info_display_plugin)->shell, info_view->window, "InfoDisplayPlugin", _("  Repository Information"), NULL, ANJUTA_SHELL_PLACEMENT_BOTTOM, NULL);

    setup_info_view();
}

void open_info_view() {
    if (!info_view || !info_view->window) {
        create_info_view();
    }
    else if (!gtk_widget_get_realized(info_view->window)) {
        gtkpod_display_widget(info_view->window);
    }

    gtk_widget_show_all(info_view->window);
}

void destroy_info_view() {
    if (!info_view)
        return;

    unregister_info_update_track_view(on_info_view_update_track_view);
    unregister_info_update_playlist_view(on_info_view_update_playlist_view);
    unregister_info_update_totals_view(on_info_view_update_totals_view);

    if (info_view->window && GTK_IS_WIDGET(info_view->window)) {
        gtk_widget_destroy(info_view->window);
    }

    info_view->tree = NULL;
    info_view->store = NULL;
    info_view = NULL;
}
