// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// FORMAT-3B canonical-authority bridge.
//
// This file is intentionally #included by PfhorgeNativeDocumentCodec.inc.
// The FORMAT-1C JSON schemas remain the public canonical contract.  FORMAT-3B
// makes that graph authoritative in real .pfhlev files and keeps only a small,
// optional namespaced fidelity extension for Cocoa-era implementation details
// that are not independent canonical semantics.

#ifndef PFHORGE_FORMAT_3B_CANONICAL_LEVEL_BRIDGE_INC
#define PFHORGE_FORMAT_3B_CANONICAL_LEVEL_BRIDGE_INC 1

#import <AppKit/AppKit.h>
#import "../../Data Objects/LEMapStuffParent.h"
#import "../../Data Objects/PhAbstractName.h"
#import "../../Data Objects/PhAbstractNumber.h"
#import "../../Data Objects/PhLayer.h"
#import "../../Data Objects/PhNoteGroup.h"
#import "../../Data Objects/Marathon Data/LEMapPoint.h"
#import "../../Data Objects/Marathon Data/LELine.h"
#import "../../Data Objects/Marathon Data/LEPolygon.h"
#import "../../Data Objects/Marathon Data/LESide.h"
#import "../../Data Objects/Marathon Data/LEMapObject.h"
#import "../../Data Objects/Marathon Data/PhLight.h"
#import "../../Data Objects/Marathon Data/PhMedia.h"
#import "../../Data Objects/Marathon Data/PhPlatform.h"
#import "../../Data Objects/Marathon Data/PhItemPlacement.h"
#import "../../Data Objects/Marathon Data/PhAmbientSound.h"
#import "../../Data Objects/Marathon Data/PhRandomSound.h"
#import "../../Data Objects/Marathon Data/PhTag.h"
#import "../../Data Objects/Marathon Data/PhAnnotationNote.h"
#import "../../Data Objects/Marathon Data/Terminal.h"
#import "../../Data Objects/Marathon Data/TerminalSection.h"

static NSString * const PFN3BCanonicalAuthorityExtensionId =
    @"org.pfhorge.format3b.canonical-authority";
static const NSInteger PFN3BCanonicalModelRevision = 3;

#pragma mark - Defensive KVC helpers

static id PFN3BValue(id object, NSString *key)
{
    if (!object || !key.length) return nil;
    @try { return [object valueForKey:key]; }
    @catch (__unused NSException *exception) { return nil; }
}

static void PFN3BSetValue(id object, NSString *key, id value)
{
    if (!object || !key.length || !value || value == NSNull.null) return;
    @try { [object setValue:value forKey:key]; }
    @catch (__unused NSException *exception) { }
}

static void PFN3BSetNil(id object, NSString *key)
{
    if (!object || !key.length) return;
    @try { [object setValue:nil forKey:key]; }
    @catch (__unused NSException *exception) { }
}

static NSString *PFN3BName(id object)
{
    if (![object respondsToSelector:@selector(phName)]) return nil;
    id value = PFN3BValue(object, @"phName");
    return ([value isKindOfClass:NSString.class] && [value length]) ? value : nil;
}

static id PFN3BRef(id object)
{
    NSString *identifier = PFN2AId(object);
    return identifier ?: NSNull.null;
}

static NSString *PFN3BStringRef(id value)
{
    return [value isKindOfClass:NSString.class] ? value : nil;
}

static id PFN3BObjectForRef(NSDictionary<NSString *, id> *objects, id ref)
{
    NSString *identifier = PFN3BStringRef(ref);
    return identifier ? objects[identifier] : nil;
}

#pragma mark - Canonical scalar helpers

static NSDictionary *PFN3BColorJSON(NSColor *color)
{
    if (!color) return nil;
    NSColor *rgb = [color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
    if (!rgb) return nil;
    return @{
        @"space": @"srgb",
        @"components": @[@(rgb.redComponent), @(rgb.greenComponent),
                         @(rgb.blueComponent), @(rgb.alphaComponent)]
    };
}

static NSColor *PFN3BColorFromJSON(id value)
{
    if (![value isKindOfClass:NSDictionary.class] ||
        ![value[@"space"] isEqual:@"srgb"] ||
        ![value[@"components"] isKindOfClass:NSArray.class] ||
        [value[@"components"] count] != 4) return nil;
    NSArray *c = value[@"components"];
    return [NSColor colorWithSRGBRed:[c[0] doubleValue]
                               green:[c[1] doubleValue]
                                blue:[c[2] doubleValue]
                               alpha:[c[3] doubleValue]];
}

static NSDictionary *PFN3BTextureRef(short descriptor)
{
    if (descriptor < 0) return nil;
    uint16_t bits = (uint16_t)descriptor;
    return @{
        @"collection": @((bits >> 8U) & 0x1FU),
        @"bitmap": @(bits & 0xFFU)
    };
}

static short PFN3BTextureDescriptor(id texture)
{
    if (![texture isKindOfClass:NSDictionary.class]) return -1;
    NSInteger collection = [texture[@"collection"] integerValue];
    NSInteger bitmap = [texture[@"bitmap"] integerValue];
    if (collection < 0 || collection > 31 || bitmap < 0 || bitmap > 255) return -1;
    return (short)(((collection & 0x1F) << 8) | (bitmap & 0xFF));
}

static NSDictionary *PFN3BPoint2(NSPoint p)
{
    return @{ @"x": @((NSInteger)p.x), @"y": @((NSInteger)p.y) };
}

static NSPoint PFN3BPoint2FromJSON(id value)
{
    if (![value isKindOfClass:NSDictionary.class]) return NSZeroPoint;
    return NSMakePoint([value[@"x"] doubleValue], [value[@"y"] doubleValue]);
}

static NSDictionary *PFN3BSurfaceJSON(side_texture_definition texture,
                                       short transferMode,
                                       PhLight *light)
{
    id textureJSON = PFN3BTextureRef(texture.texture) ?: NSNull.null;
    return @{
        @"texture": textureJSON,
        @"offset": @{ @"x": @(texture.x0), @"y": @(texture.y0) },
        @"transferMode": @(transferMode),
        @"light": PFN3BRef(light)
    };
}

static id PFN3BOptionalSurfaceJSON(side_texture_definition texture,
                                    short transferMode,
                                    PhLight *light)
{
    if (texture.texture < 0) return NSNull.null;
    return PFN3BSurfaceJSON(texture, transferMode, light);
}

static side_texture_definition PFN3BSurfaceFromJSON(NSDictionary *json)
{
    side_texture_definition result = {0};
    short descriptor = PFN3BTextureDescriptor(json[@"texture"]);
    result.texture = descriptor;
    NSDictionary *offset = [json[@"offset"] isKindOfClass:NSDictionary.class] ? json[@"offset"] : @{};
    result.x0 = [offset[@"x"] shortValue];
    result.y0 = [offset[@"y"] shortValue];
    NSDictionary *texture = [json[@"texture"] isKindOfClass:NSDictionary.class] ? json[@"texture"] : nil;
    result.textureCollection = texture ? [texture[@"collection"] shortValue] : -1;
    result.textureNumber = texture ? [texture[@"bitmap"] shortValue] : -1;
    return result;
}

static NSDictionary *PFN3BPlaneJSON(short texture,
                                     NSPoint origin,
                                     short transferMode,
                                     PhLight *light,
                                     short height)
{
    return @{
        @"height": @(height),
        @"texture": PFN3BTextureRef(texture) ?: NSNull.null,
        @"transferMode": @(transferMode),
        @"origin": PFN3BPoint2(origin),
        @"light": PFN3BRef(light)
    };
}

static NSString *PFN3BSideTypeString(LESideType type)
{
    switch (type) {
        case LESideFull: return @"full";
        case LESideHigh: return @"high";
        case LESideLow: return @"low";
        case LESideComposite: return @"composite";
        case LESideSplit: return @"split";
    }
    return @"full";
}

static LESideType PFN3BSideTypeFromString(id value)
{
    if ([value isEqual:@"high"]) return LESideHigh;
    if ([value isEqual:@"low"]) return LESideLow;
    if ([value isEqual:@"composite"]) return LESideComposite;
    if ([value isEqual:@"split"]) return LESideSplit;
    return LESideFull;
}

static NSString *PFN3BTargetKindForObject(id object)
{
    if ([object isKindOfClass:LEPolygon.class]) return @"polygon";
    if ([object isKindOfClass:PhLight.class]) return @"light";
    if ([object isKindOfClass:PhPlatform.class]) return @"platform";
    if ([object isKindOfClass:Terminal.class]) return @"terminal";
    if ([object isKindOfClass:LEMapObject.class]) return @"object";
    return nil;
}

static NSDictionary *PFN3BSemanticTarget(id object, NSInteger value, NSString *valueKind)
{
    NSString *objectKind = PFN3BTargetKindForObject(object);
    if (objectKind) {
        return @{
            @"kind": objectKind,
            @"target": PFN3BRef(object),
            @"value": @(value)
        };
    }
    return @{ @"kind": valueKind ?: @"integer", @"value": @(value) };
}

static id PFN3BTargetObject(NSDictionary *target, NSDictionary<NSString *, id> *objects)
{
    return PFN3BObjectForRef(objects, target[@"target"]);
}

static short PFN3BTargetValue(NSDictionary *target)
{
    return [target[@"value"] shortValue];
}

#pragma mark - UUID assignment and editor names

// Legacy array-index accessors in the active map objects are pointer-derived.
// For example polygon vertex/line indexes and line owner indexes resolve their
// current object pointers against the level arrays.  Do not call the abstract
// LEMapStuffParent -updateIndexesNumbersFromObjects hook here: several concrete
// classes intentionally inherit the superclass implementation, which only logs
// an "abstract method" diagnostic.  Canonical UUID references stay the sole
// persisted identity; setUpArrayPointersForEveryObject plus the relationships
// materialized below provide the legacy Cocoa view of those identities.

static void PFN3BAssignAllIds(LELevelData *level)
{
    PFN2AId(level);
    NSArray *collections = @[
        level.points ?: @[], level.lines ?: @[], level.polygons ?: @[],
        level.sides ?: @[], level.lights ?: @[], level.media ?: @[],
        level.platforms ?: @[], level.theMapObjects ?: @[],
        level.itemPlacement ?: @[], level.ambientSounds ?: @[],
        level.randomSounds ?: @[], level.tags ?: @[], level.notes ?: @[],
        level.layersInLevel ?: @[], level.noteTypes ?: @[], level.terminals ?: @[]
    ];
    for (NSArray *collection in collections)
        for (id object in collection) PFN2AId(object);
    for (Terminal *terminal in level.terminals)
        for (TerminalSection *section in terminal.theSections ?: @[]) PFN2AId(section);
}

static NSDictionary *PFN3BEditorNamesJSON(LELevelData *level)
{
    NSMutableDictionary *names = [NSMutableDictionary dictionary];
    NSArray *collections = @[
        level.polygons ?: @[], level.lights ?: @[], level.media ?: @[],
        level.platforms ?: @[], level.ambientSounds ?: @[], level.randomSounds ?: @[],
        level.tags ?: @[], level.notes ?: @[], level.layersInLevel ?: @[],
        level.noteTypes ?: @[], level.terminals ?: @[]
    ];
    for (NSArray *collection in collections) {
        for (id object in collection) {
            NSString *name = PFN3BName(object);
            NSString *identifier = PFN2AId(object);
            if (name.length && identifier.length) names[identifier] = name;
        }
    }
    for (Terminal *terminal in level.terminals) {
        for (TerminalSection *section in terminal.theSections ?: @[]) {
            NSString *name = PFN3BName(section);
            NSString *identifier = PFN2AId(section);
            if (name.length && identifier.length) names[identifier] = name;
        }
    }
    return names;
}

#pragma mark - Core FORMAT-1C JSON writers

static NSArray *PFN3BPointsJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.points.count];
    for (LEMapPoint *point in level.points)
        [result addObject:@{ @"id": PFN2AId(point), @"x": @(point.x), @"y": @(point.y) }];
    return result;
}

