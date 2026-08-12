// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// PREFS-1A
// Unified, modern AppKit preferences surface for Pfhorge.
//
// The historical PhPrefsController remains alive so its default-registration
// behavior and legacy compatibility are preserved. User-facing preference
// entry points are routed through this controller instead.
//
// The UI deliberately uses native macOS toolbar panes rather than replacing
// application chrome with a custom theme. Editor color themes only affect
// Pfhorge's map/drafting colors.

#import <AppKit/AppKit.h>
#import <objc/message.h>

static NSString *const PfhorgePrefsPaneGeneral = @"general";
static NSString *const PfhorgePrefsPaneAppearance = @"appearance";
static NSString *const PfhorgePrefsPaneObjects = @"objects";
static NSString *const PfhorgePrefsPaneVisualMode = @"visual-mode";

static NSString *const PfhorgePrefsLastPanePreference =
    @"PfhorgeUnifiedPreferencesLastPane";
static NSString *const PfhorgePrefsUserThemesPreference =
    @"PfhorgeEditorUserThemes";
static NSString *const PfhorgePrefsSelectedThemePreference =
    @"PfhorgeEditorSelectedTheme";
static NSString *const PfhorgePrefsModifiedThemeName =
    @"Custom (Modified)";


static NSColor *PfhorgePrefsRGB(CGFloat red, CGFloat green, CGFloat blue)
{
    return [NSColor colorWithCalibratedRed:red
                                     green:green
                                      blue:blue
                                     alpha:1.0];
}


static NSTextField *PfhorgePrefsLabel(
    NSString *text,
    CGFloat size,
    NSFontWeight weight,
    NSColor *color)
{
    NSTextField *label = [NSTextField labelWithString:text ?: @""];
    label.font = [NSFont systemFontOfSize:size weight:weight];
    label.textColor = color ?: NSColor.labelColor;
    label.selectable = NO;
    label.editable = NO;
    label.drawsBackground = NO;
    return label;
}


static NSButton *PfhorgePrefsCheckbox(
    NSString *title,
    id target,
    SEL action)
{
    NSButton *button = [[NSButton alloc] initWithFrame:NSZeroRect];
    button.buttonType = NSButtonTypeSwitch;
    button.title = title ?: @"";
    button.target = target;
    button.action = action;
    button.font = [NSFont systemFontOfSize:13.0];
    return button;
}


static NSView *PfhorgePrefsSeparator(void)
{
    NSBox *box = [[NSBox alloc] initWithFrame:NSZeroRect];
    box.boxType = NSBoxSeparator;
    [box.heightAnchor constraintEqualToConstant:1.0].active = YES;
    return box;
}


static NSView *PfhorgePrefsCard(
    NSString *title,
    NSString *subtitle,
    NSArray<NSView *> *rows)
{
    NSBox *box = [[NSBox alloc] initWithFrame:NSZeroRect];
    box.boxType = NSBoxCustom;
    box.cornerRadius = 9.0;
    box.borderWidth = 1.0;
    box.borderColor = NSColor.separatorColor;
    box.fillColor = NSColor.controlBackgroundColor;
    box.contentViewMargins = NSMakeSize(16.0, 14.0);

    NSStackView *stack = [[NSStackView alloc] initWithFrame:NSZeroRect];
    stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    stack.alignment = NSLayoutAttributeLeading;
    stack.spacing = 9.0;
    stack.translatesAutoresizingMaskIntoConstraints = NO;

    [box.contentView addSubview:stack];

    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor
            constraintEqualToAnchor:box.contentView.leadingAnchor],
        [stack.trailingAnchor
            constraintEqualToAnchor:box.contentView.trailingAnchor],
        [stack.topAnchor
            constraintEqualToAnchor:box.contentView.topAnchor],
        [stack.bottomAnchor
            constraintEqualToAnchor:box.contentView.bottomAnchor],
    ]];

    if (title.length > 0) {
        [stack addArrangedSubview:
            PfhorgePrefsLabel(
                title,
                14.0,
                NSFontWeightSemibold,
                NSColor.labelColor)];
    }

    if (subtitle.length > 0) {
        NSTextField *detail =
            PfhorgePrefsLabel(
                subtitle,
                11.5,
                NSFontWeightRegular,
                NSColor.secondaryLabelColor);

        detail.usesSingleLineMode = NO;
        detail.lineBreakMode = NSLineBreakByWordWrapping;
        [stack addArrangedSubview:detail];
        [detail.widthAnchor
            constraintLessThanOrEqualToConstant:680.0].active = YES;
    }

    if (title.length > 0 && rows.count > 0) {
        [stack addArrangedSubview:PfhorgePrefsSeparator()];
        [stack.arrangedSubviews.lastObject.widthAnchor
            constraintEqualToAnchor:stack.widthAnchor].active = YES;
    }

    for (NSView *row in rows) {
        [stack addArrangedSubview:row];
        [row.widthAnchor
            constraintEqualToAnchor:stack.widthAnchor].active = YES;
    }

    return box;
}


static NSView *PfhorgePrefsControlRow(
    NSString *labelText,
    NSView *control)
{
    NSStackView *row = [[NSStackView alloc] initWithFrame:NSZeroRect];
    row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    row.alignment = NSLayoutAttributeCenterY;
    row.spacing = 12.0;

    NSTextField *label =
        PfhorgePrefsLabel(
            labelText,
            13.0,
            NSFontWeightRegular,
            NSColor.labelColor);

    [label setContentHuggingPriority:NSLayoutPriorityDefaultHigh
                      forOrientation:
        NSLayoutConstraintOrientationHorizontal];

    NSView *spacer = [[NSView alloc] initWithFrame:NSZeroRect];
    [spacer setContentHuggingPriority:NSLayoutPriorityDefaultLow
                       forOrientation:
        NSLayoutConstraintOrientationHorizontal];

    [row addArrangedSubview:label];
    [row addArrangedSubview:spacer];
    [row addArrangedSubview:control];

    return row;
}


static NSArray<NSDictionary *> *PfhorgeEditorColorDescriptors(void)
{
    static NSArray<NSDictionary *> *descriptors = nil;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        descriptors = @[
            @{@"group": @"Canvas & Grid",
              @"label": @"Background",
              @"key": PhBackgroundColor},
            @{@"group": @"Canvas & Grid",
              @"label": @"World-unit grid",
              @"key": PhWorldUnitGridColor},
            @{@"group": @"Canvas & Grid",
              @"label": @"Sub-grid",
              @"key": PhSubWorldUnitGridColor},
            @{@"group": @"Canvas & Grid",
              @"label": @"Center grid",
              @"key": PhCenterWorldUnitGridColor},

            @{@"group": @"Polygons",
              @"label": @"Regular polygon",
              @"key": PhPolygonRegularColor},
            @{@"group": @"Polygons",
              @"label": @"Selected polygon",
              @"key": PhPolygonSelectedColor},
            @{@"group": @"Polygons",
              @"label": @"Platform",
              @"key": PhPolygonPlatformColor},
            @{@"group": @"Polygons",
              @"label": @"Invalid / non-convex",
              @"key": PhPolygonNonConcaveColor},
            @{@"group": @"Polygons",
              @"label": @"Teleporter / exit",
              @"key": PhPolygonTeleporterColor},
            @{@"group": @"Polygons",
              @"label": @"Zone",
              @"key": PhPolygonZoneColor},
            @{@"group": @"Polygons",
              @"label": @"Hill",
              @"key": PhPolygonHillColor},

            @{@"group": @"Lines & Points",
              @"label": @"Regular line",
              @"key": PhLineRegularColor},
            @{@"group": @"Lines & Points",
              @"label": @"Selected line",
              @"key": PhLineSelectedColor},
            @{@"group": @"Lines & Points",
              @"label": @"Line joining polygons",
              @"key": PhLineConnectsPolysColor},
            @{@"group": @"Lines & Points",
              @"label": @"Regular point",
              @"key": PhPointRegularColor},
            @{@"group": @"Lines & Points",
              @"label": @"Selected point",
              @"key": PhPointSelectedColor},

            @{@"group": @"Objects",
              @"label": @"Selected object",
              @"key": PhObjectSelectedColor},
            @{@"group": @"Objects",
              @"label": @"Item",
              @"key": PhObjectItemColor},
            @{@"group": @"Objects",
              @"label": @"Player",
              @"key": PhObjectPlayerColor},
            @{@"group": @"Objects",
              @"label": @"Enemy monster",
              @"key": PhObjectEnemyMonsterColor},
            @{@"group": @"Objects",
              @"label": @"Scenery",
              @"key": PhObjectSceanryColor},
            @{@"group": @"Objects",
              @"label": @"Sound",
              @"key": PhObjectSoundColor},
            @{@"group": @"Objects",
              @"label": @"Goal",
              @"key": PhObjectGoalColor},

            @{@"group": @"Object Outlines",
              @"label": @"Selected outline",
              @"key": PhObjectBLineSelectedColor},
            @{@"group": @"Object Outlines",
              @"label": @"Item outline",
              @"key": PhObjectBLineItemColor},
            @{@"group": @"Object Outlines",
              @"label": @"Player outline",
              @"key": PhObjectBLinePlayerColor},
            @{@"group": @"Object Outlines",
              @"label": @"Enemy outline",
              @"key": PhObjectBLineEnemyMonsterColor},
            @{@"group": @"Object Outlines",
              @"label": @"Scenery outline",
              @"key": PhObjectBLineSceanryColor},
            @{@"group": @"Object Outlines",
              @"label": @"Sound outline",
              @"key": PhObjectBLineSoundColor},
            @{@"group": @"Object Outlines",
              @"label": @"Goal outline",
              @"key": PhObjectBLineGoalColor},
        ];
    });

    return descriptors;
}


