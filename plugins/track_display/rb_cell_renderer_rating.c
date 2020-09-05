/*
 *
 * rb-cell-renderer-rating.c
 * arch-tag: Implementation of star rating GtkTreeView cell renderer
 *
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 * Copyright (C) 2002  Olivier Martin <oleevye@wanadoo.fr>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301  USA.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "rb_cell_renderer_rating.h"
#include "rb_rating_helper.h"

static void rb_cell_renderer_rating_get_property (GObject *object,
						  guint param_id,
						  GValue *value,
						  GParamSpec *pspec);
static void rb_cell_renderer_rating_set_property (GObject *object,
						  guint param_id,
						  const GValue *value,
						  GParamSpec *pspec);
static void rb_cell_renderer_rating_init (RBCellRendererRating *celltext);
static void rb_cell_renderer_rating_class_init (RBCellRendererRatingClass *class);
static void rb_cell_renderer_rating_get_size  (GtkCellRenderer *cell,
					       GtkWidget *widget,
					       const GdkRectangle *rectangle,
					       gint *x_offset,
					       gint *y_offset,
					       gint *width,
					       gint *height);
static void rb_cell_renderer_rating_render (GtkCellRenderer      *cell,
                                                                    cairo_t              *cr,
                                                                    GtkWidget            *widget,
                                                                    const GdkRectangle   *background_area,
                                                                    const GdkRectangle   *cell_area,
                                                                    GtkCellRendererState  flags);
static gboolean rb_cell_renderer_rating_activate (GtkCellRenderer *cell,
					          GdkEvent *event,
					          GtkWidget *widget,
					          const gchar *path,
					          const GdkRectangle *background_area,
					          const GdkRectangle *cell_area,
					          GtkCellRendererState flags);
static void rb_cell_renderer_rating_finalize (GObject *object);

struct RBCellRendererRatingPrivate
{
	double rating;
};

struct RBCellRendererRatingClassPrivate
{
	RBRatingPixbufs *pixbufs;
};

G_DEFINE_TYPE (RBCellRendererRating, rb_cell_renderer_rating, GTK_TYPE_CELL_RENDERER)
#define RB_CELL_RENDERER_RATING_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
						RB_TYPE_CELL_RENDERER_RATING, \
						RBCellRendererRatingPrivate))

enum
{
	PROP_0,
	PROP_RATING
};

enum
{
	RATED,
	LAST_SIGNAL
};

static guint rb_cell_renderer_rating_signals[LAST_SIGNAL] = { 0 };

static void
rb_marshal_VOID__STRING_DOUBLE (GClosure     *closure,
                                GValue       *return_value,
                                guint         n_param_values,
                                const GValue *param_values,
                                gpointer      invocation_hint,
                                gpointer      marshal_data)
{
	typedef void (*GMarshalFunc_VOID__STRING_DOUBLE) (gpointer     data1,
													gpointer     arg_1,
													gdouble      arg_2,
													gpointer     data2);
	register GMarshalFunc_VOID__STRING_DOUBLE callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2;

	g_return_if_fail (n_param_values == 3);

	if (G_CCLOSURE_SWAP_DATA (closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}

	callback = (GMarshalFunc_VOID__STRING_DOUBLE) (marshal_data ? marshal_data : cc->callback);

	callback (data1,
			(char *) g_value_get_string (param_values + 1),
			g_value_get_double (param_values + 2),
			data2);
}

static void
rb_cell_renderer_rating_init (RBCellRendererRating *cellrating)
{

	cellrating->priv = RB_CELL_RENDERER_RATING_GET_PRIVATE (cellrating);

	/* set the renderer able to be activated */
	g_object_set(cellrating, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);

	/* create the needed icons */
}

static void
rb_cell_renderer_rating_class_init (RBCellRendererRatingClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

	object_class->finalize = rb_cell_renderer_rating_finalize;

	object_class->get_property = rb_cell_renderer_rating_get_property;
	object_class->set_property = rb_cell_renderer_rating_set_property;

	cell_class->get_size = rb_cell_renderer_rating_get_size;
	cell_class->render   = rb_cell_renderer_rating_render;
	cell_class->activate = rb_cell_renderer_rating_activate;

	class->priv = g_new0 (RBCellRendererRatingClassPrivate, 1);
	class->priv->pixbufs = rb_rating_pixbufs_new ();

	rb_rating_install_rating_property (object_class, PROP_RATING);

	rb_cell_renderer_rating_signals[RATED] =
		g_signal_new ("rated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (RBCellRendererRatingClass, rated),
			      NULL, NULL,
			      rb_marshal_VOID__STRING_DOUBLE,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_STRING,
			      G_TYPE_DOUBLE);

	g_type_class_add_private (class, sizeof (RBCellRendererRatingPrivate));
}

