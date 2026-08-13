// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// This implementation is included once by MyOpenGLView2.mm through
// MyOpenGLView2.h. Keeping VM-3 in an include file avoids a risky Xcode project
// rewrite while the modern preview target structure is still being established.

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <simd/simd.h>
#import <objc/message.h>

#include "../Core/PreviewSceneBuilder.hpp"
#include "../Core/PreviewVisibility.hpp"
#include "../Core/PreviewTexture.hpp"
#include "../Core/PreviewCollision.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace {

using pfhorge::preview::PreviewScene;
using pfhorge::preview::PreviewSurface;
using pfhorge::preview::PreviewVertex;
using pfhorge::preview::SurfaceKind;
using pfhorge::preview::SurfaceTextureLayer;

enum class ClassicTextureFailure : std::uint8_t {
    None,
    InvalidDescriptor,
    NegativeCache,
    RepositoryUnavailable,
    ImageMissing,
    ImageConversionFailed,
    UploadFailed,
};

// TEX-1A classic Shapes texture rendering.
struct MetalPreviewVertex final {
    simd_float3 position;
    simd_float4 color;
    simd_float2 textureCoordinate;
};

struct MetalPreviewUniforms final {
    matrix_float4x4 viewProjection;
};

// VM-RENDER-FIDELITY-1
// Per-draw texture semantics. Keep this layout mirrored exactly in MSL.
struct MetalPreviewFragmentUniforms final {
    std::uint32_t useTexture;
    float opacity;
    std::int32_t transferMode;
    std::uint32_t landscape;

    float viewportWidth;
    float viewportHeight;
    float verticalFieldOfViewRadians;
    float aspectRatio;

    float cameraYaw;
    float cameraPitch;
    float textureAspectRatio;
    float padding;
};

struct MetalPreviewDrawRange final {
    NSUInteger vertexStart;
    NSUInteger vertexCount;
    NSUInteger textureIndex;
    float opacity;
    std::int32_t transferMode;
    bool landscape;
    bool textured;
    bool translucent;
};

[[nodiscard]] bool SurfaceIdentityMatches(
    const PreviewSurface& first,
    const PreviewSurface& second) noexcept
{
    return first.id.kind == second.id.kind &&
           first.id.owner == second.id.owner &&
           first.id.subpart == second.id.subpart &&
           first.polygonID == second.polygonID &&
           first.lineID == second.lineID &&
           first.sideID == second.sideID &&
           first.edgeIndex == second.edgeIndex &&
           first.textureLayer == second.textureLayer;
}

[[nodiscard]] NSString *PfhorgePreviewStableIDString(
    pfhorge::preview::StableID value)
{
    return value == pfhorge::preview::kInvalidPreviewID
        ? @"none"
        : [NSString stringWithFormat:@"%u",
            static_cast<unsigned int>(value)];
}

[[nodiscard]] NSString *PfhorgePreviewSurfaceKindName(
    SurfaceKind kind)
{
    switch (kind) {
        case SurfaceKind::Wall: return @"Wall";
        case SurfaceKind::Floor: return @"Floor";
        case SurfaceKind::Ceiling: return @"Ceiling";
        case SurfaceKind::Media: return @"Media";
        case SurfaceKind::Landscape: return @"Landscape";
        case SurfaceKind::Sprite: return @"Sprite";
        case SurfaceKind::EditorOverlay: return @"EditorOverlay";
    }
    return @"Unknown";
}

[[nodiscard]] NSString *PfhorgePreviewTextureLayerName(
    SurfaceTextureLayer layer)
{
    switch (layer) {
        case SurfaceTextureLayer::None: return @"None";
        case SurfaceTextureLayer::Floor: return @"Floor";
        case SurfaceTextureLayer::Ceiling: return @"Ceiling";
        case SurfaceTextureLayer::Primary: return @"Primary";
        case SurfaceTextureLayer::Secondary: return @"Secondary";
        case SurfaceTextureLayer::Transparent: return @"Transparent";
        case SurfaceTextureLayer::Media: return @"Media";
    }
    return @"Unknown";
}

[[nodiscard]] NSString *PfhorgePreviewTextureSourceName(
    pfhorge::preview::TextureDescriptorSource source)
{
    switch (source) {
        case pfhorge::preview::TextureDescriptorSource::None:
            return @"None";
        case pfhorge::preview::TextureDescriptorSource::PackedShape:
            return @"PackedShape";
        case pfhorge::preview::TextureDescriptorSource::LegacySplitFields:
            return @"LegacySplitFields";
        case pfhorge::preview::TextureDescriptorSource::MediaDefinition:
            return @"MediaDefinition";
    }
    return @"Unknown";
}

[[nodiscard]] NSString *PfhorgePreviewSideTypeName(
    std::int16_t sideType)
{
    switch (sideType) {
        case 0: return @"Full";
        case 1: return @"High";
        case 2: return @"Low";
        case 3: return @"Composite";
        case 4: return @"Split";
        default: return @"None";
    }
}

[[nodiscard]] NSString *PfhorgePreviewSurfaceDescription(
    const PreviewSurface& surface,
    std::int16_t environmentCode)
{
    return [NSString stringWithFormat:
        @"%@ poly=%@ edge=%u line=%@ side=%@ sideType=%@ "
         "sideFlags=0x%04x controlPanel=%@ panelType=%d mediaType=%d "
         "layer=%@ source=%@ rawTex=%d:%d tex=%d:%d "
         "envRemapped=%@ xfer=%d light=%d env=%d",
        PfhorgePreviewSurfaceKindName(surface.id.kind),
        PfhorgePreviewStableIDString(surface.polygonID),
        static_cast<unsigned int>(surface.edgeIndex),
        PfhorgePreviewStableIDString(surface.lineID),
        PfhorgePreviewStableIDString(surface.sideID),
        PfhorgePreviewSideTypeName(surface.sideType),
        static_cast<unsigned int>(surface.sideFlags),
        surface.isControlPanel ? @"yes" : @"no",
        static_cast<int>(surface.controlPanelType),
        static_cast<int>(surface.mediaType),
        PfhorgePreviewTextureLayerName(surface.textureLayer),
        PfhorgePreviewTextureSourceName(surface.texture.source),
        static_cast<int>(surface.texture.rawCollection),
        static_cast<int>(surface.texture.rawBitmap),
        static_cast<int>(surface.texture.collection),
        static_cast<int>(surface.texture.bitmap),
        surface.texture.environmentRemapped ? @"yes" : @"no",
        static_cast<int>(surface.texture.transferMode),
        static_cast<int>(surface.lightIndex),
        static_cast<int>(environmentCode)];
}

[[nodiscard]] bool RayTriangleIntersection(
    simd_float3 origin,
    simd_float3 direction,
    simd_float3 first,
    simd_float3 second,
    simd_float3 third,
    float& distance) noexcept
{
    constexpr float kEpsilon = 0.000001F;

    const simd_float3 edge1 = second - first;
    const simd_float3 edge2 = third - first;
    const simd_float3 h = simd_cross(direction, edge2);
    const float determinant = simd_dot(edge1, h);

    if (std::fabs(determinant) < kEpsilon) {
        return false;
    }

    const float inverseDeterminant = 1.0F / determinant;
    const simd_float3 s = origin - first;
    const float u = inverseDeterminant * simd_dot(s, h);
    if (u < 0.0F || u > 1.0F) {
        return false;
    }

    const simd_float3 q = simd_cross(s, edge1);
    const float v = inverseDeterminant * simd_dot(direction, q);
    if (v < 0.0F || u + v > 1.0F) {
        return false;
    }

    const float hitDistance =
        inverseDeterminant * simd_dot(edge2, q);
    if (hitDistance <= kEpsilon) {
        return false;
    }

    distance = hitDistance;
    return true;
}

[[nodiscard]] matrix_float4x4 MakePerspectiveMatrix(
    float verticalFieldOfViewRadians,
    float aspectRatio,
    float nearPlane,
    float farPlane) noexcept
{
    const float yScale =
        1.0F / std::tan(verticalFieldOfViewRadians * 0.5F);
    const float xScale = yScale / aspectRatio;
    const float zScale = farPlane / (nearPlane - farPlane);

    return matrix_float4x4{
        simd_make_float4(xScale, 0.0F, 0.0F, 0.0F),
        simd_make_float4(0.0F, yScale, 0.0F, 0.0F),
        simd_make_float4(0.0F, 0.0F, zScale, -1.0F),
        simd_make_float4(
            0.0F,
            0.0F,
            nearPlane * zScale,
            0.0F),
    };
}

[[nodiscard]] matrix_float4x4 MakeLookAtMatrix(
    simd_float3 eye,
    simd_float3 target,
    simd_float3 up) noexcept
{
    const simd_float3 forward =
        simd_normalize(eye - target);
    simd_float3 right = simd_cross(up, forward);

    if (simd_length_squared(right) < 0.000001F) {
        right = simd_make_float3(1.0F, 0.0F, 0.0F);
    } else {
        right = simd_normalize(right);
    }

    const simd_float3 correctedUp =
        simd_cross(forward, right);

    return matrix_float4x4{
        simd_make_float4(
            right.x,
            correctedUp.x,
            forward.x,
            0.0F),
        simd_make_float4(
            right.y,
            correctedUp.y,
            forward.y,
            0.0F),
        simd_make_float4(
            right.z,
            correctedUp.z,
            forward.z,
            0.0F),
        simd_make_float4(
            -simd_dot(right, eye),
            -simd_dot(correctedUp, eye),
            -simd_dot(forward, eye),
            1.0F),
    };
}

[[nodiscard]] simd_float4 ColorForSurface(
    const PreviewSurface& surface) noexcept
{
    const float variation =
        static_cast<float>((surface.id.owner * 37U) % 11U) /
        100.0F;

    if (surface.textureLayer == SurfaceTextureLayer::Transparent) {
        return simd_make_float4(0.10F, 0.72F, 0.78F, 0.45F);
    }

    switch (surface.id.kind) {
        case SurfaceKind::Floor:
            return simd_make_float4(
                0.20F + variation,
                0.36F + variation,
                0.48F + variation,
                1.0F);

        case SurfaceKind::Ceiling:
            return simd_make_float4(
                0.40F + variation,
                0.31F + variation,
                0.24F + variation,
                1.0F);

        case SurfaceKind::Wall:
            return simd_make_float4(
                0.48F + variation,
                0.50F + variation,
                0.54F + variation,
                1.0F);

        case SurfaceKind::Media:
            return simd_make_float4(
                0.10F,
                0.38F,
                0.58F,
                0.65F);

        case SurfaceKind::Landscape:
            return simd_make_float4(
                0.24F,
                0.29F,
                0.38F,
                1.0F);

        case SurfaceKind::Sprite:
            return simd_make_float4(
                0.80F,
                0.60F,
                0.20F,
                1.0F);

        case SurfaceKind::EditorOverlay:
            return simd_make_float4(
                1.0F,
                0.75F,
                0.15F,
                1.0F);
    }

    return simd_make_float4(1.0F, 0.0F, 1.0F, 1.0F);
}

