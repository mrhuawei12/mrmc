/*
 *      Copyright (C) 2005-2015 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */


#include "GUIDialogSimpleMenu.h"
#include "guilib/GUIWindowManager.h"
#include "GUIDialogSelect.h"
#include "settings/DiscSettings.h"
#include "settings/Settings.h"
#include "utils/URIUtils.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "utils/log.h"
#include "video/VideoInfoTag.h"
#include "URL.h"
#include "utils/Variant.h"

bool CGUIDialogSimpleMenu::ShowPlaySelection(CFileItem& item)
{
  /* if asked to resume somewhere, we should not show anything */
  if (item.m_lStartOffset || (item.HasVideoInfoTag() && item.GetVideoInfoTag()->m_iBookmarkId > 0))
    return true;

  if (CSettings::GetInstance().GetInt(CSettings::SETTING_DISC_PLAYBACK) != BD_PLAYBACK_SIMPLE_MENU)
    return true;

  std::string path;
  if (item.IsVideoDb())
    path = item.GetVideoInfoTag()->m_strFileNameAndPath;
  else
    path = item.GetPath();

  if (item.IsBDFile())
  {
    std::string root = URIUtils::GetParentPath(path);
    URIUtils::RemoveSlashAtEnd(root);
    if (URIUtils::GetFileName(root) == "BDMV")
    {
      CURL url("bluray://");
      url.SetHostName(URIUtils::GetParentPath(root));
      url.SetFileName("root");
      return ShowPlaySelection(item, url.Get());
    }
  }

  if (item.IsDiscImage())
  {
    CURL url2("udf://");
    url2.SetHostName(item.GetPath());
    url2.SetFileName("BDMV/index.bdmv");
    if (XFILE::CFile::Exists(url2.Get()))
    {
      url2.SetFileName("");

      CURL url("bluray://");
      url.SetHostName(url2.Get());
      url.SetFileName("root");
      return ShowPlaySelection(item, url.Get());
    }
  }
  return true;
}

bool CGUIDialogSimpleMenu::ShowPlaySelection(CFileItem& item, const std::string& directory)
{

  CFileItemList items;

  if (!XFILE::CDirectory::GetDirectory(directory, items, XFILE::CDirectory::CHints(), true))
  {
    CLog::Log(LOGERROR, "CGUIWindowVideoBase::ShowPlaySelection - Failed to get play directory for %s", directory.c_str());
    return true;
  }

  if (items.IsEmpty())
  {
    CLog::Log(LOGERROR, "CGUIWindowVideoBase::ShowPlaySelection - Failed to get any items %s", directory.c_str());
    return true;
  }

  /*  RK 3D Select Item */
  CURL url3("udf://");
  url3.SetHostName(item.GetPath());
  url3.SetFileName("BDMV/STREAM/SSIF");
  CFileItemList _3ditems;
  XFILE::CDirectory::GetDirectory(url3, _3ditems, XFILE::CDirectory::CHints(), true);
  if (!_3ditems.IsEmpty())
  {
    _3ditems.Sort(SortByTrackNumber,  SortOrderDescending);
    _3ditems.Sort(SortBySize, SortOrderDescending);
    //printf("%s: %s\n", __func__, _3ditems[0]->GetPath().c_str());
    CFileItemPtr _3ditem(new CFileItem("", false));
    _3ditem->SetPath(_3ditems[0]->GetPath());
    _3ditem->m_bIsFolder = false;
    _3ditem->m_strTitle = "Play with 3d mode";
    _3ditem->SetLabel("Play with 3d mode");
    _3ditem->SetIconImage("DefaultVideo.png");
    items.Add(_3ditem);
  }
  _3ditems.Clear();
  /******************/
  CGUIDialogSelect* dialog = (CGUIDialogSelect*)g_windowManager.GetWindow(WINDOW_DIALOG_SELECT);
  while (true)
  {
    dialog->Reset();
    dialog->SetHeading(CVariant{25006}); // Select playback item
    dialog->SetItems(items);
    dialog->SetUseDetails(true);
    dialog->Open();

    CFileItemPtr item_new = dialog->GetSelectedFileItem();
    if (!item_new || dialog->GetSelectedLabel() < 0)
    {
      CLog::Log(LOGDEBUG, "CGUIWindowVideoBase::ShowPlaySelection - User aborted %s", directory.c_str());
      break;
    }

    if (item_new->m_bIsFolder == false)
    {
      std::string original_path = item.GetPath();
      std::string start_percent = item.GetProperty("StartPercent").asString();
      int start_offset = item.m_lStartOffset;
      item.Reset();
      item = *item_new;
      item.SetProperty("original_listitem_url", original_path);
      item.SetProperty("StartPercent", start_percent);
      item.m_lStartOffset = start_offset;
      items.Clear();
      return true;
    }

    items.Clear();
    if (!XFILE::CDirectory::GetDirectory(item_new->GetPath(), items, XFILE::CDirectory::CHints(), true) || items.IsEmpty())
    {
      CLog::Log(LOGERROR, "CGUIWindowVideoBase::ShowPlaySelection - Failed to get any items %s", item_new->GetPath().c_str());
      break;
    }
  }

  items.Clear();
  return false;
}
