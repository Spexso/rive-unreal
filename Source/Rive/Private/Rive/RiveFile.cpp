// Copyright Rive, Inc. All rights reserved.

#include "Rive/RiveFile.h"
#include "IRiveRenderTarget.h"
#include "IRiveRenderer.h"
#include "IRiveRendererModule.h"
#include "RiveCore/Public/RiveArtboard.h"
#include "Logs/RiveLog.h"
#include "RiveCore/Public/Assets/RiveAsset.h"
#include "RiveCore/Public/Assets/URAssetImporter.h"
#include "RiveCore/Public/Assets/URFileAssetLoader.h"

#if WITH_RIVE
#include "PreRiveHeaders.h"
THIRD_PARTY_INCLUDES_START
#include "rive/pls/pls_render_context.hpp"
THIRD_PARTY_INCLUDES_END
#endif // WITH_RIVE

URiveFile::URiveFile()
{
	ArtboardIndex = 0;
}

void URiveFile::BeginDestroy()
{
	InitState = ERiveInitState::Deinitializing;
	
	RiveRenderTarget.Reset();
	
	if (IsValid(Artboard))
	{
		Artboard->MarkAsGarbage();
	}
	
	RiveNativeFileSpan = {};
	RiveNativeFilePtr.reset();
	
	Super::BeginDestroy();
}

TStatId URiveFile::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URiveFile, STATGROUP_Tickables);
}

void URiveFile::Tick(float InDeltaSeconds)
{
	if (!IsValidChecked(this))
	{
		return;
	}

#if WITH_RIVE
	if (IsInitialized() && bIsRendering)
	{
		if (GetArtboard())
		{
			Artboard->Tick(InDeltaSeconds);
			RiveRenderTarget->SubmitAndClear();
		}
	}
#endif // WITH_RIVE
}

bool URiveFile::IsTickable() const
{
	return !HasAnyFlags(RF_ClassDefaultObject) && bIsRendering;
}

void URiveFile::PostLoad()
{
	Super::PostLoad();
	
	if (!IsRunningCommandlet())
	{
		UE::Rive::Renderer::IRiveRendererModule::Get().CallOrRegister_OnRendererInitialized(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &URiveFile::Initialize));
	}
}

#if WITH_EDITOR

void URiveFile::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// TODO. WE need custom implementation here to handle the Rive File Editor Changes
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName ActiveMemberNodeName = *PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(URiveFile, ArtboardIndex) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(URiveFile, ArtboardName))
	{
		InstantiateArtboard();
	}
	else if (ActiveMemberNodeName == GET_MEMBER_NAME_CHECKED(URiveFile, Size))
	{
		ResizeRenderTargets(Size);
	}
	else if (ActiveMemberNodeName == GET_MEMBER_NAME_CHECKED(URiveFile, ClearColor))
	{
		if (RiveRenderTarget)
		{
			RiveRenderTarget->SetClearColor(ClearColor);
		}
	}

	// Update the Rive CachedPLSRenderTarget
	if (RiveRenderTarget)
	{
		RiveRenderTarget->Initialize();
	}

	FlushRenderingCommands();
}

#endif // WITH_EDITOR

URiveFile* URiveFile::CreateInstance(const FString& InArtboardName, const FString& InStateMachineName)
{
	auto NewRiveFileInstance = NewObject<
		URiveFile>(this, URiveFile::StaticClass(), NAME_None, RF_Public | RF_Transient);
	NewRiveFileInstance->ParentRiveFile = this;
	NewRiveFileInstance->ArtboardName = InArtboardName.IsEmpty() ? ArtboardName : InArtboardName;
	NewRiveFileInstance->StateMachineName = InStateMachineName.IsEmpty() ? StateMachineName : InStateMachineName;
	NewRiveFileInstance->ArtboardIndex = ArtboardIndex;
	NewRiveFileInstance->Initialize();
	return NewRiveFileInstance;
}

void URiveFile::FireTrigger(const FString& InPropertyName) const
{
#if WITH_RIVE
	if (GetArtboard())
	{
		if (UE::Rive::Core::FURStateMachine* StateMachine = Artboard->GetStateMachine())
		{
			StateMachine->FireTrigger(InPropertyName);
		}
	}
#endif // WITH_RIVE
}