static NSArray *PFN3BLinesJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.lines.count];
    NSUInteger cwPolyCount = 0U, ccPolyCount = 0U;
    NSUInteger cwSideCount = 0U, ccSideCount = 0U;
    for (LELine *line in level.lines) {
        if (line.clockwisePolygonObject) ++cwPolyCount;
        if (line.conterclockwisePolygonObject) ++ccPolyCount;
        if (line.clockwisePolygonSideObject) ++cwSideCount;
        if (line.counterclockwisePolygonSideObject) ++ccSideCount;
        [result addObject:@{
            @"id": PFN2AId(line),
            @"startPoint": PFN3BRef(line.mapPoint1),
            @"endPoint": PFN3BRef(line.mapPoint2),
            @"flags": @((NSUInteger)line.flags),
            @"clockwisePolygon": PFN3BRef(line.clockwisePolygonObject),
            @"counterclockwisePolygon": PFN3BRef(line.conterclockwisePolygonObject),
            @"clockwiseSide": PFN3BRef(line.clockwisePolygonSideObject),
            @"counterclockwiseSide": PFN3BRef(line.counterclockwisePolygonSideObject)
        }];
    }
    NSLog(@"FORMAT-3B canonical line ownership: cwPoly=%lu ccPoly=%lu cwSide=%lu ccSide=%lu",
          (unsigned long)cwPolyCount,
          (unsigned long)ccPolyCount,
          (unsigned long)cwSideCount,
          (unsigned long)ccSideCount);
    return result;
}

static NSString *PFN3BPolygonEdgeDirection(LEPolygon *polygon, NSInteger edgeIndex, LELine *line)
{
    if (!line) return @"forward";
    LEMapPoint *from = [polygon vertexObjectAtIndex:(short)edgeIndex];
    LEMapPoint *to = [polygon vertexObjectAtIndex:(short)((edgeIndex + 1) % polygon.getTheVertexCount)];
    if (line.mapPoint1 == from && line.mapPoint2 == to) return @"forward";
    if (line.mapPoint2 == from && line.mapPoint1 == to) return @"reverse";
    // A malformed legacy graph should fail reference validation later rather than
    // serializing an undocumented direction token.
    return @"forward";
}

static BOOL PFN3BSideIsInLevel(LELevelData *level, LESide *side)
{
    return level != nil &&
           side != nil &&
           [level.sides indexOfObjectIdenticalTo:side] != NSNotFound;
}

static LESide *PFN3BCanonicalSideForPolygonEdge(LELevelData *level,
                                                LEPolygon *polygon,
                                                NSInteger edgeIndex,
                                                LELine *line)
{
    if (!level || !polygon) return nil;

    LESide *direct = [polygon sideObjectAtIndex:(short)edgeIndex];

    // Keep canonical serialization aligned with Visual Mode's already-proven
    // classic-map side resolver. The line's polygon-owner relation is the
    // strongest signal; the side object's own line/polygon backreferences are
    // legacy caches and may be stale.
    if (line != nil && line.clockwisePolygonObject == polygon) {
        LESide *candidate = line.clockwisePolygonSideObject;
        if (PFN3BSideIsInLevel(level, candidate)) return candidate;
    }

    if (line != nil && line.conterclockwisePolygonObject == polygon) {
        LESide *candidate = line.counterclockwisePolygonSideObject;
        if (PFN3BSideIsInLevel(level, candidate)) return candidate;
    }

    if (PFN3BSideIsInLevel(level, direct)) {
        if (line == nil ||
            direct.lineObject == nil ||
            direct.lineObject == line) {
            return direct;
        }
    }

    if (line == nil) return nil;

    LESide *clockwise = line.clockwisePolygonSideObject;
    if (PFN3BSideIsInLevel(level, clockwise) &&
        clockwise.polygonObject == polygon) {
        return clockwise;
    }

    LESide *counterclockwise = line.counterclockwisePolygonSideObject;
    if (PFN3BSideIsInLevel(level, counterclockwise) &&
        counterclockwise.polygonObject == polygon) {
        return counterclockwise;
    }

    for (LESide *candidate in level.sides) {
        if (candidate.lineObject == line &&
            candidate.polygonObject == polygon) {
            return candidate;
        }
    }

    return nil;
}

static NSArray *PFN3BPolygonsJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.polygons.count];
    NSUInteger edgeCount = 0U;
    NSUInteger sideRefCount = 0U;
    NSMutableSet<NSString *> *uniqueSideIds = [NSMutableSet set];
    for (LEPolygon *poly in level.polygons) {
        NSInteger count = MAX(0, MIN((NSInteger)MAXIMUM_VERTICES_PER_POLYGON,
                                     (NSInteger)poly.getTheVertexCount));
        NSMutableArray *edges = [NSMutableArray arrayWithCapacity:(NSUInteger)count];
        for (NSInteger i = 0; i < count; ++i) {
            LELine *line = [poly lineObjectAtIndex:(short)i];
            LESide *side = PFN3BCanonicalSideForPolygonEdge(level, poly, i, line);
            ++edgeCount;
            if (side != nil) {
                ++sideRefCount;
                NSString *sideId = PFN2AId(side);
                if (sideId.length) [uniqueSideIds addObject:sideId];
            }
            [edges addObject:@{
                @"line": PFN3BRef(line),
                @"side": PFN3BRef(side),
                @"direction": PFN3BPolygonEdgeDirection(poly, i, line)
            }];
        }
        [result addObject:@{
            @"id": PFN2AId(poly),
            @"type": @((NSInteger)poly.type),
            @"flags": @((NSUInteger)poly.flags),
            @"permutation": PFN3BSemanticTarget(poly.permutationObject, poly.permutation, @"integer"),
            @"edges": edges,
            @"floor": PFN3BPlaneJSON(poly.floorTexture, poly.floorOrigin,
                                      poly.floorTransferMode, poly.floorLightsourceObject,
                                      poly.floorHeight),
            @"ceiling": PFN3BPlaneJSON(poly.ceilingTexture, poly.ceilingOrigin,
                                        poly.ceilingTransferMode, poly.ceilingLightsourceObject,
                                        poly.ceilingHeight),
            @"media": PFN3BRef(poly.mediaObject),
            @"ambientSound": PFN3BRef(poly.ambientSoundObject),
            @"randomSound": PFN3BRef(poly.randomSoundObject)
        }];
    }
    NSLog(@"FORMAT-3B canonical side graph: edges=%lu sideRefs=%lu null=%lu uniqueSides=%lu",
          (unsigned long)edgeCount,
          (unsigned long)sideRefCount,
          (unsigned long)(edgeCount - sideRefCount),
          (unsigned long)uniqueSideIds.count);
    return result;
}

static NSArray *PFN3BSidesJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.sides.count];
    for (LESide *side in level.sides) {
        // These two references are legacy compatibility/backreference hints.
        // Polygon-edge -> side is the authoritative wall-surface binding.
        id controlPanel = NSNull.null;
        if ((side.flags & LESideIsControlPanel) != 0) {
            NSString *fallback = @"integer";
            switch ([side permutationEffects]) {
                case _cpanel_effects_tag: fallback = @"tag"; break;
                default: break;
            }
            controlPanel = @{
                @"type": @(side.controlPanelType),
                @"target": PFN3BSemanticTarget(side.controlPanelPermutationObject,
                                                side.controlPanelPermutation,
                                                fallback)
            };
        }
        [result addObject:@{
            @"id": PFN2AId(side),
            @"line": PFN3BRef(side.lineObject),
            @"polygon": PFN3BRef(side.polygonObject),
            @"type": PFN3BSideTypeString(side.type),
            @"flags": @((NSUInteger)side.flags),
            @"primary": PFN3BSurfaceJSON(side.primaryTextureStruct, side.primaryTransferMode,
                                          side.primaryLightsourceObject),
            @"secondary": PFN3BOptionalSurfaceJSON(side.secondaryTextureStruct, side.secondaryTransferMode,
                                                    side.secondaryLightsourceObject),
            @"transparent": PFN3BOptionalSurfaceJSON(side.transparentTextureStruct, side.transparentTransferMode,
                                                      side.transparentLightsourceObject),
            @"controlPanel": controlPanel,
            @"ambientDelta": @(side.ambientDelta)
        }];
    }
    return result;
}

static NSDictionary *PFN3BLightFunctionJSON(PhLight *light, PhLightState state)
{
    return @{
        @"function": @([light functionForState:state]),
        @"period": @([light periodForState:state]),
        @"deltaPeriod": @([light deltaPeriodForState:state]),
        @"intensity": @([light intensityForState:state]),
        @"deltaIntensity": @([light deltaIntensityForState:state])
    };
}

static NSArray *PFN3BLightsJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.lights.count];
    for (PhLight *light in level.lights) {
        [result addObject:@{
            @"id": PFN2AId(light),
            @"type": @((NSInteger)light.type),
            @"flags": @((NSUInteger)light.flags),
            @"phase": @(light.phase),
            @"states": @{
                @"primaryActive": PFN3BLightFunctionJSON(light, PhLightStatePrimaryActive),
                @"secondaryActive": PFN3BLightFunctionJSON(light, PhLightStateSecondaryActive),
                @"becomingActive": PFN3BLightFunctionJSON(light, PhLightStateBecomingActive),
                @"primaryInactive": PFN3BLightFunctionJSON(light, PhLightStatePrimaryInactive),
                @"secondaryInactive": PFN3BLightFunctionJSON(light, PhLightStateSecondaryInactive),
                @"becomingInactive": PFN3BLightFunctionJSON(light, PhLightStateBecomingInactive)
            },
            @"tag": PFN3BRef(light.tagObject)
        }];
    }
    return result;
}

