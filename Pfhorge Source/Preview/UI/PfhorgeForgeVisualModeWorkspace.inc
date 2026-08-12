// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// VM-UI-1A
// Forge-inspired unified Visual Mode workspace.
//
// AppKit owns the window chrome, texture controls and thumbnail browser.
// Metal remains isolated to PfhorgeMetalPreviewView above it.
//
// This pass is intentionally selection-only:
//   * real Shapes textures are browsed through TextureRepository
//   * texture, transfer mode and light state are selectable
//   * no LESide/LEPolygon mutation occurs yet
//
// The selection state is published through a notification so the next pass can
// wire 3D surface picking to map mutation without rebuilding this UI.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/message.h>
#import <stdint.h>

static NSString *const PfhorgeVMForgeWorkspaceFrameName =
    @"PfhorgeMetalVisualModeWorkspace";

static NSString *const PfhorgeVMForgePaletteSelectionDidChangeNotification =
    @"PfhorgeVMForgePaletteSelectionDidChangeNotification";

static NSString *const PfhorgeVMForgePaletteCollectionKey = @"collection";
static NSString *const PfhorgeVMForgePaletteBitmapKey = @"bitmap";
static NSString *const PfhorgeVMForgePaletteTransferModeKey = @"transferMode";
static NSString *const PfhorgeVMForgePaletteLightKey = @"light";
static NSString *const PfhorgeVMForgePaletteApplyTexturesKey = @"applyTextures";
static NSString *const PfhorgeVMForgePaletteApplyLightsKey = @"applyLights";

static const CGFloat PfhorgeVMForgePaletteHeight = 220.0;
static const CGFloat PfhorgeVMForgeSeparatorHeight = 1.0;

typedef id (*PfhorgeVMObjectNoArgumentMessage)(id, SEL);
typedef id (*PfhorgeVMObjectInt32Message)(id, SEL, int32_t);

static NSTextField *PfhorgeVMForgeLabel(
    NSRect frame,
    NSString *text,
    CGFloat size,
    NSFontWeight weight)
{
    NSTextField *label = [[NSTextField alloc] initWithFrame:frame];
    label.stringValue = text ?: @"";
    label.editable = NO;
    label.selectable = NO;
    label.bezeled = NO;
    label.drawsBackground = NO;
    label.font = [NSFont systemFontOfSize:size weight:weight];
    label.textColor = NSColor.labelColor;
    label.usesSingleLineMode = YES;
    return label;
}

static NSString *PfhorgeVMForgeCollectionName(int32_t collection)
{
    switch (collection) {
        case 0: return @"Water";
        case 1: return @"Lava";
        case 2: return @"Sewage";
        case 3: return @"Jjaro";
        case 4: return @"Pfhor";
        case 10: return @"Landscape 1";
        case 11: return @"Landscape 2";
        case 12: return @"Landscape 3";
        case 13: return @"Landscape 4";
        default:
            return [NSString stringWithFormat:@"Collection %d", collection];
    }
}

static NSArray<NSDictionary *> *PfhorgeVMForgeCollectionDescriptors(void)
{
    static NSArray<NSDictionary *> *descriptors = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        descriptors = @[
            @{@"name": @"Water",       @"collection": @0},
            @{@"name": @"Lava",        @"collection": @1},
            @{@"name": @"Sewage",      @"collection": @2},
            @{@"name": @"Jjaro",       @"collection": @3},
            @{@"name": @"Pfhor",       @"collection": @4},
            @{@"name": @"Landscape 1", @"collection": @10},
            @{@"name": @"Landscape 2", @"collection": @11},
            @{@"name": @"Landscape 3", @"collection": @12},
            @{@"name": @"Landscape 4", @"collection": @13},
        ];
    });
    return descriptors;
}

