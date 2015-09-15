/*
 * Copyright (C) 2004-2010 Geometer Plus <contact@geometerplus.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <algorithm>

#include <gtk/gtk.h>

#include <ZLibrary.h>
#include <ZLLanguageUtil.h>

#include "ZLGtkViewWidget.h"
#include "ZLGtkPaintContext.h"
#include "../util/ZLGtkSignalUtil.h"
#include "../pixbuf/ZLGtkPixbufHack.h"

static void updatePoint(ZLGtkViewWidget *viewWidget, int &x, int &y) {
	switch (viewWidget->rotation()) {
		default:
			break;
		case ZLView::DEGREES90:
		{
			int tmp = x;
			x = viewWidget->height() - y;
			y = tmp;
			break;
		}
		case ZLView::DEGREES180:
			x = viewWidget->width() - x;
			y = viewWidget->height() - y;
			break;
		case ZLView::DEGREES270:
		{
			int tmp = x;
			x = y;
			y = viewWidget->width() - tmp;
			break;
		}
	}
}

static void mousePressed(GtkWidget *area, GdkEventButton *event, gpointer data) {
	gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_toplevel(area)), area);
	ZLGtkViewWidget *viewWidget = (ZLGtkViewWidget*)data;
	int x = (int)event->x;
	int y = (int)event->y;
	updatePoint(viewWidget, x, y);
	viewWidget->view()->onStylusMove(x, y);
	viewWidget->view()->onStylusPress(x, y);
}

static void mouseReleased(GtkWidget*, GdkEventButton *event, gpointer data) {
	ZLGtkViewWidget *viewWidget = (ZLGtkViewWidget*)data;
	int x = (int)event->x;
	int y = (int)event->y;
	updatePoint(viewWidget, x, y);
	viewWidget->view()->onStylusRelease(x, y);
}

static void mouseMoved(GtkWidget*, GdkEventMotion *event, gpointer data) {
	ZLGtkViewWidget *viewWidget = (ZLGtkViewWidget*)data;
	int x, y;
	GdkModifierType state;
	if (event->is_hint) {
		gdk_window_get_pointer(event->window, &x, &y, &state);
	} else {
		x = (int)event->x;
		y = (int)event->y;
		state = (GdkModifierType)event->state;
	}
	state = (GdkModifierType) (state & GDK_MODIFIER_MASK);
	updatePoint(viewWidget, x, y);
	if ((state & 0xFF00) == 0) {
		viewWidget->view()->onStylusMove(x, y);
	} else {
		viewWidget->view()->onStylusMovePressed(x, y);
	}
}

static void doPaint(GtkWidget *widget, cairo_t *cr, ZLGtkViewWidget *data) {
	/*if (!gtk_widget_is_drawable(widget))
		return;*/
	data->doPaint(cr);
}

int ZLGtkViewWidget::width() const {
	GtkAllocation allocation;
	if (myArea == 0){
		return 0;
	}
	gtk_widget_get_allocation(myArea, &allocation);
	return allocation.width;
}

int ZLGtkViewWidget::height() const {
	GtkAllocation allocation;
	if (myArea == 0){
		return 0;
	}
	gtk_widget_get_allocation(myArea, &allocation);
	return allocation.height;
}

bool ZLGtkViewWidget::scrollbarEvent(ZLView::Direction direction, GtkRange *range, GtkScrollType type, double newValue) {
	static bool alreadyProcessed = false;
	if (alreadyProcessed) {
		return true;
	}
	alreadyProcessed = true;

	bool code = true;
	switch (type) {
		default:
			code = false;
			break;
  	case GTK_SCROLL_JUMP:
		{
			GtkAdjustment *adjustment = gtk_range_get_adjustment(range);
			const int upper = (int)gtk_adjustment_get_upper(adjustment);
			onScrollbarMoved(
				direction,
				upper,
				std::max(std::min((int)newValue, upper), 0),
				std::max(std::min((int)(newValue + gtk_adjustment_get_page_size(adjustment)), upper), 0)
			);
			code = false;
			break;
		}
  	case GTK_SCROLL_STEP_BACKWARD:
			onScrollbarStep(direction, -1);
			break;
  	case GTK_SCROLL_STEP_FORWARD:
			onScrollbarStep(direction, 1);
			break;
  	case GTK_SCROLL_PAGE_BACKWARD:
			onScrollbarPageStep(direction, -1);
			break;
  	case GTK_SCROLL_PAGE_FORWARD:
			onScrollbarPageStep(direction, 1);
			break;
	}
	gtk_widget_send_expose(myArea, gdk_event_new(GDK_EXPOSE));
	alreadyProcessed = false;
	return code;
}

