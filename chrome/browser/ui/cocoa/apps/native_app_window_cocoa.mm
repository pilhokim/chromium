// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/apps/native_app_window_cocoa.h"

#include "apps/app_shim/extension_app_shim_handler_mac.h"
#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#include "chrome/browser/ui/cocoa/extensions/extension_keybinding_registry_cocoa.h"
#include "chrome/browser/ui/cocoa/extensions/extension_view_mac.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "extensions/common/extension.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/skia_util.h"

// NOTE: State Before Update.
//
// Internal state, such as |is_maximized_|, must be set before the window
// state is changed so that it is accurate when e.g. a resize results in a call
// to |OnNativeWindowChanged|.

// NOTE: Maximize and Zoom.
//
// Zooming is implemented manually in order to implement maximize functionality
// and to support non resizable windows. The window will be resized explicitly
// in the |WindowWillZoom| call.
//
// Attempting maximize and restore functionality with non resizable windows
// using the native zoom method did not work, even with
// windowWillUseStandardFrame, as the window would not restore back to the
// desired size.

using apps::AppWindow;

@interface NSWindow (NSPrivateApis)
- (void)setBottomCornerRounded:(BOOL)rounded;
@end

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

@interface NSWindow (LionSDKDeclarations)
- (void)toggleFullScreen:(id)sender;
@end

enum {
  NSWindowCollectionBehaviorFullScreenPrimary = 1 << 7,
  NSFullScreenWindowMask = 1 << 14
};

#endif  // MAC_OS_X_VERSION_10_7

namespace {

void SetFullScreenCollectionBehavior(NSWindow* window, bool allow_fullscreen) {
  NSWindowCollectionBehavior behavior = [window collectionBehavior];
  if (allow_fullscreen)
    behavior |= NSWindowCollectionBehaviorFullScreenPrimary;
  else
    behavior &= ~NSWindowCollectionBehaviorFullScreenPrimary;
  [window setCollectionBehavior:behavior];
}

void InitCollectionBehavior(NSWindow* window) {
  // Since always-on-top windows have a higher window level
  // than NSNormalWindowLevel, they will default to
  // NSWindowCollectionBehaviorTransient. Set the value
  // explicitly here to match normal windows.
  NSWindowCollectionBehavior behavior = [window collectionBehavior];
  behavior |= NSWindowCollectionBehaviorManaged;
  [window setCollectionBehavior:behavior];
}

// Returns the level for windows that are configured to be always on top.
// This is not a constant because NSFloatingWindowLevel is a macro defined
// as a function call.
NSInteger AlwaysOnTopWindowLevel() {
  return NSFloatingWindowLevel;
}

NSRect GfxToCocoaBounds(gfx::Rect bounds) {
  typedef apps::AppWindow::BoundsSpecification BoundsSpecification;

  NSRect main_screen_rect = [[[NSScreen screens] objectAtIndex:0] frame];

  // If coordinates are unspecified, center window on primary screen.
  if (bounds.x() == BoundsSpecification::kUnspecifiedPosition)
    bounds.set_x(floor((NSWidth(main_screen_rect) - bounds.width()) / 2));
  if (bounds.y() == BoundsSpecification::kUnspecifiedPosition)
    bounds.set_y(floor((NSHeight(main_screen_rect) - bounds.height()) / 2));

  // Convert to Mac coordinates.
  NSRect cocoa_bounds = NSRectFromCGRect(bounds.ToCGRect());
  cocoa_bounds.origin.y = NSHeight(main_screen_rect) - NSMaxY(cocoa_bounds);
  return cocoa_bounds;
}

// Return a vector of non-draggable regions that fill a window of size
// |width| by |height|, but leave gaps where the window should be draggable.
std::vector<gfx::Rect> CalculateNonDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions,
    int width,
    int height) {
  std::vector<gfx::Rect> result;
  if (regions.empty()) {
    result.push_back(gfx::Rect(0, 0, width, height));
  } else {
    scoped_ptr<SkRegion> draggable(
        AppWindow::RawDraggableRegionsToSkRegion(regions));
    scoped_ptr<SkRegion> non_draggable(new SkRegion);
    non_draggable->op(0, 0, width, height, SkRegion::kUnion_Op);
    non_draggable->op(*draggable, SkRegion::kDifference_Op);
    for (SkRegion::Iterator it(*non_draggable); !it.done(); it.next()) {
      result.push_back(gfx::SkIRectToRect(it.rect()));
    }
  }
  return result;
}

}  // namespace