static NSArray *PFN3BMediaJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.media.count];
    for (PhMedia *media in level.media) {
        // Existing classic maps use the media type as the semantic appearance.
        // The historical packed descriptor is preserved in the fidelity extension.
        [result addObject:@{
            @"id": PFN2AId(media),
            @"type": @((NSInteger)media.type),
            @"flags": @((NSUInteger)media.flags),
            @"light": PFN3BRef(media.lightObject),
            @"currentDirection": @(media.currentDirection),
            @"currentMagnitude": @(media.currentMagnitude),
            @"low": @(media.low),
            @"high": @(media.high),
            @"origin": PFN3BPoint2(media.origin),
            @"minimumLightIntensity": @(media.minimumLightIntensity),
            @"appearance": @{
                @"mode": @"type-default",
                @"texture": NSNull.null,
                @"transferMode": @(media.transferMode)
            }
        }];
    }
    return result;
}

static NSArray *PFN3BPlatformsJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.platforms.count];
    for (PhPlatform *platform in level.platforms) {
        [result addObject:@{
            @"id": PFN2AId(platform),
            @"type": @((NSInteger)platform.type),
            @"speed": @(platform.speed),
            @"delay": @(platform.delay),
            @"maximumHeight": @(platform.maximumHeight),
            @"minimumHeight": @(platform.minimumHeight),
            @"flags": @((NSUInteger)platform.staticFlags),
            @"polygon": PFN3BRef(platform.polygonObject),
            @"tag": PFN3BRef(platform.tagObject)
        }];
    }
    return result;
}

static NSArray *PFN3BObjectsJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.theMapObjects.count];
    for (LEMapObject *object in level.theMapObjects) {
        [result addObject:@{
            @"id": PFN2AId(object),
            @"type": @((NSInteger)object.type),
            @"kindIndex": @([object getObjTypeIndex]),
            @"facing": @(object.facing),
            @"polygon": PFN3BRef(object.polygonObject),
            @"position": @{ @"x": @(object.x), @"y": @(object.y), @"z": @(object.z) },
            @"flags": @((NSUInteger)object.mapFlags)
        }];
    }
    return result;
}

static NSArray *PFN3BItemPlacementsJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.itemPlacement.count];
    [level.itemPlacement enumerateObjectsUsingBlock:^(PhItemPlacement *item, NSUInteger i, BOOL *stop) {
        [result addObject:@{
            @"id": PFN2AId(item), @"slot": @(i), @"flags": @((NSUInteger)item.flags),
            @"initialCount": @(item.initialCount), @"minimumCount": @(item.minimumCount),
            @"maximumCount": @(item.maximumCount), @"randomCount": @(item.randomCount),
            @"randomChance": @((NSUInteger)item.randomChance)
        }];
    }];
    return result;
}

static NSArray *PFN3BAmbientSoundsJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.ambientSounds.count];
    for (PhAmbientSound *sound in level.ambientSounds)
        [result addObject:@{ @"id": PFN2AId(sound), @"flags": @((NSUInteger)sound.flags),
                             @"soundIndex": @(sound.soundIndex), @"volume": @(sound.volume) }];
    return result;
}

static NSArray *PFN3BRandomSoundsJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.randomSounds.count];
    for (PhRandomSound *sound in level.randomSounds) {
        [result addObject:@{
            @"id": PFN2AId(sound), @"flags": @((NSUInteger)sound.flags),
            @"soundIndex": @(sound.soundIndex), @"volume": @(sound.volume),
            @"deltaVolume": @(sound.deltaVolume), @"period": @(sound.period),
            @"deltaPeriod": @(sound.deltaPeriod), @"direction": @(sound.direction),
            @"deltaDirection": @(sound.deltaDirection), @"pitch": @(sound.pitch),
            @"deltaPitch": @(sound.deltaPitch), @"phase": @(sound.phase)
        }];
    }
    return result;
}

static NSArray *PFN3BTagsJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.tags.count];
    for (PhTag *tag in level.tags)
        [result addObject:@{ @"id": PFN2AId(tag), @"number": tag.phNumber ?: @0 }];
    return result;
}

static NSArray *PFN3BAnnotationsJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.notes.count];
    for (PhAnnotationNote *note in level.notes) {
        [result addObject:@{
            @"id": PFN2AId(note), @"type": @(note.type),
            @"location": PFN3BPoint2(note.location), @"polygon": PFN3BRef(note.polygonObject),
            @"text": note.text ?: @""
        }];
    }
    return result;
}

static NSDictionary *PFN3BLevelOptionsJSON(LELevelData *level)
{
    NSDictionary *source = [level getLevelOptionDictionary] ?: @{};
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    for (id key in source) {
        if (![key isKindOfClass:NSString.class]) continue;
        id value = source[key];
        if (!value || value == NSNull.null || [value isKindOfClass:NSString.class] ||
            [value isKindOfClass:NSNumber.class]) result[key] = value ?: NSNull.null;
    }
    return result;
}

static NSArray *PFN3BLayersJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.layersInLevel.count];
    for (PhLayer *layer in level.layersInLevel) {
        NSMutableArray *members = [NSMutableArray array];
        for (id object in layer.objectsInThisLayer ?: @[]) {
            NSString *identifier = PFN2AId(object);
            if (identifier.length) [members addObject:identifier];
        }
        [result addObject:@{
            @"id": PFN2AId(layer), @"name": PFN3BName(layer) ?: @"Layer",
            @"color": PFN3BColorJSON(layer.layerColor) ?: NSNull.null,
            @"members": members
        }];
    }
    return result;
}

static NSArray *PFN3BNoteGroupsJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.noteTypes.count];
    for (PhNoteGroup *group in level.noteTypes) {
        NSMutableArray *members = [NSMutableArray array];
        for (PhAnnotationNote *note in group.objectsInThisLayer ?: @[]) {
            NSString *identifier = PFN2AId(note);
            if (identifier.length) [members addObject:identifier];
        }
        [result addObject:@{
            @"id": PFN2AId(group), @"name": PFN3BName(group) ?: @"Note Group",
            @"color": PFN3BColorJSON(group.color) ?: NSNull.null,
            @"visible": @(group.visible), @"members": members
        }];
    }
    return result;
}

static NSArray *PFN3BLineOverridesJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.lines.count];
    for (LELine *line in level.lines) {
        [result addObject:@{
            @"line": PFN2AId(line),
            @"enabled": @([line getPermanentSetting:LELinePermanentUse]),
            @"solid": @([line getPermanentSetting:LELinePermanentSolid]),
            @"transparent": @([line getPermanentSetting:LELinePermanentTransparent]),
            @"landscape": @([line getPermanentSetting:LELinePermanentLandscape]),
            @"noSides": @([line getPermanentSetting:LELinePermanentNoSides])
        }];
    }
    return result;
}

#pragma mark - Terminal semantic text/style conversion

static NSColor *PFN3BTerminalColor(NSUInteger index)
{
    switch (index & 7U) {
        case 1: return NSColor.whiteColor;
        case 2: return NSColor.redColor;
        case 3: return [NSColor colorWithSRGBRed:0.0 green:0.5 blue:0.0 alpha:1.0];
        case 4: return NSColor.cyanColor;
        case 5: return NSColor.yellowColor;
        case 6: return [NSColor colorWithSRGBRed:0.5 green:0.0 blue:0.0 alpha:1.0];
        case 7: return NSColor.blueColor;
        default: return NSColor.greenColor;
    }
}

static NSUInteger PFN3BTerminalColorIndex(NSColor *color)
{
    if (!color) return 0;
    NSColor *rgb = [color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
    if (!rgb) return 0;
    NSUInteger best = 0;
    CGFloat bestDistance = CGFLOAT_MAX;
    for (NSUInteger i = 0; i < 8; ++i) {
        NSColor *candidate = [PFN3BTerminalColor(i) colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
        CGFloat dr = rgb.redComponent - candidate.redComponent;
        CGFloat dg = rgb.greenComponent - candidate.greenComponent;
        CGFloat db = rgb.blueComponent - candidate.blueComponent;
        CGFloat distance = dr*dr + dg*dg + db*db;
        if (distance < bestDistance) { bestDistance = distance; best = i; }
    }
    return best;
}

static NSArray *PFN3BTerminalStyles(NSAttributedString *text)
{
    if (!text.length) return @[];
    NSMutableArray *styles = [NSMutableArray array];
    NSFontManager *fontManager = NSFontManager.sharedFontManager;
    [text enumerateAttributesInRange:NSMakeRange(0, text.length)
                             options:0
                          usingBlock:^(NSDictionary<NSAttributedStringKey,id> *attrs,
                                       NSRange range,
                                       BOOL *stop) {
        NSUInteger face = PhTerminalSectionPlain;
        NSFont *font = attrs[NSFontAttributeName];
        if (font) {
            NSFontTraitMask traits = [fontManager traitsOfFont:font];
            if ((traits & NSBoldFontMask) != 0) face |= PhTerminalSectionBold;
            if ((traits & NSItalicFontMask) != 0) face |= PhTerminalSectionItalic;
        }
        if ([attrs[NSUnderlineStyleAttributeName] integerValue] != NSUnderlineStyleNone)
            face |= PhTerminalSectionUnderline;
        [styles addObject:@{
            @"start": @(range.location), @"length": @(range.length),
            @"face": @(face),
            @"colorIndex": @(PFN3BTerminalColorIndex(attrs[NSForegroundColorAttributeName]))
        }];
    }];
    return styles;
}

static NSString *PFN3BTerminalValueKind(PhTerminalSectionType type)
{
    switch (type) {
        case PhTerminalSectionTypeLevelTeleport: return @"level";
        case PhTerminalSectionTypeSound: return @"sound";
        case PhTerminalSectionTypeMovie: return @"movie";
        case PhTerminalSectionTypeTrack: return @"track";
        case PhTerminalSectionTypePict: return @"picture";
        case PhTerminalSectionTypeCamera: return @"camera";
        case PhTerminalSectionTypeStatic: return @"duration";
        case PhTerminalSectionTypeTag: return @"tag";
        default: return @"integer";
    }
}

static NSArray *PFN3BTerminalsJSON(LELevelData *level)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:level.terminals.count];
    for (Terminal *terminal in level.terminals) {
        NSMutableArray *sections = [NSMutableArray arrayWithCapacity:terminal.theSections.count];
        for (TerminalSection *section in terminal.theSections ?: @[]) {
            NSAttributedString *text = [section text] ?: [[NSAttributedString alloc] initWithString:@""];
            NSNumber *flags = PFN3BValue(section, @"flags") ?: @0;
            [sections addObject:@{
                @"id": PFN2AId(section), @"type": @((NSInteger)section.type),
                @"flags": @([flags unsignedShortValue]),
                @"target": PFN3BSemanticTarget(section.permutationObject,
                                                section.permutation,
                                                PFN3BTerminalValueKind(section.type)),
                @"text": text.string ?: @"", @"styles": PFN3BTerminalStyles(text)
            }];
        }
        [result addObject:@{
            @"id": PFN2AId(terminal), @"linesPerPage": @(terminal.lineCount), @"sections": sections
        }];
    }
    return result;
}

