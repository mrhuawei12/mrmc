/*
 *      Copyright (C) 2005-2013 Team XBMC
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

#if defined(TARGET_DARWIN_OSX)

#include "WinSystemOSX.h"

#include "Application.h"
#include "guilib/DispResource.h"
#include "guilib/GUIWindowManager.h"
#include "settings/Settings.h"
#include "settings/DisplaySettings.h"
#include "messaging/ApplicationMessenger.h"
#include "utils/log.h"
#include "utils/StringUtils.h"

#import "platform/darwin/DarwinUtils.h"
#import "platform/darwin/DictionaryUtils.h"
#import "platform/darwin/osx/CocoaInterface.h"
#import "platform/darwin/osx/OSXGLView.h"
#import "platform/darwin/osx/OSXGLWindow.h"
#import "platform/darwin/osx/OSXTextInputResponder.h"

#import <Cocoa/Cocoa.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <IOKit/graphics/IOGraphicsLib.h>

// turn off deprecated warning spew.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

//------------------------------------------------------------------------------------------
#define MAX_DISPLAYS 32

//------------------------------------------------------------------------------------------
CRect CGRectToCRect(CGRect cgrect)
{
  CRect crect = CRect(
    cgrect.origin.x,
    cgrect.origin.y,
    cgrect.origin.x + cgrect.size.width,
    cgrect.origin.y + cgrect.size.height);
  return crect;
}

//---------------------------------------------------------------------------------
void SetMenuBarVisible(bool visible)
{
  // native fullscreen stuff handles this for us...
  if ([NSApplication sharedApplication] == nil)
    printf("[NSApplication sharedApplication] nil %d\n" , visible);
  
  @try
  {
    if (visible)
      [OSXGLWindow performSelectorOnMainThread:@selector(SetMenuBarVisible) withObject:nil waitUntilDone:YES];
    else
      [OSXGLWindow performSelectorOnMainThread:@selector(SetMenuBarInvisible) withObject:nil waitUntilDone:YES];
  }
  
  @catch(NSException *exception)
  {
    NSLog(@"Error.  Make sure you have a valid combination of options.");
  }
}

//---------------------------------------------------------------------------------
CGDirectDisplayID GetDisplayID(int screen_index)
{
  CGDirectDisplayID displayArray[MAX_DISPLAYS];
  CGDisplayCount    numDisplays;

  // Get the list of displays.
  CGGetActiveDisplayList(MAX_DISPLAYS, displayArray, &numDisplays);
  return displayArray[screen_index];
}

CGDirectDisplayID GetDisplayIDFromScreen(NSScreen *screen)
{
  NSDictionary* screenInfo = [screen deviceDescription];
  NSNumber* screenID = [screenInfo objectForKey:@"NSScreenNumber"];

  return (CGDirectDisplayID)[screenID longValue];
}

int GetDisplayIndex(CGDirectDisplayID display)
{
  CGDirectDisplayID displayArray[MAX_DISPLAYS];
  CGDisplayCount    numDisplays;

  // Get the list of displays.
  CGGetActiveDisplayList(MAX_DISPLAYS, displayArray, &numDisplays);
  while (numDisplays > 0)
  {
    if (display == displayArray[--numDisplays])
	  return numDisplays;
  }
  return -1;
}

NSString* screenNameForDisplay(CGDirectDisplayID displayID)
{
  NSString *screenName = nil;

  // IODisplayCreateInfoDictionary leaks IOCFUnserializeparse, nothing we can do about it.
  NSDictionary *deviceInfo = (NSDictionary *)CFBridgingRelease(IODisplayCreateInfoDictionary(CGDisplayIOServicePort(displayID), kIODisplayOnlyPreferredName));
  NSDictionary *localizedNames = [deviceInfo objectForKey:[NSString stringWithUTF8String:kDisplayProductName]];

  if ([localizedNames count] > 0) {
      screenName = [localizedNames objectForKey:[[localizedNames allKeys] objectAtIndex:0]];
  }

  return screenName;
}

// try to find mode that matches the desired size, refreshrate
// non interlaced, nonstretched, safe for hardware
CFDictionaryRef GetMode(int width, int height, double refreshrate, int screenIdx)
{
  if (screenIdx >= (signed)[[NSScreen screens] count])
    return NULL;

  Boolean stretched;
  Boolean interlaced;
  Boolean safeForHardware;
  Boolean televisionoutput;
  int w, h, bitsperpixel;
  double rate;
  RESOLUTION_INFO res;

  CLog::Log(LOGDEBUG, "GetMode looking for suitable mode with %d x %d @ %f Hz on display %d\n", width, height, refreshrate, screenIdx);

  CFArrayRef displayModes = CGDisplayAvailableModes(GetDisplayID(screenIdx));

  if (NULL == displayModes)
  {
    CLog::Log(LOGERROR, "GetMode - no displaymodes found!");
    return NULL;
  }

  for (int i=0; i < CFArrayGetCount(displayModes); ++i)
  {
    CFDictionaryRef displayMode = (CFDictionaryRef)CFArrayGetValueAtIndex(displayModes, i);

    stretched = GetDictionaryBoolean(displayMode, kCGDisplayModeIsStretched);
    interlaced = GetDictionaryBoolean(displayMode, kCGDisplayModeIsInterlaced);
    bitsperpixel = GetDictionaryInt(displayMode, kCGDisplayBitsPerPixel);
    safeForHardware = GetDictionaryBoolean(displayMode, kCGDisplayModeIsSafeForHardware);
    televisionoutput = GetDictionaryBoolean(displayMode, kCGDisplayModeIsTelevisionOutput);
    w = GetDictionaryInt(displayMode, kCGDisplayWidth);
    h = GetDictionaryInt(displayMode, kCGDisplayHeight);
    rate = GetDictionaryDouble(displayMode, kCGDisplayRefreshRate);

    if ((bitsperpixel == 32)      &&
        (safeForHardware == YES)  &&
        (stretched == NO)         &&
        (interlaced == NO)        &&
        (w == width)              &&
        (h == height)             &&
        (rate == refreshrate || rate == 0))
    {
      CLog::Log(LOGDEBUG, "GetMode found a match!");
      return displayMode;
    }
  }
  CLog::Log(LOGERROR, "GetMode - no match found!");
  return NULL;
}

//---------------------------------------------------------------------------------
static void DisplayReconfigured(CGDirectDisplayID display,
  CGDisplayChangeSummaryFlags flags, void* userData)
{
  CWinSystemOSX *winsys = (CWinSystemOSX*)userData;
  if (!winsys)
    return;

  CLog::Log(LOGDEBUG, "CWinSystemOSX::DisplayReconfigured with flags %d", flags);

  // we fire the callbacks on start of configuration
  // or when the mode set was finished
  // or when we are called with flags == 0 (which is undocumented but seems to happen
  // on some macs - we treat it as device reset)

  // first check if we need to call OnLostDevice
  if (flags & kCGDisplayBeginConfigurationFlag)
  {
    // pre/post-reconfiguration changes
    RESOLUTION res = g_graphicsContext.GetVideoResolution();
    if (res == RES_INVALID)
      return;

    NSScreen* pScreen = nil;
    unsigned int screenIdx = CDisplaySettings::GetInstance().GetResolutionInfo(res).iScreen;

    if (screenIdx < [[NSScreen screens] count])
      pScreen = [[NSScreen screens] objectAtIndex:screenIdx];

    // kCGDisplayBeginConfigurationFlag is only fired while the screen is still
    // valid
    if (pScreen)
    {
      CGDirectDisplayID xbmc_display = GetDisplayIDFromScreen(pScreen);
      if (xbmc_display == display)
      {
        // we only respond to changes on the display we are running on.
        winsys->AnnounceOnLostDevice();
        winsys->StartLostDeviceTimer();
      }
    }
  }
  else // the else case checks if we need to call OnResetDevice
  {
    // we fire if kCGDisplaySetModeFlag is set or if flags == 0
    // (which is undocumented but seems to happen
    // on some macs - we treat it as device reset)
    // we also don't check the screen here as we might not even have
    // one anymore (e.x. when tv is turned off)
    if (flags & kCGDisplaySetModeFlag || flags == 0)
    {
      winsys->StopLostDeviceTimer(); // no need to timeout - we've got the callback
      winsys->AnnounceOnResetDevice();
    }
  }
}

//---------------------------------------------------------------------------------
//---------------------------------------------------------------------------------
CWinSystemOSX::CWinSystemOSX() : CWinSystemBase(), m_lostDeviceTimer(this)
{
  m_eWindowSystem = WINDOW_SYSTEM_OSX;
  m_obscured   = false;
  m_appWindow  = NULL;
  m_glView     = NULL;
  m_obscured_timecheck = XbmcThreads::SystemClockMillis() + 1000;
  m_use_system_screensaver = true;
  m_lastDisplayNr = -1;
  m_movedToOtherScreen = false;
  m_refreshRate = 0.0;
  m_fullscreenWillToggle = false;
}

CWinSystemOSX::~CWinSystemOSX()
{
}

// if there was a devicelost callback but no device reset for 3 secs
// a timeout fires the reset callback (for ensuring that e.x. AE isn't stuck)
#define LOST_DEVICE_TIMEOUT_MS 3000

void CWinSystemOSX::StartLostDeviceTimer()
{
  if (m_lostDeviceTimer.IsRunning())
    m_lostDeviceTimer.Restart();
  else
    m_lostDeviceTimer.Start(LOST_DEVICE_TIMEOUT_MS, false);
}

void CWinSystemOSX::StopLostDeviceTimer()
{
  m_lostDeviceTimer.Stop();
}

void CWinSystemOSX::OnTimeout()
{
  AnnounceOnResetDevice();
}

bool CWinSystemOSX::InitWindowSystem()
{
  if (!CWinSystemBase::InitWindowSystem())
    return false;

  CGDisplayRegisterReconfigurationCallback(DisplayReconfigured, (void*)this);

  return true;
}

bool CWinSystemOSX::DestroyWindowSystem()
{
  CGDisplayRemoveReconfigurationCallback(DisplayReconfigured, (void*)this);

  DestroyWindowInternal();
  
  if (m_glView)
  {
    // normally, this should happen here but we are racing internal object destructors
    // that make GL calls. They crash if the GLView is released.
    //[(OSXGLView*)m_glView release];
    m_glView = NULL;
  }
  
  return true;
}

bool CWinSystemOSX::CreateNewWindow(const std::string& name, bool fullScreen, RESOLUTION_INFO& res, PHANDLE_EVENT_FUNC userFunction)
{
  m_name = name;
  m_nWidth = res.iWidth;
  m_nHeight = res.iHeight;
  m_bFullScreen = fullScreen;

  __block NSWindow *appWindow;
  // because we are not main thread, delay any updates
  // and only become keyWindow after it finishes.
  [NSAnimationContext beginGrouping];
  [[NSAnimationContext currentContext] setCompletionHandler: ^{
    [appWindow makeKeyWindow];
  }];

  // for native fullscreen we always want to set the same windowed flags
  NSUInteger windowStyleMask;
  windowStyleMask = NSTitledWindowMask|NSResizableWindowMask|NSClosableWindowMask|NSMiniaturizableWindowMask;
  if (m_appWindow == NULL)
  {
    NSRect initialRect = NSMakeRect(0, 0, m_nWidth, m_nHeight);
    // gl contex must be on MCRuntimelib thread
    if(!m_glView)
      m_glView = [[OSXGLView alloc] initWithFrame:initialRect];
    OSXGLView *view = (OSXGLView*)m_glView;

    dispatch_sync(dispatch_get_main_queue(), ^{
      appWindow = [[OSXGLWindow alloc] initWithContentRect:NSMakeRect(0, 0, m_nWidth, m_nHeight) styleMask:windowStyleMask];
      NSString *title = [NSString stringWithUTF8String:m_name.c_str()];
      [appWindow setBackgroundColor:[NSColor blackColor]];
      [appWindow setTitle:title];
      [appWindow setOneShot:NO];

      NSWindowCollectionBehavior behavior = [appWindow collectionBehavior];
      behavior |= NSWindowCollectionBehaviorFullScreenPrimary;
      [appWindow setCollectionBehavior:behavior];

      // associate with current window
      [appWindow setContentView: view];
    });

    // make MCRuntimelib current context
    [[view getGLContext] makeCurrentContext];
    [[view getGLContext] update];

    if (!fullScreen)
    {
      NSRect rect = [appWindow contentRectForFrameRect:[appWindow frame]];
      CSettings::GetInstance().SetInt(CSettings::SETTING_WINDOW_LEFT,   rect.origin.x);
      CSettings::GetInstance().SetInt(CSettings::SETTING_WINDOW_TOP ,   rect.origin.y);
      CSettings::GetInstance().SetInt(CSettings::SETTING_WINDOW_WIDTH,  rect.size.width);
      CSettings::GetInstance().SetInt(CSettings::SETTING_WINDOW_HEIGHT, rect.size.height);
    }
    else
    {
      // set the toggle flag so that the
      // native "willenterfullscreen" et al callbacks
      // know that they are "called" by xbmc and not osx
      // toggle cocoa fullscreen mode
      m_fullscreenWillToggle = true;
      // toggleFullScreen is very nasty and really should be done on main thread
      [appWindow performSelectorOnMainThread:@selector(toggleFullScreen:) withObject:nil waitUntilDone:YES];
    }

    m_appWindow = appWindow;
    m_bWindowCreated = true;
  }

  // warning, we can order front but not become
  // key window or risk starting up with bad flicker
  // becoming key window must happen in completion block.
  [(NSWindow*)m_appWindow performSelectorOnMainThread:@selector(orderFront:) withObject:nil waitUntilDone:YES];


  [NSAnimationContext endGrouping];

  // check if we have to hide the mouse after creating the window
  // in case we start windowed with the mouse over the window
  // the tracking area mouseenter, mouseexit are not called
  // so we have to decide here to initial hide the os cursor
  NSPoint mouse = [NSEvent mouseLocation];
  if ([NSWindow windowNumberAtPoint:mouse belowWindowWithWindowNumber:0] == (m_appWindow).windowNumber)
  {
    // warp XBMC cursor to our position
    NSPoint locationInWindowCoords = [m_appWindow mouseLocationOutsideOfEventStream];
    XBMC_Event newEvent;
    memset(&newEvent, 0, sizeof(newEvent));
    newEvent.type = XBMC_MOUSEMOTION;
    newEvent.motion.type =  XBMC_MOUSEMOTION;
    newEvent.motion.x =  locationInWindowCoords.x;
    newEvent.motion.y =  locationInWindowCoords.y;
    g_application.OnEvent(newEvent);
  }

  return true;
}

bool CWinSystemOSX::DestroyWindowInternal()
{
  // set this 1st, we should really mutex protext m_appWindow in this class
  m_bWindowCreated = false;
  if (m_appWindow)
  {
    NSWindow *oldAppWindow = m_appWindow;
    m_appWindow = NULL;
    dispatch_sync(dispatch_get_main_queue(), ^{
      [oldAppWindow setContentView:nil];
    });
  }

  return true;
}

bool CWinSystemOSX::DestroyWindow()
{
  // when using native fullscreen
  // we never destroy the window
  // we reuse it ...
  return true;
}

bool CWinSystemOSX::ResizeWindowInternal(int newWidth, int newHeight, int newLeft, int newTop)
{
  NSWindow* window = m_appWindow;

  if (m_bFullScreen)
  {
    NSRect myNewFrame = NSMakeRect(newLeft, newTop, newWidth, newHeight);
    
    NSDictionary* windowResize = [NSDictionary dictionaryWithObjectsAndKeys: m_appWindow, NSViewAnimationTargetKey, [NSValue valueWithRect: myNewFrame], NSViewAnimationEndFrameKey, nil];
    NSArray* animations = [NSArray arrayWithObjects:windowResize, nil];
    NSViewAnimation* animation = [[NSViewAnimation alloc] initWithViewAnimations: animations];
    
    [animation setAnimationBlockingMode: NSAnimationBlocking];
    [animation setAnimationCurve: NSAnimationEaseIn];
    [animation setDuration: 0.5];
    [animation startAnimation];
  }
  else if ([window inLiveResize] == NO)
  {
    // during live resize, we are getting callbacks from windowDidResize
    // which propigate here, if we setFrame, that will trigger another
    // windowDidResize callback and all hell breaks lose. Do not setFrame
    // when live resize is active.
    NSRect myNewFrame = NSMakeRect(newLeft, newTop, newWidth, newHeight);
    NSRect rect_tmp = [window frameRectForContentRect:myNewFrame];
    dispatch_sync(dispatch_get_main_queue(), ^{
      [window setFrame:rect_tmp display:YES animate:YES];
    });
  }
  [window update];

  Cocoa_RunBlockOnMainQueue(^{
    OSXGLView *view = [m_appWindow contentView];
    [view setFrameOrigin:NSMakePoint(0.0, 0.0)];
    [view setFrameSize:NSMakeSize(newWidth, newHeight)];
    [[view getGLContext] update];
    view.needsDisplay = YES;
  });
  m_nWidth = newWidth;
  m_nHeight = newHeight;

  if (!m_bFullScreen)
  {
    NSRect rect = [window contentRectForFrameRect:[window frame]];
    CSettings::GetInstance().SetInt(CSettings::SETTING_WINDOW_LEFT,   rect.origin.x);
    CSettings::GetInstance().SetInt(CSettings::SETTING_WINDOW_TOP ,   rect.origin.y);
    CSettings::GetInstance().SetInt(CSettings::SETTING_WINDOW_WIDTH,  rect.size.width);
    CSettings::GetInstance().SetInt(CSettings::SETTING_WINDOW_HEIGHT, rect.size.height);
  }

  g_windowManager.MarkDirty();

  return true;
}

bool CWinSystemOSX::ResizeWindow(int newWidth, int newHeight, int newLeft, int newTop)
{
  if (!m_appWindow)
    return false;

  if (m_bFullScreen)
  {
    newTop = 0;
    newLeft = 0;
  }
  else
  {
    newTop = CSettings::GetInstance().GetInt(CSettings::SETTING_WINDOW_TOP);
    newLeft = CSettings::GetInstance().GetInt(CSettings::SETTING_WINDOW_LEFT);
  }

  return ResizeWindowInternal(newWidth, newHeight, newLeft, newTop);
}

bool CWinSystemOSX::SetFullScreen(bool fullScreen, RESOLUTION_INFO& res, bool blankOtherDisplays)
{
  CSingleLock lock (m_critSection);
  
  if (m_lastDisplayNr == -1)
    m_lastDisplayNr = res.iScreen;

  m_nWidth = res.iWidth;
  m_nHeight = res.iHeight;
  bool needToggle = m_bFullScreen || fullScreen;
  m_bFullScreen = fullScreen;
  
  NSWindow *window = m_appWindow;
  if (needToggle)
  {
    // set the toggle flag so that the
    // native "willenterfullscreen" et al callbacks
    // know that they are "called" by xbmc and not osx
    // toggle cocoa fullscreen mode
    m_fullscreenWillToggle = true;
    // toggleFullScreen is very nasty and really should be done on main thread
    [window performSelectorOnMainThread:@selector(toggleFullScreen:) withObject:nil waitUntilDone:YES];
  }

  [window setAllowsConcurrentViewDrawing:NO];

  if (m_bFullScreen)
  {
    ResizeWindowInternal(m_nWidth, m_nHeight, 0, 0);
    
    // switch videomode
    SwitchToVideoMode(res.iWidth, res.iHeight, res.fRefreshRate, res.iScreen);
    m_lastDisplayNr = res.iScreen;
    
    // hide the OS mouse
    Cocoa_HideMouse();
  }
  else
  {

    ResizeWindowInternal(
      CSettings::GetInstance().GetInt(CSettings::SETTING_WINDOW_WIDTH),
      CSettings::GetInstance().GetInt(CSettings::SETTING_WINDOW_HEIGHT),
      CSettings::GetInstance().GetInt(CSettings::SETTING_WINDOW_LEFT),
      CSettings::GetInstance().GetInt(CSettings::SETTING_WINDOW_TOP));

    // show the OS mouse
    Cocoa_ShowMouse();
  }

  [window setAllowsConcurrentViewDrawing:YES];

  return true;
}

void CWinSystemOSX::UpdateResolutions()
{
  CWinSystemBase::UpdateResolutions();

  // Add desktop resolution
  int w, h;
  double fps;

  // first screen goes into the current desktop mode
  GetScreenResolution(&w, &h, &fps, 0);
  UpdateDesktopResolution(CDisplaySettings::GetInstance().GetResolutionInfo(RES_DESKTOP), 0, w, h, fps);

  // see resolution.h enum RESOLUTION for how the resolutions
  // have to appear in the resolution info vector in CDisplaySettings
  // add the desktop resolutions of the other screens
  for(int i = 1; i < GetNumScreens(); i++)
  {
    RESOLUTION_INFO res;
    // get current resolution of screen i
    GetScreenResolution(&w, &h, &fps, i);
    UpdateDesktopResolution(res, i, w, h, fps);
    CDisplaySettings::GetInstance().AddResolutionInfo(res);
  }

  // now just fill in the possible reolutions for the attached screens
  // and push to the resolution info vector
  FillInVideoModes();
}

void CWinSystemOSX::GetScreenResolution(int* w, int* h, double* fps, int screenIdx)
{
  // Figure out the screen size. (default to main screen)
  if (screenIdx >= GetNumScreens())
    return;

  CGDirectDisplayID display_id = (CGDirectDisplayID)GetDisplayID(screenIdx);
 
  if (m_appWindow)
    display_id = GetDisplayIDFromScreen( [m_appWindow screen] );
  CGDisplayModeRef mode  = CGDisplayCopyDisplayMode(display_id);
  *w = CGDisplayModeGetWidth(mode);
  *h = CGDisplayModeGetHeight(mode);
  *fps = CGDisplayModeGetRefreshRate(mode);
  CGDisplayModeRelease(mode);
  if ((int)*fps == 0)
  {
    // NOTE: The refresh rate will be REPORTED AS 0 for many DVI and notebook displays.
    *fps = 60.0;
  }
}

void CWinSystemOSX::EnableVSync(bool enable)
{
  // OpenGL Flush synchronised with vertical retrace
  GLint swapInterval = enable ? 1 : 0;
  [[NSOpenGLContext currentContext] setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];
}

bool CWinSystemOSX::SwitchToVideoMode(int width, int height, double refreshrate, int screenIdx)
{
  // SwitchToVideoMode will not return until the display has actually switched over.
  // This can take several seconds.
  if( screenIdx >= GetNumScreens())
    return false;

  boolean_t match = false;
  CFDictionaryRef dispMode = NULL;
  // Figure out the screen size. (default to main screen)
  CGDirectDisplayID display_id = GetDisplayID(screenIdx);

  // find mode that matches the desired size, refreshrate
  // non interlaced, nonstretched, safe for hardware
  dispMode = GetMode(width, height, refreshrate, screenIdx);

  //not found - fallback to bestemdeforparameters
  if (!dispMode)
  {
    dispMode = CGDisplayBestModeForParameters(display_id, 32, width, height, &match);

    if (!match)
      dispMode = CGDisplayBestModeForParameters(display_id, 16, width, height, &match);

    if (!match)
      return false;
  }

  // switch mode and return success
  CGDisplayCapture(display_id);
  CGDisplayConfigRef cfg;
  CGBeginDisplayConfiguration(&cfg);
  // we don't need to do this, we are already faded.
  //CGConfigureDisplayFadeEffect(cfg, 0.3f, 0.5f, 0, 0, 0);
  CGConfigureDisplayMode(cfg, display_id, dispMode);
  CGError err = CGCompleteDisplayConfiguration(cfg, kCGConfigureForAppOnly);
  CGDisplayRelease(display_id);
  
  m_refreshRate = GetDictionaryDouble(dispMode, kCGDisplayRefreshRate);

  Cocoa_CVDisplayLinkUpdate();

  return (err == kCGErrorSuccess);
}

void CWinSystemOSX::FillInVideoModes()
{
  // Add full screen settings for additional monitors
  int numDisplays = [[NSScreen screens] count];

  for (int disp = 0; disp < numDisplays; disp++)
  {
    Boolean stretched;
    Boolean interlaced;
    Boolean safeForHardware;
    Boolean televisionoutput;
    int w, h, bitsperpixel;
    double refreshrate;
    RESOLUTION_INFO res;

    CFArrayRef displayModes = CGDisplayAvailableModes(GetDisplayID(disp));
    NSString *dispName = screenNameForDisplay(GetDisplayID(disp));
    CLog::Log(LOGNOTICE, "Display %i has name %s", disp, [dispName UTF8String]);

    if (NULL == displayModes)
      continue;

    for (int i = 0; i < CFArrayGetCount(displayModes); ++i)
    {
      CFDictionaryRef displayMode = (CFDictionaryRef)CFArrayGetValueAtIndex(displayModes, i);

      stretched = GetDictionaryBoolean(displayMode, kCGDisplayModeIsStretched);
      interlaced = GetDictionaryBoolean(displayMode, kCGDisplayModeIsInterlaced);
      bitsperpixel = GetDictionaryInt(displayMode, kCGDisplayBitsPerPixel);
      safeForHardware = GetDictionaryBoolean(displayMode, kCGDisplayModeIsSafeForHardware);
      televisionoutput = GetDictionaryBoolean(displayMode, kCGDisplayModeIsTelevisionOutput);

      if ((bitsperpixel == 32)      &&
          (safeForHardware == YES)  &&
          (stretched == NO)         &&
          (interlaced == NO))
      {
        w = GetDictionaryInt(displayMode, kCGDisplayWidth);
        h = GetDictionaryInt(displayMode, kCGDisplayHeight);
        refreshrate = GetDictionaryDouble(displayMode, kCGDisplayRefreshRate);
        if ((int)refreshrate == 0)  // LCD display?
        {
          // NOTE: The refresh rate will be REPORTED AS 0 for many DVI and notebook displays.
          refreshrate = 60.0;
        }
        CLog::Log(LOGNOTICE, "Found possible resolution for display %d with %d x %d @ %f Hz\n", disp, w, h, refreshrate);

        UpdateDesktopResolution(res, disp, w, h, refreshrate);

        // overwrite the mode str because  UpdateDesktopResolution adds a
        // "Full Screen". Since the current resolution is there twice
        // this would lead to 2 identical resolution entrys in the guisettings.xml.
        // That would cause problems with saving screen overscan calibration
        // because the wrong entry is picked on load.
        // So we just use UpdateDesktopResolutions for the current DESKTOP_RESOLUTIONS
        // in UpdateResolutions. And on all othere resolutions make a unique
        // mode str by doing it without appending "Full Screen".
        // this is what linux does - though it feels that there shouldn't be
        // the same resolution twice... - thats why i add a FIXME here.
        res.strMode = StringUtils::Format("%dx%d @ %.2f", w, h, refreshrate);
        g_graphicsContext.ResetOverscan(res);
        CDisplaySettings::GetInstance().AddResolutionInfo(res);
      }
    }
  }
}

bool CWinSystemOSX::FlushBuffer(void)
{
  if (m_appWindow)
  {
    OSXGLView *contentView = [m_appWindow contentView];
    NSOpenGLContext *glcontex = [contentView getGLContext];
    [glcontex flushBuffer];
  }
  return true;
}

bool CWinSystemOSX::IsObscured(void)
{
  if (m_bFullScreen && !CSettings::GetInstance().GetBool(CSettings::SETTING_VIDEOSCREEN_FAKEFULLSCREEN))
    return false;// in true fullscreen mode - we can't be obscured by anyone...
  if (m_obscured)
    CLog::Log(LOGDEBUG, "CWinSystemOSX::IsObscured(void) - TRUE");
  return m_obscured;
}

void CWinSystemOSX::NotifyAppFocusChange(bool bGaining)
{
  if (m_bFullScreen && bGaining)
  {
    // find the window
    NSOpenGLContext* context = [NSOpenGLContext currentContext];
    if (context)
    {
      NSView* view;

      view = [context view];
      if (view)
      {
        NSWindow* window;
        window = [view window];
        if (window)
        {
          SetMenuBarVisible(false);
          [window orderFront:nil];
        }
      }
    }
  }
}

void CWinSystemOSX::ShowOSMouse(bool show)
{
}

bool CWinSystemOSX::Minimize()
{
  [[NSApplication sharedApplication] miniaturizeAll:nil];
  return true;
}

bool CWinSystemOSX::Restore()
{
  [[NSApplication sharedApplication] unhide:nil];
  return true;
}

bool CWinSystemOSX::Hide()
{
  [[NSApplication sharedApplication] hide:nil];
  return true;
}

void CWinSystemOSX::HandlePossibleRefreshrateChange()
{
  static double oldRefreshRate = m_refreshRate;
  Cocoa_CVDisplayLinkUpdate();
  int dummy = 0;
  
  GetScreenResolution(&dummy, &dummy, &m_refreshRate, GetCurrentScreen());

  if (oldRefreshRate != m_refreshRate)
  {
    oldRefreshRate = m_refreshRate;
    // send a message so that videoresolution (and refreshrate) is changed
    KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_SETVIDEORESOLUTION, m_refreshRate, 0);
  }
}

void CWinSystemOSX::OnMove(int x, int y)
{
  if (!m_bFullScreen)
  {
    NSWindow *window = (NSWindow *)m_appWindow;
    NSRect rect = [window contentRectForFrameRect:[window frame]];
    CSettings::GetInstance().SetInt(CSettings::SETTING_WINDOW_LEFT,   rect.origin.x);
    CSettings::GetInstance().SetInt(CSettings::SETTING_WINDOW_TOP ,   rect.origin.y);
    CSettings::GetInstance().SetInt(CSettings::SETTING_WINDOW_WIDTH,  rect.size.width);
    CSettings::GetInstance().SetInt(CSettings::SETTING_WINDOW_HEIGHT, rect.size.height);
  }
}

IOPMAssertionID systemSleepAssertionID = kIOPMNullAssertionID;
void CWinSystemOSX::EnableSystemScreenSaver(bool bEnable)
{
  // see Technical Q&A QA1340
  static IOPMAssertionID systemIdleAssertionID = kIOPMNullAssertionID;
  static IOPMAssertionID systemSleepAssertionID = kIOPMNullAssertionID;

  if (bEnable)
  {
    if (systemIdleAssertionID != kIOPMNullAssertionID)
    {
      IOPMAssertionRelease(systemIdleAssertionID);
      systemIdleAssertionID = kIOPMNullAssertionID;
    }
    if (systemSleepAssertionID != kIOPMNullAssertionID)
    {
      IOPMAssertionRelease(systemSleepAssertionID);
      systemSleepAssertionID = kIOPMNullAssertionID;
    }
  }
  else
  {
    if (systemIdleAssertionID == kIOPMNullAssertionID)
    {
      CFStringRef reasonForActivity= CFSTR("MrMC requested disable system idle sleep");
      IOPMAssertionCreateWithName(kIOPMAssertionTypeNoIdleSleep,
        kIOPMAssertionLevelOn, reasonForActivity, &systemIdleAssertionID);
    }
    if (systemSleepAssertionID == kIOPMNullAssertionID)
    {
      CFStringRef reasonForActivity= CFSTR("MrMC requested disable system screen saver");
      IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep,
        kIOPMAssertionLevelOn, reasonForActivity, &systemSleepAssertionID);
    }
  }

  m_use_system_screensaver = bEnable;
}

bool CWinSystemOSX::IsSystemScreenSaverEnabled()
{
  return m_use_system_screensaver;
}

void CWinSystemOSX::ResetOSScreensaver()
{
  // allow os screensaver only if we are fullscreen
  EnableSystemScreenSaver(!m_bFullScreen);
}

bool CWinSystemOSX::EnableFrameLimiter()
{
  if ([NSThread currentThread] != [NSThread mainThread])
  {
    __block bool rtn;
    dispatch_sync(dispatch_get_main_queue(),^{
      rtn = IsObscured();
    });
    return rtn;
  }
  else
  {
    return IsObscured();
  }
}

void CWinSystemOSX::EnableTextInput(bool bEnable)
{
  if (bEnable)
    StartTextInput();
  else
    StopTextInput();
}

OSXTextInputResponder *g_textInputResponder = nil;
bool CWinSystemOSX::IsTextInputEnabled()
{
  return g_textInputResponder != nil && [[g_textInputResponder superview] isEqual: [[NSApp keyWindow] contentView]];
}

void CWinSystemOSX::StartTextInput()
{
  NSView *parentView = [[NSApp keyWindow] contentView];

  /* We only keep one field editor per process, since only the front most
   * window can receive text input events, so it make no sense to keep more
   * than one copy. When we switched to another window and requesting for
   * text input, simply remove the field editor from its superview then add
   * it to the front most window's content view */
  if (!g_textInputResponder) {
    g_textInputResponder =
    [[OSXTextInputResponder alloc] initWithFrame: NSMakeRect(0.0, 0.0, 0.0, 0.0)];
  }

  if (![[g_textInputResponder superview] isEqual: parentView])
  {
//    DLOG(@"add fieldEdit to window contentView");
    [g_textInputResponder removeFromSuperview];
    [parentView addSubview: g_textInputResponder];
    [[NSApp keyWindow] makeFirstResponder: g_textInputResponder];
  }
}
void CWinSystemOSX::StopTextInput()
{
  if (g_textInputResponder) {
    [g_textInputResponder removeFromSuperview];
    g_textInputResponder = nil;
  }
}