@implementation NativeAppWindowController

@synthesize appWindow = appWindow_;

- (void)windowWillClose:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowWillClose();
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidBecomeKey();
}

- (void)windowDidResignKey:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidResignKey();
}

- (void)windowDidResize:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidResize();
}

- (void)windowDidEndLiveResize:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidFinishResize();
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidFinishResize();
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidFinishResize();
}

- (void)windowDidMove:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidMove();
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidMiniaturize();
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowDidDeminiaturize();
}

- (BOOL)windowShouldZoom:(NSWindow*)window
                 toFrame:(NSRect)newFrame {
  if (appWindow_)
    appWindow_->WindowWillZoom();
  return NO;  // See top of file NOTE: Maximize and Zoom.
}

// Allow non resizable windows (without NSResizableWindowMask) to enter
// fullscreen by passing through the full size in willUseFullScreenContentSize.
- (NSSize)window:(NSWindow *)window
    willUseFullScreenContentSize:(NSSize)proposedSize {
  return proposedSize;
}

- (void)executeCommand:(int)command {
  // No-op, swallow the event.
}

- (BOOL)handledByExtensionCommand:(NSEvent*)event {
  if (appWindow_)
    return appWindow_->HandledByExtensionCommand(event);
  return NO;
}

@end

// This is really a method on NSGrayFrame, so it should only be called on the
// view passed into -[NSWindow drawCustomFrameRect:forView:].
@interface NSView (PrivateMethods)
- (CGFloat)roundedCornerRadius;
@end

// TODO(jamescook): Should these be AppNSWindow to match apps::AppWindow?
// http://crbug.com/344082
@interface ShellNSWindow : ChromeEventProcessingWindow
@end
@implementation ShellNSWindow
@end

@interface ShellCustomFrameNSWindow : ShellNSWindow

- (void)drawCustomFrameRect:(NSRect)rect forView:(NSView*)view;

@end

@implementation ShellCustomFrameNSWindow

- (void)drawCustomFrameRect:(NSRect)rect forView:(NSView*)view {
  [[NSBezierPath bezierPathWithRect:rect] addClip];
  [[NSColor clearColor] set];
  NSRectFill(rect);

  // Set up our clip.
  CGFloat cornerRadius = 4.0;
  if ([view respondsToSelector:@selector(roundedCornerRadius)])
    cornerRadius = [view roundedCornerRadius];
  [[NSBezierPath bezierPathWithRoundedRect:[view bounds]
                                   xRadius:cornerRadius
                                   yRadius:cornerRadius] addClip];
  [[NSColor whiteColor] set];
  NSRectFill(rect);
}

@end

@interface ShellFramelessNSWindow : ShellCustomFrameNSWindow

@end

@implementation ShellFramelessNSWindow

+ (NSRect)frameRectForContentRect:(NSRect)contentRect
                        styleMask:(NSUInteger)mask {
  return contentRect;
}

+ (NSRect)contentRectForFrameRect:(NSRect)frameRect
                        styleMask:(NSUInteger)mask {
  return frameRect;
}

- (NSRect)frameRectForContentRect:(NSRect)contentRect {
  return contentRect;
}

- (NSRect)contentRectForFrameRect:(NSRect)frameRect {
  return frameRect;
}

@end

@interface ControlRegionView : NSView
@end