#pragma mark - Optional Cocoa fidelity extension

static NSDictionary *PFN3BFidelityJSON(LELevelData *level)
{
    // Core FORMAT-1C JSON remains authoritative.  Only Cocoa-era fields that
    // are not represented as independent canonical semantics live here.
    // Never mirror side/polygon textures here: collection/bitmap in the core
    // textureRef is sufficient to rebuild the packed classic descriptor.
    NSMutableDictionary *mediaLegacy = [NSMutableDictionary dictionary];
    for (PhMedia *media in level.media) {
        mediaLegacy[PFN2AId(media)] = @{
            @"textureDescriptor": @(media.texture), @"height": @(media.height)
        };
    }

    // These values are legacy/engine caches rather than canonical semantic
    // identity.  Keep them only as optional Cocoa fidelity hints so a Native
    // reopen can preserve current editor/export behavior while the canonical
    // graph remains the sole authority for geometry and ownership.
    NSMutableDictionary *lineDerived = [NSMutableDictionary dictionary];
    for (LELine *line in level.lines) {
        lineDerived[PFN2AId(line)] = @{
            @"highestAdjacentFloor": @(line.highestAdjacentFloor),
            @"lowestAdjacentCeiling": @(line.lowestAdjacentCeiling)
        };
    }
    NSMutableDictionary *sideDerived = [NSMutableDictionary dictionary];
    for (LESide *side in level.sides) {
        side_exclusion_zone zone = side.exclusionZone;
        sideDerived[PFN2AId(side)] = @{
            @"exclusionZone": @{
                @"e0": PFN3BPoint2(zone.e0), @"e1": PFN3BPoint2(zone.e1),
                @"e2": PFN3BPoint2(zone.e2), @"e3": PFN3BPoint2(zone.e3)
            }
        };
    }
    NSMutableDictionary *polygonDerived = [NSMutableDictionary dictionary];
    for (LEPolygon *polygon in level.polygons) {
        polygonDerived[PFN2AId(polygon)] = @{
            @"area": @(polygon.area),
            @"center": PFN3BPoint2(polygon.center),
            @"concave": @(polygon.polygonConcaveFlag)
        };
    }

    NSMutableDictionary *terminalLegacy = [NSMutableDictionary dictionary];
    for (Terminal *terminal in level.terminals) {
        NSMutableDictionary *sections = [NSMutableDictionary dictionary];
        for (TerminalSection *section in terminal.theSections ?: @[]) {
            NSNumber *lines = PFN3BValue(section, @"lines") ?: @(-1);
            sections[PFN2AId(section)] = @{ @"lines": lines };
        }
        terminalLegacy[PFN2AId(terminal)] = @{
            @"flags": @((NSUInteger)terminal.flags),
            @"textEncoded": PFN3BValue(terminal, @"textEncoded") ?: @NO,
            @"sections": sections
        };
    }
    return @{
        @"mediaLegacy": mediaLegacy,
        @"lineDerived": lineDerived,
        @"sideDerived": sideDerived,
        @"polygonDerived": polygonDerived,
        @"terminalLegacy": terminalLegacy
    };
}

static NSDictionary *PFN3BCountsFromLevel(LELevelData *level)
{
    return @{
        @"points":@(level.points.count), @"lines":@(level.lines.count),
        @"polygons":@(level.polygons.count), @"sides":@(level.sides.count),
        @"lights":@(level.lights.count), @"media":@(level.media.count),
        @"platforms":@(level.platforms.count), @"objects":@(level.theMapObjects.count),
        @"itemPlacements":@(level.itemPlacement.count), @"ambientSounds":@(level.ambientSounds.count),
        @"randomSounds":@(level.randomSounds.count), @"tags":@(level.tags.count),
        @"annotations":@(level.notes.count), @"layers":@(level.layersInLevel.count),
        @"noteGroups":@(level.noteTypes.count), @"terminals":@(level.terminals.count)
    };
}

static NSDictionary *PFN3BCanonicalLevelJSON(LELevelData *level, NSError **outError)
{
    if (!level) {
        if (outError) *outError = PFN2AError(3300, @"Cannot canonicalize a nil level.");
        return nil;
    }
    PFN3BAssignAllIds(level);
    NSInteger environment = level.environmentCode;
    id environmentJSON = (environment >= 0 && environment <= 4)
        ? @{ @"classicCode": @(environment) } : NSNull.null;

    return @{
        @"$schema": @"urn:pfhorge:schema:level:1",
        @"id": PFN2AId(level),
        @"name": level.levelName ?: @"Untitled Level",
        @"metadata": @{
            @"environment": environmentJSON,
            @"physicsModel": @(level.physicsModel), @"songIndex": @(level.songIndex),
            @"missionFlags": @((NSUInteger)level.missionFlags),
            @"environmentFlags": @((NSUInteger)level.environmentFlags),
            @"entryPointFlags": @((NSInteger)level.entryPointFlags)
        },
        @"geometry": @{
            @"points": PFN3BPointsJSON(level), @"lines": PFN3BLinesJSON(level),
            @"polygons": PFN3BPolygonsJSON(level)
        },
        @"surfaces": @{ @"sides": PFN3BSidesJSON(level) },
        @"world": @{
            @"lights": PFN3BLightsJSON(level), @"media": PFN3BMediaJSON(level),
            @"platforms": PFN3BPlatformsJSON(level), @"objects": PFN3BObjectsJSON(level),
            @"itemPlacements": PFN3BItemPlacementsJSON(level),
            @"ambientSounds": PFN3BAmbientSoundsJSON(level),
            @"randomSounds": PFN3BRandomSoundsJSON(level), @"tags": PFN3BTagsJSON(level),
            @"annotations": PFN3BAnnotationsJSON(level)
        },
        @"terminals": @{ @"items": PFN3BTerminalsJSON(level) },
        @"editor": @{
            @"layers": PFN3BLayersJSON(level), @"noteGroups": PFN3BNoteGroupsJSON(level),
            @"names": PFN3BEditorNamesJSON(level), @"lineOverrides": PFN3BLineOverridesJSON(level),
            @"currentLayer": PFN3BRef(PFN3BValue(level, @"currentLayer")),
            @"levelOptions": PFN3BLevelOptionsJSON(level)
        },
        @"extensions": @{
            PFN3BCanonicalAuthorityExtensionId: @{
                @"canonicalAuthority": @YES,
                @"canonicalModelRevision": @(PFN3BCanonicalModelRevision),
                @"legacyBridgeRequired": @NO,
                @"objectCounts": PFN3BCountsFromLevel(level),
                @"cocoaFidelity": PFN3BFidelityJSON(level)
            }
        },
        @"provenance": @{ @"sources": @[], @"bindings": @[], @"opaqueFragments": @[] }
    };
}

#pragma mark - Graph validation used before write and before Cocoa materialization

static NSArray *PFN3BArrayAt(NSDictionary *root, NSString *a, NSString *b)
{
    id first = root[a];
    id value = [first isKindOfClass:NSDictionary.class] ? first[b] : nil;
    return [value isKindOfClass:NSArray.class] ? value : @[];
}

static BOOL PFN3BAddEntityIds(NSArray *array, NSMutableSet<NSString *> *allIds, NSError **outError)
{
    for (id raw in array) {
        if (![raw isKindOfClass:NSDictionary.class]) {
            if (outError) *outError = PFN2AError(3310, @"Canonical entity is not a JSON object.");
            return NO;
        }
        NSString *identifier = PFN3BStringRef(raw[@"id"]);
        if (!identifier.length || [allIds containsObject:identifier]) {
            if (outError) *outError = PFN2AError(3311, @"Canonical graph contains a missing or duplicate UUID.");
            return NO;
        }
        [allIds addObject:identifier];
    }
    return YES;
}

static BOOL PFN3BRequiredRef(id value, NSSet<NSString *> *ids)
{
    NSString *identifier = PFN3BStringRef(value);
    return identifier.length && [ids containsObject:identifier];
}

static BOOL PFN3BOptionalRef(id value, NSSet<NSString *> *ids)
{
    if (!value || value == NSNull.null) return YES;
    return PFN3BRequiredRef(value, ids);
}

