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

#include <gdk/gdk.h>

#include <ZLUnicodeUtil.h>
#include <ZLImage.h>

#include "ZLGtkPaintContext.h"
#include "../image/ZLGtkImageManager.h"

static bool setColor(GdkColor &gdkColor, const ZLColor &zlColor) {
	return true;
	/*
	gdkColor.red = zlColor.Red * (65535 / 255);
	gdkColor.green = zlColor.Green * (65535 / 255);
	gdkColor.blue = zlColor.Blue * (65535 / 255);
	GdkVisual *visual = gdk_screen_get_system_visual(gdk_screen_get_default());
	return gdk_colormap_alloc_color(colormap, &gdkColor, false, false);
	*/
}

static void setColor(cairo_t *cr, const ZLColor &zlColor) {
	if (cr != 0) {
		GdkColor gdkColor;
		if (setColor(gdkColor, zlColor)) {
			gdk_cairo_set_source_color(cr, &gdkColor);
		}
	}
}

ZLGtkPaintContext::ZLGtkPaintContext() {
	mySurface = 0;
	myWidth = 0;
	myHeight = 0;

	myContext = 0;

	myFontDescription = 0;
	myAnalysis.lang_engine = 0;
	myAnalysis.level = 0;
	myAnalysis.language = 0;
	myAnalysis.extra_attrs = 0;
	myString = pango_glyph_string_new();

	myTextCairo = 0;
	myFillCairo = 0;
	myBackCairo = 0;

	myTileSurface = 0;

	myStringHeight = -1;
	mySpaceWidth = -1;
	myDescent = 0;
}

ZLGtkPaintContext::~ZLGtkPaintContext() {
	if (mySurface != 0) {
		cairo_surface_destroy(mySurface);
	}
	if (myTextCairo) {
		cairo_destroy(myTextCairo);
		cairo_destroy(myFillCairo);
		cairo_destroy(myBackCairo);
		cairo_destroy(myGenericCairo);
	}

	pango_glyph_string_free(myString);
	
	if (myFontDescription != 0) {
		pango_font_description_free(myFontDescription);
	}

	if (myContext != 0) {
		g_object_unref(myContext);
	}
}

void ZLGtkPaintContext::updatePixmap(GtkWidget *area, int w, int h) {
	if ((mySurface != 0) && ((myWidth != w) || (myHeight != h))) {
		if (myTextCairo != 0) {
			cairo_destroy(myTextCairo);
			cairo_destroy(myFillCairo);
			cairo_destroy(myBackCairo);
			cairo_destroy(myGenericCairo);
			myTextCairo = 0;
			myFillCairo = 0;
			myBackCairo = 0;
			myGenericCairo = 0;
		}
		cairo_surface_destroy(mySurface);
		mySurface = 0;
	}

	if (mySurface == 0) {
		myWidth = w;
		myHeight = h;
		mySurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, myWidth, myHeight);
	}

	if (myTextCairo == 0) {
		myTextCairo = cairo_create(mySurface);
		myFillCairo = cairo_create(mySurface);
		myBackCairo = cairo_create(mySurface);
		myGenericCairo = cairo_create(mySurface);
	}

	if (myContext == 0) {
		myContext = gtk_widget_get_pango_context(area);
		if (myFontDescription != 0) {
			myAnalysis.font = pango_context_load_font(myContext, myFontDescription);
			myAnalysis.shape_engine = pango_font_find_shaper(myAnalysis.font, 0, 0);
			PangoFontMetrics *metrics = pango_font_get_metrics(myAnalysis.font, myAnalysis.language);
			myDescent = pango_font_metrics_get_descent(metrics) / PANGO_SCALE;
		}
	}
}

void ZLGtkPaintContext::fillFamiliesList(std::vector<std::string> &families) const {
	if (myContext != 0) {
		PangoFontFamily **pangoFamilies;
		int nFamilies;
		pango_context_list_families(myContext, &pangoFamilies, &nFamilies);
		for (int i = 0; i < nFamilies; ++i) {
			families.push_back(pango_font_family_get_name(pangoFamilies[i]));
		}
		std::sort(families.begin(), families.end());
		g_free(pangoFamilies);
	}
}