@implementation ControlRegionView

- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

- (NSView*)hitTest:(NSPoint)aPoint {
  return nil;
}

@end

@interface NSView (WebContentsView)
- (void)setMouseDownCanMoveWindow:(BOOL)can_move;
@end

NativeAppWindowCocoa::NativeAppWindowCocoa(
    AppWindow* app_window,
    const AppWindow::CreateParams& params)
    : app_window_(app_window),
      has_frame_(params.frame == AppWindow::FRAME_CHROME),
      is_hidden_(false),
      is_hidden_with_app_(false),
      is_maximized_(false),
      is_fullscreen_(false),
      is_resizable_(params.resizable),
      shows_resize_controls_(true),
      shows_fullscreen_controls_(true),
      attention_request_id_(0) {
  Observe(web_contents());

  base::scoped_nsobject<NSWindow> window;
  Class window_class;
  if (has_frame_) {
    bool should_use_native_frame =
        CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAppsUseNativeFrame);
    window_class = should_use_native_frame ?
        [ShellNSWindow class] : [ShellCustomFrameNSWindow class];
  } else {
    window_class = [ShellFramelessNSWindow class];
  }

  // Estimate the initial bounds of the window. Once the frame insets are known,
  // the window bounds and constraints can be set precisely.
  NSRect cocoa_bounds = GfxToCocoaBounds(
      params.GetInitialWindowBounds(gfx::Insets()));
  window.reset([[window_class alloc]
      initWithContentRect:cocoa_bounds
                styleMask:GetWindowStyleMask()
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [window setTitle:base::SysUTF8ToNSString(extension()->name())];
  [[window contentView] cr_setWantsLayer:YES];

  if (base::mac::IsOSSnowLeopard() &&
      [window respondsToSelector:@selector(setBottomCornerRounded:)])
    [window setBottomCornerRounded:NO];

  if (params.always_on_top)
    [window setLevel:AlwaysOnTopWindowLevel()];
  InitCollectionBehavior(window);

  window_controller_.reset(
      [[NativeAppWindowController alloc] initWithWindow:window.release()]);

  NSView* view = web_contents()->GetView()->GetNativeView();
  [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  InstallView();

  [[window_controller_ window] setDelegate:window_controller_];
  [window_controller_ setAppWindow:this];

  // We can now compute the precise window bounds and constraints.
  gfx::Insets insets = GetFrameInsets();
  SetBounds(params.GetInitialWindowBounds(insets));
  SetContentSizeConstraints(params.GetContentMinimumSize(insets),
                            params.GetContentMaximumSize(insets));

  // Initialize |restored_bounds_|.
  restored_bounds_ = [this->window() frame];

  extension_keybinding_registry_.reset(new ExtensionKeybindingRegistryCocoa(
      Profile::FromBrowserContext(app_window_->browser_context()),
      window,
      extensions::ExtensionKeybindingRegistry::PLATFORM_APPS_ONLY,
      NULL));
}

NSUInteger NativeAppWindowCocoa::GetWindowStyleMask() const {
  NSUInteger style_mask = NSTitledWindowMask | NSClosableWindowMask |
                          NSMiniaturizableWindowMask;
  if (shows_resize_controls_)
    style_mask |= NSResizableWindowMask;
  if (!has_frame_ ||
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAppsUseNativeFrame)) {
    style_mask |= NSTexturedBackgroundWindowMask;
  }
  return style_mask;
}

void NativeAppWindowCocoa::InstallView() {
  NSView* view = web_contents()->GetView()->GetNativeView();
  if (has_frame_) {
    [view setFrame:[[window() contentView] bounds]];
    [[window() contentView] addSubview:view];
    if (!shows_fullscreen_controls_)
      [[window() standardWindowButton:NSWindowZoomButton] setEnabled:NO];
    if (!shows_resize_controls_)
      [window() setShowsResizeIndicator:NO];
  } else {
    // TODO(jeremya): find a cleaner way to send this information to the
    // WebContentsViewCocoa view.
    DCHECK([view
        respondsToSelector:@selector(setMouseDownCanMoveWindow:)]);
    [view setMouseDownCanMoveWindow:YES];

    NSView* frameView = [[window() contentView] superview];
    [view setFrame:[frameView bounds]];
    [frameView addSubview:view];

    [[window() standardWindowButton:NSWindowZoomButton] setHidden:YES];
    [[window() standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
    [[window() standardWindowButton:NSWindowCloseButton] setHidden:YES];

    // Some third-party OS X utilities check the zoom button's enabled state to
    // determine whether to show custom UI on hover, so we disable it here to
    // prevent them from doing so in a frameless app window.
    [[window() standardWindowButton:NSWindowZoomButton] setEnabled:NO];

    UpdateDraggableRegionViews();
  }
}

void NativeAppWindowCocoa::UninstallView() {
  NSView* view = web_contents()->GetView()->GetNativeView();
  [view removeFromSuperview];
}

bool NativeAppWindowCocoa::IsActive() const {
  return [window() isKeyWindow];
}

bool NativeAppWindowCocoa::IsMaximized() const {
  return is_maximized_;
}

bool NativeAppWindowCocoa::IsMinimized() const {
  return [window() isMiniaturized];
}

bool NativeAppWindowCocoa::IsFullscreen() const {
  return is_fullscreen_;
}

void NativeAppWindowCocoa::SetFullscreen(int fullscreen_types) {
  bool fullscreen = (fullscreen_types != AppWindow::FULLSCREEN_TYPE_NONE);
  if (fullscreen == is_fullscreen_)
    return;
  is_fullscreen_ = fullscreen;

  if (base::mac::IsOSLionOrLater()) {
    // If going fullscreen, but the window is constrained (fullscreen UI control
    // is disabled), temporarily enable it. It will be disabled again on leaving
    // fullscreen.
    if (fullscreen && !shows_fullscreen_controls_)
      SetFullScreenCollectionBehavior(window(), true);
    [window() toggleFullScreen:nil];
    return;
  }

  DCHECK(base::mac::IsOSSnowLeopard());

  // Fade to black.
  const CGDisplayReservationInterval kFadeDurationSeconds = 0.6;
  bool did_fade_out = false;
  CGDisplayFadeReservationToken token;
  if (CGAcquireDisplayFadeReservation(kFadeDurationSeconds, &token) ==
      kCGErrorSuccess) {
    did_fade_out = true;
    CGDisplayFade(token, kFadeDurationSeconds / 2, kCGDisplayBlendNormal,
        kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, /*synchronous=*/true);
  }

  // Since frameless windows insert the WebContentsView into the NSThemeFrame
  // ([[window contentView] superview]), and since that NSThemeFrame is
  // destroyed and recreated when we change the styleMask of the window, we
  // need to remove the view from the window when we change the style, and
  // add it back afterwards.
  UninstallView();
  if (fullscreen) {
    UpdateRestoredBounds();
    [window() setStyleMask:NSBorderlessWindowMask];
    [window() setFrame:[window()
        frameRectForContentRect:[[window() screen] frame]]
               display:YES];
    base::mac::RequestFullScreen(base::mac::kFullScreenModeAutoHideAll);
  } else {
    base::mac::ReleaseFullScreen(base::mac::kFullScreenModeAutoHideAll);
    [window() setStyleMask:GetWindowStyleMask()];
    [window() setFrame:restored_bounds_ display:YES];
  }
  InstallView();

  // Fade back in.
  if (did_fade_out) {
    CGDisplayFade(token, kFadeDurationSeconds / 2, kCGDisplayBlendSolidColor,
        kCGDisplayBlendNormal, 0.0, 0.0, 0.0, /*synchronous=*/false);
    CGReleaseDisplayFadeReservation(token);
  }
}

bool NativeAppWindowCocoa::IsFullscreenOrPending() const {
  return is_fullscreen_;
}

bool NativeAppWindowCocoa::IsDetached() const {
  return false;
}

gfx::NativeWindow NativeAppWindowCocoa::GetNativeWindow() {
  return window();
}

gfx::Rect NativeAppWindowCocoa::GetRestoredBounds() const {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect frame = restored_bounds_;
  gfx::Rect bounds(frame.origin.x, 0, NSWidth(frame), NSHeight(frame));
  bounds.set_y(NSHeight([screen frame]) - NSMaxY(frame));
  return bounds;
}

ui::WindowShowState NativeAppWindowCocoa::GetRestoredState() const {
  if (IsMaximized())
    return ui::SHOW_STATE_MAXIMIZED;
  if (IsFullscreen())
    return ui::SHOW_STATE_FULLSCREEN;
  return ui::SHOW_STATE_NORMAL;
}

gfx::Rect NativeAppWindowCocoa::GetBounds() const {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect frame = [window() frame];
  gfx::Rect bounds(frame.origin.x, 0, NSWidth(frame), NSHeight(frame));
  bounds.set_y(NSHeight([screen frame]) - NSMaxY(frame));
  return bounds;
}

void NativeAppWindowCocoa::Show() {
  is_hidden_ = false;

  if (is_hidden_with_app_) {
    // If there is a shim to gently request attention, return here. Otherwise
    // show the window as usual.
    if (apps::ExtensionAppShimHandler::RequestUserAttentionForWindow(
            app_window_)) {
      return;
    }
  }

  [window_controller_ showWindow:nil];
  Activate();
}

void NativeAppWindowCocoa::ShowInactive() {
  is_hidden_ = false;
  [window() orderFront:window_controller_];
}

void NativeAppWindowCocoa::Hide() {
  is_hidden_ = true;
  HideWithoutMarkingHidden();
}

void NativeAppWindowCocoa::Close() {
  [window() performClose:nil];
}

void NativeAppWindowCocoa::Activate() {
  [BrowserWindowUtils activateWindowForController:window_controller_];
}

void NativeAppWindowCocoa::Deactivate() {
  // TODO(jcivelli): http://crbug.com/51364 Implement me.
  NOTIMPLEMENTED();
}

void NativeAppWindowCocoa::Maximize() {
  UpdateRestoredBounds();
  is_maximized_ = true;  // See top of file NOTE: State Before Update.
  [window() setFrame:[[window() screen] visibleFrame] display:YES animate:YES];
}

void NativeAppWindowCocoa::Minimize() {
  [window() miniaturize:window_controller_];
}

void NativeAppWindowCocoa::Restore() {
  DCHECK(!IsFullscreenOrPending());   // SetFullscreen, not Restore, expected.

  if (IsMaximized()) {
    is_maximized_ = false;  // See top of file NOTE: State Before Update.
    [window() setFrame:restored_bounds() display:YES animate:YES];
  } else if (IsMinimized()) {
    is_maximized_ = false;  // See top of file NOTE: State Before Update.
    [window() deminiaturize:window_controller_];
  }
}

void NativeAppWindowCocoa::SetBounds(const gfx::Rect& bounds) {
  // Enforce minimum/maximum bounds.
  gfx::Rect checked_bounds = bounds;

  NSSize min_size = [window() minSize];
  if (bounds.width() < min_size.width)
    checked_bounds.set_width(min_size.width);
  if (bounds.height() < min_size.height)
    checked_bounds.set_height(min_size.height);
  NSSize max_size = [window() maxSize];
  if (checked_bounds.width() > max_size.width)
    checked_bounds.set_width(max_size.width);
  if (checked_bounds.height() > max_size.height)
    checked_bounds.set_height(max_size.height);

  NSRect cocoa_bounds = GfxToCocoaBounds(checked_bounds);
  [window() setFrame:cocoa_bounds display:YES];
  // setFrame: without animate: does not trigger a windowDidEndLiveResize: so
  // call it here.
  WindowDidFinishResize();
}

void NativeAppWindowCocoa::UpdateWindowIcon() {
  // TODO(junmin): implement.
}

void NativeAppWindowCocoa::UpdateWindowTitle() {
  base::string16 title = app_window_->GetTitle();
  [window() setTitle:base::SysUTF16ToNSString(title)];
}

void NativeAppWindowCocoa::UpdateBadgeIcon() {
  // TODO(benwells): implement.
  NOTIMPLEMENTED();
}

void NativeAppWindowCocoa::UpdateShape(scoped_ptr<SkRegion> region) {
  NOTIMPLEMENTED();
}

void NativeAppWindowCocoa::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  // Draggable region is not supported for non-frameless window.
  if (has_frame_)
    return;

  draggable_regions_ = regions;
  UpdateDraggableRegionViews();
}

SkRegion* NativeAppWindowCocoa::GetDraggableRegion() {
  return NULL;
}

void NativeAppWindowCocoa::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser ||
      event.type == content::NativeWebKeyboardEvent::Char) {
    return;
  }
  [window() redispatchKeyEvent:event.os_event];
}

