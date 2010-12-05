/*
  Copyright (C) 2010 Elestel

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#pragma warning(disable: 4996)

#include <windows.h>
#include "..\..\Library\Export.h"	// Rainmeter's exported functions

#include <map>
#include "FolderInfo.h"

#define UPDATE_TIME_MIN_MS 10000

using namespace PluginFolderInfo;

/* The exported functions */
extern "C"
{
	__declspec( dllexport ) UINT Initialize(HMODULE instance, LPCTSTR iniFile, LPCTSTR section, UINT id);
	__declspec( dllexport ) void Finalize(HMODULE instance, UINT id);
	__declspec( dllexport ) LPCTSTR GetString(UINT id, UINT flags);
	__declspec( dllexport ) UINT Update(UINT id);
	__declspec( dllexport ) UINT GetPluginVersion();
	__declspec( dllexport ) LPCTSTR GetPluginAuthor();
}

enum InfoType
{
	// folder info (string)
	INFOTYPE_FOLDERSIZESTR,
	INFOTYPE_FILECOUNTSTR,
	INFOTYPE_FOLDERCOUNTSTR,

	// folder info (int)
	INFOTYPE_FOLDERSIZE,
	INFOTYPE_FILECOUNT,
	INFOTYPE_FOLDERCOUNT,

	INFOTYPE_COUNT
};

const static wchar_t* InfoTypeName[INFOTYPE_COUNT] =
{
	// folder info (string)
	L"FolderSizeStr",					// FIELD_FOLDERSIZESTR
	L"FileCountStr",					 // FIELD_FILECOUNTSTR
	L"FolderCountStr",				   // FIELD_FOLDERCOUNTSTR

	// folder info (int)
	L"FolderSize",					   // FIELD_FOLDERSIZE
	L"FileCount",						// FIELD_FILECOUNT
	L"FolderCount",					  // FIELD_FOLDERCOUNT
};

struct MeasureInfo
{
	InfoType Type;
	std::wstring Section;
	FolderInfo* Folder;

	MeasureInfo(const wchar_t* aSection)
	{
		Section = aSection;
		Type = INFOTYPE_COUNT;
	}
};

/* Couple of globals */
typedef std::map<UINT, MeasureInfo*> MeasureIdMap; // measure ID -> MeasureInfo
static MeasureIdMap sMeasures;
typedef std::map<FolderInfo*, UINT> FolderInfoMap; // FolderInfo -> ref count
static FolderInfoMap sFolderRefCount;
static bool sInitialized = false;

static MeasureInfo* GetMeasureInfo(UINT aId)
{
	MeasureIdMap::iterator it = sMeasures.find(aId);
	if (it != sMeasures.end()) {
		return it->second;
	}
	return NULL;
}

static FolderInfo* GetFolderInfo(const wchar_t* aPath)
{
	int pathLen = wcslen(aPath);
	if(pathLen > 2 && L'[' == aPath[0] && L']' == aPath[pathLen - 1]) {
		MeasureIdMap::iterator it;
		for (it = sMeasures.begin(); it != sMeasures.end(); it++) {
			if (wcsncmp(&aPath[1], it->second->Section.c_str(), pathLen - 2) == 0) {
				sFolderRefCount[it->second->Folder] = sFolderRefCount[it->second->Folder] + 1;
				return it->second->Folder;
			}
		}
		return NULL;
	}

	FolderInfo* folderInfo = new FolderInfo(aPath);
	sFolderRefCount[folderInfo] = 1;
	return folderInfo;
}

/*
  This function is called when the measure is initialized.
  The function must return the maximum value that can be measured. 
  The return value can also be 0, which means that Rainmeter will
  track the maximum value automatically. The parameters for this
  function are:

  instance  The instance of this DLL
  iniFile   The name of the ini-file (usually Rainmeter.ini)
  section   The name of the section in the ini-file for this measure
  id		The identifier for the measure. This is used to identify the measures that use the same plugin.
*/
UINT Initialize(HMODULE instance, LPCTSTR iniFile, LPCTSTR section, UINT id)
{
	if (!sInitialized) {
		sInitialized = true;
	}

	MeasureInfo* measureInfo = new MeasureInfo(section);

	const wchar_t* strFolder = ReadConfigString(section, L"Folder", L"");
	measureInfo->Folder = GetFolderInfo(strFolder);

	const wchar_t* strInfoType = ReadConfigString(section, L"InfoType", L"");
	for (int i = 0; i < INFOTYPE_COUNT; i++) {
		if (_wcsicmp(strInfoType, InfoTypeName[i]) == 0) {
			measureInfo->Type = (InfoType)i;
		}
	}

	if (measureInfo->Folder) {
		const wchar_t* strRegExpFilter = ReadConfigString(section, L"RegExpFilter", L"");
		if (strRegExpFilter && wcslen(strRegExpFilter) > 0) {
			measureInfo->Folder->SetRegExpFilter(strRegExpFilter);
		}

		const wchar_t* strIncludeSubFolders = ReadConfigString(section, L"IncludeSubFolders", L"");
		if (_wcsicmp(strIncludeSubFolders, L"1") == 0) {
			measureInfo->Folder->IncludeSubFolders(true);
		}
			
		const wchar_t* strShowHiddenFiles = ReadConfigString(section, L"IncludeHiddenFiles", L"");
		if (_wcsicmp(strShowHiddenFiles, L"1") == 0) {
			measureInfo->Folder->IncludeHiddenFiles(true);
		}

		const wchar_t* strShowSystemFiles = ReadConfigString(section, L"IncludeSystemFiles", L"");
		if (_wcsicmp(strShowSystemFiles, L"1") == 0) {
			measureInfo->Folder->IncludeSystemFiles(true);
		}

		measureInfo->Folder->Update();
	}

	sMeasures[id] = measureInfo;

	return 0;
}

