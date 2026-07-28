// SPDX-License-Identifier: GPL-3.0-or-later

#import <Cocoa/Cocoa.h>

@interface PfhorgeSplashView : NSView

@property(nonatomic, strong) NSImage *artwork;

@end


@implementation PfhorgeSplashView

- (void)drawRect:(NSRect)dirtyRect
{
    [super drawRect:dirtyRect];

    NSRect bounds = self.bounds;

    NSGradient *background =
        [[NSGradient alloc]
            initWithStartingColor:
                [NSColor colorWithCalibratedRed:0.035
                                          green:0.045
                                           blue:0.075
                                          alpha:1.0]
                      endingColor:
                [NSColor colorWithCalibratedRed:0.16
                                          green:0.07
                                           blue:0.20
                                          alpha:1.0]];

    [background drawInRect:bounds angle:90.0];

    if (self.artwork != nil) {
        [self.artwork
            drawInRect:bounds
              fromRect:NSZeroRect
             operation:NSCompositingOperationSourceOver
              fraction:1.0
        respectFlipped:YES
                 hints:nil];

        return;
    }

    NSDictionary *titleAttributes = @{
        NSFontAttributeName:
            [NSFont systemFontOfSize:44.0
                              weight:NSFontWeightBlack],

        NSForegroundColorAttributeName:
            NSColor.whiteColor,

        NSKernAttributeName:
            @2.0
    };

    NSDictionary *subtitleAttributes = @{
        NSFontAttributeName:
            [NSFont systemFontOfSize:15.0
                              weight:NSFontWeightMedium],

        NSForegroundColorAttributeName:
            [NSColor colorWithWhite:0.82
                              alpha:1.0]
    };

    NSString *title = @"PFHORGE";
    NSString *subtitle =
        @"MARATHON MAP EDITOR — REVIVAL BUILD";

    NSSize titleSize =
        [title sizeWithAttributes:titleAttributes];

    NSSize subtitleSize =
        [subtitle sizeWithAttributes:subtitleAttributes];

    [title
        drawAtPoint:
            NSMakePoint(
                NSMidX(bounds) - (titleSize.width * 0.5),
                NSMidY(bounds) + 4.0)
      withAttributes:titleAttributes];

    [subtitle
        drawAtPoint:
            NSMakePoint(
                NSMidX(bounds) - (subtitleSize.width * 0.5),
                NSMidY(bounds) - 34.0)
      withAttributes:subtitleAttributes];
}

@end


@interface PfhorgeSplashController : NSWindowController

+ (instancetype)sharedController;

- (void)showSplash;
- (void)dismissSplash;

@end


@interface PfhorgeSplashController ()

@property(nonatomic) NSTimeInterval revivalShownAt;
@property(nonatomic) BOOL revivalDismissScheduled;

@end


@implementation PfhorgeSplashController

+ (instancetype)sharedController
{
    static PfhorgeSplashController *controller = nil;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        controller =
            [[PfhorgeSplashController alloc] init];
    });

    return controller;
}

- (instancetype)init
{
    NSRect frame =
        NSMakeRect(0.0, 0.0, 720.0, 420.0);

    NSWindow *window =
        [[NSWindow alloc]
            initWithContentRect:frame
                      styleMask:NSWindowStyleMaskBorderless
                        backing:NSBackingStoreBuffered
                          defer:NO];

    self = [super initWithWindow:window];

    if (self == nil) {
        return nil;
    }

    window.opaque = YES;
    window.backgroundColor = NSColor.blackColor;
    window.hasShadow = YES;
    window.level = NSFloatingWindowLevel;
    window.releasedWhenClosed = NO;
    window.ignoresMouseEvents = YES;

    PfhorgeSplashView *view =
        [[PfhorgeSplashView alloc]
            initWithFrame:frame];

    view.autoresizingMask =
        NSViewWidthSizable |
        NSViewHeightSizable;

    view.artwork =
        [NSImage imageNamed:@"PfhorgeSplashArtwork"];

    window.contentView = view;

    return self;
}

- (void)showSplash
{
    self.revivalShownAt =
        [NSDate timeIntervalSinceReferenceDate];

    self.revivalDismissScheduled = NO;

    self.window.alphaValue = 1.0;

    [self.window center];
    [self showWindow:nil];
    [self.window orderFrontRegardless];
    [self.window displayIfNeeded];
}

- (void)dismissSplash
{
    /*
     * Prevent two callers from scheduling overlapping fade-outs.
     */
    if (self.revivalDismissScheduled) {
        return;
    }

    self.revivalDismissScheduled = YES;

    static const NSTimeInterval minimumVisibleDuration =
        2.75;

    static const NSTimeInterval fadeDuration =
        0.35;

    NSTimeInterval elapsed =
        [NSDate timeIntervalSinceReferenceDate] -
        self.revivalShownAt;

    NSTimeInterval remaining =
        MAX(
            0.0,
            minimumVisibleDuration - elapsed);

    dispatch_after(
        dispatch_time(
            DISPATCH_TIME_NOW,
            (int64_t)(remaining * NSEC_PER_SEC)),
        dispatch_get_main_queue(),
        ^{
            [NSAnimationContext
                runAnimationGroup:
                ^(NSAnimationContext *context) {
                    context.duration = fadeDuration;

                    self.window.animator.alphaValue =
                        0.0;
                }
                completionHandler:^{
                    [self.window orderOut:nil];

                    self.window.alphaValue = 1.0;
                    self.revivalDismissScheduled = NO;
                }];
        });
}

@end
