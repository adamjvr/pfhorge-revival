// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth

#pragma once

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#include <stdint.h>

NS_ASSUME_NONNULL_BEGIN

#define PfhorgeVisualModeSettingsDidChangeNotification \
    @"PfhorgeVisualModeSettingsDidChangeNotification"

#define PfhorgeVMForwardKeyPreference @"PfhorgeVMForwardKey"
#define PfhorgeVMBackwardKeyPreference @"PfhorgeVMBackwardKey"
#define PfhorgeVMStrafeLeftKeyPreference @"PfhorgeVMStrafeLeftKey"
#define PfhorgeVMStrafeRightKeyPreference @"PfhorgeVMStrafeRightKey"
#define PfhorgeVMFlyDownKeyPreference @"PfhorgeVMFlyDownKey"
#define PfhorgeVMFlyUpKeyPreference @"PfhorgeVMFlyUpKey"
#define PfhorgeVMResetKeyPreference @"PfhorgeVMResetKey"
#define PfhorgeVMOrbitKeyPreference @"PfhorgeVMOrbitKey"
#define PfhorgeVMDiagnosticsKeyPreference @"PfhorgeVMDiagnosticsKey"

// CONTENT-1A.1 keeps the old single sensitivity key as a migration source.
#define PfhorgeVMMouseSensitivityPreference @"PfhorgeVMMouseSensitivity"
#define PfhorgeVMMouseSensitivityXPreference @"PfhorgeVMMouseSensitivityX"
#define PfhorgeVMMouseSensitivityYPreference @"PfhorgeVMMouseSensitivityY"
#define PfhorgeVMInvertMouseXPreference @"PfhorgeVMInvertMouseX"
#define PfhorgeVMInvertMouseYPreference @"PfhorgeVMInvertMouseY"
#define PfhorgeVMLookSmoothingPreference @"PfhorgeVMLookSmoothing"
#define PfhorgeVMMovementSpeedPreference @"PfhorgeVMMovementSpeed"
#define PfhorgeVMVerticalMovementScalePreference @"PfhorgeVMVerticalMovementScale"
#define PfhorgeVMFieldOfViewPreference @"PfhorgeVMFieldOfViewDegrees"
#define PfhorgeVMNearPlanePreference @"PfhorgeVMNearPlane"
#define PfhorgeVMFrameRatePreference @"PfhorgeVMFrameRate"
#define PfhorgeVMFrameRateDisplayMaximum 0
#define PfhorgeVMVSyncPreference @"PfhorgeVMVSync"
#define PfhorgeVMRenderScalePreference @"PfhorgeVMRenderScale"
#define PfhorgeVMMSAASampleCountPreference @"PfhorgeVMMSAASampleCount"
#define PfhorgeVMTextureFilteringPreference @"PfhorgeVMTextureFiltering"
#define PfhorgeVMAnisotropyPreference @"PfhorgeVMAnisotropy"
#define PfhorgeVMPreferredMetalRegistryIDPreference \
    @"PfhorgeVMPreferredMetalRegistryID"
#define PfhorgeVMDiagnosticsOverlayPreference \
    @"PfhorgeVMDiagnosticsOverlay"
#define PfhorgeVMUntexturedDiagnosticPreference \
    @"PfhorgeVMUntexturedDiagnostic"
#define PfhorgeVMSettingsMigrationVersionPreference \
    @"PfhorgeVMSettingsMigrationVersion"

static inline NSArray<id<MTLDevice>> *PfhorgeAvailableMetalDevices(void)
{
    if (@available(macOS 10.13, *)) {
        return MTLCopyAllDevices();
    }
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    return device != nil ? @[device] : @[];
}