void NativeAppWindowCocoa::UpdateDraggableRegionViews() {
  if (has_frame_)
    return;

  // All ControlRegionViews should be added as children of the WebContentsView,
  // because WebContentsView will be removed and re-added when entering and
  // leaving fullscreen mode.
  NSView* webView = web_contents()->GetView()->GetNativeView();
  NSInteger webViewWidth = NSWidth([webView bounds]);
  NSInteger webViewHeight = NSHeight([webView bounds]);

  // Remove all ControlRegionViews that are added last time.
  // Note that [webView subviews] returns the view's mutable internal array and
  // it should be copied to avoid mutating the original array while enumerating
  // it.
  base::scoped_nsobject<NSArray> subviews([[webView subviews] copy]);
  for (NSView* subview in subviews.get())
    if ([subview isKindOfClass:[ControlRegionView class]])
      [subview removeFromSuperview];

  // Draggable regions is implemented by having the whole web view draggable
  // (mouseDownCanMoveWindow) and overlaying regions that are not draggable.
  std::vector<gfx::Rect> system_drag_exclude_areas =
      CalculateNonDraggableRegions(
          draggable_regions_, webViewWidth, webViewHeight);

  // Create and add a ControlRegionView for each region that needs to be
  // excluded from the dragging.
  for (std::vector<gfx::Rect>::const_iterator iter =
           system_drag_exclude_areas.begin();
       iter != system_drag_exclude_areas.end();
       ++iter) {
    base::scoped_nsobject<NSView> controlRegion(
        [[ControlRegionView alloc] initWithFrame:NSZeroRect]);
    [controlRegion setFrame:NSMakeRect(iter->x(),
                                       webViewHeight - iter->bottom(),
                                       iter->width(),
                                       iter->height())];
    [webView addSubview:controlRegion];
  }
}