[[nodiscard]] NSString *MetalPreviewShaderSource()
{
    static NSString *source = [NSString stringWithUTF8String:R"MSL(
#include <metal_stdlib>
using namespace metal;

struct PreviewVertex {
    float3 position;
    float4 color;
    float2 textureCoordinate;
};

struct PreviewUniforms {
    float4x4 viewProjection;
};

struct PreviewFragmentUniforms {
    uint useTexture;
    float opacity;
    int transferMode;
    uint landscape;

    float viewportWidth;
    float viewportHeight;
    float verticalFieldOfViewRadians;
    float aspectRatio;

    float cameraYaw;
    float cameraPitch;
    float textureAspectRatio;
    float padding;
};

struct RasterData {
    float4 position [[position]];
    float4 color;
    float2 textureCoordinate;
};

vertex RasterData pfhorgePreviewVertex(
    uint vertexID [[vertex_id]],
    constant PreviewVertex *vertices [[buffer(0)]],
    constant PreviewUniforms& uniforms [[buffer(1)]])
{
    RasterData output;
    const PreviewVertex input = vertices[vertexID];
    output.position =
        uniforms.viewProjection *
        float4(input.position, 1.0);
    output.color = input.color;
    output.textureCoordinate = input.textureCoordinate;
    return output;
}

fragment float4 pfhorgePreviewFragment(
    RasterData input [[stage_in]],
    texture2d<float> classicTexture [[texture(0)]],
    sampler classicSampler [[sampler(0)]],
    constant PreviewFragmentUniforms& uniforms [[buffer(0)]])
{
    float4 output = input.color;
    if (uniforms.useTexture != 0) {
        float2 uv = input.textureCoordinate;

        if (uniforms.landscape != 0) {
            /*
             * Classic Marathon landscape projection.
             *
             * The wall triangle still supplies the visibility/depth mask, but
             * texture lookup comes from the view instead of the wall's physical
             * UV span. Translation therefore does not "walk toward" the sky.
             *
             * Aleph One's default landscape setup uses two horizontal repeats,
             * a perspective-style zoom of 1.2, yaw/pitch compensation and
             * vertical clamping for Marathon 2/Infinity-style landscapes.
             */
            const float pi = 3.14159265358979323846;
            const float zoom = 1.2;
            const float pitchAdjust = 0.96;

            const float viewportWidth =
                max(uniforms.viewportWidth, 1.0);
            const float viewportHeight =
                max(uniforms.viewportHeight, 1.0);

            float2 ndc;
            ndc.x =
                ((input.position.x / viewportWidth) * 2.0) - 1.0;
            ndc.y =
                1.0 - ((input.position.y / viewportHeight) * 2.0);

            const float tanHalfFov =
                tan(max(0.05, uniforms.verticalFieldOfViewRadians) * 0.5);
            const float viewX =
                ndc.x * max(uniforms.aspectRatio, 0.01) * tanHalfFov;
            const float viewY =
                ndc.y * tanHalfFov;

            const float horizontal =
                (uniforms.cameraYaw / pi) +
                (viewX / (zoom * pi));

            const float verticalScale =
                max(uniforms.textureAspectRatio, 1.0) / pi;
            const float vertical =
                0.5 -
                ((viewY / zoom) +
                 sin(uniforms.cameraPitch) * pitchAdjust) *
                verticalScale;

            uv = float2(
                fract(horizontal),
                clamp(vertical, 0.001, 0.999));
        }

        output = classicTexture.sample(
            classicSampler,
            uv);
        output.rgb *= input.color.rgb;
    }
    output.a *= uniforms.opacity;
    return output;
}
)MSL"];

    return source;
}


[[nodiscard]] id<MTLDevice> PfhorgePreferredMetalDevice()
{
    PfhorgeRegisterVisualModeDefaults();
    const unsigned long long preferred =
        [[[NSUserDefaults standardUserDefaults]
            objectForKey:PfhorgeVMPreferredMetalRegistryIDPreference]
            unsignedLongLongValue];
    if (preferred != 0ULL) {
        for (id<MTLDevice> device in PfhorgeAvailableMetalDevices()) {
            if (device.registryID == preferred) {
                return device;
            }
        }
    }
    return MTLCreateSystemDefaultDevice();
}

}  // namespace

@interface PfhorgeMetalPreviewView ()

- (simd_float3)portalForwardVector;
- (void)rebuildPreviewFrame;
- (void)positionPortalCameraAtPreferredStart;
- (void)validatePortalCameraPosition;
- (void)logPortalDiagnostics;
- (void)applyVisualModeSettings;
- (void)rebuildSamplerState;
- (void)clearClassicTextureCache;
- (id<MTLTexture>)fallbackTexture;
- (id<MTLTexture>)classicTextureForCollection:(std::int16_t)collection
                                       bitmap:(std::int16_t)bitmap;
- (void)contentSelectionDidChange:(NSNotification *)notification;
- (void)rebuildSceneFromSourcePreservingCamera:(BOOL)preserveCamera
                                        reason:(NSString *)reason;
- (void)checkForLiveLevelChangesAtTime:(NSTimeInterval)time;
- (BOOL)updatePlatformSimulationForDelta:(float)delta;
- (void)toggleNearestDoor;
- (void)levelEnvironmentDidChange:(NSNotification *)notification;
- (void)visualModeSettingsDidChange:(NSNotification *)notification;
- (void)windowDidBecomeKey:(NSNotification *)notification;
- (void)windowDidResignKey:(NSNotification *)notification;
- (void)windowDidChangeScreen:(NSNotification *)notification;
- (unichar)configuredKeyForPreference:(NSString *)preference fallback:(unichar)fallback;
- (BOOL)pressedPreference:(NSString *)preference fallback:(unichar)fallback;
- (void)clearPressedKeys;
- (void)updateCameraForFrame;
- (void)updateDiagnosticsOverlay;
- (void)pickSurfaceAtViewPoint:(NSPoint)location;
- (void)rebuildVertexBuffer;
- (void)resetCamera;
- (matrix_float4x4)viewProjectionMatrix;

@end

@implementation PfhorgeMetalPreviewView {
    PreviewScene _previewScene;
    pfhorge::preview::PreviewFrame _previewFrame;
    std::vector<MetalPreviewVertex> _metalVertices;
    std::vector<MetalPreviewDrawRange> _metalDrawRanges;
    NSMutableDictionary *_classicTextureCache;
    NSMutableSet *_missingTextureKeys;
    NSMutableArray *_drawTextures;
    id<MTLTexture> _fallbackTexture;
    id<MTLSamplerState> _samplerState;
    NSUInteger _texturedSurfaceCount;
    NSUInteger _fallbackSurfaceCount;
    NSUInteger _invalidTextureDescriptorCount;
    NSUInteger _negativeTextureCacheHitCount;
    NSUInteger _textureRepositoryFailureCount;
    NSUInteger _textureImageMissingCount;
    NSUInteger _textureImageConversionFailureCount;
    NSUInteger _textureUploadFailureCount;
    ClassicTextureFailure _lastClassicTextureFailure;
    BOOL _attemptedTextureRepositoryReload;


    __weak LELevelData *_sourceLevelData;
    std::uint64_t _sourceLevelFingerprint;
    NSTimeInterval _lastLiveSyncCheck;
    std::vector<float> _platformExtensions;
    std::vector<float> _platformTargets;
    std::vector<unsigned char> _platformMoving;
    BOOL _collisionEnabled;
    BOOL _liveLevelSync;
    BOOL _followLevelEnvironment;
    BOOL _sceneRebuildInProgress;
    NSUInteger _sceneRebuildCount;
    NSString *_interactionStatus;
    BOOL _portalModeEnabled;
    simd_float3 _portalCameraPosition;
    pfhorge::preview::StableID _portalSeedPolygonID;
    pfhorge::preview::PreviewTraversalDiagnostics _portalDiagnostics;
    NSString *_portalCameraSource;
    float _portalMoveSpeed;

    // CONTENT-1A continuous input and persistent renderer settings.
    NSMutableSet<NSNumber *> *_pressedKeys;
    NSTimeInterval _lastFrameTimestamp;
    NSTimeInterval _lastFrameDelta;
    double _smoothedFPS;
    float _movementSpeed;

    // CONTENT-1A.1 independent mouse axes and camera tuning.
    float _mouseSensitivityX;
    float _mouseSensitivityY;
    float _lookSmoothing;
    float _smoothedMouseDeltaX;
    float _smoothedMouseDeltaY;
    float _verticalMovementScale;
    float _nearPlane;
    float _fieldOfViewDegrees;
    float _renderScale;
    BOOL _invertMouseX;
    BOOL _invertMouseY;
    NSTextField *_diagnosticsLabel;

    id<MTLCommandQueue> _commandQueue;
    id<MTLRenderPipelineState> _pipelineState;
    id<MTLDepthStencilState> _depthStencilState;
    id<MTLDepthStencilState> _translucentDepthStencilState;
    id<MTLBuffer> _vertexBuffer;

    simd_float3 _cameraTarget;
    float _cameraYaw;
    float _cameraPitch;
    float _cameraDistance;

    NSPoint _lastDragLocation;
    NSPoint _mouseDownLocation;
    float _mouseDragDistanceSquared;
    std::optional<PreviewSurface> _selectedSurface;
}

- (instancetype)initWithFrame:(NSRect)frameRect
                    levelData:(LELevelData *)levelData
{
    PfhorgeRegisterVisualModeDefaults();
    id<MTLDevice> device = PfhorgePreferredMetalDevice();

    if (device == nil) {
        NSLog(@"Metal Visual Mode unavailable: no Metal device.");
        return nil;
    }

    self = [super initWithFrame:frameRect device:device];

    if (self == nil) {
        return nil;
    }

    self.autoresizingMask =
        NSViewWidthSizable |
        NSViewHeightSizable;
    self.colorPixelFormat =
        MTLPixelFormatBGRA8Unorm_sRGB;
    self.depthStencilPixelFormat =
        MTLPixelFormatDepth32Float;
    self.clearColor =
        MTLClearColorMake(0.025, 0.03, 0.04, 1.0);
    self.enableSetNeedsDisplay = NO;
    self.paused = NO;
    self.delegate = self;

    _pressedKeys = [NSMutableSet set];
    _classicTextureCache = [NSMutableDictionary dictionary];
    _missingTextureKeys = [NSMutableSet set];
    _drawTextures = [NSMutableArray array];
    _lastClassicTextureFailure = ClassicTextureFailure::None;
    _attemptedTextureRepositoryReload = NO;
    _portalMoveSpeed = 1.0F;
    [self applyVisualModeSettings];

    _diagnosticsLabel = [[NSTextField alloc]
        initWithFrame:NSMakeRect(12.0, frameRect.size.height - 158.0, 920.0, 146.0)];
    _diagnosticsLabel.editable = NO;
    _diagnosticsLabel.selectable = YES;
    _diagnosticsLabel.bezeled = NO;
    _diagnosticsLabel.drawsBackground = YES;
    _diagnosticsLabel.backgroundColor =
        [NSColor colorWithCalibratedWhite:0.0 alpha:0.66];
    _diagnosticsLabel.textColor = NSColor.whiteColor;
    _diagnosticsLabel.font = [NSFont userFixedPitchFontOfSize:11.0];
    _diagnosticsLabel.usesSingleLineMode = NO;
    _diagnosticsLabel.autoresizingMask =
        NSViewMaxXMargin | NSViewMinYMargin;
    [self addSubview:_diagnosticsLabel];
    [self applyVisualModeSettings];

    _commandQueue = [device newCommandQueue];

    if (_commandQueue == nil ||
        ![self buildPipelineForDevice:device]) {
        return nil;
    }

    _sourceLevelData = levelData;
    _portalModeEnabled = YES;
    _portalMoveSpeed = 1.0F;
    _interactionStatus = @"Ready";
    [self rebuildSceneFromSourcePreservingCamera:NO
                                          reason:@"initial snapshot"];
    [self resetCamera];

    NSLog(
        @"Metal Visual Mode VM-3: %zu polygons, "
         "%zu portals, %zu surfaces, %zu rendered vertices",
        _previewScene.polygons.size(),
        _previewScene.portals.size(),
        _previewScene.surfaces.size(),
        _metalVertices.size());
    [self logPortalDiagnostics];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(visualModeSettingsDidChange:)
               name:PfhorgeVisualModeSettingsDidChangeNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(contentSelectionDidChange:)
               name:@"PfhorgeContentSelectionDidChangeNotification"
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(levelEnvironmentDidChange:)
               name:PfhorgeLevelEnvironmentDidChangeNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowDidBecomeKey:)
               name:NSWindowDidBecomeKeyNotification
             object:self.window];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowDidResignKey:)
               name:NSWindowDidResignKeyNotification
             object:self.window];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(windowDidChangeScreen:)
               name:NSWindowDidChangeScreenNotification
             object:nil];

    return self;
}

