// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// STARTUP-1A: persistent launch/about splash.
// The background art remains an external raster asset; typography, tinting,
// vignette, layout, credits, and click behavior are native AppKit drawing.

#import <Cocoa/Cocoa.h>

typedef NS_ENUM(NSUInteger, PfhorgeSplashPresentationMode) {
    PfhorgeSplashPresentationModeLaunch = 0,
    PfhorgeSplashPresentationModeAbout = 1,
};

static NSRect PfhorgeSplashAspectFillSourceRect(NSImage *image, NSRect destination)
{
    NSSize imageSize = image.size;
    if (imageSize.width <= 0.0 || imageSize.height <= 0.0 ||
        destination.size.width <= 0.0 || destination.size.height <= 0.0) {
        return NSZeroRect;
    }

    CGFloat sourceAspect = imageSize.width / imageSize.height;
    CGFloat destinationAspect = destination.size.width / destination.size.height;
    NSRect source = NSMakeRect(0.0, 0.0, imageSize.width, imageSize.height);

    if (sourceAspect > destinationAspect) {
        CGFloat croppedWidth = imageSize.height * destinationAspect;
        source.origin.x = (imageSize.width - croppedWidth) * 0.5;
        source.size.width = croppedWidth;
    } else {
        CGFloat croppedHeight = imageSize.width / destinationAspect;
        source.origin.y = (imageSize.height - croppedHeight) * 0.5;
        source.size.height = croppedHeight;
    }

    return source;
}

static void PfhorgeSplashAppendCreditLine(
    NSMutableAttributedString *credits,
    NSString *label,
    NSString *value,
    NSFont *labelFont,
    NSFont *valueFont,
    NSColor *color,
    NSMutableParagraphStyle *paragraphStyle)
{
    if (credits.length > 0U) {
        [credits appendAttributedString:
            [[NSAttributedString alloc] initWithString:@"\n"
                                            attributes:@{
            NSFontAttributeName: valueFont,
            NSForegroundColorAttributeName: color,
            NSParagraphStyleAttributeName: paragraphStyle,
        }]];
    }

    [credits appendAttributedString:
        [[NSAttributedString alloc] initWithString:label
                                        attributes:@{
        NSFontAttributeName: labelFont,
        NSForegroundColorAttributeName: color,
        NSParagraphStyleAttributeName: paragraphStyle,
    }]];

    [credits appendAttributedString:
        [[NSAttributedString alloc] initWithString:value
                                        attributes:@{
        NSFontAttributeName: valueFont,
        NSForegroundColorAttributeName: color,
        NSParagraphStyleAttributeName: paragraphStyle,
    }]];
}