bool URiveFile::GetBoolValue(const FString& InPropertyName) const
{
#if WITH_RIVE
	if (GetArtboard())
	{
		if (UE::Rive::Core::FURStateMachine* StateMachine = Artboard->GetStateMachine())
		{
			return StateMachine->GetBoolValue(InPropertyName);
		}
	}
#endif // !WITH_RIVE
	
	return false;
}

float URiveFile::GetNumberValue(const FString& InPropertyName) const
{
#if WITH_RIVE
	if (GetArtboard())
	{
		if (UE::Rive::Core::FURStateMachine* StateMachine = Artboard->GetStateMachine())
		{
			return StateMachine->GetNumberValue(InPropertyName);
		}
	}
#endif // !WITH_RIVE

	return 0.f;
}

FLinearColor URiveFile::GetClearColor() const
{
	return ClearColor;
}

FVector2f URiveFile::GetLocalCoordinates(const FVector2f& InTexturePosition) const
{
#if WITH_RIVE

	if (GetArtboard())
	{
		const FVector2f RiveAlignmentXY = FRiveAlignment::GetAlignment(RiveAlignment);

		const rive::Mat2D Transform = rive::computeAlignment(
			(rive::Fit)RiveFitType,
			rive::Alignment(RiveAlignmentXY.X, RiveAlignmentXY.Y),
			rive::AABB(0, 0, Size.X, Size.Y),
			Artboard->GetBounds()
		);

		const rive::Vec2D ResultingVector = Transform.invertOrIdentity() * rive::Vec2D(InTexturePosition.X, InTexturePosition.Y);
		return {ResultingVector.x, ResultingVector.y};
	}

#endif // WITH_RIVE

	return FVector2f::ZeroVector;
}

FVector2f URiveFile::GetLocalCoordinatesFromExtents(const FVector2f& InPosition, const FBox2f& InExtents) const
{
	const FVector2f RelativePosition = InPosition - InExtents.Min;
	const FVector2f Ratio { Size.X / InExtents.GetSize().X, SizeY / InExtents.GetSize().Y}; // Ratio should be the same for X and Y
	const FVector2f TextureRelativePosition = RelativePosition * Ratio;
	
	return GetLocalCoordinates(TextureRelativePosition);
}

void URiveFile::SetBoolValue(const FString& InPropertyName, bool bNewValue)
{
#if WITH_RIVE
	if (GetArtboard())
	{
		if (UE::Rive::Core::FURStateMachine* StateMachine = Artboard->GetStateMachine())
		{
			StateMachine->SetBoolValue(InPropertyName, bNewValue);
		}
	}
#endif // WITH_RIVE
}

void URiveFile::SetNumberValue(const FString& InPropertyName, float NewValue)
{
#if WITH_RIVE
	if (GetArtboard())
	{
		if (UE::Rive::Core::FURStateMachine* StateMachine = Artboard->GetStateMachine())
		{
			StateMachine->SetNumberValue(InPropertyName, NewValue);
		}
	}
#endif // WITH_RIVE
}

