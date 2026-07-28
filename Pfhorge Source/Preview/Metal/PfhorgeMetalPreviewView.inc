// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Adam Vadala-Roth
//
// This implementation is included once by MyOpenGLView2.mm through
// MyOpenGLView2.h. Keeping VM-2 in an include file avoids a risky Xcode project
// rewrite while the modern preview target structure is still being established.

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <simd/simd.h>

#include "../Core/PreviewSceneBuilder.hpp"

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

}  // namespace

@implementation PfhorgeMetalPreviewView {
    PreviewScene _previewScene;
    std::vector<MetalPreviewVertex> _metalVertices;

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
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();

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
    self.preferredFramesPerSecond = 60;
    self.enableSetNeedsDisplay = NO;
    self.paused = NO;
    self.delegate = self;

    _commandQueue = [device newCommandQueue];

    if (_commandQueue == nil ||
        ![self buildPipelineForDevice:device]) {
        return nil;
    }

    _previewScene =
        pfhorge::preview::BuildPreviewScene(levelData);
    [self rebuildVertexBuffer];
    [self resetCamera];

    NSLog(
        @"Metal Visual Mode VM-2: %zu polygons, "
         "%zu surfaces, %zu rendered vertices",
        _previewScene.polygons.size(),
        _previewScene.surfaces.size(),
        _metalVertices.size());

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
        @"Pfhorge VM-2 Untextured Geometry";
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat =
        self.colorPixelFormat;
    pipelineDescriptor.depthAttachmentPixelFormat =
        self.depthStencilPixelFormat;

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

- (void)rebuildVertexBuffer
{
    _metalVertices.clear();

    std::size_t indexCount = 0U;

    for (const PreviewSurface& surface :
         _previewScene.surfaces) {
        indexCount += surface.indices.size();
    }

    _metalVertices.reserve(indexCount);

    for (const PreviewSurface& surface :
         _previewScene.surfaces) {
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
        @"Pfhorge VM-2 Geometry";
}

- (void)resetCamera
{
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
    const float cosPitch = std::cos(_cameraPitch);

    const simd_float3 orbitDirection =
        simd_make_float3(
            std::sin(_cameraYaw) * cosPitch,
            std::sin(_cameraPitch),
            std::cos(_cameraYaw) * cosPitch);

    const simd_float3 eye =
        _cameraTarget +
        orbitDirection * _cameraDistance;

    const matrix_float4x4 view =
        MakeLookAtMatrix(
            eye,
            _cameraTarget,
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
        std::max(0.01F, _cameraDistance / 1000.0F);
    const float farPlane =
        std::max(1000.0F, _cameraDistance * 50.0F);

    const matrix_float4x4 projection =
        MakePerspectiveMatrix(
            60.0F *
            static_cast<float>(M_PI) /
            180.0F,
            aspectRatio,
            nearPlane,
            farPlane);

    return simd_mul(projection, view);
}

- (void)drawInMTKView:(MTKView *)view
{
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
        @"Pfhorge VM-2 Frame";

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
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];

    if (self.window != nil) {
        [self.window makeFirstResponder:self];
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

    _cameraYaw -= deltaX * 0.01F;
    _cameraPitch =
        std::clamp(
            _cameraPitch + deltaY * 0.01F,
            -1.45F,
            1.45F);

    _lastDragLocation = location;
}

- (void)scrollWheel:(NSEvent *)event
{
    const float zoomFactor =
        std::exp(
            static_cast<float>(event.scrollingDeltaY) *
            0.035F);

    _cameraDistance =
        std::clamp(
            _cameraDistance * zoomFactor,
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

    const unichar key =
        [characters characterAtIndex:0U];
    const float step =
        std::max(0.05F, _cameraDistance * 0.04F);

    simd_float3 forward =
        simd_make_float3(
            -std::sin(_cameraYaw),
            0.0F,
            -std::cos(_cameraYaw));

    if (simd_length_squared(forward) >
        0.000001F) {
        forward = simd_normalize(forward);
    }

    const simd_float3 right =
        simd_make_float3(
            -forward.z,
            0.0F,
            forward.x);

    switch (key) {
        case 'w':
            _cameraTarget += forward * step;
            break;

        case 's':
            _cameraTarget -= forward * step;
            break;

        case 'a':
            _cameraTarget -= right * step;
            break;

        case 'd':
            _cameraTarget += right * step;
            break;

        case 'q':
            _cameraTarget.y -= step;
            break;

        case 'e':
            _cameraTarget.y += step;
            break;

        case 'r':
            [self resetCamera];
            break;

        default:
            [super keyDown:event];
            break;
    }
}

@end