- (BOOL)buildPipelineForDevice:(id<MTLDevice>)device
{
    NSError *libraryError = nil;
    id<MTLLibrary> library =
        [device newLibraryWithSource:
                    MetalPreviewShaderSource()
                               options:nil
                                 error:&libraryError];

    if (library == nil) {
        NSLog(
            @"Metal Visual Mode shader compilation failed: %@",
            libraryError);
        return NO;
    }

    id<MTLFunction> vertexFunction =
        [library newFunctionWithName:
                     @"pfhorgePreviewVertex"];
    id<MTLFunction> fragmentFunction =
        [library newFunctionWithName:
                     @"pfhorgePreviewFragment"];

    if (vertexFunction == nil ||
        fragmentFunction == nil) {
        NSLog(
            @"Metal Visual Mode could not load shader functions.");
        return NO;
    }

    MTLRenderPipelineDescriptor *pipelineDescriptor =
        [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.label =
        @"Pfhorge TEX-1A Classic Shapes Geometry";
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    MTLRenderPipelineColorAttachmentDescriptor *colorAttachment =
        pipelineDescriptor.colorAttachments[0];
    colorAttachment.pixelFormat = self.colorPixelFormat;
    colorAttachment.blendingEnabled = YES;
    colorAttachment.rgbBlendOperation = MTLBlendOperationAdd;
    colorAttachment.alphaBlendOperation = MTLBlendOperationAdd;
    colorAttachment.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    colorAttachment.destinationRGBBlendFactor =
        MTLBlendFactorOneMinusSourceAlpha;
    colorAttachment.sourceAlphaBlendFactor = MTLBlendFactorOne;
    colorAttachment.destinationAlphaBlendFactor =
        MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.depthAttachmentPixelFormat =
        self.depthStencilPixelFormat;
    pipelineDescriptor.rasterSampleCount = self.sampleCount;

    NSError *pipelineError = nil;
    _pipelineState =
        [device newRenderPipelineStateWithDescriptor:
                    pipelineDescriptor
                                               error:
                    &pipelineError];

    if (_pipelineState == nil) {
        NSLog(
            @"Metal Visual Mode pipeline creation failed: %@",
            pipelineError);
        return NO;
    }

    MTLDepthStencilDescriptor *depthDescriptor =
        [[MTLDepthStencilDescriptor alloc] init];
    depthDescriptor.depthCompareFunction =
        MTLCompareFunctionLess;
    depthDescriptor.depthWriteEnabled = YES;

    _depthStencilState =
        [device newDepthStencilStateWithDescriptor:
                    depthDescriptor];
    depthDescriptor.depthWriteEnabled = NO;
    _translucentDepthStencilState =
        [device newDepthStencilStateWithDescriptor:
                    depthDescriptor];

    return _depthStencilState != nil &&
           _translucentDepthStencilState != nil;
}



- (void)applyVisualModeSettings
{
    PfhorgeRegisterVisualModeDefaults();
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    _collisionEnabled =
        [defaults boolForKey:PfhorgeVMCollisionModePreference];
    _liveLevelSync =
        [defaults boolForKey:PfhorgeVMLiveLevelSyncPreference];
    _followLevelEnvironment =
        [defaults boolForKey:PfhorgeVMFollowLevelEnvironmentPreference];

    _movementSpeed = std::clamp(
        static_cast<float>([defaults doubleForKey:PfhorgeVMMovementSpeedPreference]),
        0.05F,
        100.0F);
    _verticalMovementScale = std::clamp(
        static_cast<float>([defaults doubleForKey:PfhorgeVMVerticalMovementScalePreference]),
        0.1F,
        3.0F);
    _mouseSensitivityX = std::clamp(
        static_cast<float>([defaults doubleForKey:PfhorgeVMMouseSensitivityXPreference]),
        0.0005F,
        0.10F);
    _mouseSensitivityY = std::clamp(
        static_cast<float>([defaults doubleForKey:PfhorgeVMMouseSensitivityYPreference]),
        0.0005F,
        0.10F);
    _invertMouseX = [defaults boolForKey:PfhorgeVMInvertMouseXPreference];
    _invertMouseY = [defaults boolForKey:PfhorgeVMInvertMouseYPreference];
    _lookSmoothing = std::clamp(
        static_cast<float>([defaults doubleForKey:PfhorgeVMLookSmoothingPreference]),
        0.0F,
        0.85F);
    _nearPlane = std::clamp(
        static_cast<float>([defaults doubleForKey:PfhorgeVMNearPlanePreference]),
        0.002F,
        0.10F);
    _fieldOfViewDegrees = std::clamp(
        static_cast<float>([defaults doubleForKey:PfhorgeVMFieldOfViewPreference]),
        30.0F,
        130.0F);
    _renderScale = std::clamp(
        static_cast<float>([defaults doubleForKey:PfhorgeVMRenderScalePreference]),
        0.25F,
        2.0F);

    // CONTENT-1A.2 dynamic display frame rate. A stored value of zero
    // means follow the current screen's maximum refresh rate.
    self.preferredFramesPerSecond = PfhorgeResolvedVisualModeFrameRate(
        self.window.screen ?: NSApp.mainWindow.screen ?: NSScreen.mainScreen);

    NSInteger requestedSamples =
        [defaults integerForKey:PfhorgeVMMSAASampleCountPreference];
    if (requestedSamples != 1 && requestedSamples != 2 &&
        requestedSamples != 4 && requestedSamples != 8) {
        requestedSamples = 1;
    }
    while (requestedSamples > 1 &&
           ![self.device supportsTextureSampleCount:requestedSamples]) {
        requestedSamples /= 2;
    }
    if (_pipelineState == nil) {
        self.sampleCount = requestedSamples;
    }

    if ([self.layer isKindOfClass:CAMetalLayer.class]) {
        CAMetalLayer *metalLayer = (CAMetalLayer *)self.layer;
        if ([metalLayer respondsToSelector:@selector(setDisplaySyncEnabled:)]) {
            metalLayer.displaySyncEnabled =
                [defaults boolForKey:PfhorgeVMVSyncPreference];
        }
    }

    if (_diagnosticsLabel != nil) {
        _diagnosticsLabel.hidden =
            ![defaults boolForKey:PfhorgeVMDiagnosticsOverlayPreference];
    }
    [self rebuildSamplerState];
}

- (void)rebuildSamplerState
{
    MTLSamplerDescriptor *descriptor =
        [[MTLSamplerDescriptor alloc] init];
    descriptor.label = @"Pfhorge TEX-1A Classic Shapes Sampler";
    descriptor.sAddressMode = MTLSamplerAddressModeRepeat;
    descriptor.tAddressMode = MTLSamplerAddressModeRepeat;
    descriptor.rAddressMode = MTLSamplerAddressModeRepeat;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    const NSInteger filtering =
        [defaults integerForKey:PfhorgeVMTextureFilteringPreference];
    const NSInteger requestedAnisotropy = std::clamp<NSInteger>(
        [defaults integerForKey:PfhorgeVMAnisotropyPreference],
        1,
        16);

    if (filtering <= 0) {
        descriptor.minFilter = MTLSamplerMinMagFilterNearest;
        descriptor.magFilter = MTLSamplerMinMagFilterNearest;
        descriptor.mipFilter = MTLSamplerMipFilterNotMipmapped;
        descriptor.maxAnisotropy = 1U;
    } else {
        descriptor.minFilter = MTLSamplerMinMagFilterLinear;
        descriptor.magFilter = MTLSamplerMinMagFilterLinear;
        descriptor.mipFilter = filtering >= 2
            ? MTLSamplerMipFilterLinear
            : MTLSamplerMipFilterNotMipmapped;
        descriptor.maxAnisotropy =
            static_cast<NSUInteger>(requestedAnisotropy);
    }

    _samplerState =
        [self.device newSamplerStateWithDescriptor:descriptor];
}

- (void)clearClassicTextureCache
{
    [_classicTextureCache removeAllObjects];
    [_missingTextureKeys removeAllObjects];
    _lastClassicTextureFailure = ClassicTextureFailure::None;
    _attemptedTextureRepositoryReload = NO;
}

- (id<MTLTexture>)fallbackTexture
{
    if (_fallbackTexture != nil) {
        return _fallbackTexture;
    }

    MTLTextureDescriptor *descriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
            MTLPixelFormatRGBA8Unorm_sRGB
                                                     width:1U
                                                    height:1U
                                                 mipmapped:NO];
    descriptor.usage = MTLTextureUsageShaderRead;
    descriptor.storageMode = MTLStorageModeShared;
    _fallbackTexture = [self.device newTextureWithDescriptor:descriptor];
    _fallbackTexture.label = @"Pfhorge TEX-1A White Fallback";
    const std::uint8_t pixel[4] = {255U, 255U, 255U, 255U};
    [_fallbackTexture replaceRegion:MTLRegionMake2D(0U, 0U, 1U, 1U)
                        mipmapLevel:0U
                          withBytes:pixel
                        bytesPerRow:sizeof(pixel)];
    return _fallbackTexture;
}

