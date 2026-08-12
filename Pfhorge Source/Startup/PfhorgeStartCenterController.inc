// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// STARTUP-1A: native Pfhorge Start Center.

#import <Cocoa/Cocoa.h>

static NSString *const PfhorgeShowStartCenterAtLaunchPreference =
    @"PfhorgeShowStartCenterAtLaunch";

static NSTextField *PfhorgeStartCenterLabel(NSString *text, NSFont *font, NSColor *color)
{
    NSTextField *label = [NSTextField labelWithString:text];
    label.font = font;
    label.textColor = color;
    label.selectable = NO;
    label.editable = NO;
    label.drawsBackground = NO;
    return label;
}

static NSButton *PfhorgeStartCenterButton(NSString *title, id target, SEL action)
{
    NSButton *button = [NSButton buttonWithTitle:title target:target action:action];
    button.bezelStyle = NSBezelStyleRounded;
    if (@available(macOS 11.0, *)) {
        button.controlSize = NSControlSizeLarge;
    } else {
        button.controlSize = NSControlSizeRegular;
    }
    button.translatesAutoresizingMaskIntoConstraints = NO;
    [button.heightAnchor constraintEqualToConstant:38.0].active = YES;
    return button;
}

static NSView *PfhorgeStartCenterSeparator(void)
{
    NSBox *separator = [[NSBox alloc] initWithFrame:NSZeroRect];
    separator.boxType = NSBoxSeparator;
    separator.translatesAutoresizingMaskIntoConstraints = NO;
    [separator.heightAnchor constraintEqualToConstant:1.0].active = YES;
    return separator;
}

@interface PfhorgeStartCenterController : NSWindowController
+ (instancetype)sharedController;
+ (BOOL)shouldShowAtLaunch;
- (void)showStartCenter;
@end

@interface PfhorgeStartCenterController ()
@property(nonatomic, strong) NSStackView *recentStack;
@property(nonatomic, strong) NSButton *showAtLaunchCheckbox;
@property(nonatomic, copy) NSArray<NSURL *> *recentURLs;
@end

@implementation PfhorgeStartCenterController

+ (instancetype)sharedController
{
    static PfhorgeStartCenterController *controller = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        controller = [[PfhorgeStartCenterController alloc] init];
    });
    return controller;
}

+ (BOOL)shouldShowAtLaunch
{
    [NSUserDefaults.standardUserDefaults registerDefaults:@{
        PfhorgeShowStartCenterAtLaunchPreference: @YES,
    }];

    return [NSUserDefaults.standardUserDefaults
        boolForKey:PfhorgeShowStartCenterAtLaunchPreference];
}