static BOOL PFN3BValidateCanonicalLevelJSON(NSDictionary *root, NSError **outError)
{
    if (![root isKindOfClass:NSDictionary.class] ||
        ![root[@"$schema"] isEqual:@"urn:pfhorge:schema:level:1"]) {
        if (outError) *outError = PFN2AError(3312, @"Unsupported canonical level schema.");
        return NO;
    }
    NSDictionary *authority = [root[@"extensions"] isKindOfClass:NSDictionary.class]
        ? root[@"extensions"][PFN3BCanonicalAuthorityExtensionId] : nil;
    if (![authority isKindOfClass:NSDictionary.class] ||
        ![authority[@"canonicalAuthority"] boolValue] ||
        [authority[@"canonicalModelRevision"] integerValue] < PFN3BCanonicalModelRevision) {
        if (outError) *outError = PFN2AError(3313, @"Canonical authority declaration is missing or too old.");
        return NO;
    }

    NSArray *points = PFN3BArrayAt(root,@"geometry",@"points");
    NSArray *lines = PFN3BArrayAt(root,@"geometry",@"lines");
    NSArray *polys = PFN3BArrayAt(root,@"geometry",@"polygons");
    NSArray *sides = PFN3BArrayAt(root,@"surfaces",@"sides");
    NSArray *lights = PFN3BArrayAt(root,@"world",@"lights");
    NSArray *media = PFN3BArrayAt(root,@"world",@"media");
    NSArray *platforms = PFN3BArrayAt(root,@"world",@"platforms");
    NSArray *objects = PFN3BArrayAt(root,@"world",@"objects");
    NSArray *placements = PFN3BArrayAt(root,@"world",@"itemPlacements");
    NSArray *ambient = PFN3BArrayAt(root,@"world",@"ambientSounds");
    NSArray *random = PFN3BArrayAt(root,@"world",@"randomSounds");
    NSArray *tags = PFN3BArrayAt(root,@"world",@"tags");
    NSArray *notes = PFN3BArrayAt(root,@"world",@"annotations");
    NSArray *layers = PFN3BArrayAt(root,@"editor",@"layers");
    NSArray *groups = PFN3BArrayAt(root,@"editor",@"noteGroups");
    NSArray *terminals = PFN3BArrayAt(root,@"terminals",@"items");

    NSMutableSet<NSString *> *allIds = [NSMutableSet set];
    for (NSArray *array in @[points,lines,polys,sides,lights,media,platforms,objects,
                             placements,ambient,random,tags,notes,layers,groups,terminals])
        if (!PFN3BAddEntityIds(array, allIds, outError)) return NO;
    for (NSDictionary *terminal in terminals) {
        if (!PFN3BAddEntityIds([terminal[@"sections"] isKindOfClass:NSArray.class] ? terminal[@"sections"] : @[],
                               allIds, outError)) return NO;
    }

    NSMutableSet *pointIds=[NSMutableSet set], *lineIds=[NSMutableSet set], *polyIds=[NSMutableSet set],
                 *sideIds=[NSMutableSet set], *lightIds=[NSMutableSet set], *mediaIds=[NSMutableSet set],
                 *ambientIds=[NSMutableSet set], *randomIds=[NSMutableSet set], *tagIds=[NSMutableSet set],
                 *layerIds=[NSMutableSet set], *noteIds=[NSMutableSet set];
    NSMutableDictionary<NSString *, NSDictionary *> *lineById=[NSMutableDictionary dictionary];
    for (NSDictionary *j in points) [pointIds addObject:j[@"id"]];
    for (NSDictionary *j in lines) { [lineIds addObject:j[@"id"]]; lineById[j[@"id"]]=j; }
    for (NSDictionary *j in polys) [polyIds addObject:j[@"id"]];
    for (NSDictionary *j in sides) [sideIds addObject:j[@"id"]];
    for (NSDictionary *j in lights) [lightIds addObject:j[@"id"]];
    for (NSDictionary *j in media) [mediaIds addObject:j[@"id"]];
    for (NSDictionary *j in ambient) [ambientIds addObject:j[@"id"]];
    for (NSDictionary *j in random) [randomIds addObject:j[@"id"]];
    for (NSDictionary *j in tags) [tagIds addObject:j[@"id"]];
    for (NSDictionary *j in layers) [layerIds addObject:j[@"id"]];
    for (NSDictionary *j in notes) [noteIds addObject:j[@"id"]];

    for (NSDictionary *line in lines) {
        if (!PFN3BRequiredRef(line[@"startPoint"],pointIds) ||
            !PFN3BRequiredRef(line[@"endPoint"],pointIds) ||
            [line[@"startPoint"] isEqual:line[@"endPoint"]]) {
            if (outError) *outError = PFN2AError(3314, @"Canonical line contains invalid point references.");
            return NO;
        }

        // FORMAT-3B line ownership is semantic topology, not a derived cache.
        // Require the keys even when the corresponding value is JSON null so
        // incompatible interim draft packages fail loudly instead of
        // reopening with a zero-portal graph.
        if (!line[@"clockwisePolygon"] ||
            !line[@"counterclockwisePolygon"] ||
            !line[@"clockwiseSide"] ||
            !line[@"counterclockwiseSide"]) {
            if (outError) {
                *outError = PFN2AError(
                    3314,
                    @"Canonical line ownership fields are missing; this package was written by an incompatible FORMAT-3B draft.");
            }
            return NO;
        }

        if (!PFN3BOptionalRef(line[@"clockwisePolygon"],polyIds) ||
            !PFN3BOptionalRef(line[@"counterclockwisePolygon"],polyIds) ||
            !PFN3BOptionalRef(line[@"clockwiseSide"],sideIds) ||
            !PFN3BOptionalRef(line[@"counterclockwiseSide"],sideIds)) {
            if (outError) {
                *outError = PFN2AError(
                    3314,
                    @"Canonical line contains invalid polygon/side ownership references.");
            }
            return NO;
        }
    }
    NSMutableDictionary *sideById=[NSMutableDictionary dictionary];
    for (NSDictionary *side in sides) {
        sideById[side[@"id"]]=side;
        if (!PFN3BRequiredRef(side[@"line"],lineIds) || !PFN3BRequiredRef(side[@"polygon"],polyIds)) {
            if (outError) *outError = PFN2AError(3315, @"Canonical side contains invalid line/polygon references.");
            return NO;
        }
        for (NSString *key in @[@"primary",@"secondary",@"transparent"]) {
            NSDictionary *surface=[side[key] isKindOfClass:NSDictionary.class]?side[key]:nil;
            if (surface && !PFN3BOptionalRef(surface[@"light"],lightIds)) {
                if (outError) *outError = PFN2AError(3316, @"Canonical side surface references a missing light.");
                return NO;
            }
        }
    }
    for (NSDictionary *poly in polys) {
        NSArray *edges=[poly[@"edges"] isKindOfClass:NSArray.class]?poly[@"edges"]:@[];
        if (edges.count < 3 || edges.count > MAXIMUM_VERTICES_PER_POLYGON) {
            if (outError) *outError = PFN2AError(3317, @"Canonical polygon has an invalid edge count.");
            return NO;
        }
        NSDictionary *floor=[poly[@"floor"] isKindOfClass:NSDictionary.class]?poly[@"floor"]:@{};
        NSDictionary *ceiling=[poly[@"ceiling"] isKindOfClass:NSDictionary.class]?poly[@"ceiling"]:@{};
        if ([floor[@"height"] integerValue] > [ceiling[@"height"] integerValue] ||
            !PFN3BOptionalRef(floor[@"light"],lightIds) ||
            !PFN3BOptionalRef(ceiling[@"light"],lightIds) ||
            !PFN3BOptionalRef(poly[@"media"],mediaIds) ||
            !PFN3BOptionalRef(poly[@"ambientSound"],ambientIds) ||
            !PFN3BOptionalRef(poly[@"randomSound"],randomIds)) {
            if (outError) *outError = PFN2AError(3318, @"Canonical polygon contains invalid world references/heights.");
            return NO;
        }
        NSString *firstDirectedStart=nil, *previousDirectedEnd=nil;
        for (NSDictionary *edge in edges) {
            BOOL forward=[edge[@"direction"] isEqual:@"forward"];
            BOOL reverse=[edge[@"direction"] isEqual:@"reverse"];
            if (!PFN3BRequiredRef(edge[@"line"],lineIds) || !(forward || reverse)) {
                if (outError) *outError = PFN2AError(3319, @"Canonical polygon edge is invalid.");
                return NO;
            }
            NSDictionary *line=lineById[edge[@"line"]];
            NSString *polygonId=PFN3BStringRef(poly[@"id"]);
            NSString *clockwiseOwner=PFN3BStringRef(line[@"clockwisePolygon"]);
            NSString *counterclockwiseOwner=PFN3BStringRef(line[@"counterclockwisePolygon"]);
            BOOL ownedClockwise =
                polygonId.length && [polygonId isEqual:clockwiseOwner];
            BOOL ownedCounterclockwise =
                polygonId.length && [polygonId isEqual:counterclockwiseOwner];
            if (ownedClockwise == ownedCounterclockwise) {
                if (outError) {
                    NSString *detail = [NSString stringWithFormat:
                        @"Canonical polygon/line ownership mismatch: polygon=%@ line=%@ clockwise=%@ counterclockwise=%@.",
                        polygonId ?: @"(nil)",
                        edge[@"line"] ?: @"(nil)",
                        clockwiseOwner ?: @"(nil)",
                        counterclockwiseOwner ?: @"(nil)"];
                    *outError = PFN2AError(3320, detail);
                }
                return NO;
            }

            NSString *directedStart=forward ? line[@"startPoint"] : line[@"endPoint"];
            NSString *directedEnd=forward ? line[@"endPoint"] : line[@"startPoint"];
            if (!firstDirectedStart) firstDirectedStart=directedStart;
            if (previousDirectedEnd && ![previousDirectedEnd isEqual:directedStart]) {
                if (outError) *outError = PFN2AError(3320, @"Canonical polygon edges do not form a continuous directed loop.");
                return NO;
            }
            previousDirectedEnd=directedEnd;
            NSString *sideId=PFN3BStringRef(edge[@"side"]);
            if (sideId && sideById[sideId] == nil) {
                if (outError) {
                    NSString *detail = [NSString stringWithFormat:
                        @"Canonical polygon edge references a missing side: polygon=%@ line=%@ side=%@.",
                        poly[@"id"] ?: @"(nil)",
                        edge[@"line"] ?: @"(nil)",
                        sideId];
                    *outError = PFN2AError(3321, detail);
                }
                return NO;
            }
        }
        if (previousDirectedEnd && ![previousDirectedEnd isEqual:firstDirectedStart]) {
            if (outError) *outError = PFN2AError(3322, @"Canonical polygon edge loop is not closed.");
            return NO;
        }
    }
    for (NSDictionary *light in lights) if (!PFN3BOptionalRef(light[@"tag"],tagIds)) {
        if (outError) *outError=PFN2AError(3321,@"Canonical light references a missing tag."); return NO;
    }
    for (NSDictionary *medium in media) if (!PFN3BOptionalRef(medium[@"light"],lightIds)) {
        if (outError) *outError=PFN2AError(3322,@"Canonical media references a missing light."); return NO;
    }
    for (NSDictionary *platform in platforms) {
        if (!PFN3BRequiredRef(platform[@"polygon"],polyIds) || !PFN3BOptionalRef(platform[@"tag"],tagIds)) {
            if (outError) *outError=PFN2AError(3323,@"Canonical platform reference is invalid."); return NO;
        }
    }
    for (NSDictionary *object in objects) if (!PFN3BOptionalRef(object[@"polygon"],polyIds)) {
        if (outError) *outError=PFN2AError(3324,@"Canonical map object references a missing polygon."); return NO;
    }
    for (NSDictionary *note in notes) if (!PFN3BOptionalRef(note[@"polygon"],polyIds)) {
        if (outError) *outError=PFN2AError(3325,@"Canonical annotation references a missing polygon."); return NO;
    }

    NSDictionary *editor=[root[@"editor"] isKindOfClass:NSDictionary.class]?root[@"editor"]:@{};
    if (!PFN3BOptionalRef(editor[@"currentLayer"],layerIds)) {
        if (outError) *outError=PFN2AError(3326,@"Canonical current layer reference is invalid."); return NO;
    }
    for (NSDictionary *layer in layers) for (id member in layer[@"members"] ?: @[]) if (!PFN3BRequiredRef(member,allIds)) {
        if (outError) *outError=PFN2AError(3327,@"Canonical editor layer contains a dangling member."); return NO;
    }
    for (NSDictionary *group in groups) for (id member in group[@"members"] ?: @[]) if (!PFN3BRequiredRef(member,noteIds)) {
        if (outError) *outError=PFN2AError(3328,@"Canonical note group contains a dangling annotation."); return NO;
    }
    for (NSDictionary *override in editor[@"lineOverrides"] ?: @[]) if (!PFN3BRequiredRef(override[@"line"],lineIds)) {
        if (outError) *outError=PFN2AError(3329,@"Canonical line override references a missing line."); return NO;
    }
    for (NSString *identifier in [editor[@"names"] isKindOfClass:NSDictionary.class] ? editor[@"names"] : @{})
        if (![allIds containsObject:identifier]) {
            if (outError) *outError=PFN2AError(3330,@"Canonical editor name references a missing object."); return NO;
        }
    return YES;
}

