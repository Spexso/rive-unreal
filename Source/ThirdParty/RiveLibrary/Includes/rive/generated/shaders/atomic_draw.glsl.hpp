#pragma once

#include "atomic_draw.exports.h"

namespace rive {
namespace gpu {
namespace glsl {
const char atomic_draw[] = R"===(/*
 * Copyright 2023 Rive
 */

#ifdef _EXPORTED_DRAW_PATH
#ifdef _EXPORTED_VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0,
     float4,
     _EXPORTED_a_patchVertexData); // [localVertexID, outset, fillCoverage, vertexType]
ATTR(1, float4, _EXPORTED_a_mirroredVertexData);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, half2, v_edgeDistance);
FLAT VARYING(1, ushort, v_pathID);
VARYING_BLOCK_END

#ifdef _EXPORTED_VERTEX
VERTEX_MAIN(_EXPORTED_drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_vertexID, attrs, _EXPORTED_a_patchVertexData, float4);
    ATTR_UNPACK(_vertexID, attrs, _EXPORTED_a_mirroredVertexData, float4);

    VARYING_INIT(v_edgeDistance, half2);
    VARYING_INIT(v_pathID, ushort);

    float4 pos;
    uint pathID;
    float2 vertexPosition;
    if (unpack_tessellated_path_vertex(_EXPORTED_a_patchVertexData,
                                       _EXPORTED_a_mirroredVertexData,
                                       _instanceID,
                                       pathID,
                                       vertexPosition,
                                       v_edgeDistance VERTEX_CONTEXT_UNPACK))
    {
        v_pathID = cast_uint_to_ushort(pathID);
        pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);
    }
    else
    {
        pos = float4(uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue,
                     uniforms.vertexDiscardValue);
    }

    VARYING_PACK(v_edgeDistance);
    VARYING_PACK(v_pathID);
    EMIT_VERTEX(pos);
}
#endif // VERTEX
#endif // DRAW_PATH

#ifdef _EXPORTED_DRAW_INTERIOR_TRIANGLES
#ifdef _EXPORTED_VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0, packed_float3, _EXPORTED_a_triangleVertex);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
_EXPORTED_OPTIONALLY_FLAT VARYING(0, half, v_windingWeight);
FLAT VARYING(1, ushort, v_pathID);
VARYING_BLOCK_END

#ifdef _EXPORTED_VERTEX
VERTEX_MAIN(_EXPORTED_drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_vertexID, attrs, _EXPORTED_a_triangleVertex, float3);

    VARYING_INIT(v_windingWeight, half);
    VARYING_INIT(v_pathID, ushort);

    uint pathID;
    float2 vertexPosition =
        unpack_interior_triangle_vertex(_EXPORTED_a_triangleVertex,
                                        pathID,
                                        v_windingWeight VERTEX_CONTEXT_UNPACK);
    v_pathID = cast_uint_to_ushort(pathID);
    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);

    VARYING_PACK(v_windingWeight);
    VARYING_PACK(v_pathID);
    EMIT_VERTEX(pos);
}
#endif // VERTEX
#endif // DRAW_INTERIOR_TRIANGLES

#ifdef _EXPORTED_DRAW_IMAGE_RECT
#ifdef _EXPORTED_VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR(0, float4, _EXPORTED_a_imageRectVertex);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float2, v_texCoord);
NO_PERSPECTIVE VARYING(1, half, v_edgeCoverage);
#ifdef _EXPORTED_ENABLE_CLIP_RECT
NO_PERSPECTIVE VARYING(2, float4, v_clipRect);
#endif
VARYING_BLOCK_END

#ifdef _EXPORTED_VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN
VERTEX_TEXTURE_BLOCK_END

VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
VERTEX_STORAGE_BUFFER_BLOCK_END