static inline NSDictionary<NSString *, NSNumber *> *
PfhorgeVisualModeDefaultValues(void)
{
    return @{
        PfhorgeVMForwardKeyPreference: @((NSInteger)'w'),
        PfhorgeVMBackwardKeyPreference: @((NSInteger)'s'),
        PfhorgeVMStrafeLeftKeyPreference: @((NSInteger)'a'),
        PfhorgeVMStrafeRightKeyPreference: @((NSInteger)'d'),
        PfhorgeVMFlyDownKeyPreference: @((NSInteger)'q'),
        PfhorgeVMFlyUpKeyPreference: @((NSInteger)'e'),
        PfhorgeVMResetKeyPreference: @((NSInteger)'r'),
        PfhorgeVMOrbitKeyPreference: @((NSInteger)'p'),
        PfhorgeVMDiagnosticsKeyPreference: @((NSInteger)'i'),
        PfhorgeVMMouseSensitivityPreference: @(0.010),
        PfhorgeVMMouseSensitivityXPreference: @(0.010),
        PfhorgeVMMouseSensitivityYPreference: @(0.010),
        PfhorgeVMInvertMouseXPreference: @NO,
        PfhorgeVMInvertMouseYPreference: @NO,
        PfhorgeVMLookSmoothingPreference: @(0.0),
        PfhorgeVMMovementSpeedPreference: @(3.5),
        PfhorgeVMVerticalMovementScalePreference: @(1.0),
        PfhorgeVMFieldOfViewPreference: @(60.0),
        PfhorgeVMNearPlanePreference: @(0.01),
        PfhorgeVMFrameRatePreference: @(PfhorgeVMFrameRateDisplayMaximum),
        PfhorgeVMVSyncPreference: @YES,
        PfhorgeVMRenderScalePreference: @(1.0),
        PfhorgeVMMSAASampleCountPreference: @(1),
        PfhorgeVMTextureFilteringPreference: @(1),
        PfhorgeVMAnisotropyPreference: @(4),
        PfhorgeVMPreferredMetalRegistryIDPreference: @(0ULL),
        PfhorgeVMDiagnosticsOverlayPreference: @NO,
        PfhorgeVMUntexturedDiagnosticPreference: @NO,
    };
}

static inline void PfhorgeRegisterVisualModeDefaults(void)
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults registerDefaults:PfhorgeVisualModeDefaultValues()];

    NSString *bundleIdentifier = NSBundle.mainBundle.bundleIdentifier;
    NSDictionary *persistent = bundleIdentifier.length > 0
        ? [defaults persistentDomainForName:bundleIdentifier]
        : nil;

    const NSInteger migrationVersion =
        [persistent[PfhorgeVMSettingsMigrationVersionPreference] integerValue];
    if (migrationVersion >= 2) {
        return;
    }

    // Preserve genuinely user-selected legacy bindings when present, but do
    // not overwrite the modern WASD defaults merely because the old
    // controller registered its own defaults.
    NSDictionary<NSString *, NSString *> *legacyMap = @{
        PfhorgeVMForwardKeyPreference: @"VMForwardKey",
        PfhorgeVMBackwardKeyPreference: @"VMBackwardKey",
        PfhorgeVMStrafeLeftKeyPreference: @"VMSlideLeftKey",
        PfhorgeVMStrafeRightKeyPreference: @"VMSlideRightKey",
        PfhorgeVMFlyDownKeyPreference: @"VMDownKey",
        PfhorgeVMFlyUpKeyPreference: @"VMUpKey",
    };

    for (NSString *modernKey in legacyMap) {
        if (persistent[modernKey] != nil) {
            continue;
        }
        NSString *legacyKey = legacyMap[modernKey];
        NSNumber *legacyValue = persistent[legacyKey];
        if ([legacyValue isKindOfClass:NSNumber.class]) {
            [defaults setInteger:legacyValue.integerValue forKey:modernKey];
        }
    }

    if (persistent[PfhorgeVMMouseSensitivityPreference] == nil &&
        [persistent[@"VMMouseSpeed"] isKindOfClass:NSNumber.class]) {
        const double legacy = [persistent[@"VMMouseSpeed"] doubleValue];
        [defaults setDouble:MAX(0.002, MIN(0.030, legacy * 0.010))
                      forKey:PfhorgeVMMouseSensitivityPreference];
    }

    if (persistent[PfhorgeVMInvertMouseYPreference] == nil &&
        [persistent[@"VMInvertMouse"] isKindOfClass:NSNumber.class]) {
        [defaults setBool:[persistent[@"VMInvertMouse"] boolValue]
                   forKey:PfhorgeVMInvertMouseYPreference];
    }

    // Version 2 splits horizontal and vertical look settings. Preserve the
    // phase-1 value instead of unexpectedly changing an existing setup.
    NSNumber *oldSensitivity = persistent[PfhorgeVMMouseSensitivityPreference];
    const double migratedSensitivity = [oldSensitivity isKindOfClass:NSNumber.class]
        ? MAX(0.0005, MIN(0.10, oldSensitivity.doubleValue))
        : [defaults doubleForKey:PfhorgeVMMouseSensitivityPreference];
    if (persistent[PfhorgeVMMouseSensitivityXPreference] == nil) {
        [defaults setDouble:migratedSensitivity
                     forKey:PfhorgeVMMouseSensitivityXPreference];
    }
    if (persistent[PfhorgeVMMouseSensitivityYPreference] == nil) {
        [defaults setDouble:migratedSensitivity
                     forKey:PfhorgeVMMouseSensitivityYPreference];
    }
    [defaults setInteger:2 forKey:PfhorgeVMSettingsMigrationVersionPreference];
}