/*
 * Pfhorge Classic is intentionally byte-for-byte equivalent in intent to
 * the color defaults already registered by PhPrefsController.
 */
static NSDictionary<NSString *, NSColor *> *PfhorgeClassicTheme(void)
{
    return @{
        PhPolygonRegularColor: PfhorgePrefsRGB(0.00, 0.14, 0.00),
        PhPolygonSelectedColor: PfhorgePrefsRGB(0.00, 0.00, 1.00),
        PhPolygonPlatformColor: PfhorgePrefsRGB(0.00, 0.05, 0.58),
        PhPolygonTeleporterColor: PfhorgePrefsRGB(0.75, 0.50, 1.00),
        PhPolygonZoneColor: PfhorgePrefsRGB(0.00, 0.58, 0.58),
        PhPolygonNonConcaveColor: PfhorgePrefsRGB(1.00, 0.00, 0.00),
        PhPolygonHillColor: PfhorgePrefsRGB(0.75, 0.50, 0.00),

        PhLineRegularColor: PfhorgePrefsRGB(0.59, 0.60, 1.00),
        PhLineSelectedColor: PfhorgePrefsRGB(0.00, 0.00, 1.00),
        PhLineConnectsPolysColor: PfhorgePrefsRGB(0.00, 0.85, 0.85),

        PhPointRegularColor: PfhorgePrefsRGB(1.00, 1.00, 0.00),
        PhPointSelectedColor: PfhorgePrefsRGB(0.00, 0.00, 1.00),

        PhObjectSelectedColor: PfhorgePrefsRGB(0.00, 0.00, 0.26),
        PhObjectItemColor: PfhorgePrefsRGB(0.00, 0.36, 0.00),
        PhObjectPlayerColor: PfhorgePrefsRGB(0.31, 0.31, 0.00),
        PhObjectEnemyMonsterColor: PfhorgePrefsRGB(0.35, 0.00, 0.00),
        PhObjectSceanryColor: PfhorgePrefsRGB(0.39, 0.00, 0.39),
        PhObjectSoundColor: PfhorgePrefsRGB(0.00, 0.42, 0.42),
        PhObjectGoalColor: PfhorgePrefsRGB(0.00, 0.47, 0.22),

        PhObjectBLineSelectedColor: PfhorgePrefsRGB(0.00, 0.00, 1.00),
        PhObjectBLineItemColor: PfhorgePrefsRGB(0.00, 1.00, 0.00),
        PhObjectBLinePlayerColor: PfhorgePrefsRGB(1.00, 1.00, 0.00),
        PhObjectBLineEnemyMonsterColor: PfhorgePrefsRGB(1.00, 0.00, 0.00),
        PhObjectBLineSceanryColor: PfhorgePrefsRGB(1.00, 0.00, 1.00),
        PhObjectBLineSoundColor: PfhorgePrefsRGB(0.00, 1.00, 1.00),
        PhObjectBLineGoalColor: PfhorgePrefsRGB(0.00, 1.00, 0.57),

        PhBackgroundColor: PfhorgePrefsRGB(0.00, 0.00, 0.00),
        PhWorldUnitGridColor: PfhorgePrefsRGB(0.00, 0.00, 1.00),
        PhSubWorldUnitGridColor: PfhorgePrefsRGB(0.75, 0.75, 0.75),
        PhCenterWorldUnitGridColor: PfhorgePrefsRGB(0.33, 1.00, 0.33),
    };
}


/*
 * Forge replica presets.
 *
 * Bungie's public manual documents a Draw Mode Colors preference, but does
 * not publish numeric RGB constants. These presets therefore reproduce the
 * period Forge visual vocabulary — dark drafting field, blue grid, green
 * polygons, bright yellow vertices, cyan/blue selection and saturated object
 * codes — while keeping all values isolated here for later pixel-level tuning
 * against a specific preserved Forge screenshot if desired.
 */
static NSDictionary<NSString *, NSColor *> *PfhorgeForgeDrawTheme(void)
{
    NSMutableDictionary *theme = [PfhorgeClassicTheme() mutableCopy];

    theme[PhBackgroundColor] = PfhorgePrefsRGB(0.00, 0.00, 0.00);
    theme[PhWorldUnitGridColor] = PfhorgePrefsRGB(0.00, 0.10, 0.68);
    theme[PhSubWorldUnitGridColor] = PfhorgePrefsRGB(0.02, 0.03, 0.20);
    theme[PhCenterWorldUnitGridColor] = PfhorgePrefsRGB(0.00, 0.55, 0.28);

    theme[PhPolygonRegularColor] = PfhorgePrefsRGB(0.00, 0.20, 0.04);
    theme[PhPolygonSelectedColor] = PfhorgePrefsRGB(0.00, 0.40, 0.95);
    theme[PhPolygonPlatformColor] = PfhorgePrefsRGB(0.00, 0.12, 0.52);
    theme[PhPolygonTeleporterColor] = PfhorgePrefsRGB(0.70, 0.35, 0.95);
    theme[PhPolygonZoneColor] = PfhorgePrefsRGB(0.00, 0.50, 0.52);
    theme[PhPolygonNonConcaveColor] = PfhorgePrefsRGB(0.95, 0.05, 0.05);
    theme[PhPolygonHillColor] = PfhorgePrefsRGB(0.72, 0.46, 0.00);

    theme[PhLineRegularColor] = PfhorgePrefsRGB(0.72, 0.74, 0.86);
    theme[PhLineSelectedColor] = PfhorgePrefsRGB(0.10, 0.62, 1.00);
    theme[PhLineConnectsPolysColor] = PfhorgePrefsRGB(0.00, 0.88, 0.82);
    theme[PhPointRegularColor] = PfhorgePrefsRGB(1.00, 0.94, 0.00);
    theme[PhPointSelectedColor] = PfhorgePrefsRGB(1.00, 1.00, 1.00);

    return theme;
}


