#include "ShaderPipelineManager.h"

#include <fstream>
#include <iostream>
#include <filesystem>

#include <CoreMinimal.h>
#include "GlobalShader.h"
#include <mutex>

#include "CommonRenderResources.h"
#include "ResolveShader.h"
#include "Platform/pls_render_context_rhi_impl.hpp"
THIRD_PARTY_INCLUDES_START
#include "rive/shaders/out/generated/shaders/hlsl.glsl.hpp"
#include "rive/shaders/out/generated/shaders/constants.glsl.hpp"
THIRD_PARTY_INCLUDES_END

void FRiveGradientPixelShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("FRAGMENT"), TEXT("1"));
 }

void FRiveGradientVertexShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("VERTEX"), TEXT("1"));
}

void FRiveTessPixelShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("FRAGMENT"), TEXT("1"));
}

void FRiveTessVertexShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("VERTEX"), TEXT("1"));
 }

void FRivePathPixelShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("FRAGMENT"), TEXT("1"));
}

void FRivePathVertexShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("VERTEX"), TEXT("1"));
}

void FRiveInteriorTrianglesPixelShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("FRAGMENT"), TEXT("1"));
}

void FRiveInteriorTrianglesVertexShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("VERTEX"), TEXT("1"));
}

void FRiveImageRectPixelShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("FRAGMENT"), TEXT("1"));
}

void FRiveImageRectVertexShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("VERTEX"), TEXT("1"));
}

void FRiveImageMeshPixelShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("FRAGMENT"), TEXT("1"));
}

void FRiveImageMeshVertexShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("VERTEX"), TEXT("1"));
}

void FRiveAtomiResolvePixelShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("FRAGMENT"), TEXT("1"));
}


void FRiveAtomiResolveVertexShader::ModifyCompilationEnvironment(const FShaderPermutationParameters& Params, FShaderCompilerEnvironment& Environment)
{
    Environment.SetDefine(TEXT("VERTEX"), TEXT("1"));
}


IMPLEMENT_GLOBAL_SHADER(FRiveGradientPixelShader, "/Plugin/Rive/Private/Rive/color_ramp.usf", GLSL_colorRampFragmentMain, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FRiveGradientVertexShader, "/Plugin/Rive/Private/Rive/color_ramp.usf", GLSL_colorRampVertexMain, SF_Vertex);

IMPLEMENT_GLOBAL_SHADER(FRiveTessPixelShader, "/Plugin/Rive/Private/Rive/tessellate.usf", GLSL_tessellateFragmentMain, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FRiveTessVertexShader, "/Plugin/Rive/Private/Rive/tessellate.usf", GLSL_tessellateVertexMain, SF_Vertex);

IMPLEMENT_GLOBAL_SHADER(FRivePathPixelShader, "/Plugin/Rive/Private/Rive/atomic_draw_path.usf", GLSL_drawFragmentMain, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FRivePathVertexShader, "/Plugin/Rive/Private/Rive/atomic_draw_path.usf", GLSL_drawVertexMain, SF_Vertex);

IMPLEMENT_GLOBAL_SHADER(FRiveInteriorTrianglesPixelShader, "/Plugin/Rive/Private/Rive/atomic_draw_interior_triangles.usf", GLSL_drawFragmentMain, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FRiveInteriorTrianglesVertexShader, "/Plugin/Rive/Private/Rive/atomic_draw_interior_triangles.usf", GLSL_drawVertexMain, SF_Vertex);

IMPLEMENT_GLOBAL_SHADER(FRiveImageRectPixelShader, "/Plugin/Rive/Private/Rive/atomic_draw_image_rect.usf", GLSL_drawFragmentMain, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FRiveImageRectVertexShader, "/Plugin/Rive/Private/Rive/atomic_draw_image_rect.usf", GLSL_drawVertexMain, SF_Vertex);

IMPLEMENT_GLOBAL_SHADER(FRiveImageMeshPixelShader, "/Plugin/Rive/Private/Rive/atomic_draw_image_mesh.usf", GLSL_drawFragmentMain, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FRiveImageMeshVertexShader, "/Plugin/Rive/Private/Rive/atomic_draw_image_mesh.usf", GLSL_drawVertexMain, SF_Vertex);

IMPLEMENT_GLOBAL_SHADER(FRiveAtomiResolvePixelShader, "/Plugin/Rive/Private/Rive/atomic_resolve_pls.usf", GLSL_drawFragmentMain, SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FRiveAtomiResolveVertexShader, "/Plugin/Rive/Private/Rive/atomic_resolve_pls.usf", GLSL_drawVertexMain, SF_Vertex);

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FFlushUniforms, "uniforms");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FImageDrawUniforms, "imageDrawUniforms");