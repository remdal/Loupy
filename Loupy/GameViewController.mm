//
//  GameViewController.m
//  Loupy
//
//  Created by Rémy on 22/11/2025.
//

#import "GameViewController.h"
#import "RMDLGameRendererLoupy.hpp"

@implementation RMDLGameApplicationLoupy
{
    std::unique_ptr< GameCoordinatorLoupy > _pGameCoordinator;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return (YES);
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [self createWindow];
    [self createView];
    [self evaluateCommandLine];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    _pGameCoordinator.reset();
}

- (void)evaluateCommandLine
{
    NSArray<NSString *>* args = [[NSProcessInfo processInfo] arguments];
    BOOL exitAfterOneFrame = [args containsObject:@"--auto-close"];
    if (exitAfterOneFrame)
    {
        NSLog(@"Automatically terminating in 8 seconds...");
        [[NSApplication sharedApplication] performSelector:@selector(terminate:) withObject:self afterDelay:8];
    }
}

- (void)createWindow
{
    NSRect contentRect = NSMakeRect(0, 0, 1280, 720);
    _window.styleMask = NSWindowStyleMaskClosable | NSWindowStyleMaskTitled | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    _window.backingType = NSBackingStoreBuffered;
    NSScreen* screen = _window.screen;
    contentRect.origin.x = (screen.frame.size.width / 2) - (contentRect.size.width / 2);
    contentRect.origin.y = (screen.frame.size.height / 2) - (contentRect.size.height / 2);
    _window.releasedWhenClosed = NO;
    _window.minSize = NSMakeSize(640, 360);
//    _window. = self;
    NSString* title = [NSString stringWithFormat:@"Black Hole Metal4 x C++ (%@ @ %ldHz, EDR max: %.2f)", screen.localizedName, (long)screen.maximumFramesPerSecond, screen.maximumExtendedDynamicRangeColorComponentValue];
    _window.title = title;
    [_window makeKeyAndOrderFront:nil];
}

- (void)createView
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    _mtkView = [[MTKView alloc] initWithFrame:_window.contentLayoutRect device:device];
    _mtkView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _mtkView.colorPixelFormat = MTLPixelFormatRGBA16Float;
    _mtkView.depthStencilPixelFormat = MTLPixelFormatInvalid;
    _mtkView.framebufferOnly = YES;
    _mtkView.paused = NO;
    _mtkView.delegate = self;
    _mtkView.enableSetNeedsDisplay = NO;
    
    NSScreen *screen = _window.screen ?: [NSScreen mainScreen];
    CGFloat scale = screen.backingScaleFactor ?: 1.0;
    CGSize sizePts = _mtkView.bounds.size;
    _mtkView.drawableSize = CGSizeMake(sizePts.width * scale, sizePts.height * scale);
    _window.contentView = _mtkView;

    MTL::Device* m_pDevice = (__bridge MTL::Device *)device;
    MTL::PixelFormat pixelFormat = MTL::PixelFormatRGBA16Float;
    NS::UInteger w = (NS::UInteger)_mtkView.drawableSize.width;
    NS::UInteger h = (NS::UInteger)_mtkView.drawableSize.height;
    
    _pGameCoordinator = std::make_unique< GameCoordinatorLoupy >(m_pDevice, pixelFormat, w, h);
}

- (void)moveCameraX:(float)x Y:(float)y Z:(float)z
{
    //_pGameCoordinator->moveCamera( simd::float3 {x, y, z} );
}

- (void)rotateCameraYaw:(float)yaw Pitch:(float)pitch
{
    //_pGameCoordinator->rotateCamera(yaw, pitch);
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    _pGameCoordinator->draw((__bridge MTK::View *)view);
}

@end