const std::string ZLGtkPaintContext::realFontFamilyName(std::string &fontFamily) const {
	if (myContext == 0) {
		return fontFamily;
	}
	PangoFontDescription *description = pango_font_description_new();
	pango_font_description_set_family(description, fontFamily.c_str());
	pango_font_description_set_size(description, 12);
	PangoFont *font = pango_context_load_font(myContext, description);
	pango_font_description_free(description);
	description = pango_font_describe(font);
	std::string realFamily = pango_font_description_get_family(description);
	pango_font_description_free(description);
	return realFamily;
}

void ZLGtkPaintContext::setFont(const std::string &family, int size, bool bold, bool italic) {
	bool fontChanged = false;

	if (myFontDescription == 0) {
		myFontDescription = pango_font_description_new();
		fontChanged = true;
	}

	const char *oldFamily = pango_font_description_get_family(myFontDescription);
	if ((oldFamily == 0) || (family != oldFamily)) {
		pango_font_description_set_family(myFontDescription, family.c_str());
		fontChanged = true;
	}

	int newSize = size * PANGO_SCALE;
	if (pango_font_description_get_size(myFontDescription) != newSize) {
		pango_font_description_set_size(myFontDescription, newSize);
		fontChanged = true;
	}

	PangoWeight newWeight = bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL;
	if (pango_font_description_get_weight(myFontDescription) != newWeight) {
		pango_font_description_set_weight(myFontDescription, newWeight);
		fontChanged = true;
	}

	PangoStyle newStyle = italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL;
	if (pango_font_description_get_style(myFontDescription) != newStyle) {
		pango_font_description_set_style(myFontDescription, newStyle);
		fontChanged = true;
	}

	if (fontChanged) {
		if (myContext != 0) {
			myAnalysis.font = pango_context_load_font(myContext, myFontDescription);
			myAnalysis.shape_engine = pango_font_find_shaper(myAnalysis.font, 0, 0);
			PangoFontMetrics *metrics = pango_font_get_metrics(myAnalysis.font, myAnalysis.language);
			myDescent = pango_font_metrics_get_descent(metrics) / PANGO_SCALE;
		}
		myStringHeight = -1;
		mySpaceWidth = -1;
	}
}

void ZLGtkPaintContext::setColor(ZLColor color, LineStyle style) {
	double dashes[] = {50.0,  /* ink */
		10.0,  /* skip */
		10.0,  /* ink */
		10.0   /* skip*/
	};
	int    ndash  = sizeof (dashes)/sizeof(dashes[0]);
	double offset = -50.0;

	cairo_set_dash (myTextCairo, dashes, (style == SOLID_LINE) ? 0 : ndash, offset);

	::setColor(myTextCairo, color);
	cairo_set_line_width(myTextCairo, 0);
	cairo_set_line_join(myTextCairo, CAIRO_LINE_JOIN_MITER);
	cairo_set_line_cap(myTextCairo, CAIRO_LINE_CAP_BUTT);
}

void ZLGtkPaintContext::setFillColor(ZLColor color, FillStyle style) {
	return;
	/*
	if (style == SOLID_FILL) {
		::setColor(myFillCairo, color);
		gdk_gc_set_fill(myFillCairo, GDK_SOLID);
	} else {
		gdk_gc_set_fill(myFillCairo, GDK_TILED);
		if (mySurface != 0) {
			if (myTileSurface != 0) {
				gdk_pixmap_unref(myTileSurface);
			}
			static GdkColor fgColor;
			::setColor(fgColor, color);
			static GdkColor bgColor;
			::setColor(bgColor, myBackColor);
			static char data[] = { 0x0C, 0x0C, 0x03, 0x03 };
			myTileSurface = gdk_pixmap_create_from_data(
				mySurface, data, 4, 4, gdk_drawable_get_depth(mySurface), &fgColor, &bgColor
			);
			gdk_gc_set_tile(myFillCairo, myTileSurface);
		}
	}*/
}

int ZLGtkPaintContext::stringWidth(const char *str, int len, bool rtl) const {
	if (myContext == 0) {
		return 0;
	}

	if (!g_utf8_validate(str, len, 0)) {
		return 0;
	}

	myAnalysis.level = rtl ? 1 : 0;
	pango_shape(str, len, &myAnalysis, myString);
	PangoRectangle logicalRectangle;
	pango_glyph_string_extents(myString, myAnalysis.font, 0, &logicalRectangle);
	return (logicalRectangle.width + PANGO_SCALE / 2) / PANGO_SCALE;
}

