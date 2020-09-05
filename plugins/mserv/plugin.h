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

#ifndef PLUGIN_H_
#define PLUGIN_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <libanjuta/anjuta-plugin.h>

extern GType mserv_plugin_get_type (GTypeModule *module);
#define MSERV_TYPE_PLUGIN         (mserv_plugin_get_type (NULL))
#define MSERV_PLUGIN(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MSERV_TYPE_PLUGIN, MservPlugin))
#define MSERV_PLUGIN_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MSERV_TYPE_PLUGIN, MservPluginClass))
#define MSERV_IS_PLUGIN(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSERV_TYPE_PLUGIN))
#define MSERV_IS_PLUGIN_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MSERV_TYPE_PLUGIN))
#define MSERV_PLUGIN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MSERV_TYPE_PLUGIN, MservPluginClass))

typedef struct _MservPlugin MservPlugin;
typedef struct _MservPluginClass MservPluginClass;

struct _MservPlugin {
    AnjutaPlugin parent;
    gint uiid;
    GtkActionGroup *action_group;
    GtkWidget *prefs;
    gchar *builderpath;
};

struct _MservPluginClass {
    AnjutaPluginClass parent_class;
};

#endif /* PLUGIN_H_ */