static inline NSInteger PfhorgeVisualModeMaximumFrameRateForScreen(
    NSScreen * _Nullable screen)
{
    NSInteger maximum = 60;
    if (@available(macOS 12.0, *)) {
        if (screen != nil) maximum = MAX(1, screen.maximumFramesPerSecond);
    }
    return maximum;
}

static inline NSInteger PfhorgeResolvedVisualModeFrameRate(
    NSScreen * _Nullable screen)
{
    NSInteger requested = [NSUserDefaults.standardUserDefaults
        integerForKey:PfhorgeVMFrameRatePreference];
    const NSInteger maximum = PfhorgeVisualModeMaximumFrameRateForScreen(screen);
    if (requested == PfhorgeVMFrameRateDisplayMaximum) return maximum;
    return MAX(1, MIN(requested, maximum));
}

static inline unichar PfhorgeVisualModeKey(
    NSString *preferenceKey,
    unichar fallback)
{
    NSInteger value = [[NSUserDefaults standardUserDefaults]
        integerForKey:preferenceKey];
    if (value <= 0 || value > UINT16_MAX) {
        return fallback;
    }
    return (unichar)value;
}

static inline NSString *PfhorgeVisualModeKeyDisplayName(unichar key)
{
    switch (key) {
        case NSUpArrowFunctionKey: return @"Up Arrow";
        case NSDownArrowFunctionKey: return @"Down Arrow";
        case NSLeftArrowFunctionKey: return @"Left Arrow";
        case NSRightArrowFunctionKey: return @"Right Arrow";
        case 0x20: return @"Space";
        case 0x09: return @"Tab";
        case 0x0d: return @"Return";
        case 0x1b: return @"Escape";
        case 0x7f: return @"Delete";
        default: break;
    }

    NSString *string = [NSString stringWithCharacters:&key length:1];
    return string.uppercaseString;
}

static inline unichar PfhorgeVisualModeKeyFromString(
    NSString *string,
    unichar fallback)
{
    NSString *trimmed = [string
        stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    if (trimmed.length == 0) {
        return fallback;
    }
    NSString *lower = trimmed.lowercaseString;
    if ([lower isEqualToString:@"up arrow"]) return NSUpArrowFunctionKey;
    if ([lower isEqualToString:@"down arrow"]) return NSDownArrowFunctionKey;
    if ([lower isEqualToString:@"left arrow"]) return NSLeftArrowFunctionKey;
    if ([lower isEqualToString:@"right arrow"]) return NSRightArrowFunctionKey;
    if ([lower isEqualToString:@"space"]) return 0x20;
    if ([lower isEqualToString:@"tab"]) return 0x09;
    if ([lower isEqualToString:@"return"]) return 0x0d;
    if ([lower isEqualToString:@"escape"]) return 0x1b;
    if ([lower isEqualToString:@"delete"]) return 0x7f;
    return [lower characterAtIndex:0];
}

static inline void PfhorgePostVisualModeSettingsChanged(void)
{
    [[NSNotificationCenter defaultCenter]
        postNotificationName:PfhorgeVisualModeSettingsDidChangeNotification
                      object:nil];
}

NS_ASSUME_NONNULL_END
