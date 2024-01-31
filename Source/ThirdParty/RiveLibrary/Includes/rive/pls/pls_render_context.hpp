/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/mat2d.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/pls/pls.hpp"
#include "rive/pls/pls_factory.hpp"
#include "rive/pls/pls_render_target.hpp"
#include "rive/pls/trivial_block_allocator.hpp"
#include "rive/shapes/paint/color.hpp"
#include <array>
#include <unordered_map>

class PushRetrofittedTrianglesGMDraw;
class PLSRenderContextTest;

namespace rive
{
class RawPath;
} // namespace rive

namespace rive::pls
{
class GradientLibrary;
class IntersectionBoard;
class PLSDraw;
class PLSGradient;
class PLSPaint;
class PLSPath;
class PLSRenderContextImpl;

// Used as a key for complex gradients.
class GradientContentKey
{
public:
    inline GradientContentKey(rcp<const PLSGradient> gradient);
    inline GradientContentKey(GradientContentKey&& other);
    bool operator==(const GradientContentKey&) const;
    const PLSGradient* gradient() const { return m_gradient.get(); }

private:
    rcp<const PLSGradient> m_gradient;
};

// Hashes all stops and all colors in a complex gradient.
class DeepHashGradient
{
public:
    size_t operator()(const GradientContentKey&) const;
};

// Even though PLSDraw is block-allocated, we still need to call releaseRefs() on each individual
// instance before releasing the block. This smart pointer guarantees we always call releaseRefs()
// (implementation in pls_draw.hpp).
struct PLSDrawReleaseRefs
{
    void operator()(PLSDraw* draw);
};
using PLSDrawUniquePtr = std::unique_ptr<PLSDraw, PLSDrawReleaseRefs>;

// Top-level, API agnostic rendering context for PLSRenderer. This class manages all the GPU
// buffers, context state, and other resources required for Rive's pixel local storage path
// rendering algorithm.
//
// Main algorithm:
// https://docs.google.com/document/d/19Uk9eyFxav6dNSYsI2ZyiX9zHU1YOaJsMB2sdDFVz6s/edit
//
// Batching multiple unique paths:
// https://docs.google.com/document/d/1DLrQimS5pbNaJJ2sAW5oSOsH6_glwDPo73-mtG5_zns/edit
//
// Batching strokes as well:
// https://docs.google.com/document/d/1CRKihkFjbd1bwT08ErMCP4fwSR7D4gnHvgdw_esY9GM/edit
//
// Intended usage pattern of this class:
//
//   context->beginFrame(...);
//   for (path : paths)
//   {
//       context->pushPath(...);
//       for (contour : path.contours)
//       {
//           context->pushContour(...);
//           for (cubic : contour.cubics)
//           {
//               context->pushCubic(...);
//           }
//       }
//   }
//   context->flush();
class PLSRenderContext : public PLSFactory
{
public:
    PLSRenderContext(std::unique_ptr<PLSRenderContextImpl>);
    ~PLSRenderContext();

    PLSRenderContextImpl* impl() { return m_impl.get(); }
    template <typename T> T* static_impl_cast() { return static_cast<T*>(m_impl.get()); }

    // Returns the number of flushes that have been executed over the entire lifetime of this class,
    // logical or otherwise.
    uint64_t getFlushCount() const { return m_flushCount; }

    // Options for controlling how and where a frame is rendered.
    struct FrameDescriptor
    {
        rcp<const PLSRenderTarget> renderTarget;
        LoadAction loadAction = LoadAction::clear;
        ColorInt clearColor = 0;

        // Enables an experimental pixel local storage mode that makes use of atomics and barriers
        // instead of raster ordering in order to synchronize overlapping fragments.
        bool enableExperimentalAtomicMode = false;

        // Testing flags.
        bool wireframe = false;
        bool fillsDisabled = false;
        bool strokesDisabled = false;

        // Only used for metal backend.
        void* backendSpecificData = nullptr;
    };

    // Called at the beginning of a frame and establishes where and how it will be rendered.
    //
    // All rendering related calls must be made between beginFrame() and flush().
    void beginFrame(FrameDescriptor&&);

    const FrameDescriptor& frameDescriptor() const
    {
        assert(m_didBeginFrame);
        return m_frameDescriptor;
    }

