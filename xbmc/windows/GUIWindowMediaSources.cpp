/*
 *      Copyright (C) 2017 Team MrMC
 *      https://github.com/MrMC
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
 *  along with MrMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "GUIUserMessages.h"
#include "GUIWindowMediaSources.h"
#include "utils/FileUtils.h"
#include "Util.h"
#include "PlayListPlayer.h"
#include "GUIPassword.h"
#include "filesystem/MultiPathDirectory.h"
#include "filesystem/VideoDatabaseDirectory.h"
#include "dialogs/GUIDialogOK.h"
#include "PartyModeManager.h"
#include "music/MusicDatabase.h"
#include "guilib/GUIWindowManager.h"
#include "dialogs/GUIDialogYesNo.h"
#include "filesystem/Directory.h"
#include "FileItem.h"
#include "Application.h"
#include "messaging/ApplicationMessenger.h"
#include "profiles/ProfilesManager.h"
#include "settings/AdvancedSettings.h"
#include "settings/MediaSettings.h"
#include "settings/MediaSourceSettings.h"
#include "settings/Settings.h"
#include "services/ServicesManager.h"
#include "services/plex/PlexServices.h"
#include "services/emby/EmbyServices.h"
#include "input/Key.h"
#include "guilib/LocalizeStrings.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "guilib/GUIKeyboardFactory.h"
#include "video/VideoInfoScanner.h"
#include "video/dialogs/GUIDialogVideoInfo.h"
#include "pvr/recordings/PVRRecording.h"
#include "ContextMenuManager.h"
#include "services/ServicesManager.h"
#include "addons/AddonManager.h"
#include "addons/GUIDialogAddonInfo.h"
#include "dialogs/GUIDialogSelect.h"
#include "filesystem/AddonsDirectory.h"
#include "pvr/PVRManager.h"

#include "video/windows/GUIWindowVideoBase.h"

#include <utility>

using namespace PVR;
using namespace XFILE;
using namespace VIDEODATABASEDIRECTORY;
using namespace KODI::MESSAGING;

#define CONTROL_BTNVIEWASICONS     2
#define CONTROL_BTNSORTBY          3
#define CONTROL_BTNSORTASC         4
#define CONTROL_BTNSEARCH          8
#define CONTROL_LABELFILES        12

#define CONTROL_BTN_FILTER        19
#define CONTROL_BTNSHOWMODE       10
#define CONTROL_BTNSHOWALL        14
#define CONTROL_UNLOCK            11

#define CONTROL_FILTER            15
#define CONTROL_BTNPARTYMODE      16
#define CONTROL_LABELEMPTY        18

#define CONTROL_UPDATE_LIBRARY    20

CGUIWindowMediaSources::CGUIWindowMediaSources(void)
    : CGUIMediaWindow(WINDOW_MEDIA_SOURCES, "MyMediaSources.xml")
{
}

CGUIWindowMediaSources::~CGUIWindowMediaSources(void)
{
}

CGUIWindowMediaSources &CGUIWindowMediaSources::GetInstance()
{
  static CGUIWindowMediaSources sWNav;
  return sWNav;
}

bool CGUIWindowMediaSources::OnAction(const CAction &action)
{
  return CGUIMediaWindow::OnAction(action);
}

bool CGUIWindowMediaSources::OnBack(int actionID)
{
  if (actionID == ACTION_NAV_BACK || actionID == ACTION_PREVIOUS_MENU)
    return CGUIMediaWindow::OnBack(ACTION_PREVIOUS_MENU);
  return CGUIMediaWindow::OnBack(actionID);
}

bool CGUIWindowMediaSources::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
  case GUI_MSG_WINDOW_RESET:
    m_vecItems->SetPath("");
    break;
  case GUI_MSG_WINDOW_DEINIT:
    break;
  case GUI_MSG_WINDOW_INIT:
    break;

  case GUI_MSG_CLICKED:
    break;
  }
  return CGUIMediaWindow::OnMessage(message);
}

bool CGUIWindowMediaSources::Update(const std::string &strDirectory, bool updateFilterPath /* = true */)
{
  if (!CGUIMediaWindow::Update(strDirectory, updateFilterPath))
    return false;

  return true;
}

