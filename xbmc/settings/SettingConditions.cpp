/*
 *      Copyright (C) 2013 Team XBMC
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

#include "system.h"
#include "SettingConditions.h"
#include "Application.h"
#include "AdvancedSettings.h"
#include "GUIPassword.h"
#include "Util.h"
#include "addons/AddonManager.h"
#include "addons/Skin.h"
#if defined(TARGET_ANDROID)
#include "platform/android/activity/AndroidFeatures.h"
#endif // defined(TARGET_ANDROID)
#include "cores/AudioEngine/AEFactory.h"
#include "cores/dvdplayer/DVDCodecs/Video/DVDVideoCodec.h"
#include "guilib/LocalizeStrings.h"
#include "peripherals/Peripherals.h"
#include "profiles/ProfilesManager.h"
#include "pvr/PVRManager.h"
#include "settings/Settings.h"
#include "settings/SettingAddon.h"
#include "utils/SystemInfo.h"
#include "windowing/WindowingFactory.h"
#if defined(TARGET_DARWIN_OSX)
#include "platform/darwin/DarwinUtils.h"
#endif// defined(TARGET_DARWIN_OSX)

bool AddonHasSettings(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  if (setting == NULL)
    return false;

  const CSettingAddon *settingAddon = dynamic_cast<const CSettingAddon*>(setting);
  if (settingAddon == NULL)
    return false;

  ADDON::AddonPtr addon;
  if (!ADDON::CAddonMgr::GetInstance().GetAddon(settingAddon->GetValue(), addon, settingAddon->GetAddonType()) || addon == NULL)
    return false;

  if (addon->Type() == ADDON::ADDON_SKIN)
    return ((ADDON::CSkinInfo*)addon.get())->HasSkinFile("SkinSettings.xml");

  return addon->HasSettings();
}

bool CheckMasterLock(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return g_passwordManager.IsMasterLockUnlocked(StringUtils::EqualsNoCase(value, "true"));
}

bool CheckPVRParentalPin(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return PVR::g_PVRManager.CheckParentalPIN(g_localizeStrings.Get(19262).c_str());
}

bool HasPeripherals(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return PERIPHERALS::g_peripherals.GetNumberOfPeripherals() > 0;
}

bool IsFullscreen(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return g_Windowing.IsFullScreen();
}

bool IsMasterUser(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return g_passwordManager.bMasterUser;
}

bool PlexSignInEnable(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  // if signed in by pin, disable manual sign-in
  std::string strSignIn = g_localizeStrings.Get(1240);
  std::string strSignOut = g_localizeStrings.Get(1241);
  bool enable = true;

  if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNIN) == strSignIn &&
      CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNINPIN) == strSignIn)
  {
    enable = true;
  }
  else if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNINPIN) == strSignOut)
  {
    enable = false;
  }
  return enable;
}

bool PlexSignInPinEnable(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  // if signed in manually, disable pin sign-in
  std::string strSignIn = g_localizeStrings.Get(1240);
  std::string strSignOut = g_localizeStrings.Get(1241);
  bool enable = true;

  if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNIN) == strSignIn &&
      CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNINPIN) == strSignIn)
  {
    enable = true;
  }
  else if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNIN) == strSignOut)
  {
    enable = false;
  }
  return enable;
}

bool PlexHomeUserEnable(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  // what this, we are signed in when settings string says sign-out
  std::string strSignOut = g_localizeStrings.Get(1241);
  bool enable = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNIN) == strSignOut;
  enable = enable || CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_PLEXSIGNINPIN) == strSignOut;
  return enable;
}

bool EmbySignInEnable(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  // if signed in by pin, disable manual sign-in
  std::string strSignIn = g_localizeStrings.Get(1240);
  std::string strSignOut = g_localizeStrings.Get(1241);
  bool enable = true;

  if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNIN) == strSignIn &&
      CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNINPIN) == strSignIn)
  {
    enable = true;
  }
  else if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNINPIN) == strSignOut)
  {
    enable = false;
  }
  return enable;
}

bool EmbySignInPinEnable(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  // if signed in manually, disable pin sign-in
  std::string strSignIn = g_localizeStrings.Get(1240);
  std::string strSignOut = g_localizeStrings.Get(1241);
  bool enable = true;

  if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNIN) == strSignIn &&
      CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNINPIN) == strSignIn)
  {
    enable = true;
  }
  else if (CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNIN) == strSignOut)
  {
    enable = false;
  }
  return enable;
}

bool EmbyHomeUserEnable(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  // what this, we are signed in when settings string says sign-out
  std::string strSignOut = g_localizeStrings.Get(1241);
  bool enable = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNIN) == strSignOut;
  enable = enable || CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_EMBYSIGNINPIN) == strSignOut;
  return enable;
}

bool TraktSignInPinEnable (const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  bool enable = CSettings::GetInstance().GetString(CSettings::SETTING_SERVICES_TRAKTSIGNINPIN) == g_localizeStrings.Get(1241);

  auto settingPush = static_cast<CSettingString*>(CSettings::GetInstance().GetSetting(CSettings::SETTING_SERVICES_TRAKTPUSHWATCHED));
  settingPush->SetEnabled(enable && settingPush->IsEnabled() != enable);
  auto settingPull = static_cast<CSettingString*>(CSettings::GetInstance().GetSetting(CSettings::SETTING_SERVICES_TRAKTPULLWATCHED));
  settingPull->SetEnabled(enable && settingPull->IsEnabled() != enable);
  
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_TRAKTPULLWATCHED, "Idle");
  CSettings::GetInstance().SetString(CSettings::SETTING_SERVICES_TRAKTPUSHWATCHED, "Idle");
  
  return true;
}

bool IsUsingTTFSubtitles(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return CUtil::IsUsingTTFSubtitles();
}

bool ProfileCanWriteDatabase(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return CProfilesManager::GetInstance().GetCurrentProfile().canWriteDatabases();
}

bool ProfileCanWriteSources(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return CProfilesManager::GetInstance().GetCurrentProfile().canWriteSources();
}

bool ProfileHasAddons(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return CProfilesManager::GetInstance().GetCurrentProfile().hasAddons();
}

bool ProfileHasDatabase(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return CProfilesManager::GetInstance().GetCurrentProfile().hasDatabases();
}

bool ProfileHasSources(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return CProfilesManager::GetInstance().GetCurrentProfile().hasSources();
}

bool ProfileHasAddonManagerLocked(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return CProfilesManager::GetInstance().GetCurrentProfile().addonmanagerLocked();
}

bool ProfileHasFilesLocked(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return CProfilesManager::GetInstance().GetCurrentProfile().filesLocked();
}

bool ProfileHasMusicLocked(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return CProfilesManager::GetInstance().GetCurrentProfile().musicLocked();
}

bool ProfileHasPicturesLocked(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return CProfilesManager::GetInstance().GetCurrentProfile().picturesLocked();
}

bool ProfileHasProgramsLocked(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return CProfilesManager::GetInstance().GetCurrentProfile().programsLocked();
}

bool ProfileHasSettingsLocked(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  LOCK_LEVEL::SETTINGS_LOCK slValue=LOCK_LEVEL::ALL;
  if (StringUtils::EqualsNoCase(value, "none"))
    slValue = LOCK_LEVEL::NONE;
  else if (StringUtils::EqualsNoCase(value, "standard"))
    slValue = LOCK_LEVEL::STANDARD;
  else if (StringUtils::EqualsNoCase(value, "advanced"))
    slValue = LOCK_LEVEL::ADVANCED;
  else if (StringUtils::EqualsNoCase(value, "expert"))
    slValue = LOCK_LEVEL::EXPERT;
  return slValue <= CProfilesManager::GetInstance().GetCurrentProfile().settingsLockLevel();
}

bool ProfileHasVideosLocked(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  return CProfilesManager::GetInstance().GetCurrentProfile().videoLocked();
}

bool ProfileLockMode(const std::string &condition, const std::string &value, const CSetting *setting, void *data)
{
  char *tmp = NULL;
  LockType lock = (LockType)strtol(value.c_str(), &tmp, 0);
  if (tmp != NULL && *tmp != '\0')
    return false;

  return CProfilesManager::GetInstance().GetCurrentProfile().getLockMode() == lock;
}

std::set<std::string> CSettingConditions::m_simpleConditions;
std::map<std::string, SettingConditionCheck> CSettingConditions::m_complexConditions;

void CSettingConditions::Initialize()
{
  if (!m_simpleConditions.empty())
    return;

  // add simple conditions
  m_simpleConditions.insert("true");
#ifdef HAS_UPNP
  m_simpleConditions.insert("has_upnp");
#endif
#ifdef HAS_AIRPLAY
  m_simpleConditions.insert("has_airplay");
#endif
#ifdef HAS_EVENT_SERVER
  m_simpleConditions.insert("has_event_server");
#endif
#ifdef HAVE_X11
  m_simpleConditions.insert("have_x11");
#endif
#ifdef HAS_GL
  m_simpleConditions.insert("has_gl");
#endif
#ifdef HAS_GLX
  m_simpleConditions.insert("has_glx");
#endif
#ifdef HAS_GLES
  m_simpleConditions.insert("has_gles");
#endif
#if HAS_GLES == 2
  m_simpleConditions.insert("has_glesv2");
#endif
#ifdef HAS_SDL_JOYSTICK
  m_simpleConditions.insert("has_sdl_joystick");
#endif
#ifdef HAS_TOUCH_SKIN
  m_simpleConditions.insert("has_touch_skin");
#endif
#ifdef HAS_TIME_SERVER
  m_simpleConditions.insert("has_time_server");
#endif
#ifdef HAS_WEB_SERVER
  m_simpleConditions.insert("has_web_server");
#endif
#ifdef HAS_ZEROCONF
  m_simpleConditions.insert("has_zeroconf");
#endif
#ifdef HAVE_LIBOPENMAX
  m_simpleConditions.insert("have_libopenmax");
#endif
#ifdef HAVE_LIBVA
  m_simpleConditions.insert("have_libva");
#endif
#ifdef HAVE_LIBVDPAU
  m_simpleConditions.insert("have_libvdpau");
#endif
#ifdef TARGET_ANDROID
  m_simpleConditions.insert("has_mediacodec");
#endif
#if defined(TARGET_DARWIN)
  m_simpleConditions.insert("have_videotoolboxdecoder");
  m_simpleConditions.insert("hasvideotoolboxdecoder");
#endif
#ifdef TARGET_DARWIN_OSX
  m_simpleConditions.insert("HasVDA");
#endif
#ifdef TARGET_DARWIN_OSX
  if (CDarwinUtils::IsSnowLeopard())
    m_simpleConditions.insert("osxissnowleopard");
#endif
#ifdef TARGET_DARWIN_IOS
  m_simpleConditions.insert("hasAVF");
#endif

#ifdef TARGET_ANDROID
    m_simpleConditions.insert("isstandalone");
#else
  if (g_application.IsStandAlone())
    m_simpleConditions.insert("isstandalone");
#endif

  if(CAEFactory::SupportsQualitySetting())
    m_simpleConditions.insert("has_ae_quality_levels");

  // add complex conditions
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("addonhassettings",              AddonHasSettings));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("checkmasterlock",               CheckMasterLock));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("checkpvrparentalpin",           CheckPVRParentalPin));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("hasperipherals",                HasPeripherals));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("isfullscreen",                  IsFullscreen));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("ismasteruser",                  IsMasterUser));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("isusingttfsubtitles",           IsUsingTTFSubtitles));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("profilecanwritedatabase",       ProfileCanWriteDatabase));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("profilecanwritesources",        ProfileCanWriteSources));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("profilehasaddons",              ProfileHasAddons));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("profilehasdatabase",            ProfileHasDatabase));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("profilehassources",             ProfileHasSources));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("profilehasaddonmanagerlocked",  ProfileHasAddonManagerLocked));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("profilehasfileslocked",         ProfileHasFilesLocked));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("profilehasmusiclocked",         ProfileHasMusicLocked));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("profilehaspictureslocked",      ProfileHasPicturesLocked));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("profilehasprogramslocked",      ProfileHasProgramsLocked));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("profilehassettingslocked",      ProfileHasSettingsLocked));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("profilehasvideoslocked",        ProfileHasVideosLocked));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("profilelockmode",               ProfileLockMode));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("aesettingvisible",              CAEFactory::IsSettingVisible));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("codecoptionvisible",            CDVDVideoCodec::IsSettingVisible));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("enablemysqlgui",                CAdvancedSettings::IsSettingVisible));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("plexsignin",                    PlexSignInEnable));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("plexsigninpin",                 PlexSignInPinEnable));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("plexhomeuser",                  PlexHomeUserEnable));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("embysignin",                    EmbySignInEnable));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("embysigninpin",                 EmbySignInPinEnable));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("embyhomeuser",                  EmbyHomeUserEnable));
  m_complexConditions.insert(std::pair<std::string, SettingConditionCheck>("traktsigninpin",                TraktSignInPinEnable));
}

bool CSettingConditions::Check(const std::string &condition, const std::string &value /* = "" */, const CSetting *setting /* = NULL */)
{
  if (m_simpleConditions.find(condition) != m_simpleConditions.end())
    return true;

  std::map<std::string, SettingConditionCheck>::const_iterator itCondition = m_complexConditions.find(condition);
  if (itCondition != m_complexConditions.end())
    return itCondition->second(condition, value, setting, NULL);

  return Check(condition);
}