static NSAttributedString *PfhorgeSplashCredits(void)
{
    // STARTUP-1A.2:
    // Treat the splash like a classic Macintosh software About box: compact
    // development roster first, then smaller production/legal acknowledgements.
    NSFont *headingFont =
        [NSFont systemFontOfSize:9.75 weight:NSFontWeightBold];
    NSFont *nameFont =
        [NSFont systemFontOfSize:9.50 weight:NSFontWeightSemibold];
    NSFont *roleFont =
        [NSFont systemFontOfSize:9.50 weight:NSFontWeightRegular];
    NSFont *smallLabelFont =
        [NSFont systemFontOfSize:8.80 weight:NSFontWeightSemibold];
    NSFont *smallValueFont =
        [NSFont systemFontOfSize:8.80 weight:NSFontWeightRegular];

    NSColor *color =
        [NSColor colorWithCalibratedWhite:0.97 alpha:1.0];
    NSColor *secondaryColor =
        [NSColor colorWithCalibratedWhite:0.90 alpha:0.94];

    NSMutableParagraphStyle *paragraphStyle =
        [[NSMutableParagraphStyle alloc] init];
    paragraphStyle.lineSpacing = 0.5;
    paragraphStyle.paragraphSpacing = 0.5;
    paragraphStyle.lineBreakMode = NSLineBreakByWordWrapping;

    NSMutableParagraphStyle *compactStyle =
        [[NSMutableParagraphStyle alloc] init];
    compactStyle.lineSpacing = 0.0;
    compactStyle.paragraphSpacing = 0.0;
    compactStyle.lineBreakMode = NSLineBreakByWordWrapping;

    NSMutableAttributedString *credits =
        [[NSMutableAttributedString alloc] init];

    [credits appendAttributedString:
        [[NSAttributedString alloc]
            initWithString:@"PFHORGE DEVELOPMENT\n"
                attributes:@{
                    NSFontAttributeName: headingFont,
                    NSForegroundColorAttributeName: color,
                    NSParagraphStyleAttributeName: paragraphStyle,
                    NSKernAttributeName: @(0.45),
                }]];

    PfhorgeSplashAppendCreditLine(
        credits, @"Joshua D. Orr — ",
        @"Original creator & lead developer",
        nameFont, roleFont, color, paragraphStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"C.W. “MaddTheSane” Betts — ",
        @"Maintainer & modernization",
        nameFont, roleFont, color, paragraphStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"", @"github.com/MaddTheSane/Pfhorge",
        nameFont, smallValueFont, secondaryColor, compactStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"Adam Vadala-Roth — ",
        @"Revival lead & current maintainer",
        nameFont, roleFont, color, paragraphStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"", @"github.com/adamjvr/pfhorge-revival",
        nameFont, smallValueFont, secondaryColor, compactStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"Loren Petrich — ",
        @"Map Viewer / Visual Mode code",
        nameFont, roleFont, color, compactStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"Nathan Day — ",
        @"Internal scripting / AppleScript infrastructure",
        nameFont, roleFont, color, compactStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"Sean Shaw “Cathunter” — ",
        @"Pfhorge manual & documentation",
        nameFont, roleFont, color, compactStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"Nick Rolfe — ",
        @"Object-angle logic",
        nameFont, roleFont, color, compactStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"Raul Bonilla — ",
        @"Tool-palette artwork / icons",
        nameFont, roleFont, color, compactStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"Tom Hackett — ",
        @"Original Pfhorge application icon",
        nameFont, roleFont, color, compactStyle);

    [credits appendAttributedString:
        [[NSAttributedString alloc]
            initWithString:@"\n"
                attributes:@{
                    NSFontAttributeName: smallValueFont,
                    NSForegroundColorAttributeName: secondaryColor,
                    NSParagraphStyleAttributeName: compactStyle,
                }]];

    PfhorgeSplashAppendCreditLine(
        credits, @"Background Artwork: ",
        @"Craig Mullins — Marathon 2 concept artwork",
        smallLabelFont, smallValueFont, secondaryColor, compactStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"License: ",
        @"GNU General Public License v3",
        smallLabelFont, smallValueFont, secondaryColor, compactStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"Acknowledgements: ",
        @"Bungie, Marathon, and the Aleph One project.",
        smallLabelFont, smallValueFont, secondaryColor, compactStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"Project Notice: ",
        @"Independent open-source project; not affiliated with or endorsed by Bungie.",
        smallLabelFont, smallValueFont, secondaryColor, compactStyle);

    PfhorgeSplashAppendCreditLine(
        credits, @"Open Source: ",
        @"bungie.net/7/en/Legal/OpenSource  •  alephone.lhowon.org",
        smallLabelFont, smallValueFont, secondaryColor, compactStyle);

    return credits;
}

@interface PfhorgeSplashWindow : NSWindow
@end

@implementation PfhorgeSplashWindow
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return NO; }
@end

@interface PfhorgeSplashView : NSView
@property(nonatomic, strong) NSImage *artwork;
@property(nonatomic) PfhorgeSplashPresentationMode presentationMode;
@property(nonatomic, copy) void (^clickHandler)(void);
@end

@implementation PfhorgeSplashView

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    (void)event;
    return YES;
}

- (void)mouseDown:(NSEvent *)event
{
    (void)event;
    if (self.clickHandler != nil) {
        self.clickHandler();
    }
}