IMAGE_RECT_VERTEX_MAIN(_EXPORTED_drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    ATTR_UNPACK(_vertexID, attrs, _EXPORTED_a_imageRectVertex, float4);

    VARYING_INIT(v_texCoord, float2);
    VARYING_INIT(v_edgeCoverage, half);
#ifdef _EXPORTED_ENABLE_CLIP_RECT
    VARYING_INIT(v_clipRect, float4);
#endif

    bool isOuterVertex =
        _EXPORTED_a_imageRectVertex.z == .0 || _EXPORTED_a_imageRectVertex.w == .0;
    v_edgeCoverage = isOuterVertex ? .0 : 1.;

    float2 vertexPosition = _EXPORTED_a_imageRectVertex.xy;
    float2x2 M = make_float2x2(imageDrawUniforms.viewMatrix);
    float2x2 MIT = transpose(inverse(M));
    if (!isOuterVertex)
    {
        // Inset the inner vertices to the point where coverage == 1.
        // NOTE: if width/height ever change from 1, these equations need to be
        // updated.
        float aaRadiusX =
            AA_RADIUS * manhattan_width(MIT[1]) / dot(M[1], MIT[1]);
        if (aaRadiusX >= .5)
        {
            vertexPosition.x = .5;
            v_edgeCoverage *= cast_float_to_half(.5 / aaRadiusX);
        }
        else
        {
            vertexPosition.x += aaRadiusX * _EXPORTED_a_imageRectVertex.z;
        }
        float aaRadiusY =
            AA_RADIUS * manhattan_width(MIT[0]) / dot(M[0], MIT[0]);
        if (aaRadiusY >= .5)
        {
            vertexPosition.y = .5;
            v_edgeCoverage *= cast_float_to_half(.5 / aaRadiusY);
        }
        else
        {
            vertexPosition.y += aaRadiusY * _EXPORTED_a_imageRectVertex.w;
        }
    }

    v_texCoord = vertexPosition;
    vertexPosition = MUL(M, vertexPosition) + imageDrawUniforms.translate;

    if (isOuterVertex)
    {
        // Outset the outer vertices to the point where coverage == 0.
        float2 n = MUL(MIT, _EXPORTED_a_imageRectVertex.zw);
        n *= manhattan_width(n) / dot(n, n);
        vertexPosition += AA_RADIUS * n;
    }

#ifdef _EXPORTED_ENABLE_CLIP_RECT
    if (_EXPORTED_ENABLE_CLIP_RECT)
    {
        v_clipRect = find_clip_rect_coverage_distances(
            make_float2x2(imageDrawUniforms.clipRectInverseMatrix),
            imageDrawUniforms.clipRectInverseTranslate,
            vertexPosition);
    }
#endif

    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);

    VARYING_PACK(v_texCoord);
    VARYING_PACK(v_edgeCoverage);
#ifdef _EXPORTED_ENABLE_CLIP_RECT
    VARYING_PACK(v_clipRect);
#endif
    EMIT_VERTEX(pos);
}
#endif // VERTEX

#elif defined(_EXPORTED_DRAW_IMAGE_MESH)
#ifdef _EXPORTED_VERTEX
ATTR_BLOCK_BEGIN(PositionAttr)
ATTR(0, float2, _EXPORTED_a_position);
ATTR_BLOCK_END

ATTR_BLOCK_BEGIN(UVAttr)
ATTR(1, float2, _EXPORTED_a_texCoord);
ATTR_BLOCK_END
#endif

VARYING_BLOCK_BEGIN
NO_PERSPECTIVE VARYING(0, float2, v_texCoord);
#ifdef _EXPORTED_ENABLE_CLIP_RECT
NO_PERSPECTIVE VARYING(1, float4, v_clipRect);
#endif
VARYING_BLOCK_END

#ifdef _EXPORTED_VERTEX
IMAGE_MESH_VERTEX_MAIN(_EXPORTED_drawVertexMain,
                       PositionAttr,
                       position,
                       UVAttr,
                       uv,
                       _vertexID)
{
    ATTR_UNPACK(_vertexID, position, _EXPORTED_a_position, float2);
    ATTR_UNPACK(_vertexID, uv, _EXPORTED_a_texCoord, float2);

    VARYING_INIT(v_texCoord, float2);
#ifdef _EXPORTED_ENABLE_CLIP_RECT
    VARYING_INIT(v_clipRect, float4);
#endif

    float2x2 M = make_float2x2(imageDrawUniforms.viewMatrix);
    float2 vertexPosition = MUL(M, _EXPORTED_a_position) + imageDrawUniforms.translate;
    v_texCoord = _EXPORTED_a_texCoord;

#ifdef _EXPORTED_ENABLE_CLIP_RECT
    if (_EXPORTED_ENABLE_CLIP_RECT)
    {
        v_clipRect = find_clip_rect_coverage_distances(
            make_float2x2(imageDrawUniforms.clipRectInverseMatrix),
            imageDrawUniforms.clipRectInverseTranslate,
            vertexPosition);
    }
#endif

    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(vertexPosition);

    VARYING_PACK(v_texCoord);
#ifdef _EXPORTED_ENABLE_CLIP_RECT
    VARYING_PACK(v_clipRect);
#endif
    EMIT_VERTEX(pos);
}
#endif // VERTEX
#endif // DRAW_IMAGE_MESH

#ifdef _EXPORTED_DRAW_RENDER_TARGET_UPDATE_BOUNDS
#ifdef _EXPORTED_VERTEX
ATTR_BLOCK_BEGIN(Attrs)
ATTR_BLOCK_END
#endif // VERTEX

VARYING_BLOCK_BEGIN
VARYING_BLOCK_END

#ifdef _EXPORTED_VERTEX
VERTEX_TEXTURE_BLOCK_BEGIN
VERTEX_TEXTURE_BLOCK_END

VERTEX_STORAGE_BUFFER_BLOCK_BEGIN
VERTEX_STORAGE_BUFFER_BLOCK_END

