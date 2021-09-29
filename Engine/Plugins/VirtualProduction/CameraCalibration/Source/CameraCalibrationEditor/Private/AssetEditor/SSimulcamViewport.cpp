// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSimulcamViewport.h"

#include "EditorStyleSet.h"
#include "Materials/Material.h"
#include "NodalOffsetTool.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"


SSimulcamViewport::SSimulcamViewport()
	: Collector(this)
	, Material(nullptr)
	, MaterialBrush(nullptr)
	, TextureSampler(nullptr)
{ }

void SSimulcamViewport::Construct(const FArguments& InArgs, UTexture* InTexture)
{
	OnSimulcamViewportClicked = InArgs._OnSimulcamViewportClicked;

	// The Slate brush that renders the material.
	BrushImageSize = InArgs._BrushImageSize;

	if (InTexture != nullptr)
	{
		// create wrapper material
		Material = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);

		if (Material != nullptr)
		{
			TextureSampler = NewObject<UMaterialExpressionTextureSample>(Material);
			{
				TextureSampler->Texture = InTexture;
				TextureSampler->AutoSetSampleType();
			}

			FExpressionOutput& Output = TextureSampler->GetOutputs()[0];
			FExpressionInput& Input = Material->EmissiveColor;
			{
				Input.Expression = TextureSampler;
				Input.Mask = Output.Mask;
				Input.MaskR = Output.MaskR;
				Input.MaskG = Output.MaskG;
				Input.MaskB = Output.MaskB;
				Input.MaskA = Output.MaskA;
			}

			FExpressionInput& Opacity = Material->Opacity;
			{
				Opacity.Expression = TextureSampler;
				Opacity.Mask = Output.Mask;
				Opacity.MaskR = 0;
				Opacity.MaskG = 0;
				Opacity.MaskB = 0;
				Opacity.MaskA = 1;
			}

			Material->BlendMode = BLEND_AlphaComposite;

			Material->Expressions.Add(TextureSampler);
			Material->MaterialDomain = EMaterialDomain::MD_UI;
			Material->PostEditChange();
		}

		// create Slate brush
		MaterialBrush = MakeShared<FSlateBrush>();
		{
			MaterialBrush->SetResourceObject(Material);
		}
	}

	ChildSlot
	[
		SNew(SScaleBox)
		.Stretch_Lambda([]() -> EStretch::Type { return EStretch::Fill;	})
		[
			SNew(SImage)
			.Image(MaterialBrush.IsValid() ? MaterialBrush.Get() : FEditorStyle::GetBrush("WhiteTexture"))
			.OnMouseButtonDown_Lambda([&](const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
			{
				OnSimulcamViewportClicked.ExecuteIfBound(MyGeometry, MouseEvent);
				return FReply::Handled();
			})
		]
	];
}

void SSimulcamViewport::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

	if (BrushImageSize.IsSet())
	{
		const FVector2D Size = BrushImageSize.Get();
		MaterialBrush->ImageSize.X = Size.X;
		MaterialBrush->ImageSize.Y = Size.Y;
	}
}