- (void)drawRect:(NSRect)dirtyRect
{
    [super drawRect:dirtyRect];
    (void)dirtyRect;

    NSRect bounds = self.bounds;
    [NSColor.blackColor setFill];
    NSRectFill(bounds);

    if (self.artwork != nil) {
        NSRect source = PfhorgeSplashAspectFillSourceRect(self.artwork, bounds);
        [self.artwork drawInRect:bounds
                        fromRect:source
                       operation:NSCompositingOperationSourceOver
                        fraction:1.0
                  respectFlipped:YES
                           hints:@{NSImageHintInterpolation: @(NSImageInterpolationHigh)}];
    } else {
        NSGradient *fallback = [[NSGradient alloc]
            initWithStartingColor:[NSColor colorWithCalibratedRed:0.035 green:0.045 blue:0.050 alpha:1.0]
                      endingColor:[NSColor colorWithCalibratedRed:0.090 green:0.070 blue:0.045 alpha:1.0]];
        [fallback drawInRect:bounds angle:90.0];
    }

    // Live color/tone treatment matching the approved mockup.
    [[NSColor colorWithCalibratedRed:0.020 green:0.030 blue:0.015 alpha:0.19] setFill];
    NSRectFillUsingOperation(bounds, NSCompositingOperationSourceOver);

    NSGradient *vignette = [[NSGradient alloc]
        initWithColorsAndLocations:
            [NSColor colorWithCalibratedWhite:0.0 alpha:0.0], 0.0,
            [NSColor colorWithCalibratedWhite:0.0 alpha:0.78], 1.0,
            nil];

    CGFloat outerRadius = hypot(bounds.size.width, bounds.size.height) * 0.64;
    NSPoint vignetteCenter = NSMakePoint(NSMidX(bounds), NSMidY(bounds) + 14.0);
    [vignette drawFromCenter:vignetteCenter
                      radius:70.0
                    toCenter:vignetteCenter
                      radius:outerRadius
                     options:0];

    NSGradient *bottomShade = [[NSGradient alloc]
        initWithStartingColor:[NSColor colorWithCalibratedWhite:0.0 alpha:0.68]
                  endingColor:[NSColor colorWithCalibratedWhite:0.0 alpha:0.0]];
    [bottomShade drawInRect:NSMakeRect(0.0, 0.0, bounds.size.width, bounds.size.height * 0.46)
                      angle:90.0];

    // Bold flat white title in a translucent-tint box.
    //
    // STARTUP-1A.1: fit the box to the actual rendered wordmark instead of
    // using a broad fixed width. This keeps the approved left/right padding
    // visually balanced even if the font metrics change slightly.
    NSDictionary *titleAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:64.0 weight:NSFontWeightHeavy],
        NSForegroundColorAttributeName: NSColor.whiteColor,
        NSKernAttributeName: @(-1.25),
    };

    NSString *title = @"Pfhorge";
    NSSize titleSize = [title sizeWithAttributes:titleAttributes];

    const CGFloat titleLeftPadding = 20.0;
    const CGFloat titleRightPadding = 26.0;
    const CGFloat titleBoxWidth = MIN(
        titleSize.width + titleLeftPadding + titleRightPadding,
        bounds.size.width - 68.0);

    NSRect titleBox = NSMakeRect(
        34.0,
        bounds.size.height - 34.0 - 112.0,
        titleBoxWidth,
        112.0);

    NSBezierPath *titlePanel = [NSBezierPath bezierPathWithRect:titleBox];
    [[NSColor colorWithCalibratedWhite:0.03 alpha:0.36] setFill];
    [titlePanel fill];

    [[NSColor colorWithCalibratedWhite:1.0 alpha:0.98] setStroke];
    titlePanel.lineWidth = 3.0;
    [titlePanel stroke];

    NSPoint titlePoint = NSMakePoint(
        titleBox.origin.x + titleLeftPadding,
        NSMidY(titleBox) - (titleSize.height * 0.5) - 1.0);

    [title drawAtPoint:titlePoint withAttributes:titleAttributes];

    // STARTUP-1A.2: the classic-software contributor roster needs a
    // taller block, while staying entirely on the right half of the artwork.
    CGFloat creditWidth = MIN(520.0, bounds.size.width * 0.54);
    NSRect creditRect = NSMakeRect(
        bounds.size.width - creditWidth - 28.0,
        28.0,
        creditWidth,
        310.0);

    [PfhorgeSplashCredits() drawWithRect:creditRect
                                 options:NSStringDrawingUsesLineFragmentOrigin |
                                         NSStringDrawingUsesFontLeading];

    NSDictionary *hintAttributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:11.0 weight:NSFontWeightMedium],
        NSForegroundColorAttributeName:
            [NSColor colorWithCalibratedWhite:0.88 alpha:0.82],
    };

    NSString *hint =
        self.presentationMode == PfhorgeSplashPresentationModeAbout
        ? @"Click anywhere to close"
        : @"Click anywhere to continue";

    [hint drawAtPoint:NSMakePoint(34.0, 24.0) withAttributes:hintAttributes];
}