static NSDictionary<NSString *, NSColor *> *PfhorgeForgeClassicMacTheme(void)
{
    NSMutableDictionary *theme = [PfhorgeClassicTheme() mutableCopy];

    theme[PhBackgroundColor] = PfhorgePrefsRGB(0.93, 0.93, 0.91);
    theme[PhWorldUnitGridColor] = PfhorgePrefsRGB(0.48, 0.50, 0.57);
    theme[PhSubWorldUnitGridColor] = PfhorgePrefsRGB(0.78, 0.79, 0.81);
    theme[PhCenterWorldUnitGridColor] = PfhorgePrefsRGB(0.12, 0.38, 0.70);

    theme[PhPolygonRegularColor] = PfhorgePrefsRGB(0.74, 0.83, 0.74);
    theme[PhPolygonSelectedColor] = PfhorgePrefsRGB(0.22, 0.50, 0.95);
    theme[PhPolygonPlatformColor] = PfhorgePrefsRGB(0.60, 0.71, 0.88);
    theme[PhPolygonTeleporterColor] = PfhorgePrefsRGB(0.76, 0.57, 0.90);
    theme[PhPolygonZoneColor] = PfhorgePrefsRGB(0.52, 0.79, 0.79);
    theme[PhPolygonNonConcaveColor] = PfhorgePrefsRGB(0.92, 0.20, 0.16);
    theme[PhPolygonHillColor] = PfhorgePrefsRGB(0.84, 0.64, 0.20);

    theme[PhLineRegularColor] = PfhorgePrefsRGB(0.08, 0.08, 0.10);
    theme[PhLineSelectedColor] = PfhorgePrefsRGB(0.00, 0.22, 0.92);
    theme[PhLineConnectsPolysColor] = PfhorgePrefsRGB(0.00, 0.52, 0.55);
    theme[PhPointRegularColor] = PfhorgePrefsRGB(0.88, 0.58, 0.00);
    theme[PhPointSelectedColor] = PfhorgePrefsRGB(0.00, 0.18, 0.86);

    return theme;
}


static NSDictionary<NSString *, NSColor *> *PfhorgeBlueprintTheme(void)
{
    NSMutableDictionary *theme = [PfhorgeClassicTheme() mutableCopy];

    theme[PhBackgroundColor] = PfhorgePrefsRGB(0.025, 0.080, 0.16);
    theme[PhWorldUnitGridColor] = PfhorgePrefsRGB(0.10, 0.28, 0.50);
    theme[PhSubWorldUnitGridColor] = PfhorgePrefsRGB(0.06, 0.16, 0.30);
    theme[PhCenterWorldUnitGridColor] = PfhorgePrefsRGB(0.24, 0.62, 0.88);

    theme[PhPolygonRegularColor] = PfhorgePrefsRGB(0.05, 0.20, 0.34);
    theme[PhPolygonSelectedColor] = PfhorgePrefsRGB(0.16, 0.60, 0.95);
    theme[PhLineRegularColor] = PfhorgePrefsRGB(0.72, 0.88, 1.00);
    theme[PhLineSelectedColor] = PfhorgePrefsRGB(1.00, 1.00, 1.00);
    theme[PhLineConnectsPolysColor] = PfhorgePrefsRGB(0.26, 0.78, 0.96);
    theme[PhPointRegularColor] = PfhorgePrefsRGB(1.00, 0.85, 0.22);
    theme[PhPointSelectedColor] = PfhorgePrefsRGB(1.00, 1.00, 1.00);

    return theme;
}


static NSDictionary<NSString *, NSDictionary<NSString *, NSColor *> *> *
PfhorgeBuiltInEditorThemes(void)
{
    static NSDictionary *themes = nil;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        themes = @{
            @"Pfhorge Classic": PfhorgeClassicTheme(),
            @"Bungie Forge — Draw Mode Replica": PfhorgeForgeDrawTheme(),
            @"Bungie Forge — Classic Mac Replica": PfhorgeForgeClassicMacTheme(),
            @"Blueprint": PfhorgeBlueprintTheme(),
        };
    });

    return themes;
}


static NSArray<NSString *> *PfhorgeBuiltInEditorThemeOrder(void)
{
    return @[
        @"Pfhorge Classic",
        @"Bungie Forge — Draw Mode Replica",
        @"Bungie Forge — Classic Mac Replica",
        @"Blueprint",
    ];
}


@interface PfhorgeUnifiedPreferencesController :
    NSWindowController <NSToolbarDelegate>

+ (instancetype)sharedController;

- (void)showGeneralPane:(id)sender;
- (void)showVisualModePane:(id)sender;
- (void)showPaneIdentifier:(NSString *)identifier sender:(id)sender;

@end


@implementation PfhorgeUnifiedPreferencesController {
    NSView *_paneHost;

    NSView *_generalPane;
    NSView *_appearancePane;
    NSView *_objectsPane;
    NSView *_visualModePane;

    NSMutableArray<NSButton *> *_generalBooleanButtons;
    NSArray<NSString *> *_generalBooleanKeys;

    NSMutableArray<NSButton *> *_objectVisibilityButtons;
    NSArray<NSString *> *_objectVisibilityKeys;

    NSMutableArray<NSButton *> *_polygonColoringButtons;
    NSArray<NSString *> *_polygonColoringKeys;

    NSPopUpButton *_gridSizePopup;
    NSSlider *_snapPointDistanceSlider;
    NSTextField *_snapPointDistanceValue;
    NSSlider *_snapFromDistanceSlider;
    NSTextField *_snapFromDistanceValue;

    NSPopUpButton *_themePopup;
    NSButton *_deleteThemeButton;
    NSMutableArray<NSColorWell *> *_colorWells;
    NSArray<NSDictionary *> *_colorDescriptors;

    NSString *_currentPaneIdentifier;
    NSString *_lastSavedThemeName;
    id _embeddedVisualModeController;
}


+ (instancetype)sharedController
{
    static PfhorgeUnifiedPreferencesController *controller = nil;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        controller = [[PfhorgeUnifiedPreferencesController alloc] init];
    });

    return controller;
}


- (instancetype)init
{
    NSWindow *window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(0.0, 0.0, 900.0, 650.0)
                  styleMask:NSWindowStyleMaskTitled |
                            NSWindowStyleMaskClosable
                    backing:NSBackingStoreBuffered
                      defer:NO];

    self = [super initWithWindow:window];

    if (self == nil) {
        return nil;
    }

    window.title = @"Pfhorge Settings";
    window.releasedWhenClosed = NO;
    window.movableByWindowBackground = YES;
    window.backgroundColor = NSColor.windowBackgroundColor;
    window.minSize = NSMakeSize(820.0, 600.0);

    _paneHost = [[NSView alloc] initWithFrame:window.contentView.bounds];
    _paneHost.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    window.contentView = _paneHost;

    NSToolbar *toolbar =
        [[NSToolbar alloc] initWithIdentifier:@"PfhorgePreferencesToolbar"];

    toolbar.delegate = self;
    toolbar.allowsUserCustomization = NO;
    toolbar.autosavesConfiguration = NO;
    toolbar.displayMode = NSToolbarDisplayModeIconAndLabel;
    toolbar.sizeMode = NSToolbarSizeModeDefault;

    window.toolbar = toolbar;

    _colorDescriptors = PfhorgeEditorColorDescriptors();

    _generalBooleanKeys = @[
        PhEnableAntialiasing,
        PhEnableObjectOutling,
        PhSelectObjectWhenCreated,
        PhDrawOnlyLayerPoints,
        PhEnableGridBool,
        PhSnapToGridBool,
        PhSnapObjectsToGrid,
        PhSnapToPoints,
        PhSnapFromPoints,
        PhUseRightAngleSnap,
        PhUseIsometricAngleSnap,
    ];

    _objectVisibilityKeys = @[
        PhEnableObjectItem,
        PhEnableObjectPlayer,
        PhEnableObjectEnemyMonster,
        PhEnableObjectSceanry,
        PhEnableObjectSound,
        PhEnableObjectGoal,
    ];

    _polygonColoringKeys = @[
        PhEnablePlatfromPolyColoring,
        PhEnableConvexPolyColoring,
        PhEnableZonePolyColoring,
        PhEnableTeleporterExitPolyColoring,
        PhEnableHillPolyColoring,
    ];

    _generalBooleanButtons = [NSMutableArray array];
    _objectVisibilityButtons = [NSMutableArray array];
    _polygonColoringButtons = [NSMutableArray array];
    _colorWells = [NSMutableArray array];

    _lastSavedThemeName =
        [[NSUserDefaults standardUserDefaults]
            stringForKey:PfhorgePrefsSelectedThemePreference];

    return self;
}


- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    (void)toolbar;

    return @[
        PfhorgePrefsPaneGeneral,
        PfhorgePrefsPaneAppearance,
        PfhorgePrefsPaneObjects,
        PfhorgePrefsPaneVisualMode,
        NSToolbarFlexibleSpaceItemIdentifier,
    ];
}


- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{
    (void)toolbar;

    return @[
        PfhorgePrefsPaneGeneral,
        PfhorgePrefsPaneAppearance,
        PfhorgePrefsPaneObjects,
        PfhorgePrefsPaneVisualMode,
    ];
}


- (NSArray *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar
{
    (void)toolbar;

    return @[
        PfhorgePrefsPaneGeneral,
        PfhorgePrefsPaneAppearance,
        PfhorgePrefsPaneObjects,
        PfhorgePrefsPaneVisualMode,
    ];
}


- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar
   itemForItemIdentifier:(NSString *)itemIdentifier
willBeInsertedIntoToolbar:(BOOL)flag
{
    (void)toolbar;
    (void)flag;

    NSToolbarItem *item =
        [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

    if ([itemIdentifier isEqualToString:PfhorgePrefsPaneGeneral]) {
        item.label = @"General";
        item.paletteLabel = @"General";
        item.toolTip = @"General editor and grid behavior.";
        item.image = [NSImage imageNamed:@"NSPreferencesGeneral"];
    } else if ([itemIdentifier isEqualToString:PfhorgePrefsPaneAppearance]) {
        item.label = @"Colors & Themes";
        item.paletteLabel = @"Colors & Themes";
        item.toolTip = @"Editor colors and reusable theme presets.";
        item.image = [NSImage imageNamed:@"NSColorPanel"];
    } else if ([itemIdentifier isEqualToString:PfhorgePrefsPaneObjects]) {
        item.label = @"Objects";
        item.paletteLabel = @"Objects";
        item.toolTip = @"Object visibility and polygon coloring.";
        item.image = [NSImage imageNamed:@"NSUser"];
    } else if ([itemIdentifier isEqualToString:PfhorgePrefsPaneVisualMode]) {
        item.label = @"Visual Mode & GPU";
        item.paletteLabel = @"Visual Mode & GPU";
        item.toolTip = @"Metal Visual Mode, controls, GPU and diagnostics.";
        item.image = [NSImage imageNamed:@"NSAdvanced"];
    }

    item.target = self;
    item.action = @selector(toolbarPaneSelected:);

    return item;
}


- (void)toolbarPaneSelected:(id)sender
{
    NSToolbarItem *item = (NSToolbarItem *)sender;
    [self showPaneIdentifier:item.itemIdentifier sender:sender];
}


- (void)showGeneralPane:(id)sender
{
    [self showPaneIdentifier:PfhorgePrefsPaneGeneral sender:sender];
}


- (void)showVisualModePane:(id)sender
{
    [self showPaneIdentifier:PfhorgePrefsPaneVisualMode sender:sender];
}


- (void)showPaneIdentifier:(NSString *)identifier sender:(id)sender
{
    if (identifier.length == 0) {
        identifier = PfhorgePrefsPaneGeneral;
    }

    NSView *pane = [self paneForIdentifier:identifier];

    if (pane == nil) {
        identifier = PfhorgePrefsPaneGeneral;
        pane = [self paneForIdentifier:identifier];
    }

    for (NSView *view in _paneHost.subviews.copy) {
        [view removeFromSuperview];
    }

    pane.frame = _paneHost.bounds;
    pane.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [_paneHost addSubview:pane];

    _currentPaneIdentifier = [identifier copy];

    [[NSUserDefaults standardUserDefaults]
        setObject:identifier
           forKey:PfhorgePrefsLastPanePreference];

    self.window.toolbar.selectedItemIdentifier = identifier;

    if ([identifier isEqualToString:PfhorgePrefsPaneGeneral]) {
        self.window.title = @"General — Pfhorge Settings";
        [self reloadGeneralControls];
    } else if ([identifier isEqualToString:PfhorgePrefsPaneAppearance]) {
        self.window.title = @"Colors & Themes — Pfhorge Settings";
        [self reloadThemeMenu];
        [self reloadColorWells];
        [self reloadPolygonColoringControls];
    } else if ([identifier isEqualToString:PfhorgePrefsPaneObjects]) {
        self.window.title = @"Objects — Pfhorge Settings";
        [self reloadObjectVisibilityControls];
    } else if ([identifier isEqualToString:PfhorgePrefsPaneVisualMode]) {
        self.window.title = @"Visual Mode & GPU — Pfhorge Settings";
        [self reloadEmbeddedVisualModeSettings];
    }

    if (!self.window.isVisible) {
        [self.window center];
    }

    [super showWindow:sender];
    [self.window makeKeyAndOrderFront:sender];
}


- (void)showWindow:(id)sender
{
    NSString *identifier =
        [[NSUserDefaults standardUserDefaults]
            stringForKey:PfhorgePrefsLastPanePreference];

    if (identifier.length == 0) {
        identifier = PfhorgePrefsPaneGeneral;
    }

    [self showPaneIdentifier:identifier sender:sender];
}


- (NSView *)paneForIdentifier:(NSString *)identifier
{
    if ([identifier isEqualToString:PfhorgePrefsPaneGeneral]) {
        if (_generalPane == nil) {
            _generalPane = [self buildGeneralPane];
        }
        return _generalPane;
    }

    if ([identifier isEqualToString:PfhorgePrefsPaneAppearance]) {
        if (_appearancePane == nil) {
            _appearancePane = [self buildAppearancePane];
        }
        return _appearancePane;
    }

    if ([identifier isEqualToString:PfhorgePrefsPaneObjects]) {
        if (_objectsPane == nil) {
            _objectsPane = [self buildObjectsPane];
        }
        return _objectsPane;
    }

    if ([identifier isEqualToString:PfhorgePrefsPaneVisualMode]) {
        if (_visualModePane == nil) {
            _visualModePane = [self buildVisualModePane];
        }
        return _visualModePane;
    }

    return nil;
}


- (NSScrollView *)scrollViewForDocumentView:(NSView *)documentView
{
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    scroll.hasVerticalScroller = YES;
    scroll.hasHorizontalScroller = NO;
    scroll.autohidesScrollers = YES;
    scroll.drawsBackground = NO;
    scroll.documentView = documentView;
    return scroll;
}


- (NSView *)baseDocumentViewWithWidth:(CGFloat)width
{
    NSView *document =
        [[NSView alloc] initWithFrame:NSMakeRect(0, 0, width, 100)];

    document.translatesAutoresizingMaskIntoConstraints = NO;
    return document;
}


- (NSView *)buildGeneralPane
{
    NSView *container = [[NSView alloc] initWithFrame:NSZeroRect];

    NSStackView *stack = [[NSStackView alloc] initWithFrame:NSZeroRect];
    stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    stack.alignment = NSLayoutAttributeLeading;
    stack.spacing = 16.0;
    stack.translatesAutoresizingMaskIntoConstraints = NO;

    NSView *document = [self baseDocumentViewWithWidth:820.0];
    [document addSubview:stack];

    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor
            constraintEqualToAnchor:document.leadingAnchor
                           constant:28.0],
        [stack.trailingAnchor
            constraintEqualToAnchor:document.trailingAnchor
                           constant:-28.0],
        [stack.topAnchor
            constraintEqualToAnchor:document.topAnchor
                           constant:26.0],
        [stack.bottomAnchor
            constraintEqualToAnchor:document.bottomAnchor
                           constant:-28.0],
        [document.widthAnchor constraintEqualToConstant:820.0],
    ]];

    [stack addArrangedSubview:
        PfhorgePrefsLabel(
            @"General",
            26.0,
            NSFontWeightBold,
            NSColor.labelColor)];

    [stack addArrangedSubview:
        PfhorgePrefsLabel(
            @"Editor behavior, grid resolution, and snapping.",
            12.5,
            NSFontWeightRegular,
            NSColor.secondaryLabelColor)];

    NSArray<NSString *> *editorTitles = @[
        @"Antialias map drawing",
        @"Outline map objects",
        @"Select objects when they are created",
        @"Draw only points belonging to the current layer",
    ];

    NSMutableArray<NSView *> *editorRows = [NSMutableArray array];

    for (NSUInteger index = 0; index < editorTitles.count; ++index) {
        NSButton *button =
            PfhorgePrefsCheckbox(
                editorTitles[index],
                self,
                @selector(generalBooleanChanged:));

        button.tag = (NSInteger)index;
        [_generalBooleanButtons addObject:button];
        [editorRows addObject:button];
    }

    NSView *editorCard =
        PfhorgePrefsCard(
            @"Editor Behavior",
            @"These options affect the 2D map editor globally.",
            editorRows);

    [stack addArrangedSubview:editorCard];
    [editorCard.widthAnchor
        constraintEqualToAnchor:stack.widthAnchor].active = YES;

    NSMutableArray<NSView *> *gridRows = [NSMutableArray array];

    NSArray<NSString *> *gridTitles = @[
        @"Display grid",
        @"Snap new geometry to grid",
        @"Snap objects to grid",
        @"Snap to existing points",
        @"Snap from existing points",
        @"Enable right-angle snapping",
        @"Enable isometric-angle snapping",
    ];

    for (NSUInteger localIndex = 0;
         localIndex < gridTitles.count;
         ++localIndex) {
        NSUInteger globalIndex =
            localIndex + editorTitles.count;

        NSButton *button =
            PfhorgePrefsCheckbox(
                gridTitles[localIndex],
                self,
                @selector(generalBooleanChanged:));

        button.tag = (NSInteger)globalIndex;
        [_generalBooleanButtons addObject:button];
        [gridRows addObject:button];
    }

    _gridSizePopup =
        [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];

    [_gridSizePopup addItemsWithTitles:@[
        @"1/8 W.U.",
        @"1/4 W.U.",
        @"1/2 W.U.",
        @"1 W.U.",
        @"2 W.U.",
        @"4 W.U.",
        @"8 W.U.",
    ]];

    _gridSizePopup.target = self;
    _gridSizePopup.action = @selector(gridSizeChanged:);

    [_gridSizePopup.widthAnchor
        constraintEqualToConstant:145.0].active = YES;

    [gridRows addObject:
        PfhorgePrefsControlRow(
            @"Grid size",
            _gridSizePopup)];

    _snapPointDistanceSlider =
        [[NSSlider alloc] initWithFrame:NSZeroRect];

    _snapPointDistanceSlider.minValue = 1.0;
    _snapPointDistanceSlider.maxValue = 64.0;
    _snapPointDistanceSlider.numberOfTickMarks = 8;
    _snapPointDistanceSlider.allowsTickMarkValuesOnly = NO;
    _snapPointDistanceSlider.target = self;
    _snapPointDistanceSlider.action =
        @selector(snapPointDistanceChanged:);

    [_snapPointDistanceSlider.widthAnchor
        constraintEqualToConstant:220.0].active = YES;

    _snapPointDistanceValue =
        PfhorgePrefsLabel(
            @"",
            12.0,
            NSFontWeightMedium,
            NSColor.secondaryLabelColor);

    [_snapPointDistanceValue.widthAnchor
        constraintEqualToConstant:54.0].active = YES;

    NSStackView *pointDistanceControls =
        [NSStackView stackViewWithViews:@[
            _snapPointDistanceSlider,
            _snapPointDistanceValue
        ]];

    pointDistanceControls.orientation =
        NSUserInterfaceLayoutOrientationHorizontal;
    pointDistanceControls.alignment = NSLayoutAttributeCenterY;
    pointDistanceControls.spacing = 10.0;

    [gridRows addObject:
        PfhorgePrefsControlRow(
            @"Point snap distance",
            pointDistanceControls)];

    _snapFromDistanceSlider =
        [[NSSlider alloc] initWithFrame:NSZeroRect];

    _snapFromDistanceSlider.minValue = 1.0;
    _snapFromDistanceSlider.maxValue = 64.0;
    _snapFromDistanceSlider.numberOfTickMarks = 8;
    _snapFromDistanceSlider.allowsTickMarkValuesOnly = NO;
    _snapFromDistanceSlider.target = self;
    _snapFromDistanceSlider.action =
        @selector(snapFromDistanceChanged:);

    [_snapFromDistanceSlider.widthAnchor
        constraintEqualToConstant:220.0].active = YES;

    _snapFromDistanceValue =
        PfhorgePrefsLabel(
            @"",
            12.0,
            NSFontWeightMedium,
            NSColor.secondaryLabelColor);

    [_snapFromDistanceValue.widthAnchor
        constraintEqualToConstant:54.0].active = YES;

    NSStackView *fromDistanceControls =
        [NSStackView stackViewWithViews:@[
            _snapFromDistanceSlider,
            _snapFromDistanceValue
        ]];

    fromDistanceControls.orientation =
        NSUserInterfaceLayoutOrientationHorizontal;
    fromDistanceControls.alignment = NSLayoutAttributeCenterY;
    fromDistanceControls.spacing = 10.0;

    [gridRows addObject:
        PfhorgePrefsControlRow(
            @"Snap-from distance",
            fromDistanceControls)];

    NSView *gridCard =
        PfhorgePrefsCard(
            @"Grid & Snapping",
            @"Grid and snap changes apply immediately to open map windows.",
            gridRows);

    [stack addArrangedSubview:gridCard];
    [gridCard.widthAnchor
        constraintEqualToAnchor:stack.widthAnchor].active = YES;

    NSScrollView *scroll =
        [self scrollViewForDocumentView:document];

    scroll.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:scroll];

    [NSLayoutConstraint activateConstraints:@[
        [scroll.leadingAnchor
            constraintEqualToAnchor:container.leadingAnchor],
        [scroll.trailingAnchor
            constraintEqualToAnchor:container.trailingAnchor],
        [scroll.topAnchor
            constraintEqualToAnchor:container.topAnchor],
        [scroll.bottomAnchor
            constraintEqualToAnchor:container.bottomAnchor],
    ]];

    [self reloadGeneralControls];

    return container;
}


