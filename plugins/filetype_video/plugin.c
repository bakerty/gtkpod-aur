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

#include <glib.h>
#include "libgtkpod/gtkpod_app_iface.h"
#include "libgtkpod/filetype_iface.h"
#include "plugin.h"
#include "videofile.h"

/* Parent class. Part of standard class definition */
static gpointer parent_class;

static gboolean activate_plugin(AnjutaPlugin *plugin) {
    VideoFileTypePlugin *video_filetype_plugin;

    video_filetype_plugin = (VideoFileTypePlugin*) plugin;
    g_return_val_if_fail(FILE_IS_TYPE(video_filetype_plugin), TRUE);

    gtkpod_register_filetype(FILE_TYPE(video_filetype_plugin));

    return TRUE; /* FALSE if activation failed */
}

static gboolean deactivate_plugin(AnjutaPlugin *plugin) {
    VideoFileTypePlugin *video_filetype_plugin;

    video_filetype_plugin = (VideoFileTypePlugin*) plugin;
    gtkpod_unregister_filetype(FILE_TYPE(video_filetype_plugin));

    /* FALSE if plugin doesn't want to deactivate */
    return TRUE;
}

static void video_filetype_plugin_instance_init(GObject *obj) {
//    VideoFileTypePlugin *plugin = (VideoFileTypePlugin*) obj;
}

static void video_filetype_plugin_class_init(GObjectClass *klass) {
    AnjutaPluginClass *plugin_class = ANJUTA_PLUGIN_CLASS (klass);

    parent_class = g_type_class_peek_parent(klass);

    plugin_class->activate = activate_plugin;
    plugin_class->deactivate = deactivate_plugin;
}

static void video_filetype_iface_init(FileTypeInterface *iface) {
    iface->category = VIDEO;
    iface->description = _("Generic Video file type");
    iface->name = "generic video";
    iface->suffixes = g_list_append(iface->suffixes, "mov");
    iface->suffixes = g_list_append(iface->suffixes, "mpg");
    iface->suffixes = g_list_append(iface->suffixes, "mpeg");

    iface->get_file_info = video_get_file_info;
    iface->write_file_info = filetype_no_write_file_info; /* FIXME */
    iface->read_soundcheck = filetype_no_soundcheck; /* FIXME */
    iface->read_lyrics = filetype_no_read_lyrics; /* FIXME */
    iface->write_lyrics = filetype_no_write_lyrics; /* FIXME */
    iface->read_gapless = filetype_no_read_gapless; /* FIXME ?? */
    iface->can_convert = video_can_convert;
    iface->get_conversion_cmd = video_get_conversion_cmd;
    iface->get_gain_cmd = filetype_no_gain_cmd;
}

ANJUTA_PLUGIN_BEGIN (VideoFileTypePlugin, video_filetype_plugin);
ANJUTA_PLUGIN_ADD_INTERFACE(video_filetype, FILE_TYPE_TYPE);
ANJUTA_PLUGIN_END;

ANJUTA_SIMPLE_PLUGIN (VideoFileTypePlugin, video_filetype_plugin)
;