- (id<MTLTexture>)classicTextureForCollection:(std::int16_t)collection
                                       bitmap:(std::int16_t)bitmap
{
    _lastClassicTextureFailure = ClassicTextureFailure::None;
    const pfhorge::preview::PreviewTextureKey key{
        pfhorge::preview::NormalizeClassicCollection(collection),
        bitmap,
    };
    if (!key.valid() ||
        !pfhorge::preview::IsClassicTextureCollection(key.collection)) {
        _lastClassicTextureFailure =
            ClassicTextureFailure::InvalidDescriptor;
        return nil;
    }

    NSNumber *cacheKey = @(key.packed());
    id<MTLTexture> cached = _classicTextureCache[cacheKey];
    if (cached != nil) {
        return cached;
    }
    if ([_missingTextureKeys containsObject:cacheKey]) {
        _lastClassicTextureFailure =
            ClassicTextureFailure::NegativeCache;
        return nil;
    }

    Class repositoryClass = NSClassFromString(@"TextureRepository");
    if (repositoryClass == Nil) {
        repositoryClass = NSClassFromString(@"Pfhorge.TextureRepository");
    }
    SEL sharedSelector = NSSelectorFromString(@"sharedTextureRepository");
    SEL textureSelector =
        NSSelectorFromString(@"textureForCollection:bitmap:");
    if (repositoryClass == Nil ||
        ![repositoryClass respondsToSelector:sharedSelector]) {
        _lastClassicTextureFailure =
            ClassicTextureFailure::RepositoryUnavailable;
        [_missingTextureKeys addObject:cacheKey];
        return nil;
    }

    typedef id (*PfhorgeNoArgumentObjectMessage)(id, SEL);
    typedef id (*PfhorgeTwoIntegerObjectMessage)(
        id,
        SEL,
        std::int32_t,
        std::int32_t);
    typedef void (*PfhorgeVoidNoArgumentMessage)(id, SEL);

    id repository =
        ((PfhorgeNoArgumentObjectMessage)objc_msgSend)(
            (id)repositoryClass,
            sharedSelector);
    if (repository == nil ||
        ![repository respondsToSelector:textureSelector]) {
        _lastClassicTextureFailure =
            ClassicTextureFailure::RepositoryUnavailable;
        [_missingTextureKeys addObject:cacheKey];
        return nil;
    }

    NSImage *image =
        ((PfhorgeTwoIntegerObjectMessage)objc_msgSend)(
            repository,
            textureSelector,
            static_cast<std::int32_t>(key.collection),
            static_cast<std::int32_t>(key.bitmap));

    if (image == nil && !_attemptedTextureRepositoryReload) {
        _attemptedTextureRepositoryReload = YES;
        SEL reloadSelector = NSSelectorFromString(@"reloadClassicTextures");
        if ([repository respondsToSelector:reloadSelector]) {
            ((PfhorgeVoidNoArgumentMessage)objc_msgSend)(
                repository,
                reloadSelector);
            image =
                ((PfhorgeTwoIntegerObjectMessage)objc_msgSend)(
                    repository,
                    textureSelector,
                    static_cast<std::int32_t>(key.collection),
                    static_cast<std::int32_t>(key.bitmap));
        }
    }

    if (image == nil) {
        _lastClassicTextureFailure = ClassicTextureFailure::ImageMissing;
        [_missingTextureKeys addObject:cacheKey];
        return nil;
    }

    NSRect proposedRect = NSZeroRect;
    CGImageRef imageReference =
        [image CGImageForProposedRect:&proposedRect
                              context:nil
                                hints:nil];
    if (imageReference == nil) {
        _lastClassicTextureFailure =
            ClassicTextureFailure::ImageConversionFailed;
        [_missingTextureKeys addObject:cacheKey];
        return nil;
    }

    NSError *textureError = nil;
    MTKTextureLoader *loader =
        [[MTKTextureLoader alloc] initWithDevice:self.device];
    NSDictionary *options = @{
        MTKTextureLoaderOptionSRGB: @YES,
        MTKTextureLoaderOptionGenerateMipmaps: @YES,
        MTKTextureLoaderOptionOrigin: MTKTextureLoaderOriginTopLeft,
    };
    id<MTLTexture> texture =
        [loader newTextureWithCGImage:imageReference
                              options:options
                                error:&textureError];
    if (texture == nil) {
        _lastClassicTextureFailure = ClassicTextureFailure::UploadFailed;
        NSLog(
            @"TEX-1A.2 could not upload collection %d bitmap %d: %@",
            key.collection,
            key.bitmap,
            textureError);
        [_missingTextureKeys addObject:cacheKey];
        return nil;
    }

    texture.label = [NSString stringWithFormat:
        @"Classic Shapes c%d b%d",
        key.collection,
        key.bitmap];
    _classicTextureCache[cacheKey] = texture;
    return texture;
}

- (void)contentSelectionDidChange:(NSNotification *)notification
{
    (void)notification;
    [self clearClassicTextureCache];
    _interactionStatus = @"Active Shapes content reloaded";
    [self rebuildSceneFromSourcePreservingCamera:YES
                                          reason:@"content selection"];
}

- (void)visualModeSettingsDidChange:(NSNotification *)notification
{
    (void)notification;
    [self clearClassicTextureCache];
    [self applyVisualModeSettings];
    [self rebuildSceneFromSourcePreservingCamera:YES
                                          reason:@"Visual Mode settings"];
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
    if (notification.object != self.window) {
        return;
    }

    // Window controllers and sheets can replace the first responder after the
    // Metal view enters a window. Reassert it on the next run-loop turn.
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self.window.isKeyWindow) {
            [self.window makeFirstResponder:self];
        }
    });
}

- (void)windowDidResignKey:(NSNotification *)notification
{
    if (notification.object == self.window) {
        [self clearPressedKeys];
    }
}

- (void)windowDidChangeScreen:(NSNotification *)notification
{
    if (notification.object != self.window) {
        return;
    }

    // Re-resolve Display Maximum whenever the Visual Mode window moves
    // between monitors, including ProMotion and 240 Hz external displays.
    [self applyVisualModeSettings];
}

- (unichar)configuredKeyForPreference:(NSString *)preference
                             fallback:(unichar)fallback
{
    return PfhorgeVisualModeKey(preference, fallback);
}

- (BOOL)pressedPreference:(NSString *)preference
                 fallback:(unichar)fallback
{
    const unichar key = [self configuredKeyForPreference:preference
                                                fallback:fallback];
    return [_pressedKeys containsObject:@(key)];
}

- (void)clearPressedKeys
{
    [_pressedKeys removeAllObjects];
}

- (void)updateCameraForFrame
{
    const NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
    if (_lastFrameTimestamp <= 0.0) {
        _lastFrameTimestamp = now;
        _lastFrameDelta = 1.0 / 60.0;
        [self checkForLiveLevelChangesAtTime:now];
        return;
    }

    _lastFrameDelta = std::clamp(
        now - _lastFrameTimestamp,
        0.0,
        0.10);
    _lastFrameTimestamp = now;
    [self checkForLiveLevelChangesAtTime:now];
    [self updatePlatformSimulationForDelta:
        static_cast<float>(_lastFrameDelta)];

    if (_pressedKeys.count == 0 || _lastFrameDelta <= 0.0) {
        return;
    }

    simd_float3 forward = simd_make_float3(
        std::sin(_cameraYaw),
        0.0F,
        -std::cos(_cameraYaw));
    if (simd_length_squared(forward) > 0.000001F) {
        forward = simd_normalize(forward);
    }
    const simd_float3 right = simd_make_float3(
        -forward.z,
        0.0F,
        forward.x);
    const float step =
        _movementSpeed * _portalMoveSpeed *
        static_cast<float>(_lastFrameDelta);

    simd_float3 movement = simd_make_float3(0.0F, 0.0F, 0.0F);
    if ([self pressedPreference:PfhorgeVMForwardKeyPreference
                       fallback:'w']) {
        movement += forward * step;
    }
    if ([self pressedPreference:PfhorgeVMBackwardKeyPreference
                       fallback:'s']) {
        movement -= forward * step;
    }
    if ([self pressedPreference:PfhorgeVMStrafeLeftKeyPreference
                       fallback:'a']) {
        movement -= right * step;
    }
    if ([self pressedPreference:PfhorgeVMStrafeRightKeyPreference
                       fallback:'d']) {
        movement += right * step;
    }
    if ([self pressedPreference:PfhorgeVMFlyDownKeyPreference
                       fallback:'q']) {
        movement.y -= step * _verticalMovementScale;
    }
    if ([self pressedPreference:PfhorgeVMFlyUpKeyPreference
                       fallback:'e']) {
        movement.y += step * _verticalMovementScale;
    }

    if (simd_length_squared(movement) <= 0.0000001F) {
        return;
    }

    if (!_portalModeEnabled) {
        _cameraTarget += movement;
        return;
    }

    if (_collisionEnabled) {
        const pfhorge::preview::PreviewMovementResult result =
            pfhorge::preview::MovePreviewCameraWithCollision(
                _previewScene,
                _portalSeedPolygonID,
                pfhorge::preview::Vec3{
                    _portalCameraPosition.x,
                    _portalCameraPosition.y,
                    _portalCameraPosition.z,
                },
                pfhorge::preview::Vec3{
                    movement.x,
                    movement.y,
                    movement.z,
                });
        _portalCameraPosition = simd_make_float3(
            result.position.x,
            result.position.y,
            result.position.z);
        if (result.polygonID !=
            pfhorge::preview::kInvalidPreviewID) {
            _portalSeedPolygonID = result.polygonID;
        }
        if (result.collided) {
            _interactionStatus = @"Collision: solid wall or closed door";
        }
    } else {
        _portalCameraPosition += movement;
        [self validatePortalCameraPosition];
    }

    [self rebuildPreviewFrame];
    [self rebuildVertexBuffer];
}

- (void)updateDiagnosticsOverlay
{
    if (_diagnosticsLabel == nil || _diagnosticsLabel.hidden) {
        return;
    }

    const double instantaneousFPS = _lastFrameDelta > 0.000001
        ? 1.0 / _lastFrameDelta
        : 0.0;
    if (_smoothedFPS <= 0.0) {
        _smoothedFPS = instantaneousFPS;
    } else {
        _smoothedFPS = (_smoothedFPS * 0.90) +
                       (instantaneousFPS * 0.10);
    }

    std::size_t doorCount = 0U;
    for (const pfhorge::preview::PreviewPlatform& platform :
         _previewScene.platforms) {
        if (platform.isDoor) {
            ++doorCount;
        }
    }
    const std::size_t movingCount = static_cast<std::size_t>(
        std::count_if(
            _platformMoving.begin(),
            _platformMoving.end(),
            [](unsigned char value) { return value != 0U; }));

    _diagnosticsLabel.stringValue = [NSString stringWithFormat:
        @"%.1f FPS (%.2f ms) | camera=(%.2f, %.2f, %.2f) | seed=%u\n"
         "visible=%zu polygons / %zu surfaces | portals=%u | vertices=%zu\n"
         "textured=%zu fallback=%zu cache=%lu | wallTex=%u/%u sideGaps=%u transparent=%u\n"
         "sideResolve cw=%u cc=%u direct=%u back=%u scan=%u miss=%u rejected=%u\n"
         "lookup invalid=%zu negCache=%zu repository=%zu image=%zu conversion=%zu upload=%zu\n"
         "collision=%@ liveSync=%@ rebuilds=%lu | doors=%zu moving=%zu | %@",
        _smoothedFPS,
        _lastFrameDelta * 1000.0,
        _portalCameraPosition.x,
        _portalCameraPosition.y,
        _portalCameraPosition.z,
        _portalSeedPolygonID,
        _portalDiagnostics.visiblePolygonCount,
        _portalDiagnostics.visibleSurfaceCount,
        _portalDiagnostics.portalsAccepted,
        _metalVertices.size(),
        static_cast<std::size_t>(_texturedSurfaceCount),
        static_cast<std::size_t>(_fallbackSurfaceCount),
        (unsigned long)_classicTextureCache.count,
        _previewScene.textureAudit.wallSegmentsWithTexture,
        _previewScene.textureAudit.wallSegments,
        _previewScene.textureAudit.wallSegmentsWithoutSide,
        _previewScene.textureAudit.transparentWallSegments,
        _previewScene.textureAudit.sidesResolvedClockwise,
        _previewScene.textureAudit.sidesResolvedCounterclockwise,
        _previewScene.textureAudit.sidesResolvedDirect,
        _previewScene.textureAudit.sidesResolvedBackReference,
        _previewScene.textureAudit.sidesResolvedScan,
        _previewScene.textureAudit.sideResolutionMisses,
        _previewScene.textureAudit.rejectedStalePolygonSides,
        static_cast<std::size_t>(_invalidTextureDescriptorCount),
        static_cast<std::size_t>(_negativeTextureCacheHitCount),
        static_cast<std::size_t>(_textureRepositoryFailureCount),
        static_cast<std::size_t>(_textureImageMissingCount),
        static_cast<std::size_t>(_textureImageConversionFailureCount),
        static_cast<std::size_t>(_textureUploadFailureCount),
        _collisionEnabled ? @"on" : @"off",
        _liveLevelSync ? @"on" : @"off",
        (unsigned long)_sceneRebuildCount,
        doorCount,
        movingCount,
        _interactionStatus ?: @"Ready"];
}