- (void)reloadGeneralControls
{
    NSUserDefaults *defaults =
        [NSUserDefaults standardUserDefaults];

    for (NSUInteger index = 0;
         index < _generalBooleanButtons.count &&
         index < _generalBooleanKeys.count;
         ++index) {
        _generalBooleanButtons[index].state =
            [defaults boolForKey:_generalBooleanKeys[index]]
            ? NSControlStateValueOn
            : NSControlStateValueOff;
    }

    CGFloat factor = [defaults floatForKey:PhGridFactor];

    NSArray<NSNumber *> *factors = @[
        @0.125, @0.250, @0.500, @1.000, @2.000, @4.000, @8.000
    ];

    NSUInteger selected = 3U;
    CGFloat bestDistance = CGFLOAT_MAX;

    for (NSUInteger index = 0; index < factors.count; ++index) {
        CGFloat value = factors[index].doubleValue;
        CGFloat distance =
            factor >= value ? factor - value : value - factor;

        if (distance < bestDistance) {
            bestDistance = distance;
            selected = index;
        }
    }

    [_gridSizePopup selectItemAtIndex:(NSInteger)selected];

    NSInteger pointDistance =
        [defaults integerForKey:PhSnapToPointsLength];

    if (pointDistance <= 0) {
        pointDistance = 15;
    }

    _snapPointDistanceSlider.integerValue = pointDistance;
    _snapPointDistanceValue.stringValue =
        [NSString stringWithFormat:
            @"%ld px",
            (long)pointDistance];

    NSInteger fromDistance =
        [defaults integerForKey:PhSnapFromLength];

    if (fromDistance <= 0) {
        fromDistance = 16;
    }

    _snapFromDistanceSlider.integerValue = fromDistance;
    _snapFromDistanceValue.stringValue =
        [NSString stringWithFormat:
            @"%ld px",
            (long)fromDistance];
}


- (void)generalBooleanChanged:(id)sender
{
    NSButton *button = (NSButton *)sender;
    NSInteger index = button.tag;

    if (index < 0 ||
        (NSUInteger)index >= _generalBooleanKeys.count) {
        return;
    }

    [[NSUserDefaults standardUserDefaults]
        setBool:(button.state == NSControlStateValueOn)
         forKey:_generalBooleanKeys[(NSUInteger)index]];

    [self postPreferenceChange];
}