static NSArray<NSDictionary *> *PfhorgeVMForgeTransferModeDescriptors(void)
{
    /*
     * Names and numeric values come directly from PhDefs.h.
     * This palette exposes the transfer modes explicitly marked by the
     * historical source as valid for walls.
     */
    static NSArray<NSDictionary *> *descriptors = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        descriptors = @[
            @{@"name": @"Normal",                @"mode": @0},
            @{@"name": @"Pulsate",               @"mode": @4},
            @{@"name": @"Wobble",                @"mode": @5},
            @{@"name": @"Fast Wobble",           @"mode": @6},
            @{@"name": @"Landscape",             @"mode": @9},
            @{@"name": @"Horizontal Slide",      @"mode": @15},
            @{@"name": @"Fast Horizontal Slide", @"mode": @16},
            @{@"name": @"Vertical Slide",        @"mode": @17},
            @{@"name": @"Fast Vertical Slide",   @"mode": @18},
            @{@"name": @"Wander",                @"mode": @19},
            @{@"name": @"Fast Wander",           @"mode": @20},
        ];
    });
    return descriptors;
}

static NSArray<NSImage *> *PfhorgeVMForgeTexturesForCollection(
    int32_t collection)
{
    Class repositoryClass = NSClassFromString(@"TextureRepository");
    if (repositoryClass == Nil) {
        repositoryClass = NSClassFromString(@"Pfhorge.TextureRepository");
    }

    SEL sharedSelector = NSSelectorFromString(@"sharedTextureRepository");
    SEL collectionSelector = NSSelectorFromString(@"textureCollection:");

    if (repositoryClass == Nil ||
        ![repositoryClass respondsToSelector:sharedSelector]) {
        return @[];
    }

    id repository =
        ((PfhorgeVMObjectNoArgumentMessage)objc_msgSend)(
            repositoryClass,
            sharedSelector);

    if (repository == nil ||
        ![repository respondsToSelector:collectionSelector]) {
        return @[];
    }

    id result =
        ((PfhorgeVMObjectInt32Message)objc_msgSend)(
            repository,
            collectionSelector,
            collection);

    if (![result isKindOfClass:NSArray.class]) {
        return @[];
    }

    return (NSArray<NSImage *> *)result;
}

static void PfhorgeConfigureMetalVisualModeWindow(NSWindow *window)
{
    if (window == nil) {
        return;
    }

    window.contentMinSize = NSMakeSize(640.0, 430.0);

    /*
     * Restore the user's last workspace first. On first launch, deliberately
     * place Visual Mode at the upper-left of the usable display like the
     * classic Forge workflow, with enough vertical space for the attached
     * texture palette.
     */
    BOOL restored =
        [window setFrameUsingName:PfhorgeVMForgeWorkspaceFrameName];

    if (!restored) {
        [window setContentSize:NSMakeSize(640.0, 560.0)];

        NSScreen *screen = window.screen ?: NSScreen.mainScreen;
        if (screen != nil) {
            NSRect visible = screen.visibleFrame;
            [window setFrameTopLeftPoint:
                NSMakePoint(
                    NSMinX(visible) + 18.0,
                    NSMaxY(visible) - 18.0)];
        }
    }

    [window setFrameAutosaveName:PfhorgeVMForgeWorkspaceFrameName];
}


@interface PfhorgeForgeTextureCollectionItem : NSCollectionViewItem
- (void)configureWithImage:(NSImage *)image
                    bitmap:(NSInteger)bitmap
                  selected:(BOOL)selected;
@end

@implementation PfhorgeForgeTextureCollectionItem {
    NSImageView *_previewImageView;
    NSTextField *_bitmapLabel;
}

- (void)loadView
{
    NSView *view =
        [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 62.0, 72.0)];

    view.wantsLayer = YES;
    view.layer.cornerRadius = 3.0;
    view.layer.borderWidth = 1.0;
    view.layer.borderColor =
        [NSColor.separatorColor colorWithAlphaComponent:0.85].CGColor;
    view.layer.backgroundColor =
        [NSColor.controlBackgroundColor colorWithAlphaComponent:0.72].CGColor;

    _previewImageView =
        [[NSImageView alloc] initWithFrame:NSMakeRect(5.0, 15.0, 52.0, 52.0)];

    _previewImageView.imageScaling = NSImageScaleAxesIndependently;
    _previewImageView.imageAlignment = NSImageAlignCenter;
    _previewImageView.wantsLayer = YES;
    _previewImageView.layer.magnificationFilter = kCAFilterNearest;
    _previewImageView.layer.minificationFilter = kCAFilterLinear;

    _bitmapLabel =
        PfhorgeVMForgeLabel(
            NSMakeRect(3.0, 1.0, 56.0, 13.0),
            @"",
            9.0,
            NSFontWeightRegular);

    _bitmapLabel.alignment = NSTextAlignmentCenter;
    _bitmapLabel.textColor = NSColor.secondaryLabelColor;

    [view addSubview:_previewImageView];
    [view addSubview:_bitmapLabel];

    self.view = view;
}