- (simd_float3)portalForwardVector
{
    const float cosPitch = std::cos(_cameraPitch);

    simd_float3 forward =
        simd_make_float3(
            std::sin(_cameraYaw) * cosPitch,
            std::sin(_cameraPitch),
            -std::cos(_cameraYaw) * cosPitch);

    if (simd_length_squared(forward) < 0.000001F) {
        return simd_make_float3(0.0F, 0.0F, -1.0F);
    }

    return simd_normalize(forward);
}

- (void)rebuildPreviewFrame
{
    if (!_portalModeEnabled) {
        _previewFrame =
            pfhorge::preview::BuildWholeScenePreviewFrame(
                _previewScene);
        return;
    }

    const CGSize drawableSize = self.drawableSize;
    const float height =
        std::max(
            1.0F,
            static_cast<float>(drawableSize.height));
    const float aspectRatio =
        std::max(
            0.01F,
            static_cast<float>(drawableSize.width) /
            height);

    const simd_float3 forward =
        [self portalForwardVector];

    pfhorge::preview::PreviewCamera camera;
    camera.position =
        pfhorge::preview::Vec3{
            _portalCameraPosition.x,
            _portalCameraPosition.y,
            _portalCameraPosition.z,
        };
    camera.forward =
        pfhorge::preview::Vec3{
            forward.x,
            forward.y,
            forward.z,
        };
    camera.verticalFieldOfViewRadians =
        _fieldOfViewDegrees *
        static_cast<float>(M_PI) /
        180.0F;
    camera.aspectRatio = aspectRatio;
    camera.nearPlane = _nearPlane;

    const std::optional<pfhorge::preview::StableID> preferredSeed =
        _portalSeedPolygonID == pfhorge::preview::kInvalidPreviewID
            ? std::nullopt
            : std::optional<pfhorge::preview::StableID>(
                _portalSeedPolygonID);
    _previewFrame =
        pfhorge::preview::BuildPortalPreviewFrame(
            _previewScene,
            camera,
            preferredSeed,
            &_portalDiagnostics);
    if (_previewFrame.cameraInsideScene) {
        _portalSeedPolygonID = _previewFrame.cameraPolygonID;
    }
}

// VM-4A collision, doors, live level sync, and texture audit.
- (void)rebuildSceneFromSourcePreservingCamera:(BOOL)preserveCamera
                                        reason:(NSString *)reason
{
    if (_sourceLevelData == nil || _sceneRebuildInProgress) {
        return;
    }

    _sceneRebuildInProgress = YES;
    const NSUInteger platformCount =
        [[_sourceLevelData getPlatforms] count];
    const float notInitialized =
        std::numeric_limits<float>::quiet_NaN();

    _platformExtensions.resize(platformCount, notInitialized);
    _platformTargets.resize(platformCount, notInitialized);
    _platformMoving.resize(platformCount, 0U);

    pfhorge::preview::PreviewSceneBuildOptions options;
    options.followLevelEnvironment = _followLevelEnvironment;
    options.platformExtensions = _platformExtensions;

    PreviewScene nextScene =
        pfhorge::preview::BuildPreviewScene(_sourceLevelData, options);
    BOOL initializedPlatformState = NO;
    for (const pfhorge::preview::PreviewPlatform& platform :
         nextScene.platforms) {
        const NSUInteger identifier =
            static_cast<NSUInteger>(platform.id);
        if (identifier >= _platformExtensions.size()) {
            continue;
        }
        if (!std::isfinite(_platformExtensions[identifier])) {
            _platformExtensions[identifier] =
                platform.initialExtension;
            initializedPlatformState = YES;
        }
        if (!std::isfinite(_platformTargets[identifier])) {
            _platformTargets[identifier] =
                _platformExtensions[identifier];
        }
    }

    if (initializedPlatformState) {
        options.platformExtensions = _platformExtensions;
        nextScene =
            pfhorge::preview::BuildPreviewScene(_sourceLevelData, options);
    }

    _previewScene = std::move(nextScene);
    _sourceLevelFingerprint =
        pfhorge::preview::ComputePreviewLevelFingerprint(
            _sourceLevelData);
    ++_sceneRebuildCount;

    if (preserveCamera && _portalModeEnabled) {
        const pfhorge::preview::Vec3 currentPosition{
            _portalCameraPosition.x,
            _portalCameraPosition.y,
            _portalCameraPosition.z,
        };
        std::optional<pfhorge::preview::StableID> containing;
        if (_portalSeedPolygonID !=
                pfhorge::preview::kInvalidPreviewID &&
            pfhorge::preview::PolygonContainsPoint3D(
                _previewScene,
                _portalSeedPolygonID,
                currentPosition)) {
            containing = _portalSeedPolygonID;
        } else {
            containing = pfhorge::preview::FindContainingPolygon(
                _previewScene,
                currentPosition);
        }
        if (containing.has_value()) {
            _portalSeedPolygonID = *containing;
            [self validatePortalCameraPosition];
        } else {
            [self positionPortalCameraAtPreferredStart];
        }
        [self rebuildPreviewFrame];
    } else {
        _previewFrame =
            pfhorge::preview::BuildWholeScenePreviewFrame(
                _previewScene);
    }
    [self rebuildVertexBuffer];
    _sceneRebuildInProgress = NO;

    NSLog(
        @"TEX-1A.2 scene rebuild #%lu (%@): polygons=%zu surfaces=%zu "
         "platforms=%zu wallTextures=%u/%u sideGaps=%u transparent=%u "
         "sideResolve[cw=%u cc=%u direct=%u back=%u scan=%u miss=%u rejected=%u]",
        (unsigned long)_sceneRebuildCount,
        reason ?: @"unspecified",
        _previewScene.polygons.size(),
        _previewScene.surfaces.size(),
        _previewScene.platforms.size(),
        _previewScene.textureAudit.wallSegmentsWithTexture,
        _previewScene.textureAudit.wallSegments,
        _previewScene.textureAudit.wallSegmentsWithoutSide,
        _previewScene.textureAudit.transparentWallSegments,
        _previewScene.textureAudit.sidesResolvedClockwise,
        _previewScene.textureAudit.sidesResolvedCounterclockwise,
        _previewScene.textureAudit.sidesResolvedDirect,
        _previewScene.textureAudit.sidesResolvedBackReference,
        _previewScene.textureAudit.sidesResolvedScan,
        _previewScene.textureAudit.sideResolutionMisses,
        _previewScene.textureAudit.rejectedStalePolygonSides);

    NSLog(
        @"VM-4D topology audit: edges=%u transparent=%u "
         "adjResolve[cwOwner=%u ccOwner=%u direct=%u miss=%u self=%u] "
         "portals=%zu",
        _previewScene.topologyAudit.polygonEdges,
        _previewScene.topologyAudit.transparentEdges,
        _previewScene.topologyAudit.adjacencyResolvedClockwiseOwner,
        _previewScene.topologyAudit.adjacencyResolvedCounterclockwiseOwner,
        _previewScene.topologyAudit.adjacencyResolvedDirect,
        _previewScene.topologyAudit.adjacencyResolutionMisses,
        _previewScene.topologyAudit.rejectedSelfAdjacency,
        _previewScene.portals.size());

    NSLog(
        @"VM-RENDER-CORRECTIVE-2 audit: structuralEnvRemaps=%u "
         "mediaSurfaces=%u mediaTextureRepairs=%u "
         "mediaHeightFromLight=%u mediaHeightFallbacks=%u "
         "transparentRefs=%u transparentSegments=%u "
         "transparentOnSolid=%u controlPanelWallSegments=%u",
        _previewScene.textureAudit.structuralEnvironmentRemaps,
        _previewScene.textureAudit.mediaSurfaces,
        _previewScene.textureAudit.mediaDefinitionTextureRepairs,
        _previewScene.textureAudit.mediaHeightsDerivedFromLight,
        _previewScene.textureAudit.mediaHeightFallbacks,
        _previewScene.textureAudit.transparentSideReferences,
        _previewScene.textureAudit.transparentWallSegments,
        _previewScene.textureAudit.transparentOverlaysOnSolidLines,
        _previewScene.textureAudit.controlPanelWallSegments);
}

- (void)checkForLiveLevelChangesAtTime:(NSTimeInterval)time
{
    if (!_liveLevelSync || _sourceLevelData == nil ||
        time - _lastLiveSyncCheck < 0.20) {
        return;
    }
    _lastLiveSyncCheck = time;
    const std::uint64_t fingerprint =
        pfhorge::preview::ComputePreviewLevelFingerprint(
            _sourceLevelData);
    if (fingerprint == _sourceLevelFingerprint) {
        return;
    }

    // A source edit can change the platform count or initial state. Reset only
    // the temporary simulation state; no editable map object is mutated.
    _platformExtensions.clear();
    _platformTargets.clear();
    _platformMoving.clear();
    _interactionStatus = @"Live map change applied";
    [self rebuildSceneFromSourcePreservingCamera:YES
                                          reason:@"live map edit"];
}

- (BOOL)updatePlatformSimulationForDelta:(float)delta
{
    if (delta <= 0.0F || _previewScene.platforms.empty()) {
        return NO;
    }

    BOOL changed = NO;
    for (const pfhorge::preview::PreviewPlatform& platform :
         _previewScene.platforms) {
        const NSUInteger identifier =
            static_cast<NSUInteger>(platform.id);
        if (identifier >= _platformExtensions.size() ||
            identifier >= _platformTargets.size() ||
            identifier >= _platformMoving.size() ||
            _platformMoving[identifier] == 0U) {
            continue;
        }

        float& current = _platformExtensions[identifier];
        const float target = _platformTargets[identifier];

        // Do not close a door around the first-person camera. This is a small
        // editor-safety rule, not a complete Marathon obstruction simulation.
        if (target > current && platform.isDoor &&
            platform.polygonID == _portalSeedPolygonID) {
            _platformMoving[identifier] = 0U;
            _platformTargets[identifier] = current;
            _interactionStatus = @"Door held open while camera is inside";
            continue;
        }

        const float travel = std::max(
            0.05F,
            platform.maximumHeight - platform.minimumHeight);
        const float step = std::max(
            0.01F,
            platform.speedWorldUnitsPerSecond / travel) * delta;
        if (current < target) {
            current = std::min(target, current + step);
        } else {
            current = std::max(target, current - step);
        }
        changed = YES;
        if (std::fabs(current - target) <= 0.0001F) {
            current = target;
            _platformMoving[identifier] = 0U;
            _interactionStatus = target < 0.5F
                ? @"Door opened"
                : @"Door closed";
        }
    }

    if (changed) {
        [self rebuildSceneFromSourcePreservingCamera:YES
                                              reason:@"platform animation"];
    }
    return changed;
}