void CWinSystemOSX::Register(IDispResource *resource)
{
  CSingleLock lock(m_resourceSection);
  m_resources.push_back(resource);
}

void CWinSystemOSX::Unregister(IDispResource* resource)
{
  CSingleLock lock(m_resourceSection);
  std::vector<IDispResource*>::iterator i = find(m_resources.begin(), m_resources.end(), resource);
  if (i != m_resources.end())
    m_resources.erase(i);
}

bool CWinSystemOSX::Show(bool raise)
{
  if (raise)
  {
    [[NSApplication sharedApplication] unhide:nil];
    [[NSApplication sharedApplication] activateIgnoringOtherApps: YES];
    [[NSApplication sharedApplication] arrangeInFront:nil];
  }
  else
  {
    [[NSApplication sharedApplication] unhideWithoutActivation];
  }

  return true;
}

int CWinSystemOSX::GetNumScreens()
{
  int numDisplays = [[NSScreen screens] count];
  return(numDisplays);
}

int CWinSystemOSX::GetCurrentScreen()
{
  
  // if user hasn't moved us in windowed mode - return the
  // last display we were fullscreened at
  if (!m_movedToOtherScreen)
    return m_lastDisplayNr;

  if (m_appWindow)
  {
    m_movedToOtherScreen = false;
    return GetDisplayIndex(GetDisplayIDFromScreen( [(NSWindow *)m_appWindow screen]));
  }
  return 0;
}