static void FormatSize(wchar_t* buffer, size_t bufferSize, UINT64 size)
{
	if ((size >> 40) > 0) {
		wsprintf(buffer, L"%d.%02d T", (int)(size >> 40), (int)(( size << 24 >> 54 ) / 10.24));
	}
	else if ((size >> 30) > 0) {
		wsprintf(buffer, L"%d.%02d G", (int)(size >> 30), (int)(( size << 34 >> 54 ) / 10.24));
	}
	else if ((size >> 20) > 0) {
		wsprintf(buffer, L"%d.%02d M", (int)(size >> 20), (int)(( size << 44 >> 54 ) / 10.24));
	}
	else if ((size >> 10) > 0) {
		wsprintf(buffer, L"%d.%02d k", (int)(size >> 10), (int)(( size << 54 >> 54 ) / 10.24));
	}
	else {
		wsprintf(buffer, L"%ld ", size);
	}
}

/*
This function is called when the value should be
returned as a string.
*/
LPCTSTR GetString(UINT id, UINT flags) 
{
	static wchar_t buffer[MAX_PATH];
	buffer[0] = 0;

	MeasureInfo* measureInfo = sMeasures[id];
	if (!measureInfo->Folder) {
		return buffer;
	}

	int now = GetTickCount();
	if (now - measureInfo->Folder->GetLastUpdateTime() > UPDATE_TIME_MIN_MS) {
		measureInfo->Folder->Update();
	}
	
	switch (measureInfo->Type)
	{
		case INFOTYPE_FOLDERSIZESTR:
			FormatSize(buffer, MAX_PATH, measureInfo->Folder->GetSize());
			break;
		case INFOTYPE_FILECOUNTSTR:
			wsprintf(buffer, L"%d", measureInfo->Folder->GetFileCount());
			break;
		case INFOTYPE_FOLDERCOUNTSTR:
			wsprintf(buffer, L"%d", measureInfo->Folder->GetFolderCount());
			break;
	}
	return buffer;
}

/*
  This function is called when new value should be measured.
  The function returns the new value.
*/
UINT Update(UINT id)
{
	MeasureInfo* measureInfo = sMeasures[id];
	if (!measureInfo->Folder) {
		return 0;
	}

	int now = GetTickCount();
	if (now - measureInfo->Folder->GetLastUpdateTime() > UPDATE_TIME_MIN_MS) {
		measureInfo->Folder->Update();
	}

	switch (measureInfo->Type)
	{
		case INFOTYPE_FOLDERSIZE:
			return (UINT)measureInfo->Folder->GetSize();
			break;
		case INFOTYPE_FILECOUNT:
			return measureInfo->Folder->GetFileCount();
			break;
		case INFOTYPE_FOLDERCOUNT:
			return measureInfo->Folder->GetFolderCount();
			break;
	}
	return 0;
}

/*
  If the measure needs to free resources before quitting.
  The plugin can export Finalize function, which is called
  when Rainmeter quits (or refreshes).
*/
void Finalize(HMODULE instance, UINT id)
{
	MeasureIdMap::iterator itm = sMeasures.find(id);
	if (itm == sMeasures.end()) {
		return;
	}

	MeasureInfo* measureInfo = itm->second;
	sMeasures.erase(itm);
	FolderInfoMap::iterator itf = sFolderRefCount.find(measureInfo->Folder);
	if (itf != sFolderRefCount.end()) {
		if (1 == itf->second) {
			delete itf->first;
			sFolderRefCount.erase(itf);
		}
		else {
			itf->second = itf->second - 1;
		}
	}
	delete measureInfo;
}

/*
  Returns the version number of the plugin. The value
  can be calculated like this: Major * 1000 + Minor.
  So, e.g. 2.31 would be 2031.
*/
UINT GetPluginVersion()
{
	return 0002;
}

/*
  Returns the author of the plugin for the about dialog.
*/
LPCTSTR GetPluginAuthor()
{
	return L"Elestel";
}