- (void)configureWithImage:(NSImage *)image
                    bitmap:(NSInteger)bitmap
                  selected:(BOOL)selected
{
    if (self.view == nil) {
        [self loadView];
    }

    _previewImageView.image = image;
    _bitmapLabel.stringValue =
        [NSString stringWithFormat:@"%ld", (long)bitmap];

    self.view.layer.borderWidth = selected ? 3.0 : 1.0;
    self.view.layer.borderColor =
        selected
        ? NSColor.keyboardFocusIndicatorColor.CGColor
        : [NSColor.separatorColor colorWithAlphaComponent:0.85].CGColor;

    self.view.layer.backgroundColor =
        selected
        ? [NSColor.selectedControlColor colorWithAlphaComponent:0.18].CGColor
        : [NSColor.controlBackgroundColor colorWithAlphaComponent:0.72].CGColor;
}

@end


@interface PfhorgeForgeTexturePaletteView :
    NSView
    <NSCollectionViewDataSource,
     NSCollectionViewDelegate,
     NSCollectionViewDelegateFlowLayout>

- (instancetype)initWithFrame:(NSRect)frame
                    levelData:(LELevelData *)levelData;

@property(nonatomic, readonly) int32_t selectedCollection;
@property(nonatomic, readonly) NSInteger selectedBitmap;
@property(nonatomic, readonly) NSInteger selectedTransferMode;
@property(nonatomic, readonly) NSInteger selectedLight;
@property(nonatomic, readonly) BOOL applyTextures;
@property(nonatomic, readonly) BOOL applyLights;

@end


@implementation PfhorgeForgeTexturePaletteView {
    __weak LELevelData *_levelData;

    NSPopUpButton *_textureModePopup;
    NSPopUpButton *_lightPopup;
    NSPopUpButton *_collectionPopup;
    NSButton *_applyTexturesCheckbox;
    NSButton *_applyLightsCheckbox;
    NSTextField *_statusLabel;

    NSScrollView *_textureScrollView;
    NSCollectionView *_textureCollectionView;
    NSArray<NSImage *> *_textureImages;

    int32_t _selectedCollection;
    NSInteger _selectedBitmap;
}

static NSString *const PfhorgeForgeTextureItemIdentifier =
    @"PfhorgeForgeTextureItem";

- (instancetype)initWithFrame:(NSRect)frame
                    levelData:(LELevelData *)levelData
{
    self = [super initWithFrame:frame];
    if (self == nil) {
        return nil;
    }

    _levelData = levelData;
    _selectedBitmap = NSNotFound;

    self.autoresizingMask =
        NSViewWidthSizable | NSViewMinYMargin;
    self.wantsLayer = YES;
    self.layer.backgroundColor =
        NSColor.windowBackgroundColor.CGColor;

    [self buildInterface];
    [self configureInitialCollection];
    [self reloadLights];
    [self reloadTextures];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(contentSelectionChanged:)
               name:@"PfhorgeContentSelectionDidChangeNotification"
             object:nil];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(levelEnvironmentChanged:)
               name:@"PfhorgeLevelEnvironmentDidChangeNotification"
             object:nil];

    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)buildInterface
{
    NSBox *box =
        [[NSBox alloc] initWithFrame:NSInsetRect(self.bounds, 5.0, 4.0)];

    box.title = @"Textures";
    box.titlePosition = NSAtTop;
    box.boxType = NSBoxPrimary;
    box.autoresizingMask =
        NSViewWidthSizable | NSViewHeightSizable;

    [self addSubview:box];

    NSView *content = box.contentView;
    NSRect bounds = content.bounds;

    NSTextField *modeLabel =
        PfhorgeVMForgeLabel(
            NSMakeRect(8.0, bounds.size.height - 31.0, 78.0, 18.0),
            @"Texture Mode:",
            11.0,
            NSFontWeightRegular);

    _textureModePopup =
        [[NSPopUpButton alloc]
            initWithFrame:
                NSMakeRect(86.0, bounds.size.height - 35.0, 130.0, 25.0)
              pullsDown:NO];

    for (NSDictionary *descriptor
         in PfhorgeVMForgeTransferModeDescriptors()) {
        [_textureModePopup addItemWithTitle:descriptor[@"name"]];
        _textureModePopup.lastItem.representedObject = descriptor[@"mode"];
    }

    [_textureModePopup selectItemAtIndex:0];
    _textureModePopup.target = self;
    _textureModePopup.action = @selector(controlStateChanged:);

    NSTextField *lightLabel =
        PfhorgeVMForgeLabel(
            NSMakeRect(225.0, bounds.size.height - 31.0, 36.0, 18.0),
            @"Light:",
            11.0,
            NSFontWeightRegular);

    _lightPopup =
        [[NSPopUpButton alloc]
            initWithFrame:
                NSMakeRect(262.0, bounds.size.height - 35.0, 122.0, 25.0)
              pullsDown:NO];

    _lightPopup.target = self;
    _lightPopup.action = @selector(controlStateChanged:);

    _applyTexturesCheckbox =
        [[NSButton alloc]
            initWithFrame:
                NSMakeRect(395.0, bounds.size.height - 33.0, 118.0, 22.0)];

    _applyTexturesCheckbox.buttonType = NSButtonTypeSwitch;
    _applyTexturesCheckbox.title = @"Apply Textures";
    _applyTexturesCheckbox.state = NSControlStateValueOn;
    _applyTexturesCheckbox.font = [NSFont systemFontOfSize:11.0];
    _applyTexturesCheckbox.target = self;
    _applyTexturesCheckbox.action = @selector(controlStateChanged:);

    _applyLightsCheckbox =
        [[NSButton alloc]
            initWithFrame:
                NSMakeRect(514.0, bounds.size.height - 33.0, 108.0, 22.0)];

    _applyLightsCheckbox.buttonType = NSButtonTypeSwitch;
    _applyLightsCheckbox.title = @"Apply Lights";
    _applyLightsCheckbox.state = NSControlStateValueOn;
    _applyLightsCheckbox.font = [NSFont systemFontOfSize:11.0];
    _applyLightsCheckbox.target = self;
    _applyLightsCheckbox.action = @selector(controlStateChanged:);

    NSTextField *collectionLabel =
        PfhorgeVMForgeLabel(
            NSMakeRect(8.0, bounds.size.height - 59.0, 63.0, 18.0),
            @"Collection:",
            11.0,
            NSFontWeightRegular);

    _collectionPopup =
        [[NSPopUpButton alloc]
            initWithFrame:
                NSMakeRect(72.0, bounds.size.height - 63.0, 144.0, 25.0)
              pullsDown:NO];

    for (NSDictionary *descriptor in PfhorgeVMForgeCollectionDescriptors()) {
        [_collectionPopup addItemWithTitle:descriptor[@"name"]];
        _collectionPopup.lastItem.representedObject =
            descriptor[@"collection"];
    }

    _collectionPopup.target = self;
    _collectionPopup.action = @selector(collectionChanged:);

    _statusLabel =
        PfhorgeVMForgeLabel(
            NSMakeRect(226.0, bounds.size.height - 58.0,
                       MAX(100.0, bounds.size.width - 234.0), 18.0),
            @"Choose a texture",
            10.0,
            NSFontWeightRegular);

    _statusLabel.textColor = NSColor.secondaryLabelColor;
    _statusLabel.autoresizingMask = NSViewWidthSizable;

    NSCollectionViewFlowLayout *layout =
        [[NSCollectionViewFlowLayout alloc] init];

    layout.itemSize = NSMakeSize(62.0, 72.0);
    layout.sectionInset = NSEdgeInsetsMake(6.0, 7.0, 6.0, 7.0);
    layout.minimumInteritemSpacing = 5.0;
    layout.minimumLineSpacing = 5.0;

    _textureCollectionView =
        [[NSCollectionView alloc]
            initWithFrame:NSMakeRect(
                0.0,
                0.0,
                bounds.size.width - 8.0,
                MAX(80.0, bounds.size.height - 75.0))];

    _textureCollectionView.collectionViewLayout = layout;
    _textureCollectionView.dataSource = self;
    _textureCollectionView.delegate = self;
    _textureCollectionView.selectable = YES;
    _textureCollectionView.allowsMultipleSelection = NO;
    _textureCollectionView.wantsLayer = YES;
    _textureCollectionView.layer.backgroundColor =
        [NSColor.textBackgroundColor colorWithAlphaComponent:0.55].CGColor;

    [_textureCollectionView
        registerClass:PfhorgeForgeTextureCollectionItem.class
 forItemWithIdentifier:PfhorgeForgeTextureItemIdentifier];

    _textureScrollView =
        [[NSScrollView alloc]
            initWithFrame:NSMakeRect(
                7.0,
                7.0,
                bounds.size.width - 14.0,
                MAX(82.0, bounds.size.height - 78.0))];

    _textureScrollView.autoresizingMask =
        NSViewWidthSizable | NSViewHeightSizable;
    _textureScrollView.hasVerticalScroller = YES;
    _textureScrollView.hasHorizontalScroller = NO;
    _textureScrollView.autohidesScrollers = YES;
    _textureScrollView.drawsBackground = NO;
    _textureScrollView.documentView = _textureCollectionView;

    [content addSubview:modeLabel];
    [content addSubview:_textureModePopup];
    [content addSubview:lightLabel];
    [content addSubview:_lightPopup];
    [content addSubview:_applyTexturesCheckbox];
    [content addSubview:_applyLightsCheckbox];
    [content addSubview:collectionLabel];
    [content addSubview:_collectionPopup];
    [content addSubview:_statusLabel];
    [content addSubview:_textureScrollView];

    modeLabel.autoresizingMask = NSViewMaxXMargin | NSViewMinYMargin;
    _textureModePopup.autoresizingMask = NSViewMaxXMargin | NSViewMinYMargin;
    lightLabel.autoresizingMask = NSViewMaxXMargin | NSViewMinYMargin;
    _lightPopup.autoresizingMask = NSViewMaxXMargin | NSViewMinYMargin;
    _applyTexturesCheckbox.autoresizingMask =
        NSViewMinXMargin | NSViewMinYMargin;
    _applyLightsCheckbox.autoresizingMask =
        NSViewMinXMargin | NSViewMinYMargin;
    collectionLabel.autoresizingMask =
        NSViewMaxXMargin | NSViewMinYMargin;
    _collectionPopup.autoresizingMask =
        NSViewMaxXMargin | NSViewMinYMargin;
    _statusLabel.autoresizingMask =
        NSViewWidthSizable | NSViewMinYMargin;
}