    // Generates a unique clip ID that is guaranteed to not exist in the current clip buffer.
    //
    // Returns 0 if a unique ID could not be generated, at which point the caller must issue a
    // logical flush and try again.
    uint32_t generateClipID();

    // Get/set a "clip content ID" that uniquely identifies the current contents of the clip buffer.
    // This ID is reset to 0 on every logical flush.
    void setClipContentID(uint32_t clipID)
    {
        assert(m_didBeginFrame);
        m_clipContentID = clipID;
    }
    uint32_t getClipContentID()
    {
        assert(m_didBeginFrame);
        return m_clipContentID;
    }

    // Appends a list of high-level PLSDraws to the current frame.
    // Returns false if the draws don't fit within the current resource constraints, at which point
    // the caller must issue a logical flush and try again.
    [[nodiscard]] bool pushDrawBatch(PLSDrawUniquePtr draws[], size_t drawCount);

    // Renders all paths that have been pushed since the last call to flush(). Rendering is done in
    // two steps:
    //
    //  1. Tessellate all cubics into positions and normals in the "tessellation texture".
    //  2. Render the tessellated curves using Rive's pixel local storage path rendering algorithm.
    //
    void flush(pls::FlushType = pls::FlushType::endOfFrame);

    // Reallocates all GPU resources to the basline minimum allocation size.
    void resetGPUResources();

    // Shrinks GPU resource allocations to the maximum per-flush limits seen since the most recent
    // previous call to shrinkGPUResourcesToFit(). This method is intended to be called at a fixed
    // temporal interval. (GPU resource allocations automatically grow based on usage, but can only
    // shrink if the application calls this method.)
    void shrinkGPUResourcesToFit();

    // Returns the context's TrivialBlockAllocator, which is automatically reset at the end of every
    // frame. (Memory in this allocator is preserved between logical flushes.)
    TrivialBlockAllocator& perFrameAllocator()
    {
        assert(m_didBeginFrame);
        return m_perFrameAllocator;
    }

    // Allocators for intermediate path processing buffers.
    TrivialArrayAllocator<uint8_t>& numChopsAllocator() { return m_numChopsAllocator; }
    TrivialArrayAllocator<Vec2D>& chopVerticesAllocator() { return m_chopVerticesAllocator; }
    TrivialArrayAllocator<std::array<Vec2D, 2>>& tangentPairsAllocator()
    {
        return m_tangentPairsAllocator;
    }
    TrivialArrayAllocator<uint32_t, alignof(float4)>& polarSegmentCountsAllocator()
    {
        return m_polarSegmentCountsAllocator;
    }
    TrivialArrayAllocator<uint32_t, alignof(float4)>& parametricSegmentCountsAllocator()
    {
        return m_parametricSegmentCountsAllocator;
    }

    // Allocates a trivially destructible object that will be automatically dropped at the end of
    // the current frame.
    template <typename T, typename... Args> T* make(Args&&... args)
    {
        assert(m_didBeginFrame);
        return m_perFrameAllocator.make<T>(std::forward<Args>(args)...);
    }

    // Backend-specific PLSFactory implementation.
    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType, RenderBufferFlags, size_t) override;
    rcp<RenderImage> decodeImage(Span<const uint8_t>) override;

private:
    friend class PLSDraw;
    friend class PLSPathDraw;
    friend class MidpointFanPathDraw;
    friend class InteriorTriangulationDraw;
    friend class ImageRectDraw;
    friend class ImageMeshDraw;
    friend class ::PushRetrofittedTrianglesGMDraw; // For testing.
    friend class ::PLSRenderContextTest;           // For testing.

    // Resets the CPU-side STL containers so they don't have unbounded growth.
    void resetContainers();

    // Defines the exact size of each of our GPU resources. Computed during flush(), based on
    // LogicalFlush::ResourceCounters and LogicalFlush::LayoutCounters.
    struct ResourceAllocationCounts
    {
        using VecType = simd::gvec<size_t, 12>;

        RIVE_ALWAYS_INLINE VecType toVec() const
        {
            static_assert(sizeof(VecType) >= sizeof(*this));
            VecType vec;
            RIVE_INLINE_MEMCPY(&vec, this, sizeof(*this));
            return vec;
        }