bool CGUIWindowMediaSources::GetDirectory(const std::string &strDirectory, CFileItemList &items)
{
  items.Clear();
  bool result;

  if (strDirectory.empty() || strDirectory == "mediasources://")
  {
    CFileItemPtr vItem(new CFileItem("Video"));
    vItem->m_bIsFolder = true;
    vItem->m_bIsShareOrDrive = true;
    vItem->SetPath("mediasources://video/");
    vItem->SetLabel(g_localizeStrings.Get(157));
    items.Add(vItem);

    CFileItemPtr mItem(new CFileItem("Music"));
    mItem->m_bIsFolder = true;
    mItem->m_bIsShareOrDrive = true;
    mItem->SetPath("mediasources://music/");
    mItem->SetLabel(g_localizeStrings.Get(2));
    items.Add(mItem);

    CFileItemPtr pItem(new CFileItem("Pictures"));
    pItem->m_bIsFolder = true;
    pItem->m_bIsShareOrDrive = true;
    pItem->SetPath("mediasources://pictures/");
    pItem->SetLabel(g_localizeStrings.Get(1213));
    items.Add(pItem);
    
    CFileItemPtr plItem(new CFileItem("Playlists"));
    plItem->m_bIsFolder = true;
    plItem->m_bIsShareOrDrive = true;
    plItem->SetPath("mediasources://playlists/");
    plItem->SetLabel(g_localizeStrings.Get(136));
    plItem->SetSpecialSort(SortSpecialOnBottom);
    items.Add(plItem);
    
    CFileItemPtr pvItem(new CFileItem("PVRAddons"));
    pvItem->m_bIsFolder = true;
    pvItem->m_bIsShareOrDrive = false;
    pvItem->SetPath("mediasources://pvr/");
    pvItem->SetLabel(g_localizeStrings.Get(24019));
    pvItem->SetSpecialSort(SortSpecialOnBottom);
    items.Add(pvItem);
    
    std::string text;
    std::string strSignIn = g_localizeStrings.Get(1240);
    std::string strSignOut = g_localizeStrings.Get(1241);
    if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNINPIN) == strSignIn)
      text = StringUtils::Format("%s %s %s", strSignIn.c_str() , "Plex", g_localizeStrings.Get(706).c_str());
    else
      text = StringUtils::Format("%s %s %s", strSignOut.c_str() , "Plex", g_localizeStrings.Get(706).c_str());
    
    CFileItemPtr plexItem(new CFileItem("Plex"));
    plexItem->m_bIsFolder = true;
    plexItem->m_bIsShareOrDrive = false;
    plexItem->SetPath("mediasources://plex/");
    plexItem->SetLabel(text);
    plexItem->SetSpecialSort(SortSpecialOnBottom);
    items.Add(plexItem);
    
    if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNINPIN) == strSignIn)
      text = StringUtils::Format("%s %s %s", strSignIn.c_str() , "Emby", g_localizeStrings.Get(706).c_str());
    else
      text = StringUtils::Format("%s %s %s", strSignOut.c_str() , "Emby", g_localizeStrings.Get(706).c_str());
    CFileItemPtr embyItem(new CFileItem("Emby"));
    embyItem->m_bIsFolder = true;
    embyItem->m_bIsShareOrDrive = false;
    embyItem->SetPath("mediasources://emby/");
    embyItem->SetLabel(text);
    embyItem->SetSpecialSort(SortSpecialOnBottom);
    items.Add(embyItem);

    items.SetPath("mediasources://");
    result = true;
  }
  else
  {
    if (StringUtils::StartsWithNoCase(strDirectory, "mediasources://video/"))
    {
      std::string strParentPath;
      URIUtils::GetParentPath(strDirectory, strParentPath);
      SetHistoryForPath(strParentPath);
      std::vector<std::string> params;
      params.push_back("sources://video/");
      params.push_back("return");
      // going to ".." will put us
      // at 'sources://' and we want to go back here.
      params.push_back("parent_redirect=" + strParentPath);
      g_windowManager.ActivateWindow(WINDOW_VIDEO_NAV, params);
    }
    else if (StringUtils::StartsWithNoCase(strDirectory, "mediasources://music/"))
    {
      std::string strParentPath;
      URIUtils::GetParentPath(strDirectory, strParentPath);
      SetHistoryForPath(strParentPath);
      std::vector<std::string> params;
      params.push_back("sources://music/");
      params.push_back("return");
      // going to ".." will put us
      // at 'sources://' and we want to go back here.
      params.push_back("parent_redirect=" + strParentPath);
      g_windowManager.ActivateWindow(WINDOW_MUSIC_NAV, params);
    }
    else if (StringUtils::StartsWithNoCase(strDirectory, "mediasources://pictures/"))
    {
      std::string strParentPath;
      URIUtils::GetParentPath(strDirectory, strParentPath);
      SetHistoryForPath(strParentPath);
      std::vector<std::string> params;
      params.push_back("sources://pictures/");
      params.push_back("return");
      // going to ".." will put us
      // at 'sources://' and we want to go back here.
      params.push_back("parent_redirect=" + strParentPath);
      g_windowManager.ActivateWindow(WINDOW_PICTURES, params);
    }
    else if (StringUtils::StartsWithNoCase(strDirectory, "mediasources://playlists/"))
    {
      CFileItemPtr pItem(new CFileItem("MusicPlaylist"));
      pItem->m_bIsFolder = true;
      pItem->m_bIsShareOrDrive = true;
      pItem->SetPath("mediasources://musicplaylists/");
      pItem->SetLabel(g_localizeStrings.Get(20011));
      items.Add(pItem);
      
      CFileItemPtr plItem(new CFileItem("VideoPlaylist"));
      plItem->m_bIsFolder = true;
      plItem->m_bIsShareOrDrive = true;
      plItem->SetPath("mediasources://videoplaylists/");
      plItem->SetLabel(g_localizeStrings.Get(20012));
      plItem->SetSpecialSort(SortSpecialOnBottom);
      items.Add(plItem);
      items.SetPath("mediasources://playlists/");
      result = true;
    }
    else if (StringUtils::StartsWithNoCase(strDirectory, "mediasources://videoplaylists/"))
    {
      std::vector<std::string> params;
      params.push_back("special://videoplaylists/");
      params.push_back("return");
      // going to ".." will put us
      // at 'sources://' and we want to go back here.
      params.push_back("parent_redirect=mediasources://playlists/");
      g_windowManager.ActivateWindow(WINDOW_VIDEO_NAV, params);
    }
    else if (StringUtils::StartsWithNoCase(strDirectory, "mediasources://musicplaylists/"))
    {
      std::vector<std::string> params;
      params.push_back("special://musicplaylists/");
      params.push_back("return");
      // going to ".." will put us
      // at 'sources://' and we want to go back here.
      params.push_back("parent_redirect=mediasources://playlists/");
      g_windowManager.ActivateWindow(WINDOW_MUSIC_NAV, params);
    }
    else if (StringUtils::StartsWithNoCase(strDirectory, "mediasources://plex/"))
    {
      std::string strParentPath;
      URIUtils::GetParentPath(strDirectory, strParentPath);
      SetHistoryForPath(strParentPath);
      std::vector<std::string> params;
      params.push_back("mediasources://");
      params.push_back("return");
      // going to ".." will put us
      // at 'sources://' and we want to go back here.
      params.push_back("parent_redirect=" + strParentPath);
      std::string strSignOut = g_localizeStrings.Get(1241);
      if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNINPIN) == strSignOut &&
          !VerifyLogout("Plex"))
        return false;
      CPlexServices::GetInstance().InitiateSignIn();
      g_windowManager.ActivateWindow(WINDOW_MEDIA_SOURCES, params);
    }
    else if (StringUtils::StartsWithNoCase(strDirectory, "mediasources://emby/"))
    {
      std::string strParentPath;
      URIUtils::GetParentPath(strDirectory, strParentPath);
      SetHistoryForPath(strParentPath);
      std::vector<std::string> params;
      params.push_back("mediasources://");
      params.push_back("return");
      // going to ".." will put us
      // at 'sources://' and we want to go back here.
      params.push_back("parent_redirect=" + strParentPath);
      std::string strSignOut = g_localizeStrings.Get(1241);
      if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNINPIN) == strSignOut &&
          !VerifyLogout("Emby"))
        return false;
      CEmbyServices::GetInstance().InitiateSignIn();
      g_windowManager.ActivateWindow(WINDOW_MEDIA_SOURCES, params);
    }
    else if (StringUtils::StartsWithNoCase(strDirectory, "mediasources://pvr/"))
    {
      std::string strParentPath;
      URIUtils::GetParentPath(strDirectory, strParentPath);
      SetHistoryForPath(strParentPath);
      std::vector<std::string> params;
      params.push_back("mediasources://");
      params.push_back("return");
      // going to ".." will put us
      // at 'sources://' and we want to go back here.
      params.push_back("parent_redirect=" + strParentPath);

      ADDON::VECADDONS pvrAddons;
      ADDON::CAddonMgr::GetInstance().GetAllAddons(ADDON::ADDON_PVRDLL, pvrAddons);
      if (pvrAddons.size() > 0)
      {
        CFileItemList items;
        CGUIDialogSelect *dialog = (CGUIDialogSelect*)g_windowManager.GetWindow(WINDOW_DIALOG_SELECT);
        if (dialog == NULL)
          return false;
        int sel = 0;
        std::vector<int> selected;
        dialog->Reset();
        dialog->SetHeading(g_localizeStrings.Get(24019));
        dialog->SetMultiSelection(false);
        dialog->SetUseDetails(true);
        for (ADDON::VECADDONS::const_iterator addon = pvrAddons.begin(); addon != pvrAddons.end(); ++addon)
        {
          CFileItemPtr item(CAddonsDirectory::FileItemFromAddon(*addon, (*addon)->ID()));
          CFileItem item1(*item.get());
          dialog->Add(item1);
          if (!ADDON::CAddonMgr::GetInstance().IsAddonDisabled((*addon)->ID()))
            dialog->SetSelected(sel);
          sel++;
        }
        
        dialog->Sort();
        dialog->Open();
        
        if (!dialog->IsConfirmed())
          return false;
        
        const CFileItemPtr item = dialog->GetSelectedFileItem();
        
        CGUIDialogAddonInfo::ShowForItem(item);
        ADDON::AddonPtr addon = *new ADDON::AddonPtr;
        ADDON::CAddonMgr::GetInstance().GetAddon(item->GetPath(), addon);
        if (!ADDON::CAddonMgr::GetInstance().IsAddonDisabled(addon->ID()) && !g_PVRManager.IsStarted())
        {
          CSettings::GetInstance().SetBool(CSettings::SETTING_PVRMANAGER_ENABLED,true);
          g_application.StartPVRManager();
        }
        else
        {
          ADDON::VECADDONS pvrAddons;
          ADDON::CAddonMgr::GetInstance().GetAddons(ADDON::ADDON_PVRDLL, pvrAddons, true);
          if (pvrAddons.size() < 1)
          {
            CSettings::GetInstance().SetBool(CSettings::SETTING_PVRMANAGER_ENABLED,false);
            g_application.StopPVRManager();
          }
        }
      }
      g_windowManager.ActivateWindow(WINDOW_MEDIA_SOURCES, params);
    }
    result = true;
  }
  return result;
}

bool CGUIWindowMediaSources::VerifyLogout(std::string service)
{
  CGUIDialogYesNo* pDialogYesNo = (CGUIDialogYesNo*)g_windowManager.GetWindow(WINDOW_DIALOG_YES_NO);
  if (pDialogYesNo)
  {
    std::string text = StringUtils::Format(g_localizeStrings.Get(1257).c_str(), service.c_str());
    pDialogYesNo->SetHeading(CVariant{2116});
    pDialogYesNo->SetLine(1, text);
    pDialogYesNo->Open();
    
    return pDialogYesNo->IsConfirmed();
  }
  return false;
}

bool CGUIWindowMediaSources::OnClick(int iItem)
{
  return CGUIMediaWindow::OnClick(iItem);
}

std::string CGUIWindowMediaSources::GetStartFolder(const std::string &dir)
{
  return CGUIMediaWindow::GetStartFolder(dir);
}