static bool vScrollbarEvent(GtkRange *range, GtkScrollType type, double newValue, ZLGtkViewWidget *data) {
	return data->scrollbarEvent(ZLView::VERTICAL, range, type, newValue);
}

static bool hScrollbarEvent(GtkRange *range, GtkScrollType type, double newValue, ZLGtkViewWidget *data) {
	return data->scrollbarEvent(ZLView::HORIZONTAL, range, type, newValue);
}

GtkWidget *ZLGtkViewWidget::createVScrollbar(int pos) {
	GtkWidget *vScrollbar = gtk_vscrollbar_new(myVerticalAdjustment);
	gtk_table_attach(myTable, vScrollbar, pos, pos + 1, 1, 2, GTK_FILL, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), 0, 0);
	ZLGtkSignalUtil::connectSignal(G_OBJECT(vScrollbar), "change_value", G_CALLBACK(vScrollbarEvent), this);
	gtk_widget_hide(vScrollbar);
	return vScrollbar;
}

GtkWidget *ZLGtkViewWidget::createHScrollbar(int pos) {
	GtkWidget *hScrollbar = gtk_hscrollbar_new(myHorizontalAdjustment);
	gtk_table_attach(myTable, hScrollbar, 1, 2, pos, pos + 1, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), GTK_FILL, 0, 0);
	ZLGtkSignalUtil::connectSignal(G_OBJECT(hScrollbar), "change_value", G_CALLBACK(hScrollbarEvent), this);
	gtk_widget_hide(hScrollbar);
	return hScrollbar;
}