VERTEX_MAIN(_EXPORTED_drawVertexMain, Attrs, attrs, _vertexID, _instanceID)
{
    int2 coord;
    coord.x = (_vertexID & 1) == 0 ? uniforms.renderTargetUpdateBounds.x
                                   : uniforms.renderTargetUpdateBounds.z;
    coord.y = (_vertexID & 2) == 0 ? uniforms.renderTargetUpdateBounds.y
                                   : uniforms.renderTargetUpdateBounds.w;
    float4 pos = RENDER_TARGET_COORD_TO_CLIP_COORD(float2(coord));
    EMIT_VERTEX(pos);
}
#endif // VERTEX
#endif // DRAW_RENDER_TARGET_UPDATE_BOUNDS

#ifdef _EXPORTED_DRAW_IMAGE
#define NEEDS_IMAGE_TEXTURE
#endif

#ifdef _EXPORTED_FRAGMENT
FRAG_TEXTURE_BLOCK_BEGIN
TEXTURE_RGBA8(PER_FLUSH_BINDINGS_SET, GRAD_TEXTURE_IDX, _EXPORTED_gradTexture);
#ifdef _EXPORTED_ENABLE_FEATHER
TEXTURE_R16F(PER_FLUSH_BINDINGS_SET, FEATHER_TEXTURE_IDX, _EXPORTED_featherTexture);
#endif
#ifdef NEEDS_IMAGE_TEXTURE
TEXTURE_RGBA8(PER_DRAW_BINDINGS_SET, IMAGE_TEXTURE_IDX, _EXPORTED_imageTexture);
#endif
FRAG_TEXTURE_BLOCK_END

SAMPLER_LINEAR(GRAD_TEXTURE_IDX, gradSampler)
#ifdef _EXPORTED_ENABLE_FEATHER
SAMPLER_LINEAR(FEATHER_TEXTURE_IDX, featherSampler)
#endif
#ifdef NEEDS_IMAGE_TEXTURE
SAMPLER_MIPMAP(IMAGE_TEXTURE_IDX, imageSampler)
#endif

PLS_BLOCK_BEGIN
// We only write the framebuffer as a storage texture when there are blend
// modes. Otherwise, we render to it as a normal color attachment.
#ifndef _EXPORTED_FIXED_FUNCTION_COLOR_OUTPUT
#ifdef _EXPORTED_COLOR_PLANE_IDX_OVERRIDE
// D3D11 doesn't let us bind the framebuffer UAV to slot 0 when there is a color
// output.
PLS_DECL4F(_EXPORTED_COLOR_PLANE_IDX_OVERRIDE, colorBuffer);
#else
PLS_DECL4F(COLOR_PLANE_IDX, colorBuffer);
#endif
#endif // !FIXED_FUNCTION_COLOR_OUTPUT
#ifdef _EXPORTED_PLS_BLEND_SRC_OVER
// When PLS has src-over blending enabled, the clip buffer is RGBA8 so we can
// preserve clip contents by emitting a=0 instead of loading the current value.
// This is also is a hint to the hardware that it doesn't need to write anything
// to the clip attachment.
#define CLIP_VALUE_TYPE  half4
#define PLS_LOAD_CLIP_TYPE  PLS_LOAD4F
#define MAKE_NON_UPDATING_CLIP_VALUE  make_half4(.0)
#define HAS_UPDATED_CLIP_VALUE(X)  ((X).w != .0)
#ifdef _EXPORTED_ENABLE_CLIPPING
#ifndef _EXPORTED_RESOLVE_PLS
PLS_DECL4F(CLIP_PLANE_IDX, clipBuffer);
#else
PLS_DECL4F_READONLY(CLIP_PLANE_IDX, clipBuffer);
#endif
#endif // ENABLE_CLIPPING
#else
// When PLS does not have src-over blending, the clip buffer the usual packed
// R32UI.
#define CLIP_VALUE_TYPE  uint
#define MAKE_NON_UPDATING_CLIP_VALUE  0u
#define PLS_LOAD_CLIP_TYPE  PLS_LOADUI
#define HAS_UPDATED_CLIP_VALUE(X)  ((X) != 0u)
#ifdef _EXPORTED_ENABLE_CLIPPING
PLS_DECLUI(CLIP_PLANE_IDX, clipBuffer);
#endif // ENABLE_CLIPPING
#endif // !PLS_BLEND_SRC_OVER
PLS_DECLUI_ATOMIC(COVERAGE_PLANE_IDX, coverageAtomicBuffer);
PLS_BLOCK_END

FRAG_STORAGE_BUFFER_BLOCK_BEGIN
STORAGE_BUFFER_U32x2(PAINT_BUFFER_IDX, PaintBuffer, _EXPORTED_paintBuffer);
STORAGE_BUFFER_F32x4(PAINT_AUX_BUFFER_IDX, PaintAuxBuffer, _EXPORTED_paintAuxBuffer);
FRAG_STORAGE_BUFFER_BLOCK_END