        RIVE_ALWAYS_INLINE ResourceAllocationCounts(const VecType& vec)
        {
            static_assert(sizeof(VecType) >= sizeof(*this));
            RIVE_INLINE_MEMCPY(this, &vec, sizeof(*this));
        }

        ResourceAllocationCounts() = default;

        size_t flushUniformBufferCount = 0;
        size_t imageDrawUniformBufferCount = 0;
        size_t pathBufferCount = 0;
        size_t paintBufferCount = 0;
        size_t paintAuxBufferCount = 0;
        size_t contourBufferCount = 0;
        size_t simpleGradientBufferCount = 0;
        size_t complexGradSpanBufferCount = 0;
        size_t tessSpanBufferCount = 0;
        size_t triangleVertexBufferCount = 0;
        size_t gradTextureHeight = 0;
        size_t tessTextureHeight = 0;
    };

    // Reallocates GPU resources and updates m_currentResourceAllocations.
    // If forceRealloc is true, every GPU resource is allocated, even if the size would not change.
    void setResourceSizes(ResourceAllocationCounts, bool forceRealloc = false);

    void mapResourceBuffers(const ResourceAllocationCounts&);
    void unmapResourceBuffers();

    const std::unique_ptr<PLSRenderContextImpl> m_impl;
    const size_t m_maxPathID;

    ResourceAllocationCounts m_currentResourceAllocations;
    ResourceAllocationCounts m_maxRecentResourceRequirements;

    // Per-frame state.
    FrameDescriptor m_frameDescriptor;
    RIVE_DEBUG_CODE(bool m_didBeginFrame = false;)

    // The number of flushes that have been executed over the entire lifetime of this class, logical
    // or otherwise.
    size_t m_flushCount = 0;

    // Per-flush clipping state.
    uint32_t m_lastGeneratedClipID = 0;
    uint32_t m_clipContentID = 0;

    // Used by LogicalFlushes for re-ordering high level draws.
    std::vector<uint64_t> m_indirectDrawList;
    std::unique_ptr<IntersectionBoard> m_intersectionBoard;

    WriteOnlyMappedMemory<pls::FlushUniforms> m_flushUniformData;
    WriteOnlyMappedMemory<pls::PathData> m_pathData;
    WriteOnlyMappedMemory<pls::PaintData> m_paintData;
    WriteOnlyMappedMemory<pls::PaintAuxData> m_paintAuxData;
    WriteOnlyMappedMemory<pls::ContourData> m_contourData;
    // Simple gradients get written by the CPU.
    WriteOnlyMappedMemory<pls::TwoTexelRamp> m_simpleColorRampsData;
    // Complex gradients get rendered by the GPU.
    WriteOnlyMappedMemory<pls::GradientSpan> m_gradSpanData;
    WriteOnlyMappedMemory<pls::TessVertexSpan> m_tessSpanData;
    WriteOnlyMappedMemory<pls::TriangleVertex> m_triangleVertexData;
    WriteOnlyMappedMemory<pls::ImageDrawUniforms> m_imageDrawUniformData;

    // Simple allocator for trivially-destructible data that needs to persist until the current
    // frame has completed. All memory in this allocator is dropped at the end of the every frame.
    constexpr static size_t kPerFlushAllocatorInitialBlockSize = 1024 * 1024; // 1 MiB.
    TrivialBlockAllocator m_perFrameAllocator{kPerFlushAllocatorInitialBlockSize};

    // Allocators for intermediate path processing buffers.
    constexpr static size_t kIntermediateDataInitialStrokes = 8192;     // * 84 == 688 KiB.
    constexpr static size_t kIntermediateDataInitialFillCurves = 32768; // * 4 == 128 KiB.
    TrivialArrayAllocator<uint8_t> m_numChopsAllocator{kIntermediateDataInitialStrokes *
                                                       4}; // 4 byte per stroke curve.
    TrivialArrayAllocator<Vec2D> m_chopVerticesAllocator{kIntermediateDataInitialStrokes *
                                                         4}; // 32 bytes per stroke curve.
    TrivialArrayAllocator<std::array<Vec2D, 2>> m_tangentPairsAllocator{
        kIntermediateDataInitialStrokes * 2}; // 32 bytes per stroke curve.
    TrivialArrayAllocator<uint32_t, alignof(float4)> m_polarSegmentCountsAllocator{
        kIntermediateDataInitialStrokes * 4}; // 16 bytes per stroke curve.
    TrivialArrayAllocator<uint32_t, alignof(float4)> m_parametricSegmentCountsAllocator{
        kIntermediateDataInitialFillCurves}; // 4 bytes per fill curve.

