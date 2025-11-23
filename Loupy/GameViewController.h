//
//  GameViewController.h
//  Loupy
//
//  Created by Rémy on 22/11/2025.
//

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

@interface RMDLGameApplicationLoupy : NSObject <NSApplicationDelegate, NSWindowDelegate, MTKViewDelegate>

@property (nonatomic, strong, readonly) MTKView *mtkView;

- (void)applicationDidFinishLaunching:(NSNotification *)notification;
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender;

- (void)keyDown:(NSEvent *)event;
- (void)keyUp:(NSEvent *)event;
- (void)mouseDown:(NSEvent *)event;
- (void)mouseUp:(NSEvent *)event;
- (void)mouseDragged:(NSEvent *)event;

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size;
- (void)drawInMTKView:(MTKView *)view;

- (void)moveCameraX:(float)x Y:(float)y Z:(float)z;
- (void)rotateCameraYaw:(float)yaw Pitch:(float)pitch;

@end