INLINE uint to_fixed(float x)
{
    return uint(round(x * FIXED_COVERAGE_PRECISION + FIXED_COVERAGE_ZERO));
}

INLINE half from_fixed(uint x)
{
    return cast_float_to_half(
        float(x) * FIXED_COVERAGE_INVERSE_PRECISION +
        (-FIXED_COVERAGE_ZERO * FIXED_COVERAGE_INVERSE_PRECISION));
}

#ifdef _EXPORTED_ENABLE_CLIPPING
INLINE void apply_clip(uint clipID,
                       CLIP_VALUE_TYPE clipData,
                       INOUT(half) coverage)
{
#ifdef _EXPORTED_PLS_BLEND_SRC_OVER
    // The clipID is packed into r & g of clipData. Use a fuzzy equality test
    // since we lose precision when the clip value gets stored to and from the
    // attachment.
    if (all(lessThan(abs(clipData.xy - unpackUnorm4x8(clipID).xy),
                     make_half2(.25 / 255.))))
        coverage = min(coverage, clipData.z);
    else
        coverage = .0;
#else
    // The clipID is the top 16 bits of clipData.
    if (clipID == clipData >> 16)
        coverage = min(coverage, unpackHalf2x16(clipData).x);
    else
        coverage = .0;
#endif
}
#endif

INLINE void resolve_paint(uint pathID,
                          half coverageCount,
                          OUT(half4) fragColorOut
#if defined(_EXPORTED_ENABLE_CLIPPING) && !defined(_EXPORTED_RESOLVE_PLS)
                          ,
                          INOUT(CLIP_VALUE_TYPE) fragClipOut
#endif
                              FRAGMENT_CONTEXT_DECL PLS_CONTEXT_DECL)
{
    uint2 paintData = STORAGE_BUFFER_LOAD2(_EXPORTED_paintBuffer, pathID);
    half coverage = coverageCount;
    if ((paintData.x & (PAINT_FLAG_NON_ZERO_FILL | PAINT_FLAG_EVEN_ODD_FILL)) !=
        0u)
    {
        // This path has a legacy (non-clockwise) fill.
        coverage = abs(coverage);
#ifdef _EXPORTED_ENABLE_EVEN_ODD
        if (_EXPORTED_ENABLE_EVEN_ODD && (paintData.x & PAINT_FLAG_EVEN_ODD_FILL) != 0u)
        {
            coverage = 1. - abs(fract(coverage * .5) * 2. + -1.);
        }
#endif
    }
    // This also caps stroke coverage, which can be >1.
    coverage = clamp(coverage, make_half(.0), make_half(1.));
#ifdef _EXPORTED_ENABLE_CLIPPING
    if (_EXPORTED_ENABLE_CLIPPING)
    {
        uint clipID = paintData.x >> 16u;
        if (clipID != 0u)
        {
            apply_clip(clipID, PLS_LOAD_CLIP_TYPE(clipBuffer), coverage);
        }
    }
#endif // ENABLE_CLIPPING
#ifdef _EXPORTED_ENABLE_CLIP_RECT
    if (_EXPORTED_ENABLE_CLIP_RECT && (paintData.x & PAINT_FLAG_HAS_CLIP_RECT) != 0u)
    {
        float2x2 M = make_float2x2(
            STORAGE_BUFFER_LOAD4(_EXPORTED_paintAuxBuffer, pathID * 4u + 2u));
        float4 translate =
            STORAGE_BUFFER_LOAD4(_EXPORTED_paintAuxBuffer, pathID * 4u + 3u);
        float2 clipCoord = MUL(M, _fragCoord) + translate.xy;
        // translate.zw contains -1 / fwidth(clipCoord), which we use to
        // calculate antialiasing.
        half2 distXY =
            cast_float2_to_half2(abs(clipCoord) * translate.zw - translate.zw);
        half clipRectCoverage = clamp(min(distXY.x, distXY.y) + .5, .0, 1.);
        coverage = min(coverage, clipRectCoverage);
    }
#endif // ENABLE_CLIP_RECT
    uint paintType = paintData.x & 0xfu;
    if (paintType <= SOLID_COLOR_PAINT_TYPE) // CLIP_UPDATE_PAINT_TYPE or
                                             // SOLID_COLOR_PAINT_TYPE
    {
        fragColorOut = unpackUnorm4x8(paintData.y);
#ifdef _EXPORTED_ENABLE_CLIPPING
        if (_EXPORTED_ENABLE_CLIPPING && paintType == CLIP_UPDATE_PAINT_TYPE)
        {
#ifndef _EXPORTED_RESOLVE_PLS
#ifdef _EXPORTED_PLS_BLEND_SRC_OVER
            // This was actually a clip update. fragColorOut contains the packed
            // clipID.
            fragClipOut.xy = fragColorOut.zw; // Pack the clipID into r & g.
            fragClipOut.z = coverage;         // Put the clipCoverage in b.
            fragClipOut.w =
                1.; // a=1 so we replace the value in the clipBuffer.
#else
            fragClipOut = paintData.y | packHalf2x16(make_half2(coverage, .0));
#endif
#endif
            // Don't update the colorBuffer when this is a clip update.
            fragColorOut = make_half4(.0);
        }
#endif
    }
    else // LINEAR_GRADIENT_PAINT_TYPE or RADIAL_GRADIENT_PAINT_TYPE
    {
        float2x2 M =
            make_float2x2(STORAGE_BUFFER_LOAD4(_EXPORTED_paintAuxBuffer, pathID * 4u));
        float4 translate =
            STORAGE_BUFFER_LOAD4(_EXPORTED_paintAuxBuffer, pathID * 4u + 1u);
        float2 paintCoord = MUL(M, _fragCoord) + translate.xy;
        float t = paintType == LINEAR_GRADIENT_PAINT_TYPE
                      ? /*linear*/ paintCoord.x
                      : /*radial*/ length(paintCoord);
        t = clamp(t, .0, 1.);
        float x = t * translate.z + translate.w;
        float y = uintBitsToFloat(paintData.y);
        fragColorOut =
            TEXTURE_SAMPLE_LOD(_EXPORTED_gradTexture, gradSampler, float2(x, y), .0);
    }
    fragColorOut.w *= coverage;

#if !defined(_EXPORTED_FIXED_FUNCTION_COLOR_OUTPUT) && defined(_EXPORTED_ENABLE_ADVANCED_BLEND)
    // Apply the advanced blend mode, if applicable.
    ushort blendMode;
    if (_EXPORTED_ENABLE_ADVANCED_BLEND && fragColorOut.w != .0 &&
        (blendMode = cast_uint_to_ushort((paintData.x >> 4) & 0xfu)) != 0u)
    {
        half4 dstColor = PLS_LOAD4F(colorBuffer);
        fragColorOut.xyz =
            advanced_color_blend_pre_src_over(fragColorOut.xyz,
                                              unmultiply(dstColor),
                                              blendMode);
    }
#endif // !FIXED_FUNCTION_COLOR_OUTPUT && ENABLE_ADVANCED_BLEND

    // When PLS_BLEND_SRC_OVER is defined, the caller and/or blend state
    // multiply alpha into fragColorOut for us. Otherwise, we have to
    // premultiply it.
#ifndef _EXPORTED_PLS_BLEND_SRC_OVER
    fragColorOut.xyz *= fragColorOut.w;
#endif
}