    // Manages a list of high-level PLSDraws and their required resources.
    //
    // Since textures have hard size limits, we can't always fit an entire frame into one flush.
    // It's rare for us to require more than one flush in a single frame, but for the times that we
    // do, this flush logic is encapsulated in a nested class that can be built up into a list and
    // executed the end of a frame.
    class LogicalFlush
    {
    public:
        LogicalFlush(PLSRenderContext* parent);

        // Resets the CPU-side STL containers so they don't have unbounded growth.
        void resetContainers();

        // Rewinds this flush object back to an empty state without shrinking any internal
        // allocations held by CPU-side STL containers.
        void rewind();

        // Access this flush's pls::FlushDescriptor (which is not valid until layoutResources()).
        // NOTE: Some fields in the FlushDescriptor (tessVertexSpanCount, hasTriangleVertices,
        // drawList, and combinedShaderFeatures) do not become valid until after writeResources().
        const pls::FlushDescriptor& desc()
        {
            assert(m_hasDoneLayout);
            return m_flushDesc;
        }

        // Appends a list of high-level PLSDraws to the flush.
        // Returns false if the draws don't fit within the current resource constraints, at which
        // point the context must append a new logical flush and try again.
        [[nodiscard]] bool pushDrawBatch(PLSDrawUniquePtr draws[], size_t drawCount);

        // Running counts of data records required by PLSDraws that need to be allocated in the
        // render context's various GPU buffers.
        struct ResourceCounters
        {
            using VecType = simd::gvec<size_t, 8>;

            VecType toVec() const
            {
                static_assert(sizeof(VecType) == sizeof(*this));
                VecType vec;
                RIVE_INLINE_MEMCPY(&vec, this, sizeof(VecType));
                return vec;
            }

            ResourceCounters(const VecType& vec)
            {
                static_assert(sizeof(*this) == sizeof(VecType));
                RIVE_INLINE_MEMCPY(this, &vec, sizeof(*this));
            }

            ResourceCounters() = default;

            size_t midpointFanTessVertexCount = 0;
            size_t outerCubicTessVertexCount = 0;
            size_t pathCount = 0;
            size_t contourCount = 0;
            size_t maxTessellatedSegmentCount = 0; // lines, curves, lone joins, emulated caps, etc.
            size_t maxTriangleVertexCount = 0;
            size_t imageDrawCount = 0; // imageRect or imageMesh.
            size_t complexGradientSpanCount = 0;
        };

        // Additional counters for layout state that don't need to be tracked by individual draws.
        struct LayoutCounters
        {
            uint32_t pathPaddingCount = 0;
            uint32_t paintPaddingCount = 0;
            uint32_t paintAuxPaddingCount = 0;
            uint32_t contourPaddingCount = 0;
            uint32_t simpleGradCount = 0;
            uint32_t maxGradTextureHeight = 0;
            uint32_t maxTessTextureHeight = 0;
        };

        // Allocates a horizontal span of texels in the gradient texture and schedules either a
        // texture upload or a draw that fills it with the given gradient's color ramp.
        //
        // Fills out a ColorRampLocation record that tells the shader how to access the gradient.
        //
        // Returns false if the gradient texture is out of space, at which point the caller must
        // issue a logical flush and try again.
        [[nodiscard]] bool allocateGradient(const PLSGradient*,
                                            ResourceCounters*,
                                            pls::ColorRampLocation*);

        // Carves out space for this specific flush within the total frame's resource buffers and
        // lays out the flush-specific resource textures. Updates the total frame running conters
        // based on layout.
        void layoutResources(const FrameDescriptor&,
                             size_t flushIdx,
                             pls::FlushType,
                             ResourceCounters* runningFrameResourceCounts,
                             LayoutCounters* runningFrameLayoutCounts);