@end

@interface PfhorgeSplashController : NSWindowController
+ (instancetype)sharedController;
- (void)showSplash;
- (void)showLaunchSplashWithCompletion:(void (^)(void))completion;
- (void)showAboutSplash;
- (void)dismissSplash;
@end

@interface PfhorgeSplashController ()
@property(nonatomic) BOOL revivalDismissScheduled;
@property(nonatomic, copy) void (^revivalDismissHandler)(void);
@property(nonatomic, strong) PfhorgeSplashView *revivalSplashView;
@end

@implementation PfhorgeSplashController

+ (instancetype)sharedController
{
    static PfhorgeSplashController *controller = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        controller = [[PfhorgeSplashController alloc] init];
    });
    return controller;
}

- (instancetype)init
{
    NSRect frame = NSMakeRect(0.0, 0.0, 960.0, 540.0);

    PfhorgeSplashWindow *window = [[PfhorgeSplashWindow alloc]
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
    window.ignoresMouseEvents = NO;
    window.movableByWindowBackground = NO;

    PfhorgeSplashView *view = [[PfhorgeSplashView alloc] initWithFrame:frame];
    view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    view.artwork = [NSImage imageNamed:@"PfhorgeSplashMullins"];

    __weak PfhorgeSplashController *weakSelf = self;
    view.clickHandler = ^{
        [weakSelf dismissSplash];
    };

    self.revivalSplashView = view;
    window.contentView = view;

    return self;
}

- (void)showSplash
{
    [self showLaunchSplashWithCompletion:nil];
}

- (void)showLaunchSplashWithCompletion:(void (^)(void))completion
{
    if (self.window.isVisible &&
        self.revivalSplashView.presentationMode == PfhorgeSplashPresentationModeLaunch) {
        return;
    }

    self.revivalDismissHandler = completion;
    self.revivalDismissScheduled = NO;
    self.revivalSplashView.presentationMode = PfhorgeSplashPresentationModeLaunch;
    [self.revivalSplashView setNeedsDisplay:YES];

    self.window.alphaValue = 1.0;
    [self.window center];
    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];
    [self.window displayIfNeeded];
}

- (void)showAboutSplash
{
    if (self.window.isVisible &&
        self.revivalSplashView.presentationMode == PfhorgeSplashPresentationModeLaunch) {
        return;
    }

    self.revivalDismissHandler = nil;
    self.revivalDismissScheduled = NO;
    self.revivalSplashView.presentationMode = PfhorgeSplashPresentationModeAbout;
    [self.revivalSplashView setNeedsDisplay:YES];

    self.window.alphaValue = 1.0;
    [self.window center];
    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];
    [self.window displayIfNeeded];
}

- (void)dismissSplash
{
    if (!self.window.isVisible || self.revivalDismissScheduled) {
        return;
    }

    self.revivalDismissScheduled = YES;
    void (^dismissHandler)(void) = [self.revivalDismissHandler copy];
    self.revivalDismissHandler = nil;

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 0.22;
        self.window.animator.alphaValue = 0.0;
    } completionHandler:^{
        [self.window orderOut:nil];
        self.window.alphaValue = 1.0;
        self.revivalDismissScheduled = NO;

        if (dismissHandler != nil) {
            dismissHandler();
        }
    }];
}

@end