- (instancetype)init
{
    NSRect frame = NSMakeRect(0.0, 0.0, 760.0, 590.0);

    NSWindow *window = [[NSWindow alloc]
        initWithContentRect:frame
                  styleMask:NSWindowStyleMaskTitled |
                            NSWindowStyleMaskClosable |
                            NSWindowStyleMaskMiniaturizable
                    backing:NSBackingStoreBuffered
                      defer:NO];

    self = [super initWithWindow:window];
    if (self == nil) {
        return nil;
    }

    window.title = @"Pfhorge";
    window.releasedWhenClosed = NO;
    window.minSize = NSMakeSize(720.0, 540.0);

    NSView *content = [[NSView alloc] initWithFrame:frame];
    window.contentView = content;

    NSStackView *root = [[NSStackView alloc] initWithFrame:NSZeroRect];
    root.orientation = NSUserInterfaceLayoutOrientationVertical;
    root.alignment = NSLayoutAttributeLeading;
    root.spacing = 12.0;
    root.translatesAutoresizingMaskIntoConstraints = NO;
    [content addSubview:root];

    [NSLayoutConstraint activateConstraints:@[
        [root.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:28.0],
        [root.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-28.0],
        [root.topAnchor constraintEqualToAnchor:content.topAnchor constant:26.0],
        [root.bottomAnchor constraintLessThanOrEqualToAnchor:content.bottomAnchor constant:-22.0],
    ]];

    [root addArrangedSubview:PfhorgeStartCenterLabel(
        @"Pfhorge",
        [NSFont systemFontOfSize:30.0 weight:NSFontWeightBold],
        NSColor.labelColor)];

    [root addArrangedSubview:PfhorgeStartCenterLabel(
        @"Start Center — create, open, or continue working with Marathon content.",
        [NSFont systemFontOfSize:13.0 weight:NSFontWeightRegular],
        NSColor.secondaryLabelColor)];

    [root addArrangedSubview:PfhorgeStartCenterSeparator()];

    [root addArrangedSubview:PfhorgeStartCenterLabel(
        @"CREATE",
        [NSFont systemFontOfSize:11.0 weight:NSFontWeightSemibold],
        NSColor.secondaryLabelColor)];

    NSButton *newLevel = PfhorgeStartCenterButton(
        @"New Pfhorge Level", self, @selector(newLevel:));
    newLevel.toolTip = @"Create a new single Pfhorge level.";

    NSButton *newScenario = PfhorgeStartCenterButton(
        @"New Pfhorge Scenario", self, @selector(newScenario:));
    newScenario.toolTip = @"Create a new Pfhorge scenario / multi-level project.";

    NSStackView *createRow = [NSStackView stackViewWithViews:@[newLevel, newScenario]];
    createRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    createRow.distribution = NSStackViewDistributionFillEqually;
    createRow.spacing = 12.0;
    [root addArrangedSubview:createRow];
    [createRow.widthAnchor constraintEqualToAnchor:root.widthAnchor].active = YES;

    [root addArrangedSubview:PfhorgeStartCenterLabel(
        @"OPEN",
        [NSFont systemFontOfSize:11.0 weight:NSFontWeightSemibold],
        NSColor.secondaryLabelColor)];

    NSButton *openExisting = PfhorgeStartCenterButton(
        @"Open Existing…", self, @selector(openExisting:));
    openExisting.toolTip =
        @"Open a Pfhorge level/scenario or a supported Marathon map document.";
    [root addArrangedSubview:openExisting];
    [openExisting.widthAnchor constraintEqualToAnchor:root.widthAnchor].active = YES;

    [root addArrangedSubview:PfhorgeStartCenterLabel(
        @"RECENT",
        [NSFont systemFontOfSize:11.0 weight:NSFontWeightSemibold],
        NSColor.secondaryLabelColor)];

    self.recentStack = [[NSStackView alloc] initWithFrame:NSZeroRect];
    self.recentStack.orientation = NSUserInterfaceLayoutOrientationVertical;
    self.recentStack.alignment = NSLayoutAttributeLeading;
    self.recentStack.spacing = 4.0;
    [root addArrangedSubview:self.recentStack];
    [self.recentStack.widthAnchor constraintEqualToAnchor:root.widthAnchor].active = YES;

    [root addArrangedSubview:PfhorgeStartCenterLabel(
        @"CONTENT & RESOURCES",
        [NSFont systemFontOfSize:11.0 weight:NSFontWeightSemibold],
        NSColor.secondaryLabelColor)];

    NSButton *contentManager = PfhorgeStartCenterButton(
        @"Content Manager…", self, @selector(showContentManager:));
    contentManager.toolTip =
        @"Manage Marathon Shapes/collections, content profiles, and enhanced visual resources.";

    NSButton *visualSettings = PfhorgeStartCenterButton(
        @"Visual Mode Settings…", self, @selector(showVisualModeSettings:));

    NSButton *preferencesButton = PfhorgeStartCenterButton(
        @"Preferences…", self, @selector(showPreferences:));

    NSStackView *resourceRow = [NSStackView stackViewWithViews:@[
        contentManager, visualSettings, preferencesButton
    ]];
    resourceRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    resourceRow.distribution = NSStackViewDistributionFillEqually;
    resourceRow.spacing = 10.0;
    [root addArrangedSubview:resourceRow];
    [resourceRow.widthAnchor constraintEqualToAnchor:root.widthAnchor].active = YES;

    [root addArrangedSubview:PfhorgeStartCenterSeparator()];

    self.showAtLaunchCheckbox = [NSButton
        checkboxWithTitle:@"Show Start Center when Pfhorge launches"
                   target:self
                   action:@selector(showAtLaunchChanged:)];

    self.showAtLaunchCheckbox.state =
        [PfhorgeStartCenterController shouldShowAtLaunch]
        ? NSControlStateValueOn
        : NSControlStateValueOff;

    NSButton *closeButton = PfhorgeStartCenterButton(
        @"Close", self, @selector(closeStartCenter:));
    [closeButton.widthAnchor constraintEqualToConstant:100.0].active = YES;

    NSView *spacer = [[NSView alloc] initWithFrame:NSZeroRect];
    [spacer setContentHuggingPriority:NSLayoutPriorityDefaultLow
                       forOrientation:NSLayoutConstraintOrientationHorizontal];

    NSStackView *bottomRow = [NSStackView stackViewWithViews:@[
        self.showAtLaunchCheckbox, spacer, closeButton
    ]];
    bottomRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    bottomRow.alignment = NSLayoutAttributeCenterY;
    bottomRow.spacing = 10.0;
    [root addArrangedSubview:bottomRow];
    [bottomRow.widthAnchor constraintEqualToAnchor:root.widthAnchor].active = YES;

    return self;
}