- (void)toggleNearestDoor
{
    const simd_float3 portalForward = [self portalForwardVector];
    const std::optional<pfhorge::preview::StableID> selected =
        pfhorge::preview::FindPreviewDoorForInteraction(
            _previewScene,
            _portalSeedPolygonID,
            pfhorge::preview::Vec3{
                _portalCameraPosition.x,
                _portalCameraPosition.y,
                _portalCameraPosition.z,
            },
            pfhorge::preview::Vec3{
                portalForward.x,
                portalForward.y,
                portalForward.z,
            });
    if (!selected.has_value()) {
        _interactionStatus = @"No usable door in reach";
        NSBeep();
        return;
    }

    const auto iterator = std::find_if(
        _previewScene.platforms.begin(),
        _previewScene.platforms.end(),
        [selected](const pfhorge::preview::PreviewPlatform& platform) {
            return platform.id == *selected;
        });
    if (iterator == _previewScene.platforms.end()) {
        _interactionStatus = @"Door definition unavailable";
        NSBeep();
        return;
    }
    if (iterator->locked) {
        _interactionStatus = @"Door is locked";
        NSBeep();
        return;
    }

    const NSUInteger identifier =
        static_cast<NSUInteger>(iterator->id);
    if (identifier >= _platformExtensions.size()) {
        return;
    }
    const float current = _platformExtensions[identifier];
    float target = current > 0.5F ? 0.0F : 1.0F;
    if (identifier < _platformMoving.size() &&
        _platformMoving[identifier] != 0U) {
        target = _platformTargets[identifier] > current ? 0.0F : 1.0F;
    }
    _platformTargets[identifier] = target;
    _platformMoving[identifier] = 1U;
    _interactionStatus = target < 0.5F
        ? @"Opening door"
        : @"Closing door";
}

- (void)levelEnvironmentDidChange:(NSNotification *)notification
{
    if (notification.object != nil &&
        notification.object != _sourceLevelData) {
        return;
    }
    [self clearClassicTextureCache];
    _interactionStatus = @"Level environment synchronized";
    [self rebuildSceneFromSourcePreservingCamera:YES
                                          reason:@"level environment"];
}

- (void)positionPortalCameraAtPreferredStart
{
    _portalSeedPolygonID = pfhorge::preview::kInvalidPreviewID;
    _portalCameraSource = @"fallback: no valid polygon";

    for (const pfhorge::preview::PreviewPlayerStart& start :
         _previewScene.playerStarts) {
        if (!pfhorge::preview::PolygonContainsPoint3D(
                _previewScene,
                start.polygonID,
                start.position)) {
            continue;
        }
        _portalCameraPosition = simd_make_float3(
            start.position.x,
            start.position.y,
            start.position.z);
        _portalSeedPolygonID = start.polygonID;
        _cameraYaw = start.yawRadians;
        _cameraPitch = 0.0F;
        _portalCameraSource = [NSString stringWithFormat:
            @"player start object %u",
            start.objectID];
        return;
    }

    for (const pfhorge::preview::PreviewPolygon& polygon :
         _previewScene.polygons) {
        const std::optional<pfhorge::preview::Vec3> interior =
            pfhorge::preview::FindInteriorPoint(_previewScene, polygon.id);
        if (!interior.has_value()) {
            continue;
        }
        _portalCameraPosition = simd_make_float3(
            interior->x,
            interior->y,
            interior->z);
        _portalSeedPolygonID = polygon.id;
        _cameraYaw = 0.0F;
        _cameraPitch = 0.0F;
        _portalCameraSource = [NSString stringWithFormat:
            @"interior fallback polygon %u",
            polygon.id];
        return;
    }

    _portalCameraPosition = simd_make_float3(0.0F, 0.0F, 0.0F);
    _cameraYaw = 0.0F;
    _cameraPitch = 0.0F;
}

- (void)validatePortalCameraPosition
{
    const pfhorge::preview::Vec3 position{
        _portalCameraPosition.x,
        _portalCameraPosition.y,
        _portalCameraPosition.z,
    };

    std::optional<pfhorge::preview::StableID> containing;
    if (_portalSeedPolygonID != pfhorge::preview::kInvalidPreviewID &&
        pfhorge::preview::PolygonContainsPoint3D(
            _previewScene,
            _portalSeedPolygonID,
            position)) {
        containing = _portalSeedPolygonID;
    } else {
        containing = pfhorge::preview::FindContainingPolygon(
            _previewScene,
            position);
    }
    if (!containing.has_value()) {
        return;
    }

    _portalSeedPolygonID = *containing;
    const pfhorge::preview::PreviewPolygon *polygon =
        pfhorge::preview::FindPreviewPolygon(
            _previewScene,
            _portalSeedPolygonID);
    if (polygon == nullptr) {
        return;
    }
    if (polygon->ceilingHeight - polygon->floorHeight <= 0.10F) {
        [self positionPortalCameraAtPreferredStart];
        _interactionStatus = @"Camera moved out of a closed platform";
        return;
    }

    _portalCameraPosition.y = std::clamp(
        _portalCameraPosition.y,
        polygon->floorHeight + 0.05F,
        polygon->ceilingHeight - 0.05F);
}

- (void)logPortalDiagnostics
{
    NSLog(
        @"Metal Visual Mode VM-3C: camera=%@ position=(%.3f, %.3f, %.3f) "
         "seed=%u seedAccepted=%@ visiblePolygons=%zu visibleSurfaces=%zu "
         "portalsExamined=%u accepted=%u projectionRejects=%u "
         "clipRejects=%u duplicateRejects=%u vertices=%zu",
        _portalCameraSource ?: @"unknown",
        _portalCameraPosition.x,
        _portalCameraPosition.y,
        _portalCameraPosition.z,
        _portalSeedPolygonID,
        _portalDiagnostics.preferredSeedAccepted ? @"yes" : @"no",
        _portalDiagnostics.visiblePolygonCount,
        _portalDiagnostics.visibleSurfaceCount,
        _portalDiagnostics.portalsExamined,
        _portalDiagnostics.portalsAccepted,
        _portalDiagnostics.portalsRejectedProjection,
        _portalDiagnostics.portalsRejectedClip,
        _portalDiagnostics.portalsRejectedDuplicate,
        _metalVertices.size());
}

- (void)rebuildVertexBuffer
{
    _metalVertices.clear();
    _metalDrawRanges.clear();
    [_drawTextures removeAllObjects];
    _texturedSurfaceCount = 0U;
    _fallbackSurfaceCount = 0U;
    _invalidTextureDescriptorCount = 0U;
    _negativeTextureCacheHitCount = 0U;
    _textureRepositoryFailureCount = 0U;
    _textureImageMissingCount = 0U;
    _textureImageConversionFailureCount = 0U;
    _textureUploadFailureCount = 0U;

    if (_selectedSurface.has_value()) {
        const auto selectedIterator = std::find_if(
            _previewScene.surfaces.begin(),
            _previewScene.surfaces.end(),
            [selected = *_selectedSurface](const PreviewSurface& candidate) {
                return SurfaceIdentityMatches(
                    candidate,
                    selected);
            });
        if (selectedIterator != _previewScene.surfaces.end()) {
            _selectedSurface = *selectedIterator;
        } else {
            _selectedSurface.reset();
        }
    }

    // VM-4D: render the frame selected by the current camera mode.
    // First-person mode uses the repaired portal traversal; P/orbit mode still
    // deliberately supplies BuildWholeScenePreviewFrame for diagnostics.
    const std::vector<PreviewSurface>& surfaces =
        _previewFrame.visibleSurfaces;

    std::size_t indexCount = 0U;
    for (const PreviewSurface& surface : surfaces) {
        indexCount += surface.indices.size();
    }
    _metalVertices.reserve(indexCount);
    _metalDrawRanges.reserve(surfaces.size());

    const BOOL forceUntextured =
        [[NSUserDefaults standardUserDefaults]
            boolForKey:PfhorgeVMUntexturedDiagnosticPreference];

    for (const PreviewSurface& surface : surfaces) {
        const bool landscapeSurface =
            surface.id.kind == SurfaceKind::Landscape ||
            surface.texture.transferMode == 9 ||
            surface.texture.transferMode == 21;
        const bool selectedSurface =
            _selectedSurface.has_value() &&
            SurfaceIdentityMatches(surface, *_selectedSurface);

        const pfhorge::preview::PreviewTextureKey key =
            pfhorge::preview::ClassicTextureKeyFor(surface);
        id<MTLTexture> texture = nil;
        BOOL useTexture = NO;
        if (!forceUntextured) {
            if (!key.valid()) {
                ++_invalidTextureDescriptorCount;
            } else {
                texture = [self classicTextureForCollection:key.collection
                                                     bitmap:key.bitmap];
                useTexture = texture != nil;
                if (!useTexture) {
                    switch (_lastClassicTextureFailure) {
                        case ClassicTextureFailure::InvalidDescriptor:
                            ++_invalidTextureDescriptorCount;
                            break;
                        case ClassicTextureFailure::NegativeCache:
                            ++_negativeTextureCacheHitCount;
                            break;
                        case ClassicTextureFailure::RepositoryUnavailable:
                            ++_textureRepositoryFailureCount;
                            break;
                        case ClassicTextureFailure::ImageMissing:
                            ++_textureImageMissingCount;
                            break;
                        case ClassicTextureFailure::ImageConversionFailed:
                            ++_textureImageConversionFailureCount;
                            break;
                        case ClassicTextureFailure::UploadFailed:
                            ++_textureUploadFailureCount;
                            break;
                        case ClassicTextureFailure::None:
                            break;
                    }
                }
            }
        }
        if (texture == nil) {
            texture = [self fallbackTexture];
        }

        const NSUInteger vertexStart = _metalVertices.size();
        const simd_float4 fallbackColor = ColorForSurface(surface);
        for (const std::uint32_t index : surface.indices) {
            if (index >= surface.vertices.size()) {
                continue;
            }

            const PreviewVertex& vertex = surface.vertices[index];
            const float brightness = std::clamp(
                vertex.light,
                0.0F,
                1.0F);
            simd_float4 color;
            if (selectedSurface) {
                color = useTexture
                    ? simd_make_float4(
                        brightness,
                        brightness * 0.52F,
                        brightness * 0.18F,
                        1.0F)
                    : simd_make_float4(1.0F, 0.42F, 0.08F, 1.0F);
            } else if (useTexture && landscapeSurface) {
                color = simd_make_float4(1.0F, 1.0F, 1.0F, 1.0F);
            } else {
                color = useTexture
                    ? simd_make_float4(
                        brightness,
                        brightness,
                        brightness,
                        1.0F)
                    : fallbackColor;
            }

            _metalVertices.push_back(MetalPreviewVertex{
                simd_make_float3(
                    vertex.position.x,
                    vertex.position.y,
                    vertex.position.z),
                color,
                simd_make_float2(
                    vertex.textureCoordinate.x,
                    vertex.textureCoordinate.y),
            });
        }

        const NSUInteger vertexCount =
            _metalVertices.size() - vertexStart;
        if (vertexCount == 0U) {
            continue;
        }

        const NSUInteger textureIndex = _drawTextures.count;
        [_drawTextures addObject:texture];
        _metalDrawRanges.push_back(MetalPreviewDrawRange{
            vertexStart,
            vertexCount,
            textureIndex,
            useTexture
                ? pfhorge::preview::ClassicSurfaceOpacity(surface)
                : 1.0F,
            static_cast<std::int32_t>(
                surface.texture.transferMode),
            landscapeSurface,
            useTexture,
            surface.translucent,
        });

        if (useTexture) {
            ++_texturedSurfaceCount;
        } else {
            ++_fallbackSurfaceCount;
        }
    }

    if (_metalVertices.empty()) {
        _vertexBuffer = nil;
        return;
    }

    _vertexBuffer =
        [self.device newBufferWithBytes:_metalVertices.data()
                                 length:_metalVertices.size() *
                                        sizeof(MetalPreviewVertex)
                                options:MTLResourceStorageModeShared];
    _vertexBuffer.label = @"Pfhorge TEX-1A Classic Shapes Geometry";
}