- (void)gridSizeChanged:(id)sender
{
    NSPopUpButton *popup = (NSPopUpButton *)sender;

    NSArray<NSNumber *> *factors = @[
        @0.125, @0.250, @0.500, @1.000, @2.000, @4.000, @8.000
    ];

    NSInteger index = popup.indexOfSelectedItem;

    if (index < 0 ||
        (NSUInteger)index >= factors.count) {
        return;
    }

    [[NSUserDefaults standardUserDefaults]
        setFloat:factors[(NSUInteger)index].floatValue
          forKey:PhGridFactor];

    [self postPreferenceChange];
}


- (void)snapPointDistanceChanged:(id)sender
{
    NSSlider *slider = (NSSlider *)sender;

    [[NSUserDefaults standardUserDefaults]
        setInteger:slider.integerValue
            forKey:PhSnapToPointsLength];

    _snapPointDistanceValue.stringValue =
        [NSString stringWithFormat:
            @"%ld px",
            (long)slider.integerValue];

    [self postPreferenceChange];
}


- (void)snapFromDistanceChanged:(id)sender
{
    NSSlider *slider = (NSSlider *)sender;

    [[NSUserDefaults standardUserDefaults]
        setInteger:slider.integerValue
            forKey:PhSnapFromLength];

    _snapFromDistanceValue.stringValue =
        [NSString stringWithFormat:
            @"%ld px",
            (long)slider.integerValue];

    [self postPreferenceChange];
}


- (NSView *)buildAppearancePane
{
    NSView *container = [[NSView alloc] initWithFrame:NSZeroRect];
    NSView *document = [self baseDocumentViewWithWidth:820.0];

    NSStackView *stack = [[NSStackView alloc] initWithFrame:NSZeroRect];
    stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    stack.alignment = NSLayoutAttributeLeading;
    stack.spacing = 16.0;
    stack.translatesAutoresizingMaskIntoConstraints = NO;

    [document addSubview:stack];

    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor
            constraintEqualToAnchor:document.leadingAnchor
                           constant:28.0],
        [stack.trailingAnchor
            constraintEqualToAnchor:document.trailingAnchor
                           constant:-28.0],
        [stack.topAnchor
            constraintEqualToAnchor:document.topAnchor
                           constant:26.0],
        [stack.bottomAnchor
            constraintEqualToAnchor:document.bottomAnchor
                           constant:-28.0],
        [document.widthAnchor constraintEqualToConstant:820.0],
    ]];

    [stack addArrangedSubview:
        PfhorgePrefsLabel(
            @"Colors & Themes",
            26.0,
            NSFontWeightBold,
            NSColor.labelColor)];

    [stack addArrangedSubview:
        PfhorgePrefsLabel(
            @"Choose a complete editor palette or tune individual colors.",
            12.5,
            NSFontWeightRegular,
            NSColor.secondaryLabelColor)];

    _themePopup =
        [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];

    _themePopup.target = self;
    _themePopup.action = @selector(themeSelectionChanged:);

    [_themePopup.widthAnchor
        constraintEqualToConstant:310.0].active = YES;

    NSButton *saveTheme =
        [NSButton buttonWithTitle:@"Save As…"
                           target:self
                           action:@selector(saveThemeAs:)];

    _deleteThemeButton =
        [NSButton buttonWithTitle:@"Delete"
                           target:self
                           action:@selector(deleteSelectedTheme:)];

    NSButton *revertTheme =
        [NSButton buttonWithTitle:@"Revert"
                           target:self
                           action:@selector(revertSelectedTheme:)];

    NSStackView *themeControls =
        [NSStackView stackViewWithViews:@[
            _themePopup,
            saveTheme,
            _deleteThemeButton,
            revertTheme
        ]];

    themeControls.orientation =
        NSUserInterfaceLayoutOrientationHorizontal;
    themeControls.alignment = NSLayoutAttributeCenterY;
    themeControls.spacing = 8.0;

    NSView *themeCard =
        PfhorgePrefsCard(
            @"Editor Theme",
            @"Built-in presets are read-only. Save the current colors as a custom preset to reuse them later.",
            @[themeControls]);

    [stack addArrangedSubview:themeCard];
    [themeCard.widthAnchor
        constraintEqualToAnchor:stack.widthAnchor].active = YES;

    NSArray<NSString *> *polygonToggleTitles = @[
        @"Color platform polygons",
        @"Color invalid / non-convex polygons",
        @"Color zone polygons",
        @"Color teleporter and exit polygons",
        @"Color hill polygons",
    ];

    NSMutableArray<NSView *> *polygonRows =
        [NSMutableArray array];

    for (NSUInteger index = 0;
         index < polygonToggleTitles.count;
         ++index) {
        NSButton *button =
            PfhorgePrefsCheckbox(
                polygonToggleTitles[index],
                self,
                @selector(polygonColoringChanged:));

        button.tag = (NSInteger)index;
        [_polygonColoringButtons addObject:button];
        [polygonRows addObject:button];
    }

    NSView *polygonToggleCard =
        PfhorgePrefsCard(
            @"Polygon Color Coding",
            @"Control which special polygon types override the regular polygon color.",
            polygonRows);

    [stack addArrangedSubview:polygonToggleCard];
    [polygonToggleCard.widthAnchor
        constraintEqualToAnchor:stack.widthAnchor].active = YES;

    NSString *currentGroup = nil;
    NSMutableArray<NSView *> *currentRows =
        [NSMutableArray array];

    for (NSUInteger index = 0;
         index < _colorDescriptors.count;
         ++index) {
        NSDictionary *descriptor =
            _colorDescriptors[index];

        NSString *group = descriptor[@"group"];

        if (currentGroup != nil &&
            ![currentGroup isEqualToString:group]) {
            NSView *card =
                PfhorgePrefsCard(
                    currentGroup,
                    nil,
                    currentRows);

            [stack addArrangedSubview:card];
            [card.widthAnchor
                constraintEqualToAnchor:stack.widthAnchor].active = YES;

            currentRows = [NSMutableArray array];
        }

        currentGroup = group;

        NSColorWell *well =
            [[NSColorWell alloc]
                initWithFrame:NSMakeRect(0, 0, 58, 28)];

        well.tag = (NSInteger)index;
        well.target = self;
        well.action = @selector(editorColorChanged:);

        [well.widthAnchor
            constraintEqualToConstant:58.0].active = YES;

        [_colorWells addObject:well];

        [currentRows addObject:
            PfhorgePrefsControlRow(
                descriptor[@"label"],
                well)];
    }

    if (currentGroup != nil &&
        currentRows.count > 0) {
        NSView *card =
            PfhorgePrefsCard(
                currentGroup,
                nil,
                currentRows);

        [stack addArrangedSubview:card];
        [card.widthAnchor
            constraintEqualToAnchor:stack.widthAnchor].active = YES;
    }

    NSScrollView *scroll =
        [self scrollViewForDocumentView:document];

    scroll.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:scroll];

    [NSLayoutConstraint activateConstraints:@[
        [scroll.leadingAnchor
            constraintEqualToAnchor:container.leadingAnchor],
        [scroll.trailingAnchor
            constraintEqualToAnchor:container.trailingAnchor],
        [scroll.topAnchor
            constraintEqualToAnchor:container.topAnchor],
        [scroll.bottomAnchor
            constraintEqualToAnchor:container.bottomAnchor],
    ]];

    [self reloadThemeMenu];
    [self reloadColorWells];
    [self reloadPolygonColoringControls];

    return container;
}


- (void)reloadPolygonColoringControls
{
    NSUserDefaults *defaults =
        [NSUserDefaults standardUserDefaults];

    for (NSUInteger index = 0;
         index < _polygonColoringButtons.count &&
         index < _polygonColoringKeys.count;
         ++index) {
        _polygonColoringButtons[index].state =
            [defaults boolForKey:_polygonColoringKeys[index]]
            ? NSControlStateValueOn
            : NSControlStateValueOff;
    }
}


- (void)polygonColoringChanged:(id)sender
{
    NSButton *button = (NSButton *)sender;
    NSInteger index = button.tag;

    if (index < 0 ||
        (NSUInteger)index >= _polygonColoringKeys.count) {
        return;
    }

    [[NSUserDefaults standardUserDefaults]
        setBool:(button.state == NSControlStateValueOn)
         forKey:_polygonColoringKeys[(NSUInteger)index]];

    [self postPreferenceChange];
}