ESimpleElementBlendMode URiveFile::GetSimpleElementBlendMode() const
{
	ESimpleElementBlendMode NewBlendMode = ESimpleElementBlendMode::SE_BLEND_Opaque;

	switch (RiveBlendMode)
	{
	case ERiveBlendMode::SE_BLEND_Opaque:
		break;
	case ERiveBlendMode::SE_BLEND_Masked:
		NewBlendMode = SE_BLEND_Masked;
		break;
	case ERiveBlendMode::SE_BLEND_Translucent:
		NewBlendMode = SE_BLEND_Translucent;
		break;
	case ERiveBlendMode::SE_BLEND_Additive:
		NewBlendMode = SE_BLEND_Additive;
		break;
	case ERiveBlendMode::SE_BLEND_Modulate:
		NewBlendMode = SE_BLEND_Modulate;
		break;
	case ERiveBlendMode::SE_BLEND_MaskedDistanceField:
		NewBlendMode = SE_BLEND_MaskedDistanceField;
		break;
	case ERiveBlendMode::SE_BLEND_MaskedDistanceFieldShadowed:
		NewBlendMode = SE_BLEND_MaskedDistanceFieldShadowed;
		break;
	case ERiveBlendMode::SE_BLEND_TranslucentDistanceField:
		NewBlendMode = SE_BLEND_TranslucentDistanceField;
		break;
	case ERiveBlendMode::SE_BLEND_TranslucentDistanceFieldShadowed:
		NewBlendMode = SE_BLEND_TranslucentDistanceFieldShadowed;
		break;
	case ERiveBlendMode::SE_BLEND_AlphaComposite:
		NewBlendMode = SE_BLEND_AlphaComposite;
		break;
	case ERiveBlendMode::SE_BLEND_AlphaHoldout:
		NewBlendMode = SE_BLEND_AlphaHoldout;
		break;
	}

	return NewBlendMode;
}

#if WITH_EDITOR

bool URiveFile::EditorImport(const FString& InRiveFilePath, TArray<uint8>& InRiveFileBuffer)
{
	RiveFilePath = InRiveFilePath;
	RiveFileData = MoveTemp(InRiveFileBuffer);
	SetFlags(RF_NeedPostLoad);
	ConditionalPostLoad();

	// In Theory, the import should be synchronous as the IRiveRendererModule::Get().GetRenderer() should have been initialized,
	// and the parent Rive File (if any) should already be loaded
	ensureMsgf(WasLastInitializationSuccessful.IsSet(),
		TEXT("The initialization of the Rive File '%s' ended up being asynchronous. EditorImport returning true as we don't know if the import was successful."));
	
	return WasLastInitializationSuccessful.Get(true);
}

#endif // WITH_EDITOR