        // Called after all flushes in a frame have done their layout and the render context has
        // allocated and mapped its resource buffers. Writes the GPU data for this flush to the
        // context's actively mapped resource buffers.
        void writeResources();

        // Pushes a record to the GPU for the given path, which will be referenced by future calls
        // to pushContour() and pushCubic().
        void pushPath(PatchType,
                      const Mat2D&,
                      float strokeRadius,
                      FillRule,
                      PaintType,
                      pls::SimplePaintValue simplePaintValue,
                      const PLSGradient*,
                      const PLSTexture* imageTexture,
                      uint32_t clipID,
                      const pls::ClipRectInverseMatrix*, // Null if there is no clipRect.
                      BlendMode,
                      uint32_t tessVertexCount);

        // Pushes a contour record to the GPU for the given contour, which references the
        // most-recently pushed path and will be referenced by future calls to pushCubic().
        //
        // The first curve of the contour will be pre-padded with 'paddingVertexCount' tessellation
        // vertices, colocated at T=0. The caller must use this argument to align the end of the
        // contour on a boundary of the patch size. (See pls::PaddingToAlignUp().)
        void pushContour(Vec2D midpoint, bool closed, uint32_t paddingVertexCount);

        // Appends a cubic curve and join to the most-recently pushed contour, and reserves the
        // appropriate number of tessellated vertices in the tessellation texture.
        //
        // An instance consists of a cubic curve with "parametricSegmentCount + polarSegmentCount"
        // segments, followed by a join with "joinSegmentCount" segments, for a grand total of
        // "parametricSegmentCount + polarSegmentCount + joinSegmentCount - 1" vertices.
        //
        // If a cubic has already been pushed to the current contour, pts[0] must be equal to the
        // former cubic's pts[3].
        //
        // "joinTangent" is the ending tangent of the join that follows the cubic.
        void pushCubic(const Vec2D pts[4],
                       Vec2D joinTangent,
                       uint32_t additionalContourFlags,
                       uint32_t parametricSegmentCount,
                       uint32_t polarSegmentCount,
                       uint32_t joinSegmentCount);

        // Pushes triangles to be drawn using the data records from the most recent calls to
        // pushPath() and pushPaint().
        void pushInteriorTriangulation(GrInnerFanTriangulator*,
                                       PaintType,
                                       pls::SimplePaintValue,
                                       const PLSTexture* imageTexture,
                                       uint32_t clipID,
                                       bool hasClipRect,
                                       BlendMode);

        // Pushes an imageRect to the draw list.
        // This should only be used when we don't have bindless textures in atomic mode. Otherwise,
        // images should be drawn as rectangular paths with an image paint.
        void pushImageRect(const Mat2D&,
                           float opacity,
                           const PLSTexture* imageTexture,
                           uint32_t clipID,
                           const pls::ClipRectInverseMatrix*, // Null if there is no clipRect.
                           BlendMode);

        // Pushes an imageMesh to the draw list.
        void pushImageMesh(const Mat2D&,
                           float opacity,
                           const PLSTexture* imageTexture,
                           const RenderBuffer* vertexBuffer,
                           const RenderBuffer* uvBuffer,
                           const RenderBuffer* indexBuffer,
                           uint32_t indexCount,
                           uint32_t clipID,
                           const pls::ClipRectInverseMatrix*, // Null if there is no clipRect.
                           BlendMode);

        // Adds a barrier to the end of the draw list that prevents further combining/batching and
        // instructs the backend to issue a graphics barrier, if necessary.
        void pushBarrier();

    private:
        // Writes padding vertices to the tessellation texture, with an invalid contour ID that is
        // guaranteed to not be the same ID as any neighbors.
        void pushPaddingVertices(uint32_t tessLocation, uint32_t count);

        // Allocates a (potentially wrapped) span in the tessellation texture and pushes an instance
        // to render it. If the span does wraps, pushes multiple instances to render each horizontal
        // segment.
        RIVE_ALWAYS_INLINE void pushTessellationSpans(const Vec2D pts[4],
                                                      Vec2D joinTangent,
                                                      uint32_t totalVertexCount,
                                                      uint32_t parametricSegmentCount,
                                                      uint32_t polarSegmentCount,
                                                      uint32_t joinSegmentCount,
                                                      uint32_t contourIDWithFlags);

