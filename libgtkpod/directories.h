/*
 * directories.h
 *
 *  Created on: 26-Sep-2009
 *      Author: phantomjinx
 */

#ifndef DIRECTORIES_H_
#define DIRECTORIES_H_

#include <config.h>
#include <glib.h>

void init_directories(char *argv[]);

void dispose_directories();

gboolean using_local();

gchar * get_data_dir();

gchar * get_doc_dir();

gchar * get_glade_dir();

gchar * get_icon_dir();

gchar * get_image_dir();

gchar * get_ui_dir();

gchar * get_plugin_dir();

gchar * get_script_dir();

#endif /* DIRECTORIES_H_ */