#pragma mark - Canonical JSON materialization into LELevelData

static NSMutableArray *PFN3BAllocateEntities(NSArray *json,
                                              Class klass,
                                              NSMutableDictionary<NSString *, id> *objects,
                                              NSError **outError)
{
    NSMutableArray *result=[NSMutableArray arrayWithCapacity:json.count];
    for (NSDictionary *item in json) {
        id object=[[klass alloc] init];
        NSString *identifier=PFN3BStringRef(item[@"id"]);
        if (!object || !identifier.length || objects[identifier]) {
            if (outError) *outError=PFN2AError(3340,@"Could not allocate canonical entity or UUID is duplicated.");
            return nil;
        }
        PFN2ASetId(object,identifier); objects[identifier]=object; [result addObject:object];
    }
    return result;
}

static void PFN3BLoadLightFunction(PhLight *light, PhLightState state, NSDictionary *json)
{
    [light setFunction:(PhLightFunction)[json[@"function"] shortValue] forState:state];
    [light setPeriod:[json[@"period"] shortValue] forState:state];
    [light setDeltaPeriod:[json[@"deltaPeriod"] shortValue] forState:state];
    [light setIntensity:[json[@"intensity"] intValue] forState:state];
    [light setDeltaIntensity:[json[@"deltaIntensity"] intValue] forState:state];
}

static NSMutableAttributedString *PFN3BAttributedTerminalText(NSDictionary *sectionJSON)
{
    NSString *plain=[sectionJSON[@"text"] isKindOfClass:NSString.class]?sectionJSON[@"text"]:@"";
    NSFont *base = [NSFont fontWithName:@"Courier" size:12.0];
    if (!base) base = [NSFont systemFontOfSize:12.0];
    NSMutableAttributedString *text=[[NSMutableAttributedString alloc]
        initWithString:plain attributes:@{NSFontAttributeName:base, NSForegroundColorAttributeName:NSColor.greenColor}];
    NSFontManager *manager=NSFontManager.sharedFontManager;
    for (NSDictionary *run in ([sectionJSON[@"styles"] isKindOfClass:NSArray.class]?sectionJSON[@"styles"]:@[])) {
        NSUInteger start=[run[@"start"] unsignedIntegerValue], length=[run[@"length"] unsignedIntegerValue];
        if (start > text.length || length > text.length-start) continue;
        NSUInteger face=[run[@"face"] unsignedIntegerValue];
        NSFontTraitMask traits=0;
        if ((face & PhTerminalSectionBold)!=0) traits|=NSBoldFontMask;
        if ((face & PhTerminalSectionItalic)!=0) traits|=NSItalicFontMask;
        NSFont *font=traits ? [manager convertFont:base toHaveTrait:traits] : base;
        NSMutableDictionary *attrs=[@{NSFontAttributeName:font ?: base,
                                      NSForegroundColorAttributeName:PFN3BTerminalColor([run[@"colorIndex"] unsignedIntegerValue])} mutableCopy];
        if ((face & PhTerminalSectionUnderline)!=0) attrs[NSUnderlineStyleAttributeName]=@(NSUnderlineStyleSingle);
        [text addAttributes:attrs range:NSMakeRange(start,length)];
    }
    return text;
}

static void PFN3BRestorePolygonPermutation(LEPolygon *polygon,
                                           NSDictionary *permutationJSON,
                                           NSDictionary<NSString *, id> *objects)
{
    if (!polygon) return;

    id targetObject = PFN3BTargetObject(permutationJSON, objects);
    if (targetObject != nil) {
        // Platform, light-trigger, polygon-trigger and teleporter semantics are
        // object-backed in the current Pfhorge model.
        [polygon setPermutationObject:targetObject];
        return;
    }

    // LEPolygon -setPermutation: is deliberately obsolete: it presents a
    // modal warning and does not assign the value. Scalar classic semantics
    // are represented by an NSNumber permutationObject instead.
    switch (polygon.type) {
        case LEPolygonBase:
        case LEPolygonAutomaticExit:
            [polygon setPermutationObject:@(PFN3BTargetValue(permutationJSON))];
            break;

        default:
            // Types with no semantic target/scalar permutation remain nil.
            [polygon setPermutationObject:nil];
            break;
    }
}