void NativeAppWindowCocoa::FlashFrame(bool flash) {
  if (flash) {
    attention_request_id_ = [NSApp requestUserAttention:NSInformationalRequest];
  } else {
    [NSApp cancelUserAttentionRequest:attention_request_id_];
    attention_request_id_ = 0;
  }
}

bool NativeAppWindowCocoa::IsAlwaysOnTop() const {
  return [window() level] == AlwaysOnTopWindowLevel();
}

void NativeAppWindowCocoa::RenderViewCreated(content::RenderViewHost* rvh) {
  if (IsActive())
    web_contents()->GetView()->RestoreFocus();
}

bool NativeAppWindowCocoa::IsFrameless() const {
  return !has_frame_;
}

bool NativeAppWindowCocoa::HasFrameColor() const {
  // TODO(benwells): Implement this.
  return false;
}

SkColor NativeAppWindowCocoa::FrameColor() const {
  // TODO(benwells): Implement this.
  return SkColor();
}

gfx::Insets NativeAppWindowCocoa::GetFrameInsets() const {
  if (!has_frame_)
    return gfx::Insets();

  // Flip the coordinates based on the main screen.
  NSInteger screen_height =
      NSHeight([[[NSScreen screens] objectAtIndex:0] frame]);

  NSRect frame_nsrect = [window() frame];
  gfx::Rect frame_rect(NSRectToCGRect(frame_nsrect));
  frame_rect.set_y(screen_height - NSMaxY(frame_nsrect));

  NSRect content_nsrect = [window() contentRectForFrameRect:frame_nsrect];
  gfx::Rect content_rect(NSRectToCGRect(content_nsrect));
  content_rect.set_y(screen_height - NSMaxY(content_nsrect));

  return frame_rect.InsetsFrom(content_rect);
}

