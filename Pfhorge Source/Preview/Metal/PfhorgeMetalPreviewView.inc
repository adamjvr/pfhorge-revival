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

#include "../Core/PreviewSceneBuilder.hpp"
#include "../Core/PreviewVisibility.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {

using pfhorge::preview::PreviewScene;
using pfhorge::preview::PreviewSurface;
using pfhorge::preview::PreviewVertex;
using pfhorge::preview::SurfaceKind;

struct MetalPreviewVertex final {
    simd_float3 position;
    simd_float4 color;
};

struct MetalPreviewUniforms final {
    matrix_float4x4 viewProjection;
};

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
};

struct PreviewUniforms {
    float4x4 viewProjection;
};

struct RasterData {
    float4 position [[position]];
    float4 color;
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
    return output;
}

fragment float4 pfhorgePreviewFragment(
    RasterData input [[stage_in]])
{
    return input.color;
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
- (void)visualModeSettingsDidChange:(NSNotification *)notification;
- (void)windowDidBecomeKey:(NSNotification *)notification;
- (void)windowDidResignKey:(NSNotification *)notification;
- (void)windowDidChangeScreen:(NSNotification *)notification;
- (unichar)configuredKeyForPreference:(NSString *)preference fallback:(unichar)fallback;
- (BOOL)pressedPreference:(NSString *)preference fallback:(unichar)fallback;
- (void)clearPressedKeys;
- (void)updateCameraForFrame;
- (void)updateDiagnosticsOverlay;
- (void)rebuildVertexBuffer;
- (void)resetCamera;
- (matrix_float4x4)viewProjectionMatrix;

@end

@implementation PfhorgeMetalPreviewView {
    PreviewScene _previewScene;
    pfhorge::preview::PreviewFrame _previewFrame;
    std::vector<MetalPreviewVertex> _metalVertices;

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
    id<MTLBuffer> _vertexBuffer;

    simd_float3 _cameraTarget;
    float _cameraYaw;
    float _cameraPitch;
    float _cameraDistance;

    NSPoint _lastDragLocation;
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
    _portalMoveSpeed = 1.0F;
    [self applyVisualModeSettings];

    _diagnosticsLabel = [[NSTextField alloc]
        initWithFrame:NSMakeRect(12.0, frameRect.size.height - 70.0, 560.0, 58.0)];
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

    _previewScene =
        pfhorge::preview::BuildPreviewScene(levelData);
    _portalModeEnabled = YES;
    _portalMoveSpeed = 1.0F;
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
        @"Pfhorge VM-3 Untextured Geometry";
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat =
        self.colorPixelFormat;
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

    return _depthStencilState != nil;
}



- (void)applyVisualModeSettings
{
    PfhorgeRegisterVisualModeDefaults();
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

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
}

- (void)visualModeSettingsDidChange:(NSNotification *)notification
{
    (void)notification;
    [self applyVisualModeSettings];
    if (_portalModeEnabled) {
        [self rebuildPreviewFrame];
        [self rebuildVertexBuffer];
    }
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
        return;
    }

    _lastFrameDelta = std::clamp(now - _lastFrameTimestamp, 0.0, 0.10);
    _lastFrameTimestamp = now;
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
    BOOL moved = NO;

    simd_float3 *position = _portalModeEnabled
        ? &_portalCameraPosition
        : &_cameraTarget;

    if ([self pressedPreference:PfhorgeVMForwardKeyPreference fallback:'w']) {
        *position += forward * step;
        moved = YES;
    }
    if ([self pressedPreference:PfhorgeVMBackwardKeyPreference fallback:'s']) {
        *position -= forward * step;
        moved = YES;
    }
    if ([self pressedPreference:PfhorgeVMStrafeLeftKeyPreference fallback:'a']) {
        *position -= right * step;
        moved = YES;
    }
    if ([self pressedPreference:PfhorgeVMStrafeRightKeyPreference fallback:'d']) {
        *position += right * step;
        moved = YES;
    }
    if ([self pressedPreference:PfhorgeVMFlyDownKeyPreference fallback:'q']) {
        position->y -= step * _verticalMovementScale;
        moved = YES;
    }
    if ([self pressedPreference:PfhorgeVMFlyUpKeyPreference fallback:'e']) {
        position->y += step * _verticalMovementScale;
        moved = YES;
    }

    if (moved && _portalModeEnabled) {
        [self validatePortalCameraPosition];
        [self rebuildPreviewFrame];
        [self rebuildVertexBuffer];
    }
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

    _diagnosticsLabel.stringValue = [NSString stringWithFormat:
        @"%.1f FPS | camera=(%.2f, %.2f, %.2f) | seed=%u\n"
         "visible=%zu polygons / %zu surfaces | portals=%u accepted | vertices=%zu",
        _smoothedFPS,
        _portalCameraPosition.x,
        _portalCameraPosition.y,
        _portalCameraPosition.z,
        _portalSeedPolygonID,
        _portalDiagnostics.visiblePolygonCount,
        _portalDiagnostics.visibleSurfaceCount,
        _portalDiagnostics.portalsAccepted,
        _metalVertices.size()];
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
    const pfhorge::preview::StableID seedPolygonID =
        _portalSeedPolygonID;
    const auto iterator = std::find_if(
        _previewScene.polygons.begin(),
        _previewScene.polygons.end(),
        [seedPolygonID](const pfhorge::preview::PreviewPolygon& polygon) {
            return polygon.id == seedPolygonID;
        });
    if (iterator == _previewScene.polygons.end()) {
        return;
    }

    _portalCameraPosition.y = std::clamp(
        _portalCameraPosition.y,
        iterator->floorHeight + 0.05F,
        iterator->ceilingHeight - 0.05F);
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

    const std::vector<PreviewSurface>& surfaces =
        (_portalModeEnabled &&
         _previewFrame.cameraInsideScene)
            ? _previewFrame.visibleSurfaces
            : _previewScene.surfaces;

    std::size_t indexCount = 0U;

    for (const PreviewSurface& surface : surfaces) {
        indexCount += surface.indices.size();
    }

    _metalVertices.reserve(indexCount);

    for (const PreviewSurface& surface : surfaces) {
        const simd_float4 color =
            ColorForSurface(surface);

        for (const std::uint32_t index :
             surface.indices) {
            if (index >= surface.vertices.size()) {
                continue;
            }

            const PreviewVertex& vertex =
                surface.vertices[index];

            _metalVertices.push_back(MetalPreviewVertex{
                simd_make_float3(
                    vertex.position.x,
                    vertex.position.y,
                    vertex.position.z),
                color,
            });
        }
    }

    if (_metalVertices.empty()) {
        _vertexBuffer = nil;
        return;
    }

    _vertexBuffer =
        [self.device newBufferWithBytes:
                         _metalVertices.data()
                                 length:
                         _metalVertices.size() *
                         sizeof(MetalPreviewVertex)
                                options:
                         MTLResourceStorageModeShared];
    _vertexBuffer.label =
        @"Pfhorge VM-3 Portal Geometry";
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
        std::max<CGFloat>(1.0, self.bounds.size.width * backingScale * _renderScale),
        std::max<CGFloat>(1.0, self.bounds.size.height * backingScale * _renderScale));
    if (std::fabs(self.drawableSize.width - desiredDrawableSize.width) > 1.0 ||
        std::fabs(self.drawableSize.height - desiredDrawableSize.height) > 1.0) {
        self.drawableSize = desiredDrawableSize;
    }
    [self updateDiagnosticsOverlay];

    MTLRenderPassDescriptor *passDescriptor =
        view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable =
        view.currentDrawable;

    if (passDescriptor == nil ||
        drawable == nil ||
        _pipelineState == nil ||
        _commandQueue == nil) {
        return;
    }

    id<MTLCommandBuffer> commandBuffer =
        [_commandQueue commandBuffer];
    commandBuffer.label =
        @"Pfhorge VM-3 Frame";

    id<MTLRenderCommandEncoder> encoder =
        [commandBuffer renderCommandEncoderWithDescriptor:
                       passDescriptor];

    if (encoder == nil) {
        return;
    }

    [encoder setRenderPipelineState:_pipelineState];
    [encoder setDepthStencilState:_depthStencilState];
    [encoder setCullMode:MTLCullModeNone];

    if (_vertexBuffer != nil &&
        !_metalVertices.empty()) {
        MetalPreviewUniforms uniforms{
            [self viewProjectionMatrix],
        };

        [encoder setVertexBuffer:_vertexBuffer
                         offset:0U
                        atIndex:0U];
        [encoder setVertexBytes:&uniforms
                         length:sizeof(uniforms)
                        atIndex:1U];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                    vertexStart:0U
                    vertexCount:_metalVertices.size()];
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
    [self.window makeFirstResponder:self];
}

- (void)mouseDragged:(NSEvent *)event
{
    const NSPoint location =
        [self convertPoint:event.locationInWindow
                  fromView:nil];

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
    const unichar orbitKey =
        [self configuredKeyForPreference:PfhorgeVMOrbitKeyPreference fallback:'p'];
    const unichar resetKey =
        [self configuredKeyForPreference:PfhorgeVMResetKeyPreference fallback:'r'];
    const unichar diagnosticsKey =
        [self configuredKeyForPreference:PfhorgeVMDiagnosticsKeyPreference fallback:'i'];

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
            @"Metal Visual Mode VM-3 camera mode: %@",
            _portalModeEnabled
                ? @"portal first-person"
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
    for (NSUInteger index = 0; index < movementPreferences.count; ++index) {
        if (key == [self configuredKeyForPreference:movementPreferences[index]
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