static LELevelData *PFN3BLevelFromCanonicalJSON(NSDictionary *root, NSError **outError)
{
    NSError *error=nil;
    if (!PFN3BValidateCanonicalLevelJSON(root,&error)) { if(outError)*outError=error; return nil; }

    LELevelData *level=[[LELevelData alloc] init];
    if (!level) { if(outError)*outError=PFN2AError(3350,@"Could not allocate LELevelData."); return nil; }
    PFN2ASetId(level,root[@"id"]);

    NSArray *pointJSON=PFN3BArrayAt(root,@"geometry",@"points"), *lineJSON=PFN3BArrayAt(root,@"geometry",@"lines"),
            *polyJSON=PFN3BArrayAt(root,@"geometry",@"polygons"), *sideJSON=PFN3BArrayAt(root,@"surfaces",@"sides"),
            *lightJSON=PFN3BArrayAt(root,@"world",@"lights"), *mediaJSON=PFN3BArrayAt(root,@"world",@"media"),
            *platformJSON=PFN3BArrayAt(root,@"world",@"platforms"), *objectJSON=PFN3BArrayAt(root,@"world",@"objects"),
            *placementJSON=PFN3BArrayAt(root,@"world",@"itemPlacements"), *ambientJSON=PFN3BArrayAt(root,@"world",@"ambientSounds"),
            *randomJSON=PFN3BArrayAt(root,@"world",@"randomSounds"), *tagJSON=PFN3BArrayAt(root,@"world",@"tags"),
            *noteJSON=PFN3BArrayAt(root,@"world",@"annotations"), *layerJSON=PFN3BArrayAt(root,@"editor",@"layers"),
            *groupJSON=PFN3BArrayAt(root,@"editor",@"noteGroups"), *terminalJSON=PFN3BArrayAt(root,@"terminals",@"items");

    NSMutableDictionary<NSString *,id> *objects=[NSMutableDictionary dictionary];
    NSMutableArray *points=PFN3BAllocateEntities(pointJSON,LEMapPoint.class,objects,&error),
                   *lines=PFN3BAllocateEntities(lineJSON,LELine.class,objects,&error),
                   *polys=PFN3BAllocateEntities(polyJSON,LEPolygon.class,objects,&error),
                   *sides=PFN3BAllocateEntities(sideJSON,LESide.class,objects,&error),
                   *lights=PFN3BAllocateEntities(lightJSON,PhLight.class,objects,&error),
                   *media=PFN3BAllocateEntities(mediaJSON,PhMedia.class,objects,&error),
                   *platforms=PFN3BAllocateEntities(platformJSON,PhPlatform.class,objects,&error),
                   *mapObjects=PFN3BAllocateEntities(objectJSON,LEMapObject.class,objects,&error),
                   *placements=PFN3BAllocateEntities(placementJSON,PhItemPlacement.class,objects,&error),
                   *ambient=PFN3BAllocateEntities(ambientJSON,PhAmbientSound.class,objects,&error),
                   *random=PFN3BAllocateEntities(randomJSON,PhRandomSound.class,objects,&error),
                   *tags=PFN3BAllocateEntities(tagJSON,PhTag.class,objects,&error),
                   *notes=PFN3BAllocateEntities(noteJSON,PhAnnotationNote.class,objects,&error),
                   *layers=PFN3BAllocateEntities(layerJSON,PhLayer.class,objects,&error),
                   *groups=PFN3BAllocateEntities(groupJSON,PhNoteGroup.class,objects,&error),
                   *terminals=PFN3BAllocateEntities(terminalJSON,Terminal.class,objects,&error);
    if (!points||!lines||!polys||!sides||!lights||!media||!platforms||!mapObjects||!placements||
        !ambient||!random||!tags||!notes||!layers||!groups||!terminals) {
        if(outError)*outError=error?:PFN2AError(3351,@"Could not allocate canonical entities."); return nil;
    }

    PFN3BSetValue(level,@"points",points); PFN3BSetValue(level,@"lines",lines); PFN3BSetValue(level,@"polys",polys);
    PFN3BSetValue(level,@"sides",sides); PFN3BSetValue(level,@"lights",lights); PFN3BSetValue(level,@"media",media);
    PFN3BSetValue(level,@"platforms",platforms); PFN3BSetValue(level,@"mapObjects",mapObjects);
    PFN3BSetValue(level,@"itemPlacement",placements); PFN3BSetValue(level,@"ambientSounds",ambient);
    PFN3BSetValue(level,@"randomSounds",random); PFN3BSetValue(level,@"tags",tags); PFN3BSetValue(level,@"notes",notes);
    PFN3BSetValue(level,@"layersInLevel",layers); PFN3BSetValue(level,@"noteTypes",groups); PFN3BSetValue(level,@"terimals",terminals);
    PFN3BSetValue(level,@"layerPoints",[NSMutableArray array]); PFN3BSetValue(level,@"layerLines",[NSMutableArray array]);
    PFN3BSetValue(level,@"layerPolys",[NSMutableArray array]); PFN3BSetValue(level,@"layerMapObjects",[NSMutableArray array]);
    PFN3BSetValue(level,@"layerNotes",[NSMutableArray array]); PFN3BSetValue(level,@"namedPolyObjects",[NSMutableArray array]);

    NSDictionary *metadata=[root[@"metadata"] isKindOfClass:NSDictionary.class]?root[@"metadata"]:@{};
    NSDictionary *environment=[metadata[@"environment"] isKindOfClass:NSDictionary.class]?metadata[@"environment"]:nil;
    PFN3BSetValue(level,@"environment_code",environment?environment[@"classicCode"]:@0); // metadata only: bypass mutating setter
    PFN3BSetValue(level,@"physics_model",metadata[@"physicsModel"]?:@0); PFN3BSetValue(level,@"song_index",metadata[@"songIndex"]?:@0);
    PFN3BSetValue(level,@"mission_flags",metadata[@"missionFlags"]?:@0); PFN3BSetValue(level,@"environment_flags",metadata[@"environmentFlags"]?:@0);
    PFN3BSetValue(level,@"entry_point_flags",metadata[@"entryPointFlags"]?:@0);
    level.levelName=[root[@"name"] isKindOfClass:NSString.class]?root[@"name"]:@"Untitled Level";
    NSDictionary *editor=[root[@"editor"] isKindOfClass:NSDictionary.class]?root[@"editor"]:@{};
    if ([editor[@"levelOptions"] isKindOfClass:NSDictionary.class]) PFN3BSetValue(level,@"levelOptions",[editor[@"levelOptions"] mutableCopy]);

    [level setUpArrayPointersForEveryObject];
    // The historical helper omits Terminal objects from its loops. They are
    // first-class canonical entities and need the same level-array context so
    // editor/export paths can resolve their dynamic indexes and references.
    for (Terminal *terminal in terminals) [level setUpArrayPointersFor:terminal];

    [pointJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){ [(LEMapPoint*)points[i] setX:[j[@"x"] shortValue] Y:[j[@"y"] shortValue]]; }];
    [lineJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        LELine *line=lines[i]; line.mapPoint1=PFN3BObjectForRef(objects,j[@"startPoint"]); line.mapPoint2=PFN3BObjectForRef(objects,j[@"endPoint"]);
        line.flags=(LELineFlags)[j[@"flags"] unsignedShortValue];
        line.clockwisePolygonObject=PFN3BObjectForRef(objects,j[@"clockwisePolygon"]);
        line.conterclockwisePolygonObject=PFN3BObjectForRef(objects,j[@"counterclockwisePolygon"]);
        line.clockwisePolygonSideObject=PFN3BObjectForRef(objects,j[@"clockwiseSide"]);
        line.counterclockwisePolygonSideObject=PFN3BObjectForRef(objects,j[@"counterclockwiseSide"]);
        [line recalc];
    }];
    [tagJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){ ((PhTag*)tags[i]).phNumber=j[@"number"]?:@0; }];
    [lightJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        PhLight *light=lights[i]; light.type=(PhLightTypes)[j[@"type"] shortValue]; light.flags=(PhLightStaticFlags)[j[@"flags"] unsignedShortValue];
        light.phase=[j[@"phase"] shortValue]; light.tagObject=PFN3BObjectForRef(objects,j[@"tag"]); light.tag = light.tagObject ? [[(PhTag *)light.tagObject phNumber] shortValue] : 0;
        NSDictionary *states=[j[@"states"] isKindOfClass:NSDictionary.class]?j[@"states"]:@{};
        PFN3BLoadLightFunction(light,PhLightStatePrimaryActive,states[@"primaryActive"]?:@{});
        PFN3BLoadLightFunction(light,PhLightStateSecondaryActive,states[@"secondaryActive"]?:@{});
        PFN3BLoadLightFunction(light,PhLightStateBecomingActive,states[@"becomingActive"]?:@{});
        PFN3BLoadLightFunction(light,PhLightStatePrimaryInactive,states[@"primaryInactive"]?:@{});
        PFN3BLoadLightFunction(light,PhLightStateSecondaryInactive,states[@"secondaryInactive"]?:@{});
        PFN3BLoadLightFunction(light,PhLightStateBecomingInactive,states[@"becomingInactive"]?:@{});
    }];

    NSDictionary *authority=root[@"extensions"][PFN3BCanonicalAuthorityExtensionId];
    NSDictionary *fidelity=[authority[@"cocoaFidelity"] isKindOfClass:NSDictionary.class]?authority[@"cocoaFidelity"]:@{};
    NSDictionary *mediaLegacy=[fidelity[@"mediaLegacy"] isKindOfClass:NSDictionary.class]?fidelity[@"mediaLegacy"]:@{};
    NSDictionary *lineDerived=[fidelity[@"lineDerived"] isKindOfClass:NSDictionary.class]?fidelity[@"lineDerived"]:@{};
    NSDictionary *sideDerived=[fidelity[@"sideDerived"] isKindOfClass:NSDictionary.class]?fidelity[@"sideDerived"]:@{};
    NSDictionary *polygonDerived=[fidelity[@"polygonDerived"] isKindOfClass:NSDictionary.class]?fidelity[@"polygonDerived"]:@{};
    NSDictionary *terminalLegacy=[fidelity[@"terminalLegacy"] isKindOfClass:NSDictionary.class]?fidelity[@"terminalLegacy"]:@{};

    [mediaJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        PhMedia *medium=media[i]; medium.type=(PhMediaType)[j[@"type"] shortValue]; medium.flags=(PhMediaFlags)[j[@"flags"] unsignedShortValue];
        medium.lightObject=PFN3BObjectForRef(objects,j[@"light"]); medium.currentDirection=[j[@"currentDirection"] shortValue];
        medium.currentMagnitude=[j[@"currentMagnitude"] shortValue]; medium.low=[j[@"low"] shortValue]; medium.high=[j[@"high"] shortValue];
        medium.origin=PFN3BPoint2FromJSON(j[@"origin"]); medium.minimumLightIntensity=[j[@"minimumLightIntensity"] intValue];
        NSDictionary *appearance=[j[@"appearance"] isKindOfClass:NSDictionary.class]?j[@"appearance"]:@{};
        medium.transferMode=[appearance[@"transferMode"] shortValue]; NSDictionary *legacy=mediaLegacy[j[@"id"]];
        if ([legacy isKindOfClass:NSDictionary.class]) { medium.texture=[legacy[@"textureDescriptor"] shortValue]; medium.height=[legacy[@"height"] shortValue]; }
        else if ([appearance[@"mode"] isEqual:@"explicit"]) medium.texture=PFN3BTextureDescriptor(appearance[@"texture"]);
        else { medium.texture=-1; medium.height=medium.low; }
    }];
    [platformJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        PhPlatform *p=platforms[i]; p.type=(PhPlatformType)[j[@"type"] shortValue]; p.speed=[j[@"speed"] shortValue]; p.delay=[j[@"delay"] shortValue];
        p.maximumHeight=[j[@"maximumHeight"] shortValue]; p.minimumHeight=[j[@"minimumHeight"] shortValue]; p.staticFlags=(PhPlatformStaticFlags)[j[@"flags"] unsignedLongValue];
        p.polygonObject=PFN3BObjectForRef(objects,j[@"polygon"]); p.tagObject=PFN3BObjectForRef(objects,j[@"tag"]); p.tag = p.tagObject ? [[(PhTag *)p.tagObject phNumber] shortValue] : 0;
    }];
    [objectJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        LEMapObject *o=mapObjects[i]; o.type=(LEMapObjectType)[j[@"type"] shortValue]; [o setIndex:[j[@"kindIndex"] shortValue]]; o.facing=[j[@"facing"] shortValue];
        NSDictionary *position=[j[@"position"] isKindOfClass:NSDictionary.class]?j[@"position"]:@{}; o.x=[position[@"x"] shortValue]; o.y=[position[@"y"] shortValue]; o.z=[position[@"z"] shortValue];
        o.mapFlags=(LEMapObjectFlags)[j[@"flags"] unsignedShortValue]; o.polygonObject=PFN3BObjectForRef(objects,j[@"polygon"]);
    }];
    [placementJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        PhItemPlacement *o=placements[i]; o.flags=(PhItemPlacementFlags)[j[@"flags"] unsignedShortValue]; o.initialCount=[j[@"initialCount"] shortValue];
        o.minimumCount=[j[@"minimumCount"] shortValue]; o.maximumCount=[j[@"maximumCount"] shortValue]; o.randomCount=[j[@"randomCount"] shortValue]; o.randomChance=[j[@"randomChance"] unsignedShortValue];
    }];
    [ambientJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){ PhAmbientSound *o=ambient[i]; o.flags=[j[@"flags"] unsignedShortValue]; o.soundIndex=[j[@"soundIndex"] shortValue]; o.volume=[j[@"volume"] shortValue]; }];
    [randomJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        PhRandomSound *o=random[i]; o.flags=(PhRandomSoundFlags)[j[@"flags"] unsignedShortValue]; o.soundIndex=[j[@"soundIndex"] shortValue]; o.volume=[j[@"volume"] shortValue];
        o.deltaVolume=[j[@"deltaVolume"] shortValue]; o.period=[j[@"period"] shortValue]; o.deltaPeriod=[j[@"deltaPeriod"] shortValue]; o.direction=[j[@"direction"] shortValue];
        o.deltaDirection=[j[@"deltaDirection"] shortValue]; o.pitch=[j[@"pitch"] intValue]; o.deltaPitch=[j[@"deltaPitch"] intValue]; o.phase=[j[@"phase"] shortValue];
    }];

    [sideJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        LESide *side=sides[i]; side.type=PFN3BSideTypeFromString(j[@"type"]); side.flags=(LESideFlags)[j[@"flags"] unsignedShortValue];
        NSDictionary *primary=[j[@"primary"] isKindOfClass:NSDictionary.class]?j[@"primary"]:@{};
        NSDictionary *secondary=[j[@"secondary"] isKindOfClass:NSDictionary.class]?j[@"secondary"]:@{};
        NSDictionary *transparent=[j[@"transparent"] isKindOfClass:NSDictionary.class]?j[@"transparent"]:@{};
        side.primaryTextureStruct=PFN3BSurfaceFromJSON(primary); side.secondaryTextureStruct=PFN3BSurfaceFromJSON(secondary); side.transparentTextureStruct=PFN3BSurfaceFromJSON(transparent);
        side.primaryTransferMode=[primary[@"transferMode"] shortValue]; side.secondaryTransferMode=[secondary[@"transferMode"] shortValue]; side.transparentTransferMode=[transparent[@"transferMode"] shortValue];
        side.primaryLightsourceObject=PFN3BObjectForRef(objects,primary[@"light"]); side.secondaryLightsourceObject=PFN3BObjectForRef(objects,secondary[@"light"]); side.transparentLightsourceObject=PFN3BObjectForRef(objects,transparent[@"light"]);
        side.lineObject=PFN3BObjectForRef(objects,j[@"line"]); side.polygonObject=PFN3BObjectForRef(objects,j[@"polygon"]); side.ambientDelta=[j[@"ambientDelta"] intValue];
        NSDictionary *cp=[j[@"controlPanel"] isKindOfClass:NSDictionary.class]?j[@"controlPanel"]:nil;
        if (cp) { side.controlPanelType=[cp[@"type"] shortValue]; NSDictionary *target=[cp[@"target"] isKindOfClass:NSDictionary.class]?cp[@"target"]:@{}; side.controlPanelPermutationObject=PFN3BTargetObject(target,objects); [side setControlPanelPermutation:PFN3BTargetValue(target)]; }
    }];

    [polyJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        LEPolygon *poly=polys[i]; poly.type=(LEPolygonType)[j[@"type"] shortValue]; poly.flags=(LEPolygonFlags)[j[@"flags"] unsignedShortValue];
        NSDictionary *perm=[j[@"permutation"] isKindOfClass:NSDictionary.class]?j[@"permutation"]:@{}; PFN3BRestorePolygonPermutation(poly,perm,objects);
        NSDictionary *floor=[j[@"floor"] isKindOfClass:NSDictionary.class]?j[@"floor"]:@{}, *ceiling=[j[@"ceiling"] isKindOfClass:NSDictionary.class]?j[@"ceiling"]:@{};
        [poly setFloorTexture:PFN3BTextureDescriptor(floor[@"texture"])];
        [poly setCeilingTexture:PFN3BTextureDescriptor(ceiling[@"texture"])];
        [poly setFloorHeightNoSides:[floor[@"height"] shortValue]]; [poly setCeilingHeightNoSides:[ceiling[@"height"] shortValue]]; poly.floorOrigin=PFN3BPoint2FromJSON(floor[@"origin"]); poly.ceilingOrigin=PFN3BPoint2FromJSON(ceiling[@"origin"]);
        poly.floorTransferMode=[floor[@"transferMode"] shortValue]; poly.ceilingTransferMode=[ceiling[@"transferMode"] shortValue]; poly.floorLightsourceObject=PFN3BObjectForRef(objects,floor[@"light"]); poly.ceilingLightsourceObject=PFN3BObjectForRef(objects,ceiling[@"light"]);
        poly.mediaObject=PFN3BObjectForRef(objects,j[@"media"]);
        poly.mediaLightsourceObject=poly.mediaObject ? poly.mediaObject.lightObject : nil;
        poly.ambientSoundObject=PFN3BObjectForRef(objects,j[@"ambientSound"]); poly.randomSoundObject=PFN3BObjectForRef(objects,j[@"randomSound"]);
        NSArray *edges=[j[@"edges"] isKindOfClass:NSArray.class]?j[@"edges"]:@[]; [poly setVertextCount:(short)edges.count];
        [edges enumerateObjectsUsingBlock:^(NSDictionary *edge,NSUInteger edgeIndex,BOOL *edgeStop){
            LELine *line=PFN3BObjectForRef(objects,edge[@"line"]); LESide *side=PFN3BObjectForRef(objects,edge[@"side"]); BOOL forward=[edge[@"direction"] isEqual:@"forward"];
            LEMapPoint *from=forward?line.mapPoint1:line.mapPoint2; [poly setVertexWithObject:from toIndex:(short)edgeIndex]; [poly setLinesObject:line toIndex:(short)edgeIndex]; [poly setSidesObject:side toIndex:(short)edgeIndex];
        }];
    }];

    // Restore non-authoritative Cocoa/legacy cache hints only after the
    // canonical graph and directional ownership relationships exist.
    [lineJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        NSDictionary *legacy=[lineDerived[j[@"id"]] isKindOfClass:NSDictionary.class]?lineDerived[j[@"id"]]:nil;
        if(!legacy)return; LELine *line=lines[i]; line.highestAdjacentFloor=[legacy[@"highestAdjacentFloor"] shortValue]; line.lowestAdjacentCeiling=[legacy[@"lowestAdjacentCeiling"] shortValue];
    }];
    [sideJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        NSDictionary *legacy=[sideDerived[j[@"id"]] isKindOfClass:NSDictionary.class]?sideDerived[j[@"id"]]:nil; NSDictionary *z=[legacy[@"exclusionZone"] isKindOfClass:NSDictionary.class]?legacy[@"exclusionZone"]:nil;
        if(!z)return; side_exclusion_zone zone={0}; zone.e0=PFN3BPoint2FromJSON(z[@"e0"]); zone.e1=PFN3BPoint2FromJSON(z[@"e1"]); zone.e2=PFN3BPoint2FromJSON(z[@"e2"]); zone.e3=PFN3BPoint2FromJSON(z[@"e3"]); ((LESide*)sides[i]).exclusionZone=zone;
    }];
    [polyJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        NSDictionary *legacy=[polygonDerived[j[@"id"]] isKindOfClass:NSDictionary.class]?polygonDerived[j[@"id"]]:nil;
        if(!legacy)return; LEPolygon *polygon=polys[i]; polygon.area=[legacy[@"area"] intValue]; polygon.center=PFN3BPoint2FromJSON(legacy[@"center"]); polygon.polygonConcaveFlag=[legacy[@"concave"] boolValue];
    }];

    [noteJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){ PhAnnotationNote *note=notes[i]; note.type=[j[@"type"] shortValue]; note.location=PFN3BPoint2FromJSON(j[@"location"]); note.polygonObject=PFN3BObjectForRef(objects,j[@"polygon"]); note.text=[j[@"text"] isKindOfClass:NSString.class]?j[@"text"]:@""; [note updateBounds]; }];

    [layerJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        PhLayer *layer=layers[i]; layer.phName=[j[@"name"] isKindOfClass:NSString.class]?j[@"name"]:@"Layer"; NSColor *color=PFN3BColorFromJSON(j[@"color"]); if(color)layer.layerColor=color;
        for(id ref in ([j[@"members"] isKindOfClass:NSArray.class]?j[@"members"]:@[])){ id member=PFN3BObjectForRef(objects,ref); if(member){ [layer addObjectToLayer:member]; if([member isKindOfClass:LEPolygon.class])[(LEPolygon*)member setPolyLayer:layer]; } }
    }];
    [groupJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        PhNoteGroup *group=groups[i]; group.phName=[j[@"name"] isKindOfClass:NSString.class]?j[@"name"]:@"Note Group"; group.visible=[j[@"visible"] boolValue]; NSColor *color=PFN3BColorFromJSON(j[@"color"]); if(color)group.color=color;
        for(id ref in ([j[@"members"] isKindOfClass:NSArray.class]?j[@"members"]:@[])){ PhAnnotationNote *note=PFN3BObjectForRef(objects,ref); if(note){ [group addObject:note]; note.group=group; } }
    }];

    [terminalJSON enumerateObjectsUsingBlock:^(NSDictionary *j,NSUInteger i,BOOL *stop){
        Terminal *terminal=terminals[i]; terminal.lineCount=[j[@"linesPerPage"] shortValue]; NSDictionary *legacy=[terminalLegacy[j[@"id"]] isKindOfClass:NSDictionary.class]?terminalLegacy[j[@"id"]]:@{}; terminal.flags=(PhTerminalFlags)[legacy[@"flags"] unsignedShortValue]; PFN3BSetValue(terminal,@"textEncoded",legacy[@"textEncoded"]?:@NO);
        NSMutableArray *sections=[NSMutableArray array]; NSDictionary *legacySections=[legacy[@"sections"] isKindOfClass:NSDictionary.class]?legacy[@"sections"]:@{};
        for(NSDictionary *sj in ([j[@"sections"] isKindOfClass:NSArray.class]?j[@"sections"]:@[])){
            TerminalSection *section=[[TerminalSection alloc] init]; NSString *sid=PFN3BStringRef(sj[@"id"]); if(!sid.length||objects[sid])continue; PFN2ASetId(section,sid); objects[sid]=section; [level setUpArrayPointersFor:section];
            section.type=(PhTerminalSectionType)[sj[@"type"] shortValue]; NSDictionary *target=[sj[@"target"] isKindOfClass:NSDictionary.class]?sj[@"target"]:@{}; section.permutation=PFN3BTargetValue(target); section.permutationObject=PFN3BTargetObject(target,objects); PFN3BSetValue(section,@"flags",sj[@"flags"]?:@0);
            NSMutableAttributedString *text=PFN3BAttributedTerminalText(sj); NSMutableParagraphStyle *paragraph=[section setNameAndReturnStyleFromType]; if(paragraph&&text.length)[text addAttribute:NSParagraphStyleAttributeName value:paragraph range:NSMakeRange(0,text.length)]; [section setText:text];
            NSDictionary *slegacy=[legacySections[sid] isKindOfClass:NSDictionary.class]?legacySections[sid]:@{}; PFN3BSetValue(section,@"lines",slegacy[@"lines"]?:@(-1)); [sections addObject:section];
        }
        terminal.theSections=sections;
    }];

    NSDictionary *names=[editor[@"names"] isKindOfClass:NSDictionary.class]?editor[@"names"]:@{};
    for(NSString *identifier in names){ id object=objects[identifier]; NSString *name=[names[identifier] isKindOfClass:NSString.class]?names[identifier]:nil; if(object&&name.length&&[object respondsToSelector:@selector(setPhName:)])PFN3BSetValue(object,@"phName",name); }

    for(NSDictionary *override in ([editor[@"lineOverrides"] isKindOfClass:NSArray.class]?editor[@"lineOverrides"]:@[])){
        LELine *line=PFN3BObjectForRef(objects,override[@"line"]); if(!line)continue; [line setPermanentSetting:LELinePermanentUse to:[override[@"enabled"] boolValue]]; [line setPermanentSetting:LELinePermanentSolid to:[override[@"solid"] boolValue]]; [line setPermanentSetting:LELinePermanentTransparent to:[override[@"transparent"] boolValue]]; [line setPermanentSetting:LELinePermanentLandscape to:[override[@"landscape"] boolValue]]; [line setPermanentSetting:LELinePermanentNoSides to:[override[@"noSides"] boolValue]];
    }

    [level havePointsScanForLines];
    [level updateCounts];
    [level setUpArrayNamesForEveryObject];
    if (level.lights.count > 0) [level setupDefaultObjects];
    [level resetAdjacentPolygonAssociations];

    NSUInteger loadedCWPoly=0U, loadedCCPoly=0U, loadedCWSide=0U, loadedCCSide=0U;
    for (LELine *line in lines) {
        if (line.clockwisePolygonObject) ++loadedCWPoly;
        if (line.conterclockwisePolygonObject) ++loadedCCPoly;
        if (line.clockwisePolygonSideObject) ++loadedCWSide;
        if (line.counterclockwisePolygonSideObject) ++loadedCCSide;
    }
    NSLog(@"FORMAT-3B materialized line ownership: cwPoly=%lu ccPoly=%lu cwSide=%lu ccSide=%lu",
          (unsigned long)loadedCWPoly,
          (unsigned long)loadedCCPoly,
          (unsigned long)loadedCWSide,
          (unsigned long)loadedCCSide);

    PFN3BSetNil(level,@"currentLayer");
    id selected=PFN3BObjectForRef(objects,editor[@"currentLayer"]);
    if(selected&&[selected isKindOfClass:PhLayer.class]) [level setLayerModeTo:selected];
    else {
        PFN3BSetValue(level,@"layerPoints",[points mutableCopy]); PFN3BSetValue(level,@"layerLines",[lines mutableCopy]); PFN3BSetValue(level,@"layerPolys",[polys mutableCopy]);
        PFN3BSetValue(level,@"layerMapObjects",[mapObjects mutableCopy]); PFN3BSetValue(level,@"layerNotes",[notes mutableCopy]);
    }
    return level;
}

#endif /* PFHORGE_FORMAT_3B_CANONICAL_LEVEL_BRIDGE_INC */