        // Same as pushTessellationSpans(), but also pushes a reflection of the span, rendered right
        // to left, that emits a mirrored version of the patch with negative coverage. (See
        // TessVertexSpan.)
        RIVE_ALWAYS_INLINE void pushMirroredTessellationSpans(const Vec2D pts[4],
                                                              Vec2D joinTangent,
                                                              uint32_t totalVertexCount,
                                                              uint32_t parametricSegmentCount,
                                                              uint32_t polarSegmentCount,
                                                              uint32_t joinSegmentCount,
                                                              uint32_t contourIDWithFlags);

        // Either appends a new drawBatch to m_drawList or merges into m_drawList.tail().
        // Updates the batch's ShaderFeatures according to the passed parameters.
        DrawBatch& pushPathDraw(DrawType,
                                size_t baseVertex,
                                FillRule,
                                PaintType,
                                pls::SimplePaintValue,
                                const PLSTexture* imageTexture,
                                uint32_t clipID,
                                bool hasClipRect,
                                BlendMode);
        DrawBatch& pushDraw(DrawType,
                            size_t baseVertex,
                            PaintType,
                            const PLSTexture* imageTexture,
                            uint32_t clipID,
                            bool hasClipRect,
                            BlendMode);

        // Instance pointer to the outer parent class.
        PLSRenderContext* const m_ctx;

        // Running counts of GPU data records that need to be allocated for draws.
        ResourceCounters m_resourceCounts;

        // High-level draw list. These get built into a low-level list of pls::DrawBatch objects
        // during writeResources().
        std::vector<PLSDrawUniquePtr> m_plsDraws;

        // Simple gradients have one stop at t=0 and one stop at t=1. They're implemented with 2
        // texels.
        std::unordered_map<uint64_t, uint32_t> m_simpleGradients; // [color0, color1] -> texelsIdx.
        std::vector<pls::TwoTexelRamp> m_pendingSimpleGradientWrites;

        // Complex gradients have stop(s) between t=0 and t=1. In theory they should be scaled to a
        // ramp where every stop lands exactly on a pixel center, but for now we just always scale
        // them to the entire gradient texture width.
        std::unordered_map<GradientContentKey, uint16_t, DeepHashGradient>
            m_complexGradients; // [colors[0..n], stops[0..n]] -> rowIdx
        std::vector<const PLSGradient*> m_pendingComplexColorRampDraws;

        // Layout state.
        uint32_t m_pathPaddingCount = 0;
        uint32_t m_paintPaddingCount = 0;
        uint32_t m_paintAuxPaddingCount = 0;
        uint32_t m_contourPaddingCount = 0;
        uint32_t m_midpointFanTessEndLocation = 0;
        uint32_t m_outerCubicTessEndLocation = 0;
        uint32_t m_outerCubicTessVertexIdx = 0;
        uint32_t m_midpointFanTessVertexIdx = 0;

        pls::FlushDescriptor m_flushDesc;
        pls::GradTextureLayout m_gradTextureLayout; // Not determined until writeResources().

        BlockAllocatedLinkedList<DrawBatch> m_drawList;
        pls::ShaderFeatures m_combinedShaderFeatures = pls::ShaderFeatures::NONE;

        // Most recent path and contour state.
        bool m_currentPathIsStroked = false;
        bool m_currentPathNeedsMirroredContours = false;
        uint32_t m_currentPathID = 0;
        uint32_t m_currentContourID = 0;
        uint32_t m_currentContourPaddingVertexCount = 0; // Padding to add to the first curve.
        uint32_t m_pathTessLocation = 0;
        uint32_t m_pathMirroredTessLocation = 0; // Used for back-face culling and mirrored patches.
        RIVE_DEBUG_CODE(uint32_t m_expectedPathTessLocationAtEndOfPath = 0;)
        RIVE_DEBUG_CODE(uint32_t m_expectedPathMirroredTessLocationAtEndOfPath = 0;)
        RIVE_DEBUG_CODE(uint32_t m_pathCurveCount = 0;)

        RIVE_DEBUG_CODE(bool m_hasDoneLayout = false;)
    };

    std::vector<std::unique_ptr<LogicalFlush>> m_logicalFlushes;
};
} // namespace rive::pls