int ZLGtkPaintContext::spaceWidth() const {
	if (mySpaceWidth == -1) {
		mySpaceWidth = stringWidth(" ", 1, false);
	}
	return mySpaceWidth;
}

int ZLGtkPaintContext::stringHeight() const {
	if (myFontDescription == 0) {
		return 0;
	}
	if (myStringHeight == -1) {
#if GTK_CHECK_VERSION(2,10,0)
		if (pango_font_description_get_size_is_absolute(myFontDescription)) {
			myStringHeight = pango_font_description_get_size(myFontDescription) / PANGO_SCALE + 2;
		} else {
			static const int resolution = gdk_screen_get_resolution(gdk_screen_get_default());
			myStringHeight = pango_font_description_get_size(myFontDescription) * resolution / 72 / PANGO_SCALE + 2;
		}
#else // GTK_CHECK_VERSION(2,10,0)
		myStringHeight = pango_font_description_get_size(myFontDescription) / PANGO_SCALE + 2;
#endif // GTK_CHECK_VERSION(2,10,0)
	}
	return myStringHeight;
}

int ZLGtkPaintContext::descent() const {
	return myDescent;
}

void ZLGtkPaintContext::drawString(int x, int y, const char *str, int len, bool rtl) {
	if (!g_utf8_validate(str, len, 0)) {
		return;
	}

	myAnalysis.level = rtl ? 1 : 0;
	pango_shape(str, len, &myAnalysis, myString);
	cairo_move_to(myTextCairo, x, y);
	pango_cairo_show_glyph_string(myTextCairo, myAnalysis.font, myString);
}

void ZLGtkPaintContext::drawImage(int x, int y, const ZLImageData &image) {
	GdkPixbuf *imageRef = ((const ZLGtkImageData&)image).pixbuf();
	if (imageRef != 0) {
		gdk_cairo_set_source_pixbuf(myGenericCairo, imageRef, x, y);
		cairo_paint(myGenericCairo);
	}
}

void ZLGtkPaintContext::drawImage(int x, int y, const ZLImageData &image, int width, int height, ScalingType type) {
	GdkPixbuf *imageRef = ((const ZLGtkImageData&)image).pixbuf();
	if (imageRef == 0) {
		return;
	}

	const int realWidth = imageWidth(image, width, height, type);
	const int realHeight = imageHeight(image, width, height, type);
	GdkPixbuf *scaled = gdk_pixbuf_scale_simple(imageRef, realWidth, realHeight, GDK_INTERP_BILINEAR);

	if (imageRef != 0) {
		gdk_cairo_set_source_pixbuf(myGenericCairo, scaled, x, y);
		cairo_paint(myGenericCairo);
	}
	gdk_pixbuf_unref(scaled);
}

void ZLGtkPaintContext::drawLine(int x0, int y0, int x1, int y1) {
	cairo_move_to(myGenericCairo, x0, y0);
	cairo_line_to(myGenericCairo, x1, y1);
}

void ZLGtkPaintContext::fillRectangle(int x0, int y0, int x1, int y1) {
	if (x1 < x0) {
		int tmp = x1;
		x1 = x0;
		x0 = tmp;
	}
	if (y1 < y0) {
		int tmp = y1;
		y1 = y0;
		y0 = tmp;
	}
	cairo_rectangle(myFillCairo, x0, y0,
										 x1 - x0 + 1, y1 - y0 + 1);
	cairo_fill(myFillCairo);
}

void ZLGtkPaintContext::drawFilledCircle(int x, int y, int r) {
	cairo_arc(myFillCairo, x, y, r, 0, 360 * 64);
}

void ZLGtkPaintContext::clear(ZLColor color) {
	myBackColor = color;
	if (mySurface != 0) {
		::setColor(myBackCairo, color);
		cairo_rectangle(myBackCairo, 0, 0, myWidth, myHeight);
	}
}

int ZLGtkPaintContext::width() const {
	if (mySurface == 0) {
		return 0;
	}
	return myWidth;
}

int ZLGtkPaintContext::height() const {
	if (mySurface == 0) {
		return 0;
	}
	return myHeight;
}

// vim:ts=2:sw=2:noet