gfx::NativeView NativeAppWindowCocoa::GetHostView() const {
  NOTIMPLEMENTED();
  return NULL;
}

gfx::Point NativeAppWindowCocoa::GetDialogPosition(const gfx::Size& size) {
  NOTIMPLEMENTED();
  return gfx::Point();
}

gfx::Size NativeAppWindowCocoa::GetMaximumDialogSize() {
  NOTIMPLEMENTED();
  return gfx::Size();
}

void NativeAppWindowCocoa::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  NOTIMPLEMENTED();
}

void NativeAppWindowCocoa::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  NOTIMPLEMENTED();
}

void NativeAppWindowCocoa::WindowWillClose() {
  [window_controller_ setAppWindow:NULL];
  app_window_->OnNativeWindowChanged();
  app_window_->OnNativeClose();
}

void NativeAppWindowCocoa::WindowDidBecomeKey() {
  content::RenderWidgetHostView* rwhv =
      web_contents()->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetActive(true);
  app_window_->OnNativeWindowActivated();

  web_contents()->GetView()->RestoreFocus();
}

void NativeAppWindowCocoa::WindowDidResignKey() {
  // If our app is still active and we're still the key window, ignore this
  // message, since it just means that a menu extra (on the "system status bar")
  // was activated; we'll get another |-windowDidResignKey| if we ever really
  // lose key window status.
  if ([NSApp isActive] && ([NSApp keyWindow] == window()))
    return;

  web_contents()->GetView()->StoreFocus();

  content::RenderWidgetHostView* rwhv =
      web_contents()->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetActive(false);
}

