/*
 * Copyright (C) 2005 Nikolay Pultsin <geometer@mawhrin.net>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define NOZLIBDEFS
#include <zlib.h>

#include <Extensions/ExpansionMgr/VFSMgr.h>

#include <abstract/ZLStringUtil.h>

#include "ZLPalmFSManager.h"
#include "ZLPalmFSDir.h"
#include "ZLPalmFileInputStream.h"
#include "../../abstract/filesystem/ZLZipInputStream.h"
#include "../../abstract/filesystem/ZLGzipInputStream.h"
#include "ZLPalmFileOutputStream.h"

void ZLPalmFSManager::createInstance() {
	ourInstance = new ZLPalmFSManager();
}

ZLPalmFSManager::ZLPalmFSManager() : ZLFSManager() {
}

ZLPalmFSManager::~ZLPalmFSManager() {
}

void ZLPalmFSManager::normalize(std::string&) {
}

ZLFSDir *ZLPalmFSManager::createDirectory(const std::string &name) {
	return new ZLPalmFSDir(name);
}

ZLInputStream *ZLPalmFSManager::createPlainInputStream(const std::string &name) {
	if ((int)name.find(':') != -1) {
		return (ZLibRef != sysInvalidRefNum) ? new ZLZipInputStream(name) : 0;
	} else {
		return new ZLPalmFileInputStream(name);
	}
}

ZLInputStream *ZLPalmFSManager::createInputStream(const std::string &name) {
	if ((int)name.find(':') != -1) {
		return (ZLibRef != sysInvalidRefNum) ? new ZLZipInputStream(name) : 0;
	} else if (ZLStringUtil::stringEndsWith(name, ".gz")) {
		return new ZLGzipInputStream(name);
	} else {
		return new ZLPalmFileInputStream(name);
	}
}

ZLOutputStream *ZLPalmFSManager::createOutputStream(const std::string &name) {
	return new ZLPalmFileOutputStream(name);
}

ZLFileInfo ZLPalmFSManager::fileInfo(const std::string &name) {
	ZLFileInfo info;

	UInt16  unVol;
	UInt32  ulIter;
	UInt32  ulVFSMgrVer;

	FtrGet(sysFileCVFSMgr, vfsFtrIDVersion, &ulVFSMgrVer);
	ulIter = vfsIteratorStart;
	VFSVolumeEnumerate(&unVol, &ulIter);

	UInt32 fileRef;
	VFSFileOpen(unVol, name.c_str(), vfsModeRead, &fileRef);

	if (fileRef != 0) {
		info.Exists = true;
		VFSFileSize(fileRef, &info.Size);
		VFSFileGetDate(fileRef, vfsFileDateModified, &info.MTime);
		VFSFileClose(fileRef);
	} else {
		info.Exists = false;
		info.Size = 0;
		info.MTime = 0;
	}
	return info;
}
