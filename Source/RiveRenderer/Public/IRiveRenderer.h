// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IRiveRenderTarget.h"

class URiveFile;
class UTextureRenderTarget2D;

#if WITH_RIVE
THIRD_PARTY_INCLUDES_START
#include "rive/pls/pls_render_context.hpp"
#include "rive/pls/pls_renderer.hpp"
THIRD_PARTY_INCLUDES_END

// just for testig
namespace rive
{
    class Artboard;

    namespace pls
    {
        class PLSRenderContext;
        class PLSRenderer;
    }
}

#endif // WITH_RIVE

namespace UE::Rive::Renderer
{
    class IRiveRenderer;

    /**
     * Type definition for shared pointer reference to the instance of IRiveRenderer.
     */
    using IRiveRendererPtr = TSharedPtr<IRiveRenderer>;

    /**
     * Type definition for shared pointer reference to the instance of IRiveRenderer.
     */
    using IRiveRendererRef = TSharedRef<IRiveRenderer>;

    class IRiveRenderer : public TSharedFromThis<IRiveRenderer>
    {
        /**
         * Structor(s)
         */

    public:

        virtual ~IRiveRenderer() = default;

        /**
         * Implementation(s)
         */

    public:

        virtual void Initialize() = 0;

        virtual bool IsInitialized() const = 0;
        
        virtual void QueueTextureRendering(TObjectPtr<URiveFile> InRiveFile) = 0;

        virtual IRiveRenderTargetPtr CreateTextureTarget_GameThread(const FName& InRiveName, UTextureRenderTarget2D* InRenderTarget) = 0;

        virtual void CreatePLSContext_RenderThread(FRHICommandListImmediate& RHICmdList) = 0;

        virtual void CreatePLSRenderer_RenderThread(FRHICommandListImmediate& RHICmdList) = 0;

        virtual UTextureRenderTarget2D* CreateDefaultRenderTarget(FIntPoint InTargetSize) = 0;
    
#if WITH_RIVE

        // just  for testing, we should not pass native artboard
        virtual void DebugColorDraw(UTextureRenderTarget2D* InTexture, const FLinearColor DebugColor, rive::Artboard* InNativeArtboard) = 0;

        virtual rive::pls::PLSRenderContext* GetPLSRenderContextPtr() = 0;
    
        virtual rive::pls::PLSRenderer* GetPLSRendererPtr() = 0;

#endif // WITH_RIVE
    };
}