void NativeAppWindowCocoa::WindowDidFinishResize() {
  // Update |is_maximized_| if needed:
  // - Exit maximized state if resized.
  // - Consider us maximized if resize places us back to maximized location.
  //   This happens when returning from fullscreen.
  NSRect frame = [window() frame];
  NSRect screen = [[window() screen] visibleFrame];
  if (!NSEqualSizes(frame.size, screen.size))
    is_maximized_ = false;
  else if (NSEqualPoints(frame.origin, screen.origin))
    is_maximized_ = true;

  // Update |is_fullscreen_| if needed.
  is_fullscreen_ = ([window() styleMask] & NSFullScreenWindowMask) != 0;
  // If not fullscreen but the window is constrained, disable the fullscreen UI
  // control.
  if (!is_fullscreen_ && !shows_fullscreen_controls_)
    SetFullScreenCollectionBehavior(window(), false);

  UpdateRestoredBounds();
}

void NativeAppWindowCocoa::WindowDidResize() {
  app_window_->OnNativeWindowChanged();
  UpdateDraggableRegionViews();
}

void NativeAppWindowCocoa::WindowDidMove() {
  UpdateRestoredBounds();
  app_window_->OnNativeWindowChanged();
}

void NativeAppWindowCocoa::WindowDidMiniaturize() {
  app_window_->OnNativeWindowChanged();
}