- (void)resetCamera
{
    if (_portalModeEnabled) {
        [self positionPortalCameraAtPreferredStart];
        [self rebuildPreviewFrame];
        [self rebuildVertexBuffer];
        return;
    }

    if (_metalVertices.empty()) {
        _cameraTarget =
            simd_make_float3(0.0F, 0.0F, 0.0F);
        _cameraYaw = 0.75F;
        _cameraPitch = -0.30F;
        _cameraDistance = 8.0F;
        return;
    }

    const float largestValue =
        std::numeric_limits<float>::max();
    const float smallestValue =
        std::numeric_limits<float>::lowest();

    simd_float3 minimum =
        simd_make_float3(
            largestValue,
            largestValue,
            largestValue);
    simd_float3 maximum =
        simd_make_float3(
            smallestValue,
            smallestValue,
            smallestValue);

    for (const MetalPreviewVertex& vertex :
         _metalVertices) {
        minimum = simd_min(minimum, vertex.position);
        maximum = simd_max(maximum, vertex.position);
    }

    _cameraTarget = (minimum + maximum) * 0.5F;

    const simd_float3 extent = maximum - minimum;
    const float largestExtent =
        std::max({
            extent.x,
            extent.y,
            extent.z,
            1.0F,
        });

    _cameraYaw = 0.75F;
    _cameraPitch = -0.35F;
    _cameraDistance =
        std::max(3.0F, largestExtent * 1.8F);
}


- (matrix_float4x4)viewProjectionMatrix
{
    simd_float3 eye;
    simd_float3 target;

    if (_portalModeEnabled) {
        eye = _portalCameraPosition;
        target =
            eye + [self portalForwardVector];
    } else {
        const float cosPitch = std::cos(_cameraPitch);

        const simd_float3 orbitDirection =
            simd_make_float3(
                std::sin(_cameraYaw) * cosPitch,
                std::sin(_cameraPitch),
                std::cos(_cameraYaw) * cosPitch);

        eye =
            _cameraTarget +
            orbitDirection * _cameraDistance;
        target = _cameraTarget;
    }

    const matrix_float4x4 view =
        MakeLookAtMatrix(
            eye,
            target,
            simd_make_float3(0.0F, 1.0F, 0.0F));

    const CGSize drawableSize = self.drawableSize;
    const float height =
        std::max(
            1.0F,
            static_cast<float>(drawableSize.height));
    const float aspectRatio =
        std::max(
            0.01F,
            static_cast<float>(drawableSize.width) /
            height);

    const float nearPlane =
        _portalModeEnabled
            ? _nearPlane
            : std::max(
                0.01F,
                _cameraDistance / 1000.0F);
    const float farPlane =
        _portalModeEnabled
            ? 10000.0F
            : std::max(
                1000.0F,
                _cameraDistance * 50.0F);

    const matrix_float4x4 projection =
        MakePerspectiveMatrix(
            _fieldOfViewDegrees *
            static_cast<float>(M_PI) /
            180.0F,
            aspectRatio,
            nearPlane,
            farPlane);

    return simd_mul(projection, view);
}


- (void)drawInMTKView:(MTKView *)view
{
    [self updateCameraForFrame];

    const CGFloat backingScale =
        self.window != nil ? self.window.backingScaleFactor : 1.0;
    const CGSize desiredDrawableSize = CGSizeMake(
        std::max<CGFloat>(
            1.0,
            self.bounds.size.width * backingScale * _renderScale),
        std::max<CGFloat>(
            1.0,
            self.bounds.size.height * backingScale * _renderScale));
    if (std::fabs(
            self.drawableSize.width - desiredDrawableSize.width) > 1.0 ||
        std::fabs(
            self.drawableSize.height - desiredDrawableSize.height) > 1.0) {
        self.drawableSize = desiredDrawableSize;
    }
    [self updateDiagnosticsOverlay];

    MTLRenderPassDescriptor *passDescriptor =
        view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable = view.currentDrawable;

    if (passDescriptor == nil ||
        drawable == nil ||
        _pipelineState == nil ||
        _commandQueue == nil) {
        return;
    }

    id<MTLCommandBuffer> commandBuffer =
        [_commandQueue commandBuffer];
    commandBuffer.label = @"Pfhorge TEX-1A Frame";

    id<MTLRenderCommandEncoder> encoder =
        [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
    if (encoder == nil) {
        return;
    }

    [encoder setRenderPipelineState:_pipelineState];
    [encoder setCullMode:MTLCullModeNone];

    if (_vertexBuffer != nil &&
        !_metalVertices.empty() &&
        !_metalDrawRanges.empty()) {
        MetalPreviewUniforms uniforms{
            [self viewProjectionMatrix],
        };
        [encoder setVertexBuffer:_vertexBuffer
                          offset:0U
                         atIndex:0U];
        [encoder setVertexBytes:&uniforms
                         length:sizeof(uniforms)
                        atIndex:1U];
        [encoder setFragmentSamplerState:_samplerState
                                 atIndex:0U];

        const auto drawPass = [&](bool translucent) {
            [encoder setDepthStencilState:
                translucent
                    ? _translucentDepthStencilState
                    : _depthStencilState];
            for (const MetalPreviewDrawRange& range :
                 _metalDrawRanges) {
                if (range.translucent != translucent ||
                    range.textureIndex >= _drawTextures.count) {
                    continue;
                }

                id<MTLTexture> texture =
                    _drawTextures[range.textureIndex];
                const float textureAspectRatio =
                    texture.height > 0U
                        ? static_cast<float>(texture.width) /
                          static_cast<float>(texture.height)
                        : 1.0F;
                const float drawableHeight =
                    std::max(
                        1.0F,
                        static_cast<float>(self.drawableSize.height));
                const float drawableAspectRatio =
                    std::max(
                        0.01F,
                        static_cast<float>(self.drawableSize.width) /
                        drawableHeight);

                MetalPreviewFragmentUniforms fragmentUniforms{
                    range.textured ? 1U : 0U,
                    range.opacity,
                    range.transferMode,
                    range.landscape ? 1U : 0U,

                    static_cast<float>(self.drawableSize.width),
                    drawableHeight,
                    _fieldOfViewDegrees *
                        static_cast<float>(M_PI) /
                        180.0F,
                    drawableAspectRatio,

                    _cameraYaw,
                    _cameraPitch,
                    textureAspectRatio,
                    0.0F,
                };
                [encoder setFragmentTexture:texture atIndex:0U];
                [encoder setFragmentBytes:&fragmentUniforms
                                   length:sizeof(fragmentUniforms)
                                  atIndex:0U];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                            vertexStart:range.vertexStart
                            vertexCount:range.vertexCount];
            }
        };

        drawPass(false);
        drawPass(true);
    }

    [encoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}

- (void)mtkView:(MTKView *)view
drawableSizeWillChange:(CGSize)size
{
    (void)view;
    (void)size;
    if (_portalModeEnabled) {
        [self rebuildPreviewFrame];
        [self rebuildVertexBuffer];
    }
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];

    if (self.window != nil) {
        self.window.acceptsMouseMovedEvents = YES;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (self.window != nil) {
                [self.window makeFirstResponder:self];
            }
        });
    }
}

- (void)mouseDown:(NSEvent *)event
{
    _lastDragLocation =
        [self convertPoint:event.locationInWindow
                  fromView:nil];
    _mouseDownLocation = _lastDragLocation;
    _mouseDragDistanceSquared = 0.0F;
    [self.window makeFirstResponder:self];
}

- (void)mouseDragged:(NSEvent *)event
{
    const NSPoint location =
        [self convertPoint:event.locationInWindow
                  fromView:nil];

    const float downDeltaX =
        static_cast<float>(location.x - _mouseDownLocation.x);
    const float downDeltaY =
        static_cast<float>(location.y - _mouseDownLocation.y);
    _mouseDragDistanceSquared =
        std::max(
            _mouseDragDistanceSquared,
            (downDeltaX * downDeltaX) +
            (downDeltaY * downDeltaY));

    const float deltaX =
        static_cast<float>(
            location.x - _lastDragLocation.x);
    const float deltaY =
        static_cast<float>(
            location.y - _lastDragLocation.y);

    const float blend = 1.0F - _lookSmoothing;
    _smoothedMouseDeltaX =
        (_smoothedMouseDeltaX * _lookSmoothing) +
        (static_cast<float>(deltaX) * blend);
    _smoothedMouseDeltaY =
        (_smoothedMouseDeltaY * _lookSmoothing) +
        (static_cast<float>(deltaY) * blend);

    // AppKit drag deltas are positive to the right. Positive yaw turns the
    // Marathon camera right, so the default direction is deliberately +.
    const float yawDirection = _invertMouseX ? -1.0F : 1.0F;
    _cameraYaw +=
        _smoothedMouseDeltaX * _mouseSensitivityX * yawDirection;
    const float pitchDirection = _invertMouseY ? -1.0F : 1.0F;
    _cameraPitch =
        std::clamp(
            _cameraPitch +
                _smoothedMouseDeltaY *
                _mouseSensitivityY *
                pitchDirection,
            -1.45F,
            1.45F);

    _lastDragLocation = location;

    if (_portalModeEnabled) {
        [self rebuildPreviewFrame];
        [self rebuildVertexBuffer];
    }
}


- (void)mouseUp:(NSEvent *)event
{
    const NSPoint location =
        [self convertPoint:event.locationInWindow
                  fromView:nil];

    if (_mouseDragDistanceSquared <= 16.0F) {
        [self pickSurfaceAtViewPoint:location];
    }
}

- (void)pickSurfaceAtViewPoint:(NSPoint)location
{
    const float viewWidth =
        std::max(
            1.0F,
            static_cast<float>(self.bounds.size.width));
    const float viewHeight =
        std::max(
            1.0F,
            static_cast<float>(self.bounds.size.height));
    const float aspectRatio =
        std::max(0.01F, viewWidth / viewHeight);

    const float normalizedX =
        (2.0F * static_cast<float>(location.x) / viewWidth) - 1.0F;
    const float normalizedY =
        (2.0F * static_cast<float>(location.y) / viewHeight) - 1.0F;

    simd_float3 eye;
    simd_float3 forward;

    if (_portalModeEnabled) {
        eye = _portalCameraPosition;
        forward = [self portalForwardVector];
    } else {
        const float cosPitch = std::cos(_cameraPitch);
        const simd_float3 orbitDirection =
            simd_make_float3(
                std::sin(_cameraYaw) * cosPitch,
                std::sin(_cameraPitch),
                std::cos(_cameraYaw) * cosPitch);
        eye =
            _cameraTarget +
            orbitDirection * _cameraDistance;
        forward = simd_normalize(_cameraTarget - eye);
    }

    const simd_float3 worldUp =
        simd_make_float3(0.0F, 1.0F, 0.0F);
    simd_float3 right = simd_cross(forward, worldUp);
    if (simd_length_squared(right) < 0.000001F) {
        right = simd_make_float3(1.0F, 0.0F, 0.0F);
    } else {
        right = simd_normalize(right);
    }
    const simd_float3 cameraUp =
        simd_normalize(simd_cross(right, forward));

    const float tanHalfFov =
        std::tan(
            _fieldOfViewDegrees *
            static_cast<float>(M_PI) /
            360.0F);

    const simd_float3 rayDirection =
        simd_normalize(
            forward +
            right * (normalizedX * aspectRatio * tanHalfFov) +
            cameraUp * (normalizedY * tanHalfFov));

    const PreviewSurface *nearestSurface = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();

    for (const PreviewSurface& surface :
         _previewFrame.visibleSurfaces) {
        for (std::size_t index = 0U;
             index + 2U < surface.indices.size();
             index += 3U) {
            const std::uint32_t firstIndex = surface.indices[index];
            const std::uint32_t secondIndex = surface.indices[index + 1U];
            const std::uint32_t thirdIndex = surface.indices[index + 2U];

            if (firstIndex >= surface.vertices.size() ||
                secondIndex >= surface.vertices.size() ||
                thirdIndex >= surface.vertices.size()) {
                continue;
            }

            const PreviewVertex& first = surface.vertices[firstIndex];
            const PreviewVertex& second = surface.vertices[secondIndex];
            const PreviewVertex& third = surface.vertices[thirdIndex];

            float distance = 0.0F;
            if (!RayTriangleIntersection(
                    eye,
                    rayDirection,
                    simd_make_float3(
                        first.position.x,
                        first.position.y,
                        first.position.z),
                    simd_make_float3(
                        second.position.x,
                        second.position.y,
                        second.position.z),
                    simd_make_float3(
                        third.position.x,
                        third.position.y,
                        third.position.z),
                    distance)) {
                continue;
            }

            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestSurface = &surface;
            }
        }
    }

    if (nearestSurface == nullptr) {
        _selectedSurface.reset();
        _interactionStatus = @"Surface pick: none";
        NSLog(@"VM-SURFACE-PICK none");
        [self rebuildVertexBuffer];
        return;
    }

    _selectedSurface = *nearestSurface;
    NSString *description =
        PfhorgePreviewSurfaceDescription(
            *nearestSurface,
            _previewScene.environmentCode);
    _interactionStatus =
        [NSString stringWithFormat:@"Selected %@", description];

    NSLog(
        @"VM-SURFACE-PICK %@ distance=%.3f",
        description,
        nearestDistance);

    [self rebuildVertexBuffer];
}