- (NSDictionary *)userThemes
{
    NSDictionary *themes =
        [[NSUserDefaults standardUserDefaults]
            dictionaryForKey:PfhorgePrefsUserThemesPreference];

    return themes ?: @{};
}


- (void)setUserThemes:(NSDictionary *)themes
{
    [[NSUserDefaults standardUserDefaults]
        setObject:themes ?: @{}
           forKey:PfhorgePrefsUserThemesPreference];
}


- (void)reloadThemeMenu
{
    if (_themePopup == nil) {
        return;
    }

    NSString *selected =
        [[NSUserDefaults standardUserDefaults]
            stringForKey:PfhorgePrefsSelectedThemePreference];

    [_themePopup removeAllItems];

    for (NSString *name in PfhorgeBuiltInEditorThemeOrder()) {
        [_themePopup addItemWithTitle:name];
    }

    NSDictionary *customThemes = [self userThemes];

    if (customThemes.count > 0) {
        [_themePopup.menu addItem:NSMenuItem.separatorItem];

        NSArray<NSString *> *sortedNames =
            [[customThemes allKeys]
                sortedArrayUsingSelector:
                    @selector(localizedCaseInsensitiveCompare:)];

        for (NSString *name in sortedNames) {
            [_themePopup addItemWithTitle:name];
        }
    }

    if ([selected isEqualToString:PfhorgePrefsModifiedThemeName]) {
        [_themePopup.menu addItem:NSMenuItem.separatorItem];
        [_themePopup addItemWithTitle:PfhorgePrefsModifiedThemeName];
    } else if (selected.length == 0 ||
               [_themePopup itemWithTitle:selected] == nil) {
        selected = @"Pfhorge Classic";
        [[NSUserDefaults standardUserDefaults]
            setObject:selected
               forKey:PfhorgePrefsSelectedThemePreference];
    }

    [_themePopup selectItemWithTitle:selected];
    [self updateThemeButtons];
}


- (void)reloadColorWells
{
    for (NSUInteger index = 0;
         index < _colorWells.count &&
         index < _colorDescriptors.count;
         ++index) {
        NSString *key =
            _colorDescriptors[index][@"key"];

        NSColor *color = getArchColor(key);

        if (color != nil) {
            _colorWells[index].color = color;
        }
    }
}


- (void)editorColorChanged:(id)sender
{
    NSColorWell *well = (NSColorWell *)sender;
    NSInteger index = well.tag;

    if (index < 0 ||
        (NSUInteger)index >= _colorDescriptors.count) {
        return;
    }

    NSString *key =
        _colorDescriptors[(NSUInteger)index][@"key"];

    setArchColor(key, well.color);

    NSString *selected =
        _themePopup.titleOfSelectedItem;

    if (selected.length > 0 &&
        ![selected isEqualToString:PfhorgePrefsModifiedThemeName]) {
        _lastSavedThemeName = [selected copy];
    }

    [[NSUserDefaults standardUserDefaults]
        setObject:PfhorgePrefsModifiedThemeName
           forKey:PfhorgePrefsSelectedThemePreference];

    [self reloadThemeMenu];
    [_themePopup selectItemWithTitle:PfhorgePrefsModifiedThemeName];

    [self postPreferenceChange];
}


- (void)themeSelectionChanged:(id)sender
{
    NSPopUpButton *popup = (NSPopUpButton *)sender;
    NSString *name = popup.titleOfSelectedItem;

    if (name.length == 0 ||
        [name isEqualToString:PfhorgePrefsModifiedThemeName]) {
        return;
    }

    [self applyThemeNamed:name];

    _lastSavedThemeName = [name copy];

    [[NSUserDefaults standardUserDefaults]
        setObject:name
           forKey:PfhorgePrefsSelectedThemePreference];

    [self reloadColorWells];
    [self updateThemeButtons];
    [self postPreferenceChange];
}


- (void)applyThemeNamed:(NSString *)name
{
    NSDictionary *builtIn =
        PfhorgeBuiltInEditorThemes()[name];

    if (builtIn != nil) {
        [builtIn enumerateKeysAndObjectsUsingBlock:
            ^(NSString *key, NSColor *color, BOOL *stop) {
                (void)stop;
                setArchColor(key, color);
            }];

        return;
    }

    NSDictionary *custom =
        [self userThemes][name];

    if (![custom isKindOfClass:NSDictionary.class]) {
        return;
    }

    NSUserDefaults *defaults =
        [NSUserDefaults standardUserDefaults];

    [custom enumerateKeysAndObjectsUsingBlock:
        ^(NSString *key, NSData *value, BOOL *stop) {
            (void)stop;

            if ([value isKindOfClass:NSData.class]) {
                [defaults setObject:value forKey:key];
            }
        }];
}


- (NSDictionary *)snapshotCurrentTheme
{
    NSMutableDictionary *snapshot =
        [NSMutableDictionary dictionary];

    NSUserDefaults *defaults =
        [NSUserDefaults standardUserDefaults];

    for (NSDictionary *descriptor in _colorDescriptors) {
        NSString *key = descriptor[@"key"];
        NSData *data = [defaults objectForKey:key];

        if ([data isKindOfClass:NSData.class]) {
            snapshot[key] = data;
        }
    }

    return snapshot.copy;
}


- (void)saveThemeAs:(id)sender
{
    (void)sender;

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Save Editor Theme";
    alert.informativeText =
        @"Enter a name for this reusable Pfhorge editor color theme.";

    [alert addButtonWithTitle:@"Save"];
    [alert addButtonWithTitle:@"Cancel"];

    NSTextField *nameField =
        [[NSTextField alloc]
            initWithFrame:NSMakeRect(0, 0, 320, 24)];

    nameField.placeholderString = @"Theme Name";
    alert.accessoryView = nameField;

    if ([alert runModal] != NSAlertFirstButtonReturn) {
        return;
    }

    NSString *name =
        [nameField.stringValue
            stringByTrimmingCharactersInSet:
                NSCharacterSet.whitespaceAndNewlineCharacterSet];

    if (name.length == 0) {
        NSBeep();
        return;
    }

    if (PfhorgeBuiltInEditorThemes()[name] != nil ||
        [name isEqualToString:PfhorgePrefsModifiedThemeName]) {
        NSAlert *reserved = [[NSAlert alloc] init];
        reserved.messageText = @"That name is reserved";
        reserved.informativeText =
            @"Built-in theme names cannot be overwritten. Choose another name.";
        [reserved runModal];
        return;
    }

    NSMutableDictionary *themes =
        [[self userThemes] mutableCopy];

    themes[name] = [self snapshotCurrentTheme];
    [self setUserThemes:themes];

    _lastSavedThemeName = [name copy];

    [[NSUserDefaults standardUserDefaults]
        setObject:name
           forKey:PfhorgePrefsSelectedThemePreference];

    [self reloadThemeMenu];
    [_themePopup selectItemWithTitle:name];
    [self updateThemeButtons];
}


- (void)deleteSelectedTheme:(id)sender
{
    (void)sender;

    NSString *name =
        _themePopup.titleOfSelectedItem;

    if (name.length == 0 ||
        PfhorgeBuiltInEditorThemes()[name] != nil ||
        [name isEqualToString:PfhorgePrefsModifiedThemeName]) {
        NSBeep();
        return;
    }

    NSAlert *alert = [[NSAlert alloc] init];

    alert.messageText =
        [NSString stringWithFormat:
            @"Delete “%@”?",
            name];

    alert.informativeText =
        @"This removes the saved preset. It does not change the colors currently in use.";

    [alert addButtonWithTitle:@"Delete"];
    [alert addButtonWithTitle:@"Cancel"];

    if ([alert runModal] != NSAlertFirstButtonReturn) {
        return;
    }

    NSMutableDictionary *themes =
        [[self userThemes] mutableCopy];

    [themes removeObjectForKey:name];
    [self setUserThemes:themes];

    _lastSavedThemeName = nil;

    [[NSUserDefaults standardUserDefaults]
        setObject:PfhorgePrefsModifiedThemeName
           forKey:PfhorgePrefsSelectedThemePreference];

    [self reloadThemeMenu];
}