- (void)configureInitialCollection
{
    int32_t environment = 0;

    if (_levelData != nil &&
        [_levelData respondsToSelector:@selector(environmentCode)]) {
        environment = (int32_t)[_levelData environmentCode];
    }

    if (environment < 0 || environment > 4) {
        environment = 0;
    }

    _selectedCollection = environment;

    for (NSMenuItem *item in _collectionPopup.itemArray) {
        if ([item.representedObject intValue] == environment) {
            [_collectionPopup selectItem:item];
            break;
        }
    }
}

- (void)reloadLights
{
    [_lightPopup removeAllItems];

    NSArray *names = nil;
    if (_levelData != nil &&
        [_levelData respondsToSelector:@selector(getLightNamesCopy)]) {
        names = [_levelData getLightNamesCopy];
    }

    if ([names isKindOfClass:NSArray.class] && names.count > 0) {
        [_lightPopup addItemsWithTitles:names];
    } else {
        [_lightPopup addItemWithTitle:@"0"];
    }

    [_lightPopup selectItemAtIndex:0];
}

- (void)reloadTextures
{
    _textureImages =
        PfhorgeVMForgeTexturesForCollection(_selectedCollection);

    _selectedBitmap = NSNotFound;
    [_textureCollectionView reloadData];

    if (_textureImages.count == 0) {
        _statusLabel.stringValue =
            [NSString stringWithFormat:
                @"%@ — no textures loaded",
                PfhorgeVMForgeCollectionName(_selectedCollection)];
    } else {
        _statusLabel.stringValue =
            [NSString stringWithFormat:
                @"%@ — %lu textures • click a thumbnail to select",
                PfhorgeVMForgeCollectionName(_selectedCollection),
                (unsigned long)_textureImages.count];
    }

    [self publishSelectionState];
}