- (void)showStartCenter
{
    [self reloadRecentDocuments];

    self.showAtLaunchCheckbox.state =
        [PfhorgeStartCenterController shouldShowAtLaunch]
        ? NSControlStateValueOn
        : NSControlStateValueOff;

    if (!self.window.isVisible) {
        [self.window center];
    }

    [self showWindow:nil];
    [self.window makeKeyAndOrderFront:nil];
}

- (void)reloadRecentDocuments
{
    for (NSView *view in self.recentStack.arrangedSubviews.copy) {
        [self.recentStack removeArrangedSubview:view];
        [view removeFromSuperview];
    }

    NSArray<NSURL *> *allRecent =
        [NSDocumentController sharedDocumentController].recentDocumentURLs;

    NSMutableArray<NSURL *> *usable = [NSMutableArray array];
    for (NSURL *url in allRecent) {
        if (usable.count >= 5U) {
            break;
        }
        if (url != nil) {
            [usable addObject:url];
        }
    }

    self.recentURLs = usable.copy;

    if (self.recentURLs.count == 0U) {
        [self.recentStack addArrangedSubview:PfhorgeStartCenterLabel(
            @"No recent documents yet.",
            [NSFont systemFontOfSize:12.0 weight:NSFontWeightRegular],
            NSColor.tertiaryLabelColor)];
        return;
    }

    [self.recentURLs enumerateObjectsUsingBlock:
        ^(NSURL *url, NSUInteger index, BOOL *stop) {
            (void)stop;

            NSButton *button = [NSButton
                buttonWithTitle:(url.lastPathComponent ?: url.path)
                         target:self
                         action:@selector(openRecent:)];

            button.bezelStyle = NSBezelStyleInline;
            button.alignment = NSTextAlignmentLeft;
            button.tag = (NSInteger)index;
            button.toolTip = url.path;
            button.translatesAutoresizingMaskIntoConstraints = NO;

            [button.heightAnchor constraintEqualToConstant:26.0].active = YES;
            [self.recentStack addArrangedSubview:button];
            [button.widthAnchor constraintEqualToAnchor:self.recentStack.widthAnchor].active = YES;
        }];
}

- (void)closeBeforeDocumentAction
{
    [self.window orderOut:nil];
}

- (IBAction)newLevel:(id)sender
{
    (void)sender;
    [self closeBeforeDocumentAction];

    id delegate = NSApp.delegate;
    if ([delegate respondsToSelector:@selector(newPhorgeLevel:)]) {
        [NSApp sendAction:@selector(newPhorgeLevel:) to:delegate from:self];
    } else {
        NSBeep();
    }
}

- (IBAction)newScenario:(id)sender
{
    (void)sender;
    [self closeBeforeDocumentAction];

    id delegate = NSApp.delegate;
    if ([delegate respondsToSelector:@selector(newPhorgeScenario:)]) {
        [NSApp sendAction:@selector(newPhorgeScenario:) to:delegate from:self];
    } else {
        NSBeep();
    }
}

- (IBAction)openExisting:(id)sender
{
    (void)sender;
    [self closeBeforeDocumentAction];
    [[NSDocumentController sharedDocumentController] openDocument:self];
}