static void
rb_cell_renderer_rating_finalize (GObject *object)
{
	G_OBJECT_CLASS (rb_cell_renderer_rating_parent_class)->finalize (object);
}

static void
rb_cell_renderer_rating_get_property (GObject *object,
				      guint param_id,
				      GValue *value,
				      GParamSpec *pspec)
{
	RBCellRendererRating *cellrating = RB_CELL_RENDERER_RATING (object);

	switch (param_id) {
	case PROP_RATING:
		g_value_set_double (value, cellrating->priv->rating);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
rb_cell_renderer_rating_set_property (GObject *object,
				      guint param_id,
				      const GValue *value,
				      GParamSpec *pspec)
{
	RBCellRendererRating *cellrating= RB_CELL_RENDERER_RATING (object);

	switch (param_id) {
	case PROP_RATING:
		cellrating->priv->rating = g_value_get_double (value);
		if (cellrating->priv->rating < 0)
			cellrating->priv->rating = 0;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * rb_cell_renderer_rating_new: create a cell renderer that will
 * display some pixbufs for representing the rating of a song.
 * It is also able to update the rating.
 *
 * Return value: the new cell renderer
 **/

GtkCellRenderer *
rb_cell_renderer_rating_new ()
{
	return GTK_CELL_RENDERER (g_object_new (rb_cell_renderer_rating_get_type (), NULL));
}

static void
rb_cell_renderer_rating_get_size (GtkCellRenderer *cell,
				  GtkWidget *widget,
				  const GdkRectangle *cell_area,
				  gint *x_offset,
				  gint *y_offset,
				  gint *width,
				  gint *height)
{
	int icon_width;
	int xpad;
	int ypad;

	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_width, NULL);
	gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

	if (x_offset)
		*x_offset = 0;

	if (y_offset)
		*y_offset = 0;

	if (width)
		*width = (gint) xpad * 2 + icon_width * RB_RATING_MAX_SCORE;

	if (height)
		*height = (gint) ypad * 2 + icon_width;
}

static void
rb_cell_renderer_rating_render (GtkCellRenderer      *cell,
                                                     cairo_t              *cr,
                                                     GtkWidget            *widget,
                                                     const GdkRectangle   *background_area,
                                                     const GdkRectangle   *cell_area,
                                                     GtkCellRendererState  flags)
{
	gboolean selected;
	GdkRectangle pix_rect, draw_rect;
	RBCellRendererRating *cellrating = (RBCellRendererRating *) cell;
	RBCellRendererRatingClass *cell_class;
	int xpad;
	int ypad;

	cellrating = RB_CELL_RENDERER_RATING (cell);
	cell_class = RB_CELL_RENDERER_RATING_GET_CLASS (cellrating);
	rb_cell_renderer_rating_get_size (cell, widget, cell_area,
					  &pix_rect.x,
					  &pix_rect.y,
					  &pix_rect.width,
					  &pix_rect.height);

	gtk_cell_renderer_get_padding (cell, &xpad, &ypad);
	pix_rect.x += cell_area->x;
	pix_rect.y += cell_area->y;
	pix_rect.width -= xpad * 2;
	pix_rect.height -= ypad * 2;

	if (gdk_rectangle_intersect (cell_area, &pix_rect, &draw_rect) == FALSE)
		return;

	selected = (flags & GTK_CELL_RENDERER_SELECTED);


	rb_rating_render_stars (widget, cr, cell_class->priv->pixbufs,
				draw_rect.x - pix_rect.x,
				draw_rect.y - pix_rect.y,
				draw_rect.x, draw_rect.y,
				cellrating->priv->rating, selected);
}

static gboolean
rb_cell_renderer_rating_activate (GtkCellRenderer *cell,
				  GdkEvent *event,
				  GtkWidget *widget,
				  const gchar *path,
				  const GdkRectangle *background_area,
				  const GdkRectangle *cell_area,
				  GtkCellRendererState flags)
{
	int mouse_x, mouse_y;
	double rating;

	RBCellRendererRating *cellrating = (RBCellRendererRating *) cell;

	g_return_val_if_fail (RB_IS_CELL_RENDERER_RATING (cellrating), FALSE);

	gtk_widget_get_pointer (widget, &mouse_x, &mouse_y);
	gtk_tree_view_convert_bin_window_to_tree_coords (GTK_TREE_VIEW (widget),
					     mouse_x,
					     mouse_y,
					     &mouse_x,
					     &mouse_y);

	rating = rb_rating_get_rating_from_widget (widget,
						   mouse_x - cell_area->x,
						   cell_area->width,
						   cellrating->priv->rating);

	if (rating != -1.0) {
		g_signal_emit (G_OBJECT (cellrating),
			       rb_cell_renderer_rating_signals[RATED],
			       0, path, rating);
	}

	return TRUE;
}