- (void)collectionChanged:(id)sender
{
    (void)sender;

    NSNumber *collectionNumber =
        _collectionPopup.selectedItem.representedObject;

    if (![collectionNumber isKindOfClass:NSNumber.class]) {
        return;
    }

    _selectedCollection = collectionNumber.intValue;
    [self reloadTextures];
}

- (void)controlStateChanged:(id)sender
{
    (void)sender;
    [self publishSelectionState];
}

- (void)contentSelectionChanged:(NSNotification *)notification
{
    (void)notification;

    dispatch_async(dispatch_get_main_queue(), ^{
        [self reloadTextures];
    });
}

- (void)levelEnvironmentChanged:(NSNotification *)notification
{
    if (notification.object != nil &&
        notification.object != _levelData) {
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        [self configureInitialCollection];
        [self reloadTextures];
    });
}

- (NSInteger)numberOfSectionsInCollectionView:
    (NSCollectionView *)collectionView
{
    (void)collectionView;
    return 1;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
    numberOfItemsInSection:(NSInteger)section
{
    (void)collectionView;
    (void)section;
    return (NSInteger)_textureImages.count;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
    itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    PfhorgeForgeTextureCollectionItem *item =
        (PfhorgeForgeTextureCollectionItem *)
        [collectionView
            makeItemWithIdentifier:PfhorgeForgeTextureItemIdentifier
                      forIndexPath:indexPath];

    NSInteger bitmap = indexPath.item;

    NSImage *image =
        bitmap >= 0 &&
        (NSUInteger)bitmap < _textureImages.count
        ? _textureImages[(NSUInteger)bitmap]
        : nil;

    [item configureWithImage:image
                     bitmap:bitmap
                   selected:(bitmap == _selectedBitmap)];

    return item;
}

- (void)collectionView:(NSCollectionView *)collectionView
    didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSIndexPath *indexPath = indexPaths.anyObject;
    if (indexPath == nil) {
        return;
    }

    _selectedBitmap = indexPath.item;

    _statusLabel.stringValue =
        [NSString stringWithFormat:
            @"%@ • bitmap %ld selected",
            PfhorgeVMForgeCollectionName(_selectedCollection),
            (long)_selectedBitmap];

    /*
     * Reload to refresh the high-contrast selection border. The collection
     * is tiny by modern standards, so this remains cheap and deterministic.
     */
    [collectionView reloadData];
    [self publishSelectionState];
}

- (NSSize)collectionView:(NSCollectionView *)collectionView
    layout:(NSCollectionViewLayout *)collectionViewLayout
    sizeForItemAtIndexPath:(NSIndexPath *)indexPath
{
    (void)collectionView;
    (void)collectionViewLayout;
    (void)indexPath;
    return NSMakeSize(62.0, 72.0);
}

- (NSInteger)selectedTransferMode
{
    NSNumber *number =
        _textureModePopup.selectedItem.representedObject;

    return [number isKindOfClass:NSNumber.class]
        ? number.integerValue
        : 0;
}

- (NSInteger)selectedLight
{
    return MAX(0, _lightPopup.indexOfSelectedItem);
}

- (BOOL)applyTextures
{
    return _applyTexturesCheckbox.state == NSControlStateValueOn;
}

- (BOOL)applyLights
{
    return _applyLightsCheckbox.state == NSControlStateValueOn;
}

- (int32_t)selectedCollection
{
    return _selectedCollection;
}

- (NSInteger)selectedBitmap
{
    return _selectedBitmap;
}