- (void)revertSelectedTheme:(id)sender
{
    (void)sender;

    NSString *name =
        _themePopup.titleOfSelectedItem;

    if ([name isEqualToString:PfhorgePrefsModifiedThemeName]) {
        name = _lastSavedThemeName;
    }

    if (name.length == 0) {
        NSBeep();
        return;
    }

    if (PfhorgeBuiltInEditorThemes()[name] == nil &&
        [self userThemes][name] == nil) {
        NSBeep();
        return;
    }

    [self applyThemeNamed:name];

    [[NSUserDefaults standardUserDefaults]
        setObject:name
           forKey:PfhorgePrefsSelectedThemePreference];

    [self reloadThemeMenu];
    [_themePopup selectItemWithTitle:name];
    [self reloadColorWells];
    [self postPreferenceChange];
}


- (void)updateThemeButtons
{
    NSString *name =
        _themePopup.titleOfSelectedItem;

    BOOL isBuiltIn =
        name.length > 0 &&
        PfhorgeBuiltInEditorThemes()[name] != nil;

    BOOL isModified =
        [name isEqualToString:PfhorgePrefsModifiedThemeName];

    _deleteThemeButton.enabled =
        name.length > 0 &&
        !isBuiltIn &&
        !isModified;
}


- (NSView *)buildObjectsPane
{
    NSView *container =
        [[NSView alloc] initWithFrame:NSZeroRect];

    NSStackView *stack =
        [[NSStackView alloc] initWithFrame:NSZeroRect];

    stack.orientation =
        NSUserInterfaceLayoutOrientationVertical;
    stack.alignment = NSLayoutAttributeLeading;
    stack.spacing = 16.0;
    stack.translatesAutoresizingMaskIntoConstraints = NO;

    [container addSubview:stack];

    [NSLayoutConstraint activateConstraints:@[
        [stack.leadingAnchor
            constraintEqualToAnchor:container.leadingAnchor
                           constant:28.0],
        [stack.trailingAnchor
            constraintEqualToAnchor:container.trailingAnchor
                           constant:-28.0],
        [stack.topAnchor
            constraintEqualToAnchor:container.topAnchor
                           constant:26.0],
    ]];

    [stack addArrangedSubview:
        PfhorgePrefsLabel(
            @"Objects",
            26.0,
            NSFontWeightBold,
            NSColor.labelColor)];

    [stack addArrangedSubview:
        PfhorgePrefsLabel(
            @"Choose which Marathon objects are visible in the 2D editor.",
            12.5,
            NSFontWeightRegular,
            NSColor.secondaryLabelColor)];

    NSArray<NSString *> *titles = @[
        @"Show items",
        @"Show players",
        @"Show enemy monsters",
        @"Show scenery",
        @"Show sounds",
        @"Show goals",
    ];

    NSMutableArray<NSView *> *rows =
        [NSMutableArray array];

    for (NSUInteger index = 0;
         index < titles.count;
         ++index) {
        NSButton *button =
            PfhorgePrefsCheckbox(
                titles[index],
                self,
                @selector(objectVisibilityChanged:));

        button.tag = (NSInteger)index;
        [_objectVisibilityButtons addObject:button];
        [rows addObject:button];
    }

    NSView *card =
        PfhorgePrefsCard(
            @"Map Object Visibility",
            @"Object display changes are global and apply to open maps immediately.",
            rows);

    [stack addArrangedSubview:card];
    [card.widthAnchor
        constraintEqualToAnchor:stack.widthAnchor].active = YES;

    NSView *note =
        PfhorgePrefsCard(
            @"Object Colors",
            @"Object fill and outline colors now live in Colors & Themes so complete palettes can be saved and restored together.",
            @[]);

    [stack addArrangedSubview:note];
    [note.widthAnchor
        constraintEqualToAnchor:stack.widthAnchor].active = YES;

    [self reloadObjectVisibilityControls];

    return container;
}


- (void)reloadObjectVisibilityControls
{
    NSUserDefaults *defaults =
        [NSUserDefaults standardUserDefaults];

    for (NSUInteger index = 0;
         index < _objectVisibilityButtons.count &&
         index < _objectVisibilityKeys.count;
         ++index) {
        _objectVisibilityButtons[index].state =
            [defaults boolForKey:_objectVisibilityKeys[index]]
            ? NSControlStateValueOn
            : NSControlStateValueOff;
    }
}


- (void)objectVisibilityChanged:(id)sender
{
    NSButton *button = (NSButton *)sender;
    NSInteger index = button.tag;

    if (index < 0 ||
        (NSUInteger)index >= _objectVisibilityKeys.count) {
        return;
    }

    [[NSUserDefaults standardUserDefaults]
        setBool:(button.state == NSControlStateValueOn)
         forKey:_objectVisibilityKeys[(NSUInteger)index]];

    [self postPreferenceChange];
}


- (NSView *)buildVisualModePane
{
    NSView *container =
        [[NSView alloc] initWithFrame:NSZeroRect];

    NSTextField *placeholder =
        PfhorgePrefsLabel(
            @"Loading Visual Mode & GPU settings…",
            13.0,
            NSFontWeightRegular,
            NSColor.secondaryLabelColor);

    placeholder.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:placeholder];

    [NSLayoutConstraint activateConstraints:@[
        [placeholder.centerXAnchor
            constraintEqualToAnchor:container.centerXAnchor],
        [placeholder.centerYAnchor
            constraintEqualToAnchor:container.centerYAnchor],
    ]];

    dispatch_async(dispatch_get_main_queue(), ^{
        [self installVisualModeSettingsIntoContainer:container
                                         placeholder:placeholder];
    });

    return container;
}


- (void)installVisualModeSettingsIntoContainer:(NSView *)container
                                    placeholder:(NSView *)placeholder
{
    if (_embeddedVisualModeController != nil) {
        return;
    }

    Class visualClass =
        NSClassFromString(@"PfhorgeVisualModeSettingsController");

    if (visualClass == Nil ||
        ![visualClass respondsToSelector:@selector(sharedController)]) {
        ((NSTextField *)placeholder).stringValue =
            @"Visual Mode settings controller is unavailable.";
        return;
    }

    id controller =
        ((id (*)(id, SEL))objc_msgSend)(
            visualClass,
            @selector(sharedController));

    if (controller == nil) {
        ((NSTextField *)placeholder).stringValue =
            @"Visual Mode settings controller could not be created.";
        return;
    }

    _embeddedVisualModeController = controller;

    if ([controller respondsToSelector:@selector(loadSettings)]) {
        ((void (*)(id, SEL))objc_msgSend)(
            controller,
            @selector(loadSettings));
    }

    NSWindow *sourceWindow =
        ((NSWindow *(*)(id, SEL))objc_msgSend)(
            controller,
            @selector(window));

    NSView *sourceView = sourceWindow.contentView;

    if (sourceView == nil) {
        ((NSTextField *)placeholder).stringValue =
            @"Visual Mode settings view is unavailable.";
        return;
    }

    sourceWindow.contentView =
        [[NSView alloc] initWithFrame:sourceView.bounds];

    [placeholder removeFromSuperview];

    sourceView.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:sourceView];

    [NSLayoutConstraint activateConstraints:@[
        [sourceView.centerXAnchor
            constraintEqualToAnchor:container.centerXAnchor],
        [sourceView.centerYAnchor
            constraintEqualToAnchor:container.centerYAnchor],
        [sourceView.widthAnchor constraintEqualToConstant:700.0],
        [sourceView.heightAnchor constraintEqualToConstant:550.0],
    ]];
}


- (void)reloadEmbeddedVisualModeSettings
{
    if (_embeddedVisualModeController != nil &&
        [_embeddedVisualModeController
            respondsToSelector:@selector(loadSettings)]) {
        ((void (*)(id, SEL))objc_msgSend)(
            _embeddedVisualModeController,
            @selector(loadSettings));
    }
}


- (void)postPreferenceChange
{
    [[NSNotificationCenter defaultCenter]
        postNotificationName:
            PhUserDidChangePreferencesNotification
                      object:nil];
}

@end
