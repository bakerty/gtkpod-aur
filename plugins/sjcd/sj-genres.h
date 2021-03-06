/* 
 * Copyright (C) 2007 Jonh Wendell <wendell@bani.com.br>
 *
 * Sound Juicer - sj-genres.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Jonh Wendell <wendell@bani.com.br>
 */

#ifndef SJ_GENRES_H
#define SJ_GENRES_H

#include <gtk/gtk.h>

void setup_genre_entry (GtkWidget *entry);
void save_genre (GtkWidget *entry);

#endif /* SJ_GENRES_H */