ZLGtkViewWidget::ZLGtkViewWidget(ZLApplication *application, ZLView::Angle initialAngle) : ZLViewWidget(initialAngle) {
	myApplication = application;
	myArea = gtk_drawing_area_new();
	gtk_widget_set_can_focus(myArea, true);

	myTable = GTK_TABLE(gtk_table_new(3, 3, false));
	gtk_table_attach(myTable, myArea, 1, 2, 1, 2, (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), (GtkAttachOptions)(GTK_FILL | GTK_EXPAND), 0, 0);
	gtk_widget_show(GTK_WIDGET(myTable));
	gtk_widget_show(myArea);

	myVerticalAdjustment = GTK_ADJUSTMENT(gtk_adjustment_new(100, 0, 100, 1, 1, 1));
	myShowScrollBarAtRight = true;
	myVerticalScrollbarIsVisible = false;
	myLeftScrollBar = createVScrollbar(0);
	myRightScrollBar = createVScrollbar(2);

	myHorizontalAdjustment = GTK_ADJUSTMENT(gtk_adjustment_new(100, 0, 100, 1, 1, 1));
	myShowScrollBarAtBottom = true;
	myHorizontalScrollbarIsVisible = false;
	myTopScrollBar = createHScrollbar(0);
	myBottomScrollBar = createHScrollbar(2);

	myOriginalPixbuf = 0;
	myRotatedPixbuf = 0;
	gtk_widget_set_double_buffered(myArea, false);
	gtk_widget_set_events(myArea, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	ZLGtkSignalUtil::connectSignal(G_OBJECT(myArea), "button_press_event", G_CALLBACK(mousePressed), this);
	ZLGtkSignalUtil::connectSignal(G_OBJECT(myArea), "button_release_event", G_CALLBACK(mouseReleased), this);
	ZLGtkSignalUtil::connectSignal(G_OBJECT(myArea), "motion_notify_event", G_CALLBACK(mouseMoved), this);
	ZLGtkSignalUtil::connectSignal(G_OBJECT(myArea), "draw", G_CALLBACK(::doPaint), this);
	myRepaintBlocked = false;
}

void ZLGtkViewWidget::setScrollbarEnabled(ZLView::Direction direction, bool enabled) {
	GtkWidget *scrollbar = (direction == ZLView::VERTICAL) ?
		(myShowScrollBarAtRight ? myRightScrollBar : myLeftScrollBar) :
		(myShowScrollBarAtBottom ? myBottomScrollBar : myTopScrollBar);

	if (direction == ZLView::VERTICAL) {
		myVerticalScrollbarIsVisible = enabled;
	} else {
		myHorizontalScrollbarIsVisible = enabled;
	}

	if (enabled) {
		gtk_widget_show(scrollbar);
		gtk_widget_queue_draw(scrollbar);
	} else {
		gtk_widget_hide(scrollbar);
	}
}

void ZLGtkViewWidget::setScrollbarPlacement(ZLView::Direction direction, bool standard) {
	if ((rotation() == ZLView::DEGREES90) || (rotation() == ZLView::DEGREES270)) {
		if (ZLLanguageUtil::isRTLLanguage(ZLibrary::Language())) {
			standard = !standard;
		}
	}

	if (direction == ZLView::VERTICAL) {
		if (myVerticalScrollbarIsVisible) {
			gtk_widget_hide(myShowScrollBarAtRight ? myRightScrollBar : myLeftScrollBar);
		}
		myShowScrollBarAtRight = standard;
		if (myVerticalScrollbarIsVisible) {
			gtk_widget_show(myShowScrollBarAtRight ? myRightScrollBar : myLeftScrollBar);
		}
	} else {
		if (myHorizontalScrollbarIsVisible) {
			gtk_widget_hide(myShowScrollBarAtBottom ? myBottomScrollBar : myTopScrollBar);
		}
		myShowScrollBarAtBottom = standard;
		if (myHorizontalScrollbarIsVisible) {
			gtk_widget_show(myShowScrollBarAtBottom ? myBottomScrollBar : myTopScrollBar);
		}
	}
}

void ZLGtkViewWidget::setScrollbarParameters(ZLView::Direction direction, size_t full, size_t from, size_t to) {
	GtkAdjustment *adjustment =
		(direction == ZLView::VERTICAL) ? myVerticalAdjustment : myHorizontalAdjustment;
	gtk_adjustment_set_lower(adjustment, 0);
	gtk_adjustment_set_upper(adjustment, full);
	gtk_adjustment_set_value(adjustment, from);
	gtk_adjustment_set_step_increment(adjustment, to - from);
	gtk_adjustment_set_page_increment(adjustment, to - from);
	gtk_adjustment_set_page_size(adjustment, to - from);

	GtkWidget *scrollbar = (direction == ZLView::VERTICAL) ?
		(myShowScrollBarAtRight ? myRightScrollBar : myLeftScrollBar) :
		(myShowScrollBarAtBottom ? myBottomScrollBar : myTopScrollBar);
	gtk_widget_queue_draw(scrollbar);
}

ZLGtkViewWidget::~ZLGtkViewWidget() {
	cleanOriginalPixbuf();
	cleanRotatedPixbuf();
}

void ZLGtkViewWidget::cleanOriginalPixbuf() {
	if (myOriginalPixbuf != 0) {
		gdk_pixbuf_unref(myOriginalPixbuf);
		myOriginalPixbuf = 0;
	}
}

void ZLGtkViewWidget::cleanRotatedPixbuf() {
	if (myRotatedPixbuf != 0) {
		gdk_pixbuf_unref(myRotatedPixbuf);
		myRotatedPixbuf = 0;
	}
}

void ZLGtkViewWidget::trackStylus(bool track) {
	// TODO: implement
}

void ZLGtkViewWidget::repaint()	{
	if (!myRepaintBlocked) {
		gtk_widget_queue_draw(myArea);
	}
}

void ZLGtkViewWidget::doPaint(cairo_t *cr)	{
	ZLGtkPaintContext &gtkContext = (ZLGtkPaintContext&)view()->context();
	ZLView::Angle angle = rotation();
	bool isRotated = (angle == ZLView::DEGREES90) || (angle == ZLView::DEGREES270);
	int w = isRotated ? height() : width();
	int h = isRotated ? width() : height();

	gtkContext.updatePixmap(myArea, w, h);
	view()->paint();

	cairo_set_source_surface(cr, gtkContext.pixmap(), 0, 0);

	/*
	switch (angle) {
		default:
			cleanOriginalPixbuf();
			cleanRotatedPixbuf();
			gdk_draw_pixmap(gtk_widget_get_window(myArea), myArea->style->white_gc, gtkContext.pixmap(), 0, 0, 0, 0, myArea->allocation.width, myArea->allocation.height);
			break;
		case ZLView::DEGREES180:
			cleanRotatedPixbuf();
			if ((myOriginalPixbuf != 0) &&
					((gdk_pixbuf_get_width(myOriginalPixbuf) != w) ||
					 (gdk_pixbuf_get_height(myOriginalPixbuf) != h))) {
				cleanOriginalPixbuf();
			}
			if (myOriginalPixbuf == 0) {
				myOriginalPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, w, h);
				myImage = gdk_image_new(GDK_IMAGE_FASTEST, gdk_drawable_get_visual(gtkContext.pixmap()), w, h);
			}
			gdk_drawable_copy_to_image(gtkContext.pixmap(), myImage, 0, 0, 0, 0, w, h);
			gdk_pixbuf_get_from_image(myOriginalPixbuf, myImage, gdk_drawable_get_colormap(gtkContext.pixmap()), 0, 0, 0, 0, w, h);
			::rotate180(myOriginalPixbuf);
			gdk_draw_pixbuf(myArea->window, myArea->style->white_gc, myOriginalPixbuf, 0, 0, 0, 0, w, h, GDK_RGB_DITHER_NONE, 0, 0);
			gdk_cairo_set_source_pixbuf(myGenericCairo, myOriginalPixbuf, 0, 0);
			break;
		case ZLView::DEGREES90:
		case ZLView::DEGREES270:
			if ((myOriginalPixbuf != 0) &&
					((gdk_pixbuf_get_width(myOriginalPixbuf) != w) ||
					 (gdk_pixbuf_get_height(myOriginalPixbuf) != h))) {
				cleanOriginalPixbuf();
			}
			if ((myRotatedPixbuf != 0) &&
					((gdk_pixbuf_get_width(myRotatedPixbuf) != h) ||
					 (gdk_pixbuf_get_height(myRotatedPixbuf) != w))) {
				cleanRotatedPixbuf();
			}
			if (myOriginalPixbuf == 0) {
				myOriginalPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, w, h);
				myImage = gdk_image_new(GDK_IMAGE_FASTEST, gdk_drawable_get_visual(gtkContext.pixmap()), w, h);
			}
			if (myRotatedPixbuf == 0) {
				myRotatedPixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, h, w);
			}
			gdk_drawable_copy_to_image(gtkContext.pixmap(), myImage, 0, 0, 0, 0, w, h);
			gdk_pixbuf_get_from_image(myOriginalPixbuf, myImage, gdk_drawable_get_colormap(gtkContext.pixmap()), 0, 0, 0, 0, w, h);
			::rotate90(myRotatedPixbuf, myOriginalPixbuf, angle == ZLView::DEGREES90);
			gdk_draw_pixbuf(myArea->window, myArea->style->white_gc, myRotatedPixbuf, 0, 0, 0, 0, h, w, GDK_RGB_DITHER_NONE, 0, 0);
			break;
	}
*/
	cairo_paint(cr);
	/*myRepaintBlocked = true;
	myApplication->refreshWindow();
	myRepaintBlocked = false;*/
}

GtkWidget *ZLGtkViewWidget::area() {
	return myArea;
}

GtkWidget *ZLGtkViewWidget::areaWithScrollbars() {
	return GTK_WIDGET(myTable);
}