- (void)scrollWheel:(NSEvent *)event
{
    const float factor =
        std::exp(
            static_cast<float>(event.scrollingDeltaY) *
            0.035F);

    if (_portalModeEnabled) {
        _portalMoveSpeed =
            std::clamp(
                _portalMoveSpeed * factor,
                0.01F,
                10.0F);
        return;
    }

    _cameraDistance =
        std::clamp(
            _cameraDistance * factor,
            0.20F,
            100000.0F);
}



- (void)keyDown:(NSEvent *)event
{
    NSString *characters =
        event.charactersIgnoringModifiers.lowercaseString;
    if (characters.length == 0U) {
        [super keyDown:event];
        return;
    }

    const unichar key = [characters characterAtIndex:0U];
    const unichar actionKey =
        [self configuredKeyForPreference:PfhorgeVMActionKeyPreference
                                fallback:0x20];
    const unichar orbitKey =
        [self configuredKeyForPreference:PfhorgeVMOrbitKeyPreference
                                fallback:'p'];
    const unichar resetKey =
        [self configuredKeyForPreference:PfhorgeVMResetKeyPreference
                                fallback:'r'];
    const unichar diagnosticsKey =
        [self configuredKeyForPreference:PfhorgeVMDiagnosticsKeyPreference
                                fallback:'i'];

    if (!event.isARepeat && key == actionKey && _portalModeEnabled) {
        [self toggleNearestDoor];
        return;
    }
    if (!event.isARepeat && key == orbitKey) {
        [self clearPressedKeys];
        _portalModeEnabled = !_portalModeEnabled;
        if (_portalModeEnabled) {
            [self resetCamera];
        } else {
            _previewFrame =
                pfhorge::preview::BuildWholeScenePreviewFrame(
                    _previewScene);
            [self rebuildVertexBuffer];
            [self resetCamera];
        }
        NSLog(
            @"Metal Visual Mode VM-4A camera mode: %@",
            _portalModeEnabled
                ? @"collision-aware first-person"
                : @"orbit diagnostic");
        return;
    }
    if (!event.isARepeat && key == resetKey) {
        [self clearPressedKeys];
        [self resetCamera];
        return;
    }
    if (!event.isARepeat && key == diagnosticsKey) {
        [self logPortalDiagnostics];
        NSLog(
            @"VM-4D topology audit: edges=%u transparent=%u "
             "adjResolve[cwOwner=%u ccOwner=%u direct=%u miss=%u self=%u] "
             "portals=%zu cameraPoly=%u",
            _previewScene.topologyAudit.polygonEdges,
            _previewScene.topologyAudit.transparentEdges,
            _previewScene.topologyAudit.adjacencyResolvedClockwiseOwner,
            _previewScene.topologyAudit.adjacencyResolvedCounterclockwiseOwner,
            _previewScene.topologyAudit.adjacencyResolvedDirect,
            _previewScene.topologyAudit.adjacencyResolutionMisses,
            _previewScene.topologyAudit.rejectedSelfAdjacency,
            _previewScene.portals.size(),
            _portalSeedPolygonID);
        NSLog(
            @"VM-4A texture audit: refs=%u valid=%u invalid=%u empty=%u "
             "walls=%u textured=%u missingSide=%u platforms=%zu",
            _previewScene.textureAudit.totalReferences,
            _previewScene.textureAudit.validReferences,
            _previewScene.textureAudit.invalidReferences,
            _previewScene.textureAudit.emptyReferences,
            _previewScene.textureAudit.wallSegments,
            _previewScene.textureAudit.wallSegmentsWithTexture,
            _previewScene.textureAudit.wallSegmentsWithoutSide,
            _previewScene.platforms.size());
        NSLog(
            @"TEX-1A.2 render audit: textured=%lu fallback=%lu "
             "invalid=%lu negativeCache=%lu repository=%lu image=%lu "
             "conversion=%lu upload=%lu",
            (unsigned long)_texturedSurfaceCount,
            (unsigned long)_fallbackSurfaceCount,
            (unsigned long)_invalidTextureDescriptorCount,
            (unsigned long)_negativeTextureCacheHitCount,
            (unsigned long)_textureRepositoryFailureCount,
            (unsigned long)_textureImageMissingCount,
            (unsigned long)_textureImageConversionFailureCount,
            (unsigned long)_textureUploadFailureCount);

        // Clear negative-cache state so this explicit audit records the
        // original failure (for example ImageMissing) instead of merely
        // reporting that a previous lookup was cached as missing.
        [self clearClassicTextureCache];

        NSMutableSet<NSNumber *> *testedTextureKeys = [NSMutableSet set];
        NSUInteger missingWallSurfaceCount = 0U;
        NSUInteger invalidWallDescriptorCount = 0U;
        NSUInteger failedUniqueTextureCount = 0U;

        for (const PreviewSurface& surface : _previewScene.surfaces) {
            if (surface.id.kind != SurfaceKind::Wall &&
                surface.id.kind != SurfaceKind::Landscape) {
                continue;
            }

            const pfhorge::preview::PreviewTextureKey textureKey =
                pfhorge::preview::ClassicTextureKeyFor(surface);

            if (!textureKey.valid()) {
                ++missingWallSurfaceCount;
                ++invalidWallDescriptorCount;
                NSLog(
                    @"TEX-WALL-MISSING invalidDescriptor "
                     "polygon=%u line=%u side=%u edge=%u subpart=%u "
                     "layer=%u collection=%d bitmap=%d transfer=%d",
                    static_cast<unsigned int>(surface.polygonID),
                    static_cast<unsigned int>(surface.lineID),
                    static_cast<unsigned int>(surface.sideID),
                    static_cast<unsigned int>(surface.edgeIndex),
                    static_cast<unsigned int>(surface.id.subpart),
                    static_cast<unsigned int>(surface.textureLayer),
                    static_cast<int>(surface.texture.collection),
                    static_cast<int>(surface.texture.bitmap),
                    static_cast<int>(surface.texture.transferMode));
                continue;
            }

            NSNumber *packedKey = @(textureKey.packed());
            if ([testedTextureKeys containsObject:packedKey]) {
                continue;
            }
            [testedTextureKeys addObject:packedKey];

            id<MTLTexture> diagnosticTexture =
                [self classicTextureForCollection:textureKey.collection
                                           bitmap:textureKey.bitmap];
            if (diagnosticTexture != nil) {
                continue;
            }

            ++missingWallSurfaceCount;
            ++failedUniqueTextureCount;
            NSString *failureName = @"unknown";
            switch (_lastClassicTextureFailure) {
                case ClassicTextureFailure::None:
                    failureName = @"none";
                    break;
                case ClassicTextureFailure::InvalidDescriptor:
                    failureName = @"invalidDescriptor";
                    break;
                case ClassicTextureFailure::NegativeCache:
                    failureName = @"negativeCache";
                    break;
                case ClassicTextureFailure::RepositoryUnavailable:
                    failureName = @"repositoryUnavailable";
                    break;
                case ClassicTextureFailure::ImageMissing:
                    failureName = @"imageMissing";
                    break;
                case ClassicTextureFailure::ImageConversionFailed:
                    failureName = @"imageConversionFailed";
                    break;
                case ClassicTextureFailure::UploadFailed:
                    failureName = @"uploadFailed";
                    break;
            }

            NSLog(
                @"TEX-WALL-MISSING %@ polygon=%u line=%u side=%u "
                 "edge=%u subpart=%u layer=%u collection=%d bitmap=%d "
                 "transfer=%d",
                failureName,
                static_cast<unsigned int>(surface.polygonID),
                static_cast<unsigned int>(surface.lineID),
                static_cast<unsigned int>(surface.sideID),
                static_cast<unsigned int>(surface.edgeIndex),
                static_cast<unsigned int>(surface.id.subpart),
                static_cast<unsigned int>(surface.textureLayer),
                static_cast<int>(textureKey.collection),
                static_cast<int>(textureKey.bitmap),
                static_cast<int>(surface.texture.transferMode));
        }

        NSLog(
            @"TEX-1A.2 detailed wall audit complete: "
             "missingWallSurfaces=%lu invalidDescriptors=%lu "
             "failedUniqueTextures=%lu testedUniqueTextures=%lu",
            (unsigned long)missingWallSurfaceCount,
            (unsigned long)invalidWallDescriptorCount,
            (unsigned long)failedUniqueTextureCount,
            (unsigned long)testedTextureKeys.count);
        return;
    }

    NSArray<NSString *> *movementPreferences = @[
        PfhorgeVMForwardKeyPreference,
        PfhorgeVMBackwardKeyPreference,
        PfhorgeVMStrafeLeftKeyPreference,
        PfhorgeVMStrafeRightKeyPreference,
        PfhorgeVMFlyDownKeyPreference,
        PfhorgeVMFlyUpKeyPreference,
    ];
    const unichar fallbacks[] = {'w', 's', 'a', 'd', 'q', 'e'};
    for (NSUInteger index = 0;
         index < movementPreferences.count;
         ++index) {
        if (key == [self configuredKeyForPreference:
                        movementPreferences[index]
                                            fallback:fallbacks[index]]) {
            [_pressedKeys addObject:@(key)];
            return;
        }
    }
    [super keyDown:event];
}

- (void)keyUp:(NSEvent *)event
{
    NSString *characters =
        event.charactersIgnoringModifiers.lowercaseString;
    if (characters.length > 0U) {
        NSNumber *releasedKey = @([characters characterAtIndex:0U]);
        if ([_pressedKeys containsObject:releasedKey]) {
            [_pressedKeys removeObject:releasedKey];
            return;
        }
    }
    [super keyUp:event];
}

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
    if (newWindow == nil) {
        [self clearPressedKeys];
    }
    [super viewWillMoveToWindow:newWindow];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

@end