void URiveFile::Initialize()
{
	if (InitState != ERiveInitState::Uninitialized)
	{
		return;
	}
	WasLastInitializationSuccessful.Reset();
	
	if (!UE::Rive::Renderer::IRiveRendererModule::IsAvailable())
	{
		UE_LOG(LogRive, Error, TEXT("Could not load rive file as the required Rive Renderer Module is either missing or not loaded properly."));
		BroadcastInitializationResult(false);
		return;
	}

	UE::Rive::Renderer::IRiveRenderer* RiveRenderer = UE::Rive::Renderer::IRiveRendererModule::Get().GetRenderer();
	if (!ensure(RiveRenderer))
	{
		UE_LOG(LogRive, Error, TEXT("Failed to import rive file as we do not have a valid renderer."));
		BroadcastInitializationResult(false);
		return;
	}

#if WITH_RIVE
	
	if (ParentRiveFile)
	{
		if (!ensure(IsValid(ParentRiveFile)))
		{
			UE_LOG(LogRive, Error, TEXT("Unable to Initialize this URiveFile Instance '%s' because its Parent is not valid"), *GetNameSafe(this));
			BroadcastInitializationResult(false);
			return;
		}

		if (!ParentRiveFile->IsInitialized())
		{
			ParentRiveFile->Initialize();
			if (ParentRiveFile->InitState < ERiveInitState::Initializing) 
			{
				UE_LOG(LogRive, Error, TEXT("Unable to Initialize this URiveFile Instance '%s' because its Parent '%s' cannot be initialized"), *GetNameSafe(this), *GetNameSafe(ParentRiveFile));
				return;
			}
			
			InitState = ERiveInitState::Initializing;
			ParentRiveFile->CallOrRegister_OnInitialized(FOnRiveFileInitialized::FDelegate::CreateLambda([this](URiveFile* ParentRiveFileInitialized, bool bSuccess)
			{
				InitState = ERiveInitState::Uninitialized; // to be able to enter the Initialize function
				if (bSuccess)
				{
					Initialize();
				}
				else
				{
					UE_LOG(LogRive, Error, TEXT("Unable to Initialize this URiveFile Instance '%s' because its Parent '%s' cannot be initialized successfully"), *GetNameSafe(this), *GetNameSafe(ParentRiveFileInitialized));
				}
			}));
			return;
		}
	}
	else if (RiveNativeFileSpan.empty())
	{
		if (RiveFileData.IsEmpty())
		{
			UE_LOG(LogRive, Error, TEXT("Could not load an empty Rive File Data."));
			return;
		}
		RiveNativeFileSpan = rive::make_span(RiveFileData.GetData(), RiveFileData.Num());
	}

	InitState = ERiveInitState::Initializing;
	
	RiveRenderer->CallOrRegister_OnInitialized(UE::Rive::Renderer::IRiveRenderer::FOnRendererInitialized::FDelegate::CreateLambda(
	[this](UE::Rive::Renderer::IRiveRenderer* RiveRenderer)
	{
		rive::pls::PLSRenderContext* PLSRenderContext;
		{
			FScopeLock Lock(&RiveRenderer->GetThreadDataCS());
			PLSRenderContext = RiveRenderer->GetPLSRenderContextPtr();
		}
		
		if (ensure(PLSRenderContext))
		{
			ArtboardNames.Empty();
			if (!ParentRiveFile)
			{
				const TUniquePtr<UE::Rive::Assets::FURFileAssetLoader> FileAssetLoader = MakeUnique<
					UE::Rive::Assets::FURFileAssetLoader>(this, GetAssets());
				rive::ImportResult ImportResult;

				FScopeLock Lock(&RiveRenderer->GetThreadDataCS());
				RiveNativeFilePtr = rive::File::import(RiveNativeFileSpan, PLSRenderContext, &ImportResult,
													   FileAssetLoader.Get());
				// UI Helper
				for (int i = 0; i < RiveNativeFilePtr->artboardCount(); ++i)
				{
					rive::Artboard* NativeArtboard = RiveNativeFilePtr->artboard(i);
					ArtboardNames.Add(NativeArtboard->name().c_str());
				}
				if (ImportResult != rive::ImportResult::success)
				{
					UE_LOG(LogRive, Error, TEXT("Failed to import rive file."));
					BroadcastInitializationResult(false);
					return;
				}
			}
			else if (ensure(IsValid(ParentRiveFile)))
			{
				ArtboardNames = ParentRiveFile->ArtboardNames;
			}
			
			InstantiateArtboard(false); // We want the ArtboardChanged event to be raised after the Initialization Result
			if (ensure(GetArtboard()))
			{
				BroadcastInitializationResult(true);
			}
			else
			{
				UE_LOG(LogRive, Error, TEXT("Failed to instantiate the Artboard after importing the rive file."));
				BroadcastInitializationResult(false);
			}
			OnArtboardChanged.Broadcast(this, Artboard); // Now we can broadcast the Artboard Changed Event
			return;
		}

		UE_LOG(LogRive, Error, TEXT("Failed to import rive file."));
		BroadcastInitializationResult(false);
	}));
#endif // WITH_RIVE
}

void URiveFile::CallOrRegister_OnInitialized(FOnRiveFileInitialized::FDelegate&& Delegate)
{
	if (WasLastInitializationSuccessful.IsSet())
	{
		Delegate.Execute(this, WasLastInitializationSuccessful.GetValue());
	}
	else
	{
		OnInitializedDelegate.Add(MoveTemp(Delegate));
	}
}

void URiveFile::BroadcastInitializationResult(bool bSuccess)
{
	WasLastInitializationSuccessful = bSuccess;
	InitState = bSuccess ? ERiveInitState::Initialized : ERiveInitState::Uninitialized;
	OnInitializedDelegate.Broadcast(this, bSuccess);
}