- (void)publishSelectionState
{
    NSInteger bitmapForNotification =
        _selectedBitmap == NSNotFound
        ? -1
        : _selectedBitmap;

    NSDictionary *userInfo = @{
        PfhorgeVMForgePaletteCollectionKey:
            @(_selectedCollection),
        PfhorgeVMForgePaletteBitmapKey:
            @(bitmapForNotification),
        PfhorgeVMForgePaletteTransferModeKey:
            @(self.selectedTransferMode),
        PfhorgeVMForgePaletteLightKey:
            @(self.selectedLight),
        PfhorgeVMForgePaletteApplyTexturesKey:
            @(self.applyTextures),
        PfhorgeVMForgePaletteApplyLightsKey:
            @(self.applyLights),
    };

    [[NSNotificationCenter defaultCenter]
        postNotificationName:
            PfhorgeVMForgePaletteSelectionDidChangeNotification
                      object:self
                    userInfo:userInfo];
}

@end


@interface PfhorgeForgeVisualModeWorkspaceView : NSView

- (instancetype)initWithFrame:(NSRect)frame
                    metalView:(PfhorgeMetalPreviewView *)metalView
                    levelData:(LELevelData *)levelData;

@property(nonatomic, readonly)
    PfhorgeForgeTexturePaletteView *texturePalette;

@end


@implementation PfhorgeForgeVisualModeWorkspaceView {
    PfhorgeMetalPreviewView *_metalView;
    PfhorgeForgeTexturePaletteView *_texturePalette;
    NSBox *_separator;
}

- (instancetype)initWithFrame:(NSRect)frame
                    metalView:(PfhorgeMetalPreviewView *)metalView
                    levelData:(LELevelData *)levelData
{
    self = [super initWithFrame:frame];
    if (self == nil) {
        return nil;
    }

    self.autoresizingMask =
        NSViewWidthSizable | NSViewHeightSizable;
    self.wantsLayer = YES;
    self.layer.backgroundColor =
        NSColor.windowBackgroundColor.CGColor;

    _metalView = metalView;
    _metalView.autoresizingMask =
        NSViewWidthSizable | NSViewHeightSizable;

    _texturePalette =
        [[PfhorgeForgeTexturePaletteView alloc]
            initWithFrame:NSMakeRect(
                0.0,
                0.0,
                frame.size.width,
                PfhorgeVMForgePaletteHeight)
               levelData:levelData];

    _texturePalette.autoresizingMask =
        NSViewWidthSizable | NSViewMaxYMargin;

    _separator = [[NSBox alloc] initWithFrame:NSZeroRect];
    _separator.boxType = NSBoxSeparator;
    _separator.autoresizingMask =
        NSViewWidthSizable | NSViewMaxYMargin;

    [self addSubview:_metalView];
    [self addSubview:_separator];
    [self addSubview:_texturePalette];

    [self layoutWorkspace];

    return self;
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldSize
{
    [super resizeSubviewsWithOldSize:oldSize];
    [self layoutWorkspace];
}

- (void)layoutWorkspace
{
    NSRect bounds = self.bounds;

    CGFloat paletteHeight =
        MIN(
            PfhorgeVMForgePaletteHeight,
            MAX(160.0, bounds.size.height * 0.42));

    CGFloat metalHeight =
        MAX(
            180.0,
            bounds.size.height -
            paletteHeight -
            PfhorgeVMForgeSeparatorHeight);

    /*
     * If the window is made exceptionally short, preserve a usable Metal
     * viewport and let the palette take the remainder.
     */
    paletteHeight =
        MAX(
            120.0,
            bounds.size.height -
            metalHeight -
            PfhorgeVMForgeSeparatorHeight);

    _texturePalette.frame =
        NSMakeRect(
            0.0,
            0.0,
            bounds.size.width,
            paletteHeight);

    _separator.frame =
        NSMakeRect(
            0.0,
            paletteHeight,
            bounds.size.width,
            PfhorgeVMForgeSeparatorHeight);

    _metalView.frame =
        NSMakeRect(
            0.0,
            paletteHeight + PfhorgeVMForgeSeparatorHeight,
            bounds.size.width,
            MAX(
                0.0,
                bounds.size.height -
                paletteHeight -
                PfhorgeVMForgeSeparatorHeight));
}

- (PfhorgeForgeTexturePaletteView *)texturePalette
{
    return _texturePalette;
}

@end