#ifndef _EXPORTED_FIXED_FUNCTION_COLOR_OUTPUT
INLINE void emit_pls_color(half4 fragColorOut PLS_CONTEXT_DECL)
{
#ifndef _EXPORTED_PLS_BLEND_SRC_OVER
    if (fragColorOut.w == .0)
        return;
    float oneMinusSrcAlpha = 1. - fragColorOut.w;
    if (oneMinusSrcAlpha != .0)
        fragColorOut =
            PLS_LOAD4F(colorBuffer) * oneMinusSrcAlpha + fragColorOut;
#endif
    PLS_STORE4F(colorBuffer, fragColorOut);
}
#endif // !FIXED_FUNCTION_COLOR_OUTPUT

#if defined(_EXPORTED_ENABLE_CLIPPING) && !defined(_EXPORTED_RESOLVE_PLS)
INLINE void emit_pls_clip(CLIP_VALUE_TYPE fragClipOut PLS_CONTEXT_DECL)
{
#ifdef _EXPORTED_PLS_BLEND_SRC_OVER
    PLS_STORE4F(clipBuffer, fragClipOut);
#else
    if (fragClipOut != 0u)
        PLS_STOREUI(clipBuffer, fragClipOut);
#endif
}
#endif // ENABLE_CLIPPING && !RESOLVE_PLS

#ifdef _EXPORTED_FIXED_FUNCTION_COLOR_OUTPUT
#define ATOMIC_PLS_MAIN  PLS_FRAG_COLOR_MAIN
#define ATOMIC_PLS_MAIN_WITH_IMAGE_UNIFORMS                                     \
    PLS_FRAG_COLOR_MAIN_WITH_IMAGE_UNIFORMS
#define EMIT_ATOMIC_PLS  EMIT_PLS_AND_FRAG_COLOR
#else // !FIXED_FUNCTION_COLOR_OUTPUT
#define ATOMIC_PLS_MAIN  PLS_MAIN
#define ATOMIC_PLS_MAIN_WITH_IMAGE_UNIFORMS  PLS_MAIN_WITH_IMAGE_UNIFORMS
#define EMIT_ATOMIC_PLS  EMIT_PLS
#endif