void URiveFile::InstantiateArtboard(bool bRaiseArtboardChangedEvent)
{
	Artboard = nullptr;
	
	if (!UE::Rive::Renderer::IRiveRendererModule::IsAvailable())
	{
		UE_LOG(LogRive, Error, TEXT("Could not load rive file as the required Rive Renderer Module is either missing or not loaded properly."));
		return;
	}

	UE::Rive::Renderer::IRiveRenderer* RiveRenderer = UE::Rive::Renderer::IRiveRendererModule::Get().GetRenderer();

	if (!RiveRenderer)
	{
		UE_LOG(LogRive, Error, TEXT("Failed to import rive file as we do not have a valid renderer."));
		return;
	}

	if (!RiveRenderer->IsInitialized())
	{
		UE_LOG(LogRive, Error, TEXT("Could not load rive file as the required Rive Renderer is not initialized."));
		return;
	}

	if (!GetNativeFile())
	{
		UE_LOG(LogRive, Error, TEXT("Could not instance artboard as our native rive file is invalid."));
		return;
	}
	
	Artboard = NewObject<URiveArtboard>(this);
	
	RiveRenderTarget.Reset();
	RiveRenderTarget = RiveRenderer->CreateTextureTarget_GameThread(GetFName(), this);
	RiveRenderTarget->SetClearColor(ClearColor);
	
	if (ArtboardName.IsEmpty())
	{
		Artboard->Initialize(GetNativeFile(), RiveRenderTarget, ArtboardIndex, StateMachineName);
	}
	else
	{
		Artboard->Initialize(GetNativeFile(), RiveRenderTarget, ArtboardName, StateMachineName);
	}
	Artboard->RiveFitType = RiveFitType;
	Artboard->RiveAlignment = RiveAlignment;

	ResizeRenderTargets(bManualSize ? Size : Artboard->GetSize());
	RiveRenderTarget->Initialize();

	PrintStats();
	
	if (bRaiseArtboardChangedEvent)
	{
		OnArtboardChanged.Broadcast(this, Artboard);
	}
}

void URiveFile::OnResourceInitialized_RenderThread(FRHICommandListImmediate& RHICmdList, FTextureRHIRef& NewResource) const
{
	// When the resource change, we need to tell the Render Target otherwise we will keep on drawing on an outdated RT
	if (const UE::Rive::Renderer::IRiveRenderTargetPtr RenderTarget = RiveRenderTarget) //todo: might need a lock
	{
		RenderTarget->CacheTextureTarget_RenderThread(RHICmdList, NewResource);
	}
}

void URiveFile::SetWidgetClass(TSubclassOf<UUserWidget> InWidgetClass)
{
	WidgetClass = InWidgetClass;
}

const URiveArtboard* URiveFile::GetArtboard() const
{
#if WITH_RIVE
	if (Artboard && Artboard->IsInitialized())
	{
		return Artboard;
	}
#endif // WITH_RIVE
	return nullptr;
}

void URiveFile::PrintStats() const
{
	const rive::File* NativeFile = GetNativeFile();
	if (!NativeFile)
	{
		UE_LOG(LogRive, Error, TEXT("Could not print statistics as we have detected an empty rive file."));
		return;
	}

	FFormatNamedArguments RiveFileLoadArgs;
	RiveFileLoadArgs.Add(TEXT("Major"), FText::AsNumber(static_cast<int>(NativeFile->majorVersion)));
	RiveFileLoadArgs.Add(TEXT("Minor"), FText::AsNumber(static_cast<int>(NativeFile->minorVersion)));
	RiveFileLoadArgs.Add(TEXT("NumArtboards"), FText::AsNumber(static_cast<uint32>(NativeFile->artboardCount())));
	RiveFileLoadArgs.Add(TEXT("NumAssets"), FText::AsNumber(static_cast<uint32>(NativeFile->assets().size())));

	if (const rive::Artboard* NativeArtboard = NativeFile->artboard())
	{
		RiveFileLoadArgs.Add(
			TEXT("NumAnimations"), FText::AsNumber(static_cast<uint32>(NativeArtboard->animationCount())));
	}
	else
	{
		RiveFileLoadArgs.Add(TEXT("NumAnimations"), FText::AsNumber(0));
	}

	const FText RiveFileLoadMsg = FText::Format(NSLOCTEXT("FURFile", "RiveFileLoadMsg",
														  "Using Rive Runtime : {Major}.{Minor}; Artboard(s) Count : {NumArtboards}; Asset(s) Count : {NumAssets}; Animation(s) Count : {NumAnimations}"), RiveFileLoadArgs);

	UE_LOG(LogRive, Display, TEXT("%s"), *RiveFileLoadMsg.ToString());
}