- (IBAction)openRecent:(id)sender
{
    NSButton *recentButton = (NSButton *)sender;
    NSInteger index = recentButton.tag;
    if (index < 0 || (NSUInteger)index >= self.recentURLs.count) {
        NSBeep();
        return;
    }

    NSURL *url = self.recentURLs[(NSUInteger)index];
    [self closeBeforeDocumentAction];

    [[NSDocumentController sharedDocumentController]
        openDocumentWithContentsOfURL:url
                             display:YES
                   completionHandler:^(NSDocument *document,
                                       BOOL documentWasAlreadyOpen,
                                       NSError *error) {
        (void)document;
        (void)documentWasAlreadyOpen;
        if (error != nil) {
            [NSApp presentError:error];
        }
    }];
}

- (IBAction)showContentManager:(id)sender
{
    (void)sender;
    [self.window orderOut:nil];

    if (![NSApp sendAction:@selector(showPfhorgeContentManager:)
                        to:NSApp.delegate
                      from:self]) {
        NSBeep();
    }
}

- (IBAction)showVisualModeSettings:(id)sender
{
    (void)sender;
    [self.window orderOut:nil];

    if (![NSApp sendAction:@selector(showPfhorgeVisualModeSettings:)
                        to:NSApp.delegate
                      from:self]) {
        NSBeep();
    }
}

- (IBAction)showPreferences:(id)sender
{
    (void)sender;
    [self.window orderOut:nil];

    if (![NSApp sendAction:@selector(thePrefs:)
                        to:NSApp.delegate
                      from:self]) {
        NSBeep();
    }
}

- (IBAction)showAtLaunchChanged:(id)sender
{
    (void)sender;
    BOOL enabled = self.showAtLaunchCheckbox.state == NSControlStateValueOn;
    [NSUserDefaults.standardUserDefaults
        setBool:enabled
         forKey:PfhorgeShowStartCenterAtLaunchPreference];
}

- (IBAction)closeStartCenter:(id)sender
{
    (void)sender;
    [self.window orderOut:nil];
}

@end

@interface LEDelegate (PfhorgeStartupExperienceActions)
- (IBAction)showPfhorgeAbout:(id)sender;
- (IBAction)showPfhorgeStartCenter:(id)sender;
@end

@implementation LEDelegate (PfhorgeStartupExperienceActions)

- (IBAction)showPfhorgeAbout:(id)sender
{
    (void)sender;
    [[PfhorgeSplashController sharedController] showAboutSplash];
}

- (IBAction)showPfhorgeStartCenter:(id)sender
{
    (void)sender;
    [[PfhorgeStartCenterController sharedController] showStartCenter];
}

@end

static void PfhorgeInstallStartupExperience(LEDelegate *delegate)
{
    [NSUserDefaults.standardUserDefaults registerDefaults:@{
        PfhorgeShowStartCenterAtLaunchPreference: @YES,
    }];

    NSMenu *mainMenu = NSApp.mainMenu;

    // Rewire the existing About item without rewriting MainMenu.xib.
    NSMenuItem *applicationMenuItem = mainMenu.itemArray.firstObject;
    NSMenu *applicationMenu = applicationMenuItem.submenu;

    for (NSMenuItem *item in applicationMenu.itemArray) {
        if ([item.title hasPrefix:@"About Pfhorge"]) {
            item.target = delegate;
            item.action = @selector(showPfhorgeAbout:);
            break;
        }
    }

    // Make the closable Start Center available again during the same session.
    NSMenu *fileMenu = nil;
    for (NSMenuItem *item in mainMenu.itemArray) {
        if ([item.title isEqualToString:@"File"]) {
            fileMenu = item.submenu;
            break;
        }
    }

    if (fileMenu == nil) {
        return;
    }

    for (NSMenuItem *item in fileMenu.itemArray) {
        if (item.action == @selector(showPfhorgeStartCenter:)) {
            return;
        }
    }

    NSMenuItem *startCenterItem = [[NSMenuItem alloc]
        initWithTitle:@"Start Center…"
               action:@selector(showPfhorgeStartCenter:)
        keyEquivalent:@""];

    startCenterItem.target = delegate;
    [fileMenu insertItem:startCenterItem atIndex:0];
    [fileMenu insertItem:NSMenuItem.separatorItem atIndex:1];
}