#ifdef _EXPORTED_DRAW_PATH
ATOMIC_PLS_MAIN(_EXPORTED_drawFragmentMain)
{
    VARYING_UNPACK(v_edgeDistance, half2);
    VARYING_UNPACK(v_pathID, ushort);

    half fragmentCoverage;
#ifdef _EXPORTED_ENABLE_FEATHER
    if (_EXPORTED_ENABLE_FEATHER && is_feathered_stroke(v_edgeDistance))
    {
        fragmentCoverage = feathered_stroke_coverage(
            v_edgeDistance,
            SAMPLED_R16F(_EXPORTED_featherTexture, featherSampler));
    }
    else if (_EXPORTED_ENABLE_FEATHER && is_feathered_fill(v_edgeDistance))
    {
        fragmentCoverage = feathered_fill_coverage(
            v_edgeDistance,
            SAMPLED_R16F(_EXPORTED_featherTexture, featherSampler));
    }
    else
#endif
    {
        // Cover stroke and fill both in a branchless expression.
        fragmentCoverage =
            min(min(v_edgeDistance.x, abs(v_edgeDistance.y)), make_half(1.));
    }

    // Since v_pathID increases monotonically with every draw, and since it
    // lives in the most significant bits of the coverage data, an atomic max()
    // function will serve 3 purposes:
    //
    //    * The invocation that changes the pathID is the single first fragment
    //      invocation to hit the new path, and the one that should resolve the
    //      previous path in the framebuffer.
    //    * Properly resets coverage to zero when we do cross over into
    //      processing a new path.
    //    * Accumulates coverage for strokes.
    //
    uint fixedCoverage = to_fixed(fragmentCoverage);
    uint minCoverageData =
        (make_uint(v_pathID) << FIXED_COVERAGE_BIT_COUNT) | fixedCoverage;
    uint lastCoverageData =
        PLS_ATOMIC_MAX(coverageAtomicBuffer, minCoverageData);
    ushort lastPathID =
        cast_uint_to_ushort(lastCoverageData >> FIXED_COVERAGE_BIT_COUNT);
    if (lastPathID == v_pathID)
    {
        // This is not the first fragment of the current path to touch this
        // pixel. We already resolved the previous path, so just update coverage
        // (if we're a fill) and move on.
        if (!is_stroke(v_edgeDistance))
        {
            // Only apply the effect of the min() the first time we cross into a
            // path.
            fixedCoverage +=
                lastCoverageData - max(minCoverageData, lastCoverageData);
            fixedCoverage -=
                FIXED_COVERAGE_ZERO_UINT; // Only apply the zero bias once.
            PLS_ATOMIC_ADD(coverageAtomicBuffer,
                           fixedCoverage); // Count coverage.
        }
        discard;
    }

    // We crossed into a new path! Resolve the previous path now that we know
    // its exact coverage.
    half coverageCount = from_fixed(lastCoverageData & FIXED_COVERAGE_MASK);
    half4 fragColorOut;
#ifdef _EXPORTED_ENABLE_CLIPPING
    CLIP_VALUE_TYPE fragClipOut = MAKE_NON_UPDATING_CLIP_VALUE;
#endif
    resolve_paint(lastPathID,
                  coverageCount,
                  fragColorOut
#ifdef _EXPORTED_ENABLE_CLIPPING
                  ,
                  fragClipOut
#endif
                      FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK);

#ifdef _EXPORTED_FIXED_FUNCTION_COLOR_OUTPUT
    _fragColor = fragColorOut;
#else
    emit_pls_color(fragColorOut PLS_CONTEXT_UNPACK);
#endif
#ifdef _EXPORTED_ENABLE_CLIPPING
    emit_pls_clip(fragClipOut PLS_CONTEXT_UNPACK);
#endif

    EMIT_ATOMIC_PLS
}
#endif // DRAW_PATH

#ifdef _EXPORTED_DRAW_INTERIOR_TRIANGLES
ATOMIC_PLS_MAIN(_EXPORTED_drawFragmentMain)
{
    VARYING_UNPACK(v_windingWeight, half);
    VARYING_UNPACK(v_pathID, ushort);

    uint lastCoverageData = PLS_LOADUI_ATOMIC(coverageAtomicBuffer);
    ushort lastPathID =
        cast_uint_to_ushort(lastCoverageData >> FIXED_COVERAGE_BIT_COUNT);

    // Update coverageAtomicBuffer with the coverage weight of the current
    // triangle. This does not need to be atomic since interior triangles don't
    // overlap.
    int coverageDeltaFixed =
        int(round(v_windingWeight * FIXED_COVERAGE_PRECISION));
    uint currPathCoverageData =
        lastPathID == v_pathID
            ? lastCoverageData
            : (make_uint(v_pathID) << FIXED_COVERAGE_BIT_COUNT) +
                  FIXED_COVERAGE_ZERO_UINT;
    PLS_STOREUI_ATOMIC(coverageAtomicBuffer,
                       currPathCoverageData + uint(coverageDeltaFixed));

    if (lastPathID == v_pathID)
    {
        // This is not the first fragment of the current path to touch this
        // pixel. We already resolved the previous path, so just move on.
        discard;
    }

    // We crossed into a new path! Resolve the previous path now that we know
    // its exact coverage.
    half lastCoverageCount = from_fixed(lastCoverageData & FIXED_COVERAGE_MASK);
    half4 fragColorOut;
#ifdef _EXPORTED_ENABLE_CLIPPING
    CLIP_VALUE_TYPE fragClipOut = MAKE_NON_UPDATING_CLIP_VALUE;
#endif
    resolve_paint(lastPathID,
                  lastCoverageCount,
                  fragColorOut
#ifdef _EXPORTED_ENABLE_CLIPPING
                  ,
                  fragClipOut
#endif
                      FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK);

#ifdef _EXPORTED_FIXED_FUNCTION_COLOR_OUTPUT
    _fragColor = fragColorOut;
#else
    emit_pls_color(fragColorOut PLS_CONTEXT_UNPACK);
#endif
#ifdef _EXPORTED_ENABLE_CLIPPING
    emit_pls_clip(fragClipOut PLS_CONTEXT_UNPACK);
#endif

    EMIT_ATOMIC_PLS
}
#endif // DRAW_INTERIOR_TRIANGLES

