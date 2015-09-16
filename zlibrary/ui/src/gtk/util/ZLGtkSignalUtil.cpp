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

#include "ZLGtkSignalUtil.h"

std::vector<std::pair<GObject*, gulong> > ZLGtkSignalUtil::ourConnectedSignals;

void ZLGtkSignalUtil::connectSignal(GObject *object, const char *name, void(*function)(void), void *data) {
	gulong handlerId = g_signal_connect(object, name, function, data);
	g_object_ref(object);
	ourConnectedSignals.push_back(std::make_pair(object, handlerId));
	//g_warning ("cs: %lu %p %p", handlerId, object, function);
}

void ZLGtkSignalUtil::connectSignalAfter(GObject *object, const char *name, void(*function)(void), void *data) {
	gulong handlerId = g_signal_connect_after(object, name, function, data);
	g_object_ref(object);
	ourConnectedSignals.push_back(std::make_pair(object, handlerId));
	//g_warning ("csa: %lu %p %p", handlerId, object, function);
}

void ZLGtkSignalUtil::removeAllSignals() {
	for (std::vector<std::pair<GObject*, gulong> >::const_iterator it = ourConnectedSignals.begin(); it != ourConnectedSignals.end(); ++it) {
		//g_warning ("ras: %lu %p", it->second, it->first);
		g_signal_handler_disconnect(it->first, it->second);
		g_object_unref(it->first);
	}
}
