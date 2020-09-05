/* Time-stamp: <2006-11-23 00:43:34 jcs>
|
|  Copyright (C) 2002 Corey Donohoe <atmos at atmos.org>
|  Copyright (C) 2004-2005 Jorg Schuler <jcsjcs at users.sourceforge.net>
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
|  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
|  USA
|
|  iTunes and iPod are trademarks of Apple
|
|  This product is not supported/written/published by Apple!
|
|  $Id$
*/
#ifndef _GTKPOD_SHA1_H_
#define _GTKPOD_SHA1_H_

#include "gp_itdb.h"

void setup_sha1();
gchar *sha1_hash_on_filename (gchar *name, gboolean silent);
/* Any calls to the following functions immediately return if sha1sums
 * is not on */
Track *sha1_file_exists (iTunesDB *itdb, gchar *file, gboolean silent);
Track *sha1_sha1_exists (iTunesDB *itdb, gchar *sha1);
Track *sha1_track_exists (iTunesDB *itdb, Track *s);
Track *sha1_track_exists_insert (iTunesDB *itdb, Track *s);
void sha1_track_remove (Track *s);
void sha1_free (iTunesDB *itdb);
void sha1_free_eitdb (ExtraiTunesDBData *eitdb);

#endif