#ifdef _EXPORTED_DRAW_IMAGE
ATOMIC_PLS_MAIN_WITH_IMAGE_UNIFORMS(_EXPORTED_drawFragmentMain)
{
    VARYING_UNPACK(v_texCoord, float2);
#ifdef _EXPORTED_DRAW_IMAGE_RECT
    VARYING_UNPACK(v_edgeCoverage, half);
#endif
#ifdef _EXPORTED_ENABLE_CLIP_RECT
    VARYING_UNPACK(v_clipRect, float4);
#endif

    // Start by finding the image color. We have to do this immediately instead
    // of allowing it to get resolved later like other draws because the
    // @imageTexture binding is liable to change, and furthermore in the case of
    // imageMeshes, we can't calculate UV coordinates based on fragment
    // position.
    half4 imageColor = TEXTURE_SAMPLE(_EXPORTED_imageTexture, imageSampler, v_texCoord);
    half imageCoverage = 1.;
#ifdef _EXPORTED_DRAW_IMAGE_RECT
    imageCoverage = min(v_edgeCoverage, imageCoverage);
#endif
#ifdef _EXPORTED_ENABLE_CLIP_RECT
    if (_EXPORTED_ENABLE_CLIP_RECT)
    {
        half clipRectCoverage = min_value(cast_float4_to_half4(v_clipRect));
        imageCoverage = clamp(clipRectCoverage, make_half(.0), imageCoverage);
    }
#endif

    // Resolve the previous path.
    uint lastCoverageData = PLS_LOADUI_ATOMIC(coverageAtomicBuffer);
    ushort lastPathID =
        cast_uint_to_ushort(lastCoverageData >> FIXED_COVERAGE_BIT_COUNT);
    half lastCoverageCount = from_fixed(lastCoverageData & FIXED_COVERAGE_MASK);
    half4 fragColorOut;
#ifdef _EXPORTED_ENABLE_CLIPPING
    CLIP_VALUE_TYPE fragClipOut = MAKE_NON_UPDATING_CLIP_VALUE;
#endif
    // TODO: consider not resolving the prior paint if we're solid and the prior
    // paint is not a clip update: (imageColor.a == 1. &&
    //                              imageDrawUniforms.blendMode ==
    //                              BLEND_SRC_OVER && priorPaintType !=
    //                              CLIP_UPDATE_PAINT_TYPE)
    resolve_paint(lastPathID,
                  lastCoverageCount,
                  fragColorOut
#ifdef _EXPORTED_ENABLE_CLIPPING
                  ,
                  fragClipOut
#endif
                      FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK);

#ifdef _EXPORTED_PLS_BLEND_SRC_OVER
    // Image draws use a premultiplied blend state, but resolve_paint() did not
    // premultiply fragColorOut. Multiply fragColorOut by alpha now.
    fragColorOut.xyz *= fragColorOut.w;
#endif

    // Clip the image after resolving the previous path, since that can affect
    // the clip buffer.
#ifdef _EXPORTED_ENABLE_CLIPPING // TODO! ENABLE_IMAGE_CLIPPING in addition to
                        // ENABLE_CLIPPING?
    if (_EXPORTED_ENABLE_CLIPPING && imageDrawUniforms.clipID != 0u)
    {
        CLIP_VALUE_TYPE clipData = HAS_UPDATED_CLIP_VALUE(fragClipOut)
                                       ? fragClipOut
                                       : PLS_LOAD_CLIP_TYPE(clipBuffer);
        apply_clip(imageDrawUniforms.clipID, clipData, imageCoverage);
    }
#endif // ENABLE_CLIPPING
    imageColor.w *=
        imageCoverage * cast_float_to_half(imageDrawUniforms.opacity);

    // Prepare imageColor for premultiplied src-over blending.
#if !defined(_EXPORTED_FIXED_FUNCTION_COLOR_OUTPUT) && defined(_EXPORTED_ENABLE_ADVANCED_BLEND)
    if (_EXPORTED_ENABLE_ADVANCED_BLEND && imageDrawUniforms.blendMode != BLEND_SRC_OVER)
    {
        // Calculate what dstColor will be after applying fragColorOut.
        half4 dstColor =
            PLS_LOAD4F(colorBuffer) * (1. - fragColorOut.w) + fragColorOut;
        // Calculate the imageColor to emit *BEFORE* src-over blending, such
        // that the post-src-over-blend result is equivalent to the blendMode.
        imageColor.xyz = advanced_color_blend_pre_src_over(
            imageColor.xyz,
            unmultiply(dstColor),
            cast_uint_to_ushort(imageDrawUniforms.blendMode));
    }
#endif // !FIXED_FUNCTION_COLOR_OUTPUT && ENABLE_ADVANCED_BLEND

    // Image draws use a premultiplied blend state; premultiply alpha here in
    // the shader.
    imageColor.xyz *= imageColor.w;

    // Leverage the property that premultiplied src-over blending is associative
    // and blend the imageColor and fragColorOut before passing them on to the
    // blending pipeline.
    fragColorOut = fragColorOut * (1. - imageColor.w) + imageColor;

#ifdef _EXPORTED_FIXED_FUNCTION_COLOR_OUTPUT
    _fragColor = fragColorOut;
#else
    emit_pls_color(fragColorOut PLS_CONTEXT_UNPACK);
#endif
#ifdef _EXPORTED_ENABLE_CLIPPING
    emit_pls_clip(fragClipOut PLS_CONTEXT_UNPACK);
#endif

    // Write out a coverage value of "zero at pathID=0" so a future resolve
    // attempt doesn't affect this pixel.
    PLS_STOREUI_ATOMIC(coverageAtomicBuffer, FIXED_COVERAGE_ZERO_UINT);

    EMIT_ATOMIC_PLS
}
#endif // DRAW_IMAGE