int CWinSystemOSX::CheckDisplayChanging()
{
  NSOpenGLContext* context = [NSOpenGLContext currentContext];
  
  // if user hasn't moved us in windowed mode - return the
  // last display we were fullscreened at
  if (!m_movedToOtherScreen)
    return m_lastDisplayNr;
  
  // if we are here the user dragged the window to a different
  // screen and we return the screen of the window
  if (context)
  {
    NSView* view;

    view = [context view];
    if (view)
    {
      NSWindow* window;
      window = [view window];
      if (window)
      {
        m_movedToOtherScreen = false;
        return GetDisplayIndex(GetDisplayIDFromScreen( [window screen] ));
      }
        
    }
  }
  return 0;
}

void CWinSystemOSX::SetMovedToOtherScreen(bool moved)
{
  m_movedToOtherScreen = moved;
  WindowChangedScreen();
}

void CWinSystemOSX::SetOcclusionState(bool occluded)
{
  m_obscured = occluded;
  CLog::Log(LOGDEBUG, "CWinSystemOSX::SetOcclusionState(bool occluded) - %s", occluded ? "true":"false");
}

void CWinSystemOSX::WindowChangedScreen()
{
  // user has moved the window to a
  // different screen
  m_movedToOtherScreen = true;
  Cocoa_CVDisplayLinkUpdate();
//  HandlePossibleRefreshrateChange();
  CheckDisplayChanging();
}