void NativeAppWindowCocoa::WindowDidDeminiaturize() {
  app_window_->OnNativeWindowChanged();
}

void NativeAppWindowCocoa::WindowWillZoom() {
  // See top of file NOTE: Maximize and Zoom.
  if (IsMaximized())
    Restore();
  else
    Maximize();
}

bool NativeAppWindowCocoa::HandledByExtensionCommand(NSEvent* event) {
  return extension_keybinding_registry_->ProcessKeyEvent(
      content::NativeWebKeyboardEvent(event));
}

void NativeAppWindowCocoa::ShowWithApp() {
  is_hidden_with_app_ = false;
  if (!is_hidden_)
    ShowInactive();
}

void NativeAppWindowCocoa::HideWithApp() {
  is_hidden_with_app_ = true;
  HideWithoutMarkingHidden();
}

void NativeAppWindowCocoa::UpdateShelfMenu() {
  // TODO(tmdiep): To be implemented for Mac.
  NOTIMPLEMENTED();
}

gfx::Size NativeAppWindowCocoa::GetContentMinimumSize() const {
  return size_constraints_.GetMinimumSize();
}

gfx::Size NativeAppWindowCocoa::GetContentMaximumSize() const {
  return size_constraints_.GetMaximumSize();
}

void NativeAppWindowCocoa::SetContentSizeConstraints(
    const gfx::Size& min_size, const gfx::Size& max_size) {
  // Update the size constraints.
  size_constraints_.set_minimum_size(min_size);
  size_constraints_.set_maximum_size(max_size);

  gfx::Size minimum_size = size_constraints_.GetMinimumSize();
  [window() setContentMinSize:NSMakeSize(minimum_size.width(),
                                         minimum_size.height())];

  gfx::Size maximum_size = size_constraints_.GetMaximumSize();
  const int kUnboundedSize = apps::SizeConstraints::kUnboundedSize;
  CGFloat max_width = maximum_size.width() == kUnboundedSize ?
      CGFLOAT_MAX : maximum_size.width();
  CGFloat max_height = maximum_size.height() == kUnboundedSize ?
      CGFLOAT_MAX : maximum_size.height();
  [window() setContentMaxSize:NSMakeSize(max_width, max_height)];

  // Update the window controls.
  shows_resize_controls_ =
      is_resizable_ && !size_constraints_.HasFixedSize();
  shows_fullscreen_controls_ =
      is_resizable_ && !size_constraints_.HasMaximumSize() && has_frame_;

  if (!is_fullscreen_) {
    [window() setStyleMask:GetWindowStyleMask()];

    // Set the window to participate in Lion Fullscreen mode. Setting this flag
    // has no effect on Snow Leopard or earlier. UI controls for fullscreen are
    // only shown for apps that have unbounded size.
    SetFullScreenCollectionBehavior(window(), shows_fullscreen_controls_);
  }

  if (has_frame_) {
    [window() setShowsResizeIndicator:shows_resize_controls_];
    [[window() standardWindowButton:NSWindowZoomButton]
        setEnabled:shows_fullscreen_controls_];
  }
}

void NativeAppWindowCocoa::SetAlwaysOnTop(bool always_on_top) {
  [window() setLevel:(always_on_top ? AlwaysOnTopWindowLevel() :
                                      NSNormalWindowLevel)];
}

NativeAppWindowCocoa::~NativeAppWindowCocoa() {
}

ShellNSWindow* NativeAppWindowCocoa::window() const {
  NSWindow* window = [window_controller_ window];
  CHECK(!window || [window isKindOfClass:[ShellNSWindow class]]);
  return static_cast<ShellNSWindow*>(window);
}

void NativeAppWindowCocoa::UpdateRestoredBounds() {
  if (IsRestored(*this))
    restored_bounds_ = [window() frame];
}

void NativeAppWindowCocoa::HideWithoutMarkingHidden() {
  [window() orderOut:window_controller_];
}