#ifdef _EXPORTED_INITIALIZE_PLS

ATOMIC_PLS_MAIN(_EXPORTED_drawFragmentMain)
{
#ifdef _EXPORTED_STORE_COLOR_CLEAR
    PLS_STORE4F(colorBuffer, unpackUnorm4x8(uniforms.colorClearValue));
#endif
#ifdef _EXPORTED_SWIZZLE_COLOR_BGRA_TO_RGBA
    half4 color = PLS_LOAD4F(colorBuffer);
    PLS_STORE4F(colorBuffer, color.zyxw);
#endif
    PLS_STOREUI_ATOMIC(coverageAtomicBuffer, uniforms.coverageClearValue);
#ifdef _EXPORTED_ENABLE_CLIPPING
    if (_EXPORTED_ENABLE_CLIPPING)
    {
        PLS_STOREUI(clipBuffer, 0u);
    }
#endif
#ifdef _EXPORTED_FIXED_FUNCTION_COLOR_OUTPUT
    discard;
#endif
    EMIT_ATOMIC_PLS
}

#endif // INITIALIZE_PLS

#ifdef _EXPORTED_RESOLVE_PLS

#ifdef _EXPORTED_COALESCED_PLS_RESOLVE_AND_TRANSFER
PLS_FRAG_COLOR_MAIN(_EXPORTED_drawFragmentMain)
#else
ATOMIC_PLS_MAIN(_EXPORTED_drawFragmentMain)
#endif
{
    uint lastCoverageData = PLS_LOADUI_ATOMIC(coverageAtomicBuffer);
    half coverageCount = from_fixed(lastCoverageData & FIXED_COVERAGE_MASK);
    ushort lastPathID =
        cast_uint_to_ushort(lastCoverageData >> FIXED_COVERAGE_BIT_COUNT);
    half4 fragColorOut;
    resolve_paint(lastPathID,
                  coverageCount,
                  fragColorOut FRAGMENT_CONTEXT_UNPACK PLS_CONTEXT_UNPACK);
#ifdef _EXPORTED_COALESCED_PLS_RESOLVE_AND_TRANSFER
    _fragColor = PLS_LOAD4F(colorBuffer) * (1. - fragColorOut.w) + fragColorOut;
    EMIT_PLS_AND_FRAG_COLOR
#else
#ifdef _EXPORTED_FIXED_FUNCTION_COLOR_OUTPUT
    _fragColor = fragColorOut;
#else
    emit_pls_color(fragColorOut PLS_CONTEXT_UNPACK);
#endif
    EMIT_ATOMIC_PLS
#endif // COALESCED_PLS_RESOLVE_AND_TRANSFER
}
#endif // RESOLVE_PLS
#endif // FRAGMENT
)===";
} // namespace glsl
} // namespace gpu
} // namespace rive