void CWinSystemOSX::AnnounceOnLostDevice()
{
  CSingleLock lock(m_resourceSection);
  // tell any shared resources
  CLog::Log(LOGDEBUG, "CWinSystemOSX::AnnounceOnLostDevice");
  for (std::vector<IDispResource *>::iterator i = m_resources.begin(); i != m_resources.end(); i++)
    (*i)->OnLostDevice();
}

void CWinSystemOSX::AnnounceOnResetDevice()
{
  CSingleLock lock(m_resourceSection);
  // tell any shared resources
  CLog::Log(LOGDEBUG, "CWinSystemOSX::AnnounceOnResetDevice");
  for (std::vector<IDispResource *>::iterator i = m_resources.begin(); i != m_resources.end(); i++)
    (*i)->OnResetDevice();
}

CGLContextObj CWinSystemOSX::GetCGLContextObj()
{
  CGLContextObj cglcontex = NULL;
  if(m_appWindow)
  {
    OSXGLView *contentView = [(NSWindow*)m_appWindow contentView];
    cglcontex = [[contentView getGLContext] CGLContextObj];
  }

  return cglcontex;
}

std::string CWinSystemOSX::GetClipboardText(void)
{
  std::string utf8_text;

  const char *szStr = Cocoa_Paste();
  if (szStr)
    utf8_text = szStr;

  return utf8_text;
}

float CWinSystemOSX::CocoaToNativeFlip(float y)
{
  // OpenGL specifies that the default origin is at bottom-left.
  // Cocoa specifies that the default origin is at bottom-left.
  // Direct3D specifies that the default origin is at top-left.
  // SDL specifies that the default origin is at top-left.
  // WTF ?

  // TODO hook height and width up to resize events of window and cache them as member
  if (m_appWindow)
  {
    NSWindow *win = (NSWindow *)m_appWindow;
    NSRect frame = [[win contentView] frame];
    y = frame.size.height - y;
  }
  return y;
}

#endif
