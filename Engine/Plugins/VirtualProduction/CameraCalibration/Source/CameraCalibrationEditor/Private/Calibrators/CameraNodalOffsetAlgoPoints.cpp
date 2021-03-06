// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraNodalOffsetAlgoPoints.h"
#include "AssetEditor/CameraCalibrationStepsController.h"
#include "AssetEditor/NodalOffsetTool.h"
#include "CalibrationPointComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CameraCalibrationEditorLog.h"
#include "CameraCalibrationUtils.h"
#include "CineCameraComponent.h"
#include "DistortionRenderingUtils.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Input/Events.h"
#include "Internationalization/Text.h"
#include "Layout/Geometry.h"
#include "LensDistortionModelHandlerBase.h"
#include "LensFile.h"
#include "Math/Vector.h"
#include "Misc/MessageDialog.h"
#include "PropertyCustomizationHelpers.h"
#include "UI/SFilterableActorPicker.h"
#include "ScopedTransaction.h"
#include "UI/CameraCalibrationWidgetHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/SWidget.h"

#include <vector>

#if WITH_OPENCV
#include "OpenCVHelper.h"
OPENCV_INCLUDES_START
#undef check 
#include "opencv2/opencv.hpp"
#include "opencv2/calib3d.hpp"
OPENCV_INCLUDES_END
#endif

#define LOCTEXT_NAMESPACE "CameraNodalOffsetAlgoPoints"

namespace CameraNodalOffsetAlgoPoints
{
	class SCalibrationRowGenerator
		: public SMultiColumnTableRow<TSharedPtr<UCameraNodalOffsetAlgoPoints::FCalibrationRowData>>
	{
		using FCalibrationRowData = UCameraNodalOffsetAlgoPoints::FCalibrationRowData;

	public:
		SLATE_BEGIN_ARGS(SCalibrationRowGenerator) {}

		/** The list item for this row */
		SLATE_ARGUMENT(TSharedPtr<FCalibrationRowData>, CalibrationRowData)

		SLATE_END_ARGS()

		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
		{
			CalibrationRowData = Args._CalibrationRowData;

			SMultiColumnTableRow<TSharedPtr<FCalibrationRowData>>::Construct(
				FSuperRowType::FArguments()
				.Padding(1.0f),
				OwnerTableView
			);
		}

		//~Begin SMultiColumnTableRow
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == TEXT("Name"))
			{
				const FString Text = CalibrationRowData->CalibratorPointData.Name;
				return SNew(STextBlock).Text(FText::FromString(Text));
			}

			if (ColumnName == TEXT("Point2D"))
			{
				const FString Text = FString::Printf(TEXT("(%.2f, %.2f)"),
					CalibrationRowData->Point2D.X,
					CalibrationRowData->Point2D.Y);

				return SNew(STextBlock).Text(FText::FromString(Text));
			}

			if (ColumnName == TEXT("Point3D"))
			{
				const FString Text = FString::Printf(TEXT("(%.0f, %.0f, %.0f)"),
					CalibrationRowData->CalibratorPointData.Location.X,
					CalibrationRowData->CalibratorPointData.Location.Y,
					CalibrationRowData->CalibratorPointData.Location.Z);

				return SNew(STextBlock).Text(FText::FromString(Text));
			}

			return SNullWidget::NullWidget;
		}
		//~End SMultiColumnTableRow


	private:
		TSharedPtr<FCalibrationRowData> CalibrationRowData;
	};

	/** Contains basic result of a nodal offset calibration based on a single camera pose for all samples */
	struct FSinglePoseResult
	{
		/** Tranform that can be a world coordinate or an offset */
		FTransform Transform;

		/** Number of calibration samples/rows to generate this result */
		int32 NumSamples;
	};

	/** 
	 * Weight-averages the transform of all single camera pose results. 
	 * Weights are given by relative number of samples used for each calibration result.
	 * 
	 * @param SinglePoseResults Array with independent calibration results for each camera pose
	 * @param OutAvgTransform Weighted average of 
	 * 
	 * @result True if successful
	 */
	bool AverageSinglePoseResults(const TArray<FSinglePoseResult>& SinglePoseResults, FTransform& OutAvgTransform)
	{
		// Calculate the total number of samples in order to later calculate the weights of each single pose result.

		int32 TotalNumSamples = 0;

		for (const FSinglePoseResult& SinglePoseResult : SinglePoseResults)
		{
			TotalNumSamples  += SinglePoseResult.NumSamples;
		}

		if (TotalNumSamples < 1)
		{
			return false;
		}

		// Average the location

		FVector AverageLocation(0);

		for (const FSinglePoseResult& SinglePoseResult : SinglePoseResults)
		{
			const float Weight = float(SinglePoseResult.NumSamples) / TotalNumSamples;
			AverageLocation += Weight * SinglePoseResult.Transform.GetLocation();
		}

		// Average the rotation

		float AverageQuatVec[4] = { 0 }; // Simple averaging should work for similar quaterions, which these are.

		for (const FSinglePoseResult& SinglePoseResult : SinglePoseResults)
		{
			const FQuat Rotation = SinglePoseResult.Transform.GetRotation();

			const float ThisQuat[4] = {
				Rotation.X,
				Rotation.Y,
				Rotation.Z,
				Rotation.W,
			};

			float Weight = float(SinglePoseResult.NumSamples) / TotalNumSamples;

			if ((Rotation | SinglePoseResults[0].Transform.GetRotation()) < 0)
			{
				Weight = -Weight;
			}

			for (int32 QuatIdx = 0; QuatIdx < 4; ++QuatIdx)
			{
				AverageQuatVec[QuatIdx] += Weight * ThisQuat[QuatIdx];
			}
		}

		const FQuat AverageQuat(AverageQuatVec[0], AverageQuatVec[1], AverageQuatVec[2], AverageQuatVec[3]);

		// Populate output

		OutAvgTransform.SetTranslation(AverageLocation);
		OutAvgTransform.SetRotation(AverageQuat.GetNormalized());
		OutAvgTransform.SetScale3D(FVector(1));

		return true;
	}
};

void UCameraNodalOffsetAlgoPoints::Initialize(UNodalOffsetTool* InNodalOffsetTool)
{
	NodalOffsetTool = InNodalOffsetTool;

	// Guess which calibrator to use by searching for actors with CalibrationPointComponents.
	SetCalibrator(FindFirstCalibrator());
}

void UCameraNodalOffsetAlgoPoints::Shutdown()
{
	NodalOffsetTool.Reset();
}

void UCameraNodalOffsetAlgoPoints::Tick(float DeltaTime)
{
	if (!NodalOffsetTool.IsValid())
	{
		return;
	}

	FCameraCalibrationStepsController* StepsController = NodalOffsetTool->GetCameraCalibrationStepsController();

	if (!StepsController)
	{
		return;
	}

	// If not paused, cache calibrator 3d point position
	if (!StepsController->IsPaused())
	{
		// Get calibration point data
		do 
		{	
			LastCalibratorPoints.Empty();

			for (const TSharedPtr<FCalibratorPointData>& CalibratorPoint : CurrentCalibratorPoints)
			{
				if (!CalibratorPoint.IsValid())
				{
					continue;
				}

				FCalibratorPointCache PointCache;

				if (!CalibratorPointCacheFromName(CalibratorPoint->Name, PointCache))
				{
					continue;
				}

				LastCalibratorPoints.Emplace(MoveTemp(PointCache));
			}
		} while (0);

		// Get camera data
		do
		{
			LastCameraData.bIsValid = false;

			const FLensFileEvalData* LensFileEvalData = StepsController->GetLensFileEvalData();

			// We require lens evaluation data, and that distortion was evaluated so that 2d correlations are valid
			// Note: The comp enforces distortion application.
			if (!LensFileEvalData || !LensFileEvalData->Distortion.bWasEvaluated)
			{
				break;
			}

			const ACameraActor* Camera = StepsController->GetCamera();

			if (!Camera)
			{
				break;
			}

			const UCameraComponent* CameraComponent = Camera->GetCameraComponent();

			if (!CameraComponent)
			{
				break;
			}

			LastCameraData.Pose = CameraComponent->GetComponentToWorld();
			LastCameraData.UniqueId = Camera->GetUniqueID();
			LastCameraData.LensFileEvalData = *LensFileEvalData;

			const AActor* CameraParentActor = Camera->GetAttachParentActor();

			if (CameraParentActor)
			{
				LastCameraData.ParentPose = CameraParentActor->GetTransform();
				LastCameraData.ParentUniqueId = CameraParentActor->GetUniqueID();
			}
			else
			{
				LastCameraData.ParentUniqueId = INDEX_NONE;
			}

			if (Calibrator.IsValid())
			{
				LastCameraData.CalibratorPose = Calibrator->GetTransform();
				LastCameraData.CalibratorUniqueId = Calibrator->GetUniqueID();

				const AActor* CalibratorParentActor = Calibrator->GetAttachParentActor();

				if (CalibratorParentActor)
				{
					LastCameraData.CalibratorParentPose = CalibratorParentActor->GetTransform();
					LastCameraData.CalibratorParentUniqueId = CalibratorParentActor->GetUniqueID();
				}
				else
				{
					LastCameraData.CalibratorParentUniqueId = INDEX_NONE;
				}
			}
			else
			{
				LastCameraData.CalibratorUniqueId = INDEX_NONE;
				LastCameraData.CalibratorParentUniqueId = INDEX_NONE;
			}

			LastCameraData.bIsValid = true;

		} while (0);
	}
}

bool UCameraNodalOffsetAlgoPoints::OnViewportClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// We only respond to left clicks
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return false;
	}

	if (!ensure(NodalOffsetTool.IsValid()))
	{
		return true;
	}

	FCameraCalibrationStepsController* StepsController = NodalOffsetTool->GetCameraCalibrationStepsController();

	if (!ensure(StepsController))
	{
		return true;
	}

	// Get currently selected calibrator point
	FCalibratorPointCache LastCalibratorPoint;
	LastCalibratorPoint.bIsValid = false;
	{
		TSharedPtr<FCalibratorPointData> CalibratorPoint = CalibratorPointsComboBox->GetSelectedItem();

		if (!CalibratorPoint.IsValid())
		{
			return true;
		}

		// Find its values in the cache
		for (const FCalibratorPointCache& PointCache : LastCalibratorPoints)
		{
			if (PointCache.bIsValid && (PointCache.Name == CalibratorPoint->Name))
			{
				LastCalibratorPoint = PointCache;
				break;
			}
		}
	}

	// Check that we have a valid calibrator 3dpoint or camera data
	if (!LastCalibratorPoint.bIsValid || !LastCameraData.bIsValid)
	{
		return true;
	}

	// Create the row that we're going to add
	TSharedPtr<FCalibrationRowData> Row = MakeShared<FCalibrationRowData>();

	Row->CameraData = LastCameraData;
	Row->CalibratorPointData = LastCalibratorPoint;

	// Get the mouse click 2d position
	if (!StepsController->CalculateNormalizedMouseClickPosition(MyGeometry, MouseEvent, Row->Point2D))
	{
		return true;
	}

	// Validate the new row, show a message if validation fails.
	{
		FText ErrorMessage;

		if (!ValidateNewRow(Row, ErrorMessage))
		{
			const FText TitleError = LOCTEXT("NewRowError", "New Row Error");
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);
			return true;
		}
	}

	// Add this data point
	CalibrationRows.Add(Row);

	// Notify the ListView of the new data
	if (CalibrationListView.IsValid())
	{
		CalibrationListView->RequestListRefresh();
		CalibrationListView->RequestNavigateToItem(Row);
	}

	// Auto-advance to the next calibration point (if it exists)
	if (AdvanceCalibratorPoint())
	{
		// Play media if this was the last point in the object
		StepsController->Play();
	}

	return true;
}

bool UCameraNodalOffsetAlgoPoints::AdvanceCalibratorPoint()
{
	TSharedPtr<FCalibratorPointData> CurrentItem = CalibratorPointsComboBox->GetSelectedItem();

	if (!CurrentItem.IsValid())
	{
		return false;
	}

	for (int32 PointIdx = 0; PointIdx < CurrentCalibratorPoints.Num(); PointIdx++)
	{
		if (CurrentCalibratorPoints[PointIdx]->Name == CurrentItem->Name)
		{
			const int32 NextIdx = (PointIdx + 1) % CurrentCalibratorPoints.Num();
			CalibratorPointsComboBox->SetSelectedItem(CurrentCalibratorPoints[NextIdx]);

			// return true if we wrapped around (NextIdx is zero)
			return !NextIdx;
		}
	}

	return false;
}

bool UCameraNodalOffsetAlgoPoints::GetCurrentCalibratorPointLocation(FVector& OutLocation)
{
	TSharedPtr<FCalibratorPointData> CalibratorPointData = CalibratorPointsComboBox->GetSelectedItem();

	if (!CalibratorPointData.IsValid())
	{
		return false;
	}

	FCalibratorPointCache PointCache;

	if (!CalibratorPointCacheFromName(CalibratorPointData->Name, PointCache))
	{
		return false;
	}

	OutLocation = PointCache.Location;

	return true;
}

TSharedRef<SWidget> UCameraNodalOffsetAlgoPoints::BuildUI()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot() // Calibrator picker
		.VAlign(EVerticalAlignment::VAlign_Top)
		.AutoHeight()
		.MaxHeight(FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		[ FCameraCalibrationWidgetHelpers::BuildLabelWidgetPair(LOCTEXT("Calibrator", "Calibrator"), BuildCalibrationDevicePickerWidget()) ]
		
		+SVerticalBox::Slot() // Calibrator point names
		.AutoHeight()
		.MaxHeight(FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		[ FCameraCalibrationWidgetHelpers::BuildLabelWidgetPair(LOCTEXT("CalibratorPoint", "Calibrator Point"), BuildCalibrationPointsComboBox()) ]
		
		+ SVerticalBox::Slot() // Calibration Rows
		.AutoHeight()
		.MaxHeight(12 * FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		[ BuildCalibrationPointsTable() ]
		
		+ SVerticalBox::Slot() // Action buttons (e.g. Remove, Clear)
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.AutoHeight()
		.Padding(0,20)
		[ BuildCalibrationActionButtons() ]
		;
}

bool UCameraNodalOffsetAlgoPoints::ValidateNewRow(TSharedPtr<FCalibrationRowData>& Row, FText& OutErrorMessage) const
{
	const FCameraCalibrationStepsController* StepsController;
	const ULensFile* LensFile;

	if (!ensure(GetStepsControllerAndLensFile(&StepsController, &LensFile)))
	{
		OutErrorMessage = LOCTEXT("ToolNotFound", "Tool not found");
		return false;
	}

	if (!Row.IsValid())
	{
		OutErrorMessage = LOCTEXT("InvalidRowPointer", "Invalid row pointer");
		return false;
	}

	if (!CalibrationRows.Num())
	{
		return true;
	}

	// Distortion was evaluated

	if (!Row->CameraData.LensFileEvalData.Distortion.bWasEvaluated)
	{
		OutErrorMessage = LOCTEXT("DistortionNotEvaluated", "Distortion was not evaluated");
		return false;
	}

	// Same LensFile

	const TSharedPtr<FCalibrationRowData>& FirstRow = CalibrationRows[0];

	if (Row->CameraData.LensFileEvalData.LensFile != LensFile)
	{
		OutErrorMessage = LOCTEXT("LensFileWasNotTheSame", "Lens file was not the same");
		return false;
	}

	// Same camera in view

	const ACameraActor* Camera = StepsController->GetCamera();

	if (!Camera || !Camera->GetCameraComponent())
	{
		OutErrorMessage = LOCTEXT("MissingCamera", "Missing camera");
		return false;
	}

	if (Camera->GetUniqueID() != Row->CameraData.UniqueId)
	{
		OutErrorMessage = LOCTEXT("DifferentCameraAsSelected", "Different camera as selected");
		return false;
	}

	// Same camera as before

	if (FirstRow->CameraData.UniqueId != Row->CameraData.UniqueId)
	{
		OutErrorMessage = LOCTEXT("CameraChangedDuringTheTest", "Camera changed during the test");
		return false;
	}

	// Same parent as before

	if (FirstRow->CameraData.ParentUniqueId != Row->CameraData.ParentUniqueId)
	{
		OutErrorMessage = LOCTEXT("CameraParentChangedDuringTheTest", "Camera parent changed during the test");
		return false;
	}

	// FZ inputs are always valid, no need to verify them. They could be coming from LiveLink or fallback to a default one

	// bApplyNodalOffset did not change.
	//
	// It can't change because we need to know if the camera pose is being affected or not by the current nodal offset evaluation.
	// And we need to know that because the offset we calculate will need to either subtract or not the current evaluation when adding it to the LUT.
	if (FirstRow->CameraData.LensFileEvalData.NodalOffset.bWasApplied != Row->CameraData.LensFileEvalData.NodalOffset.bWasApplied)
	{
		OutErrorMessage = LOCTEXT("ApplyNodalOffsetChanged", "Apply nodal offset changed");
		return false;
	}

	//@todo Focus and zoom did not change much (i.e. inputs to distortion and nodal offset). 
	//      Threshold for physical units should differ from normalized encoders.

	return true;
}

bool UCameraNodalOffsetAlgoPoints::BasicCalibrationChecksPass(const TArray<TSharedPtr<FCalibrationRowData>>& Rows, FText& OutErrorMessage) const
{
	const FCameraCalibrationStepsController* StepsController;
	const ULensFile* LensFile;

	if (!ensure(GetStepsControllerAndLensFile(&StepsController, &LensFile)))
	{
		OutErrorMessage = LOCTEXT("ToolNotFound", "Tool not found");
		return false;
	}

	// Sanity checks
	//

	// Enough points
	if (Rows.Num() < 4)
	{
		OutErrorMessage = LOCTEXT("NotEnoughSamples", "At least 4 correspondence points are required");
		return false;
	}

	// All points are valid
	for (const TSharedPtr<FCalibrationRowData>& Row : Rows)
	{
		if (!ensure(Row.IsValid()))
		{
			OutErrorMessage = LOCTEXT("InvalidRow", "Invalid Row");
			return false;
		}
	}

	// Get camera.

	const ACameraActor* Camera = StepsController->GetCamera();

	if (!Camera || !Camera->GetCameraComponent())
	{
		OutErrorMessage = LOCTEXT("MissingCamera", "Missing camera");
		return false;
	}

	UCameraComponent* CameraComponent = Camera->GetCameraComponent();
	check(CameraComponent);

	UCineCameraComponent* CineCameraComponent = Cast<UCineCameraComponent>(CameraComponent);

	if (!CineCameraComponent)
	{
		OutErrorMessage = LOCTEXT("OnlyCineCamerasSupported", "Only cine cameras are supported");
		return false;
	}

	const TSharedPtr<FCalibrationRowData>& FirstRow = Rows[0];

	// Still same camera (since we need it to get the distortion handler, which much be the same)

	if (Camera->GetUniqueID() != FirstRow->CameraData.UniqueId)
	{
		OutErrorMessage = LOCTEXT("DifferentCameraAsSelected", "Different camera as selected");
		return false;
	}

	// Camera did not move much.
	for (const TSharedPtr<FCalibrationRowData>& Row : Rows)
	{
		if (!FCameraCalibrationUtils::IsNearlyEqual(FirstRow->CameraData.Pose, Row->CameraData.Pose))
		{
			OutErrorMessage = LOCTEXT("CameraMoved", "Camera moved too much between samples.");
			return false;
		}
	}

	return true;
}

bool UCameraNodalOffsetAlgoPoints::CalculatedOptimalCameraComponentPose(
	FTransform& OutDesiredCameraTransform, 
	const TArray<TSharedPtr<FCalibrationRowData>>& Rows, 
	FText& OutErrorMessage) const
{
	if (!BasicCalibrationChecksPass(Rows, OutErrorMessage))
	{
		return false;
	}

	const FCameraCalibrationStepsController* StepsController;
	const ULensFile* LensFile;

	if (!ensure(GetStepsControllerAndLensFile(&StepsController, &LensFile)))
	{
		OutErrorMessage = LOCTEXT("ToolNotFound", "Tool not found");
		return false;
	}

	const ULensDistortionModelHandlerBase* DistortionHandler = StepsController->GetDistortionHandler();
	if (!DistortionHandler)
	{
		OutErrorMessage = LOCTEXT("DistortionHandlerNotFound", "No distortion source found");
		return false;
	}

	// Get parameters from the handler
	FLensDistortionState DistortionState = DistortionHandler->GetCurrentDistortionState();

#if WITH_OPENCV

	// Find the pose that minimizes the reprojection error

	// Populate the 3d/2d correlation points

	std::vector<cv::Point3f> Points3d;
	std::vector<cv::Point2f> Points2d;

	TArray<FVector2D> ImagePoints;

	for (const TSharedPtr<FCalibrationRowData>& Row : Rows)
	{
		// Convert from UE coordinates to OpenCV coordinates

		FTransform Transform;
		Transform.SetIdentity();
		Transform.SetLocation(Row->CalibratorPointData.Location);

		FCameraCalibrationUtils::ConvertUnrealToOpenCV(Transform);

		// Calibrator 3d points
		Points3d.push_back(cv::Point3f(
			Transform.GetLocation().X,
			Transform.GetLocation().Y,
			Transform.GetLocation().Z));

		ImagePoints.Add(FVector2D(Row->Point2D.X, Row->Point2D.Y));
	}

	// Populate camera matrix

	cv::Mat CameraMatrix(3, 3, cv::DataType<double>::type);
	cv::setIdentity(CameraMatrix);

	// Note: cv::Mat uses (row,col) indexing.
	//
	//  Fx  0  Cx
	//  0  Fy  Cy
	//  0   0   1

	CameraMatrix.at<double>(0, 0) = DistortionState.FocalLengthInfo.FxFy.X;
	CameraMatrix.at<double>(1, 1) = DistortionState.FocalLengthInfo.FxFy.Y;

	// The displacement map will correct for image center offset
	CameraMatrix.at<double>(0, 2) = 0.5;
	CameraMatrix.at<double>(1, 2) = 0.5;

	// Manually undistort the 2D image points
	TArray<FVector2D> UndistortedPoints;
	UndistortedPoints.AddZeroed(ImagePoints.Num());
	DistortionRenderingUtils::UndistortImagePoints(DistortionHandler->GetDistortionDisplacementMap(), ImagePoints, UndistortedPoints);

	Points2d.reserve(UndistortedPoints.Num());
	for (FVector2D Point : UndistortedPoints)
	{
		Points2d.push_back(cv::Point2f(
			Point.X,
			Point.Y));
	}

	// Solve for camera position
	cv::Mat Rrod = cv::Mat::zeros(3, 1, cv::DataType<double>::type); // Rotation vector in Rodrigues notation. 3x1.
	cv::Mat Tobj = cv::Mat::zeros(3, 1, cv::DataType<double>::type); // Translation vector. 3x1.

	// We send no distortion parameters, because Points2d was manually undistorted already
	if (!cv::solvePnP(Points3d, Points2d, CameraMatrix, cv::noArray(), Rrod, Tobj))
	{
		OutErrorMessage = LOCTEXT("SolvePnpFailed", "Failed to resolve a camera position given the data in the calibration rows. Please retry the calibration.");
		return false;
	}

	// Check for invalid data
	{
		const double Tx = Tobj.at<double>(0);
		const double Ty = Tobj.at<double>(1);
		const double Tz = Tobj.at<double>(2);

		const double MaxValue = 1e16;

		if (abs(Tx) > MaxValue || abs(Ty) > MaxValue || abs(Tz) > MaxValue)
		{
			OutErrorMessage = LOCTEXT("DataOutOfBounds", "The triangulated camera position had invalid values, please retry the calibration.");
			return false;
		}
	}

	// Convert to camera pose

	// [R|t]' = [R'|-R'*t]

	// Convert from Rodrigues to rotation matrix
	cv::Mat Robj;
	cv::Rodrigues(Rrod, Robj); // Robj is 3x3

	// Calculate camera translation
	cv::Mat Tcam = -Robj.t() * Tobj;

	// Invert/transpose to get camera orientation
	cv::Mat Rcam = Robj.t();

	// Convert back to UE coordinates

	FMatrix M = FMatrix::Identity;

	// Fill rotation matrix
	for (int32 Column = 0; Column < 3; ++Column)
	{
		M.SetColumn(Column, FVector(
			Rcam.at<double>(Column, 0),
			Rcam.at<double>(Column, 1),
			Rcam.at<double>(Column, 2))
		);
	}

	// Fill translation vector
	M.M[3][0] = Tcam.at<double>(0);
	M.M[3][1] = Tcam.at<double>(1);
	M.M[3][2] = Tcam.at<double>(2);

	OutDesiredCameraTransform.SetFromMatrix(M);
	FCameraCalibrationUtils::ConvertOpenCVToUnreal(OutDesiredCameraTransform);

	return true;

#else
	{
		OutErrorMessage = LOCTEXT("OpenCVRequired", "OpenCV is required");
		return false;
	}
#endif //WITH_OPENCV
}

bool UCameraNodalOffsetAlgoPoints::CalibratorMovedInAnyRow(
	const TArray<TSharedPtr<FCalibrationRowData>>& Rows) const
{
	if (!Rows.Num())
	{
		return false;
	}

	TSharedPtr<FCalibrationRowData> FirstRow;

	for (const TSharedPtr<FCalibrationRowData>& Row : Rows)
	{
		if (!FirstRow.IsValid())
		{
			if (ensure(Row.IsValid()))
			{
				FirstRow = Row;
			}

			continue;
		}

		if (!ensure(Row.IsValid()))
		{
			continue;
		}

		if (!FCameraCalibrationUtils::IsNearlyEqual(FirstRow->CameraData.CalibratorPose, Row->CameraData.CalibratorPose))
		{
			return true;
		}
	}

	return false;
}


bool UCameraNodalOffsetAlgoPoints::CalibratorMovedAcrossGroups(
	const TArray<TSharedPtr<TArray<TSharedPtr<FCalibrationRowData>>>>& SamePoseRowGroups) const
{
	TArray<TSharedPtr<FCalibrationRowData>> Rows;

	for (const auto& SamePoseRowGroup : SamePoseRowGroups)
	{
		if (!ensure(SamePoseRowGroup.IsValid()))
		{
			continue;
		}

		Rows.Append(*SamePoseRowGroup);
	}

	return CalibratorMovedInAnyRow(Rows);
}

bool UCameraNodalOffsetAlgoPoints::GetNodalOffsetSinglePose(
	FNodalPointOffset& OutNodalOffset, 
	float& OutFocus, 
	float& OutZoom, 
	float& OutError, 
	const TArray<TSharedPtr<FCalibrationRowData>>& Rows, 
	FText& OutErrorMessage) const
{
	const FCameraCalibrationStepsController* StepsController;
	const ULensFile* LensFile;

	if (!ensure(GetStepsControllerAndLensFile(&StepsController, &LensFile)))
	{
		OutErrorMessage = LOCTEXT("LensNotFound", "Lens not found");
		return false;
	}

	FTransform DesiredCameraTransform;

	if (!CalculatedOptimalCameraComponentPose(DesiredCameraTransform, Rows, OutErrorMessage))
	{
		return false;
	}

	// This is how we update the offset even when the camera is evaluating the current
	// nodal offset curve in the Lens File:
	// 
	// CameraTransform = ExistingOffset * CameraTransformWithoutOffset
	// => CameraTransformWithoutOffset = ExistingOffset' * CameraTransform
	//
	// DesiredTransform = Offset * CameraTransformWithoutOffset
	// => Offset = DesiredTransform * CameraTransformWithoutOffset'
	// => Offset = DesiredTransform * (ExistingOffset' * CameraTransform)'
	// => Offset = DesiredTransform * (CameraTransform' * ExistingOffset)

	// Evaluate nodal offset

	// Determine the input values to the LUT (focus and zoom)

	check(Rows.Num()); // There must have been rows for CalculatedOptimalCameraComponentPose to have succeeded.
	check(Rows[0].IsValid()); // All rows should be valid.

	const TSharedPtr<FCalibrationRowData>& FirstRow = Rows[0];

	OutFocus = FirstRow->CameraData.LensFileEvalData.Input.Focus;
	OutZoom  = FirstRow->CameraData.LensFileEvalData.Input.Zoom;

	// See if the camera already had an offset applied, in which case we need to account for it.

	FTransform ExistingOffset = FTransform::Identity;

	if (FirstRow->CameraData.LensFileEvalData.NodalOffset.bWasApplied)
	{
		FNodalPointOffset NodalPointOffset;

		if (LensFile->EvaluateNodalPointOffset(OutFocus, OutZoom, NodalPointOffset))
		{
			ExistingOffset.SetTranslation(NodalPointOffset.LocationOffset);
			ExistingOffset.SetRotation(NodalPointOffset.RotationOffset);
		}
	}

	FTransform DesiredOffset = DesiredCameraTransform * FirstRow->CameraData.Pose.Inverse() * ExistingOffset;

	OutNodalOffset.LocationOffset = DesiredOffset.GetLocation();
	OutNodalOffset.RotationOffset = DesiredOffset.GetRotation();

	return true;
}

bool UCameraNodalOffsetAlgoPoints::GetNodalOffset(FNodalPointOffset& OutNodalOffset, float& OutFocus, float& OutZoom, float& OutError, FText& OutErrorMessage)
{
	using namespace CameraNodalOffsetAlgoPoints;

	const FCameraCalibrationStepsController* StepsController;
	const ULensFile* LensFile;

	if (!ensure(GetStepsControllerAndLensFile(&StepsController, &LensFile)))
	{
		OutErrorMessage = LOCTEXT("LensNotFound", "Lens not found");
		return false;
	}

	// Group Rows by camera poses.
	TArray<TSharedPtr<TArray<TSharedPtr<FCalibrationRowData>>>> SamePoseRowGroups;
	GroupRowsByCameraPose(SamePoseRowGroups, CalibrationRows);

	if (!SamePoseRowGroups.Num())
	{
		OutErrorMessage = LOCTEXT("NotEnoughRows", "Not enough calibration rows. Please add more samples and try again.");
		return false;
	}

	// Do some basic checks on each group
	for (const auto& SamePoseRowGroup : SamePoseRowGroups)
	{
		if (!BasicCalibrationChecksPass(*SamePoseRowGroup, OutErrorMessage))
		{
			return false;
		}
	}

	TArray<FSinglePoseResult> SinglePoseResults;
	SinglePoseResults.Reserve(SamePoseRowGroups.Num());

	// Solve each group independently
	for (const auto& SamePoseRowGroup : SamePoseRowGroups)
	{
		FNodalPointOffset NodalOffset;

		if (!GetNodalOffsetSinglePose(
			NodalOffset,
			OutFocus,
			OutZoom,
			OutError,
			*SamePoseRowGroup,
			OutErrorMessage))
		{
			return false;
		}

		// Add results to the array of single pose results

		FSinglePoseResult SinglePoseResult;

		SinglePoseResult.Transform.SetLocation(NodalOffset.LocationOffset);
		SinglePoseResult.Transform.SetRotation(NodalOffset.RotationOffset);
		SinglePoseResult.NumSamples = SamePoseRowGroup->Num();

		SinglePoseResults.Add(SinglePoseResult);
	}

	check(SinglePoseResults.Num()); // If any single pose result failed then we should not have reached here.

	FTransform AverageTransform;

	if (!AverageSinglePoseResults(SinglePoseResults, AverageTransform))
	{
		OutErrorMessage = LOCTEXT("CouldNotAverageSinglePoseResults",
			"There was an error when trying to average the single pose results");

		return false;
	}

	// Assign output nodal offset.

	OutNodalOffset.LocationOffset = AverageTransform.GetLocation();
	OutNodalOffset.RotationOffset = AverageTransform.GetRotation();

	// OutFocus, OutZoom were already assigned.
	// Note that OutError will have the error of the last camera pose instead of a global error.

	return true;
}

TSharedRef<SWidget> UCameraNodalOffsetAlgoPoints::BuildCalibrationDevicePickerWidget()
{
	return SNew(SFilterableActorPicker)
		.OnSetObject_Lambda([&](const FAssetData& AssetData) -> void
		{
			if (AssetData.IsValid())
			{
				SetCalibrator(Cast<AActor>(AssetData.GetAsset()));
			}
		})
		.OnShouldFilterAsset_Lambda([&](const FAssetData& AssetData) -> bool
		{
			const AActor* Actor = Cast<AActor>(AssetData.GetAsset());

			if (!Actor)
			{
				return false;
			}

			TArray<UCalibrationPointComponent*, TInlineAllocator<4>> CalibrationPoints;
			Actor->GetComponents<UCalibrationPointComponent, TInlineAllocator<4>>(CalibrationPoints);

			return (CalibrationPoints.Num() > 0);
		})
		.ActorAssetData_Lambda([&]() -> FAssetData
		{
			return FAssetData(GetCalibrator(), true);
		});
}

TSharedRef<SWidget> UCameraNodalOffsetAlgoPoints::BuildCalibrationActionButtons()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot() // Row manipulation
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot() // Button to clear all rows
			.AutoWidth()
			[ 
				SNew(SButton)
				.Text(LOCTEXT("ClearAll", "Clear All"))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked_Lambda([&]() -> FReply
				{
					ClearCalibrationRows();
					return FReply::Handled();
				})
			]
		]
		+ SVerticalBox::Slot() // Spacer
		[
			SNew(SBox)
			.MinDesiredHeight(0.5 * FCameraCalibrationWidgetHelpers::DefaultRowHeight)
			.MaxDesiredHeight(0.5 * FCameraCalibrationWidgetHelpers::DefaultRowHeight)
		]
		+ SVerticalBox::Slot() // Apply To Calibrator
		[
			SNew(SButton)
			.Text(LOCTEXT("ApplyToCalibrator", "Apply To Calibrator"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked_Lambda([&]() -> FReply
			{
				FScopedTransaction Transaction(LOCTEXT("ApplyNodalOffsetToCalibrator", "Applying Nodal Offset to Calibrator"));
				ApplyNodalOffsetToCalibrator();
				return FReply::Handled();
			})
		]
		+ SVerticalBox::Slot() // Apply To Camera Parent
		[
			SNew(SButton)
			.Text(LOCTEXT("ApplyToTrackingOrigin", "Apply To Camera Parent"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked_Lambda([&]() -> FReply
			{
				FScopedTransaction Transaction(LOCTEXT("ApplyNodalOffsetToTrackingOrigin", "Applying Nodal Offset to Tracking Origin"));
				ApplyNodalOffsetToTrackingOrigin();
				return FReply::Handled();
			})
		]
		+ SVerticalBox::Slot() // Apply To Calibrator Parent
		[
			SNew(SButton)
			.Text(LOCTEXT("ApplyToCalibratorParent", "Apply To Calibrator Parent"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked_Lambda([&]() -> FReply
			{
				FScopedTransaction Transaction(LOCTEXT("ApplyNodalOffsetToCalibratorParent", "Applying Nodal Offset to Calibrator Parent"));
				ApplyNodalOffsetToCalibratorParent();
				return FReply::Handled();
			})
		]
		;
}

bool UCameraNodalOffsetAlgoPoints::ApplyNodalOffsetToCalibrator()
{
	using namespace CameraNodalOffsetAlgoPoints;

	// Get the desired camera component world pose

	FText ErrorMessage;
	const FText TitleError = LOCTEXT("CalibrationError", "CalibrationError");

	// Get the calibrator

	if (!Calibrator.IsValid())
	{
		ErrorMessage = LOCTEXT("MissingCalibrator", "Missing Calibrator");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	if (!CalibrationRows.Num())
	{
		ErrorMessage = LOCTEXT("NotEnoughSampleRows", "Not enough sample rows. Please add more and try again.");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	// All calibration points should correspond to the same calibrator

	for (const TSharedPtr<FCalibrationRowData>& Row : CalibrationRows)
	{
		check(Row.IsValid());

		if (Row->CameraData.CalibratorUniqueId != Calibrator->GetUniqueID())
		{
			ErrorMessage = LOCTEXT("WrongCalibrator", "All rows must belong to the same calibrator");
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

			return false;
		}
	}

	const TSharedPtr<FCalibrationRowData>& LastRow = CalibrationRows[CalibrationRows.Num() - 1];
	check(LastRow.IsValid());

	// Verify that calibrator did not move much for all the samples
	if (CalibratorMovedInAnyRow(CalibrationRows))
	{
		ErrorMessage = LOCTEXT("CalibratorMoved", "The calibrator moved during the calibration");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	// Group Rows by camera poses.
	TArray<TSharedPtr<TArray<TSharedPtr<FCalibrationRowData>>>> SamePoseRowGroups;
	GroupRowsByCameraPose(SamePoseRowGroups, CalibrationRows);

	if (!SamePoseRowGroups.Num())
	{
		ErrorMessage = LOCTEXT("NotEnoughRows", "Not enough calibration rows. Please add more samples and try again.");
		return false;
	}

	TArray<FSinglePoseResult> SinglePoseResults;
	SinglePoseResults.Reserve(SamePoseRowGroups.Num());

	// Solve each group independently
	for (const auto& SamePoseRowGroup : SamePoseRowGroups)
	{
		FSinglePoseResult SinglePoseResult;

		const bool bSucceeded = CalcCalibratorPoseForSingleCamPose(*SamePoseRowGroup, SinglePoseResult.Transform, ErrorMessage);

		if (!bSucceeded)
		{
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

			return false;
		}

		SinglePoseResult.NumSamples = SamePoseRowGroup->Num();
		SinglePoseResults.Add(SinglePoseResult);
	}

	if (!SinglePoseResults.Num())
	{
		ErrorMessage = LOCTEXT("NoSinglePoseResults",
			"There were no valid single pose results. See Output Log for additional details.");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	FTransform DesiredCalibratorPose;

	if (!AverageSinglePoseResults(SinglePoseResults, DesiredCalibratorPose))
	{
		ErrorMessage = LOCTEXT("CouldNotAverageSinglePoseResults",
			"There was an error when averaging the single pose results");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	// apply the new calibrator transform
	Calibrator->Modify();
	Calibrator->SetActorTransform(DesiredCalibratorPose);

	// Since the offset was applied, there is no further use for the current samples.
	ClearCalibrationRows();

	return true;
}

bool UCameraNodalOffsetAlgoPoints::CalcTrackingOriginPoseForSingleCamPose(
	const TArray<TSharedPtr<FCalibrationRowData>>& Rows, 
	FTransform& OutTransform, 
	FText& OutErrorMessage)
{
	// Here we are assuming that the camera parent is the tracking origin.

	// Get the desired camera component world pose

	FTransform DesiredCameraPose;

	if (!CalculatedOptimalCameraComponentPose(DesiredCameraPose, Rows, OutErrorMessage))
	{
		return false;
	}

	check(Rows.Num()); // Must be non-zero if CalculatedOptimalCameraComponentPose succeeded.

	const TSharedPtr<FCalibrationRowData>& LastRow = Rows[Rows.Num() - 1];
	check(LastRow.IsValid());

	// calculate the new parent transform

	// CameraPose = RelativeCameraPose * ParentPose
	// => RelativeCameraPose = CameraPose * ParentPose'
	// 
	// DesiredCameraPose = RelativeCameraPose * DesiredParentPose
	// => DesiredParentPose = RelativeCameraPose' * DesiredCameraPose
	// => DesiredParentPose = (CameraPose * ParentPose')' * DesiredCameraPose
	// => DesiredParentPose = ParentPose * CameraPose' * DesiredCameraPose

	OutTransform = LastRow->CameraData.ParentPose * LastRow->CameraData.Pose.Inverse() * DesiredCameraPose;

	return true;
}


bool UCameraNodalOffsetAlgoPoints::ApplyNodalOffsetToTrackingOrigin()
{
	using namespace CameraNodalOffsetAlgoPoints;

	// Here we are assuming that the camera parent is the tracking origin.

	// get camera

	const FCameraCalibrationStepsController* StepsController;
	const ULensFile* LensFile;

	const FText TitleError = LOCTEXT("CalibrationError", "CalibrationError");
	FText ErrorMessage;

	if (!ensure(GetStepsControllerAndLensFile(&StepsController, &LensFile)))
	{
		ErrorMessage = LOCTEXT("ToolNotFound", "Tool not found");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	const ACameraActor* Camera = StepsController->GetCamera();

	if (!Camera)
	{
		ErrorMessage = LOCTEXT("CameraNotFound", "Camera Not Found");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	// Get the parent transform

	AActor* ParentActor = Camera->GetAttachParentActor();

	if (!ParentActor)
	{
		ErrorMessage = LOCTEXT("CameraParentNotFound", "Camera Parent not found");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	if (!CalibrationRows.Num())
	{
		ErrorMessage = LOCTEXT("NotEnoughSamples", "Not Enough Samples");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	const TSharedPtr<FCalibrationRowData>& LastRow = CalibrationRows[CalibrationRows.Num() - 1];
	check(LastRow.IsValid());

	if (LastRow->CameraData.ParentUniqueId != ParentActor->GetUniqueID())
	{
		ErrorMessage = LOCTEXT("ParentChanged", "Parent changed");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	// Group Rows by camera poses.
	TArray<TSharedPtr<TArray<TSharedPtr<FCalibrationRowData>>>> SamePoseRowGroups;
	GroupRowsByCameraPose(SamePoseRowGroups, CalibrationRows);

	TArray<FSinglePoseResult> SinglePoseResults;
	SinglePoseResults.Reserve(SamePoseRowGroups.Num());

	// Solve each group independently
	for (const auto& SamePoseRowGroup : SamePoseRowGroups)
	{
		FSinglePoseResult SinglePoseResult;

		const bool bSucceeded = CalcTrackingOriginPoseForSingleCamPose(*SamePoseRowGroup, SinglePoseResult.Transform, ErrorMessage);

		if (!bSucceeded)
		{
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

			return false;
		}

		SinglePoseResult.NumSamples = SamePoseRowGroup->Num();
		SinglePoseResults.Add(SinglePoseResult);
	}

	if (!SinglePoseResults.Num())
	{
		ErrorMessage = LOCTEXT("NoSinglePoseResults",
			"There were no valid single pose results. See Output Log for additional details.");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	FTransform DesiredParentPose;

	if (!AverageSinglePoseResults(SinglePoseResults, DesiredParentPose))
	{
		ErrorMessage = LOCTEXT("CouldNotAverageSinglePoseResults",
			"There was an error when averaging the single pose results");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	// apply the new parent transform
	ParentActor->Modify();
	ParentActor->SetActorTransform(DesiredParentPose);

	// Since the offset was applied, there is no further use for the current samples.
	ClearCalibrationRows();

	return true;
}

bool UCameraNodalOffsetAlgoPoints::CalcCalibratorPoseForSingleCamPose(
	const TArray<TSharedPtr<FCalibrationRowData>>& Rows,
	FTransform& OutTransform,
	FText& OutErrorMessage)
{
	FTransform DesiredCameraPose;

	if (!CalculatedOptimalCameraComponentPose(DesiredCameraPose, Rows, OutErrorMessage))
	{
		return false;
	}

	check(Rows.Num());

	const TSharedPtr<FCalibrationRowData>& LastRow = Rows[Rows.Num() - 1];
	check(LastRow.IsValid());

	// Calculate the offset
	// 
	// Calibrator = DesiredCalibratorToCamera * DesiredCamera
	// => DesiredCalibratorToCamera = Calibrator * DesiredCamera'
	// 
	// DesiredCalibrator = DesiredCalibratorToCamera * Camera
	// => DesiredCalibrator = Calibrator * DesiredCamera' * Camera

	OutTransform = LastRow->CameraData.CalibratorPose * DesiredCameraPose.Inverse() * LastRow->CameraData.Pose;

	return true;
}

bool UCameraNodalOffsetAlgoPoints::ApplyNodalOffsetToCalibratorParent()
{
	using namespace CameraNodalOffsetAlgoPoints;

	// Get the desired camera component world pose

	FText ErrorMessage;
	const FText TitleError = LOCTEXT("CalibrationError", "CalibrationError");

	// Get the calibrator

	if (!Calibrator.IsValid())
	{
		ErrorMessage = LOCTEXT("MissingCalibrator", "Missing Calibrator");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	// Get the parent

	AActor* ParentActor = Calibrator->GetAttachParentActor();

	if (!ParentActor)
	{
		ErrorMessage = LOCTEXT("CalibratorParentNotFound", "Calibrator Parent not found");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	// All calibration points should correspond to the same calibrator and calibrator parent

	for (const TSharedPtr<FCalibrationRowData>& Row : CalibrationRows)
	{
		check(Row.IsValid());

		if (Row->CameraData.CalibratorUniqueId != Calibrator->GetUniqueID())
		{
			ErrorMessage = LOCTEXT("WrongCalibrator", "All rows must belong to the same calibrator");
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

			return false;
		}

		if (Row->CameraData.CalibratorParentUniqueId != ParentActor->GetUniqueID())
		{
			ErrorMessage = LOCTEXT("WrongCalibrator", "All rows must belong to the same calibrator parent");
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

			return false;
		}
	}

	// Verify that calibrator did not move much for all the samples
	if (CalibratorMovedInAnyRow(CalibrationRows))
	{
		ErrorMessage = LOCTEXT("CalibratorMoved", "The calibrator moved during the calibration");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	// Group Rows by camera poses.
	TArray<TSharedPtr<TArray<TSharedPtr<FCalibrationRowData>>>> SamePoseRowGroups;
	GroupRowsByCameraPose(SamePoseRowGroups, CalibrationRows);

	if (!SamePoseRowGroups.Num())
	{
		ErrorMessage = LOCTEXT("NotEnoughRows", "Not enough calibration rows. Please add more samples and try again.");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	TArray<FSinglePoseResult> SinglePoseResults;
	SinglePoseResults.Reserve(SamePoseRowGroups.Num());

	// Solve each group independently
	for (const auto& SamePoseRowGroup : SamePoseRowGroups)
	{
		FSinglePoseResult SinglePoseResult;

		const bool bSucceeded = CalcCalibratorPoseForSingleCamPose(*SamePoseRowGroup, SinglePoseResult.Transform, ErrorMessage);

		if (!bSucceeded)
		{
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

			return false;
		}

		SinglePoseResult.NumSamples = SamePoseRowGroup->Num();
		SinglePoseResults.Add(SinglePoseResult);
	}

	if (!SinglePoseResults.Num())
	{
		ErrorMessage = LOCTEXT("NoSinglePoseResults",
			"There were no valid single pose results. See Output Log for additional details.");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	FTransform DesiredCalibratorPose;

	if (!AverageSinglePoseResults(SinglePoseResults, DesiredCalibratorPose))
	{
		ErrorMessage = LOCTEXT("CouldNotAverageSinglePoseResults",
			"There was an error when averaging the single pose results");
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, &TitleError);

		return false;
	}

	const TSharedPtr<FCalibrationRowData>& LastRow = CalibrationRows[CalibrationRows.Num() - 1];
	check(LastRow.IsValid());

	// Apply the new calibrator parent transform
	ParentActor->Modify();
	ParentActor->SetActorTransform(LastRow->CameraData.CalibratorParentPose * LastRow->CameraData.CalibratorPose.Inverse() * DesiredCalibratorPose);

	// Since the offset was applied, there is no further use for the current samples.
	ClearCalibrationRows();

	return true;
}

TSharedRef<SWidget> UCameraNodalOffsetAlgoPoints::BuildCalibrationPointsComboBox()
{
	CalibratorPointsComboBox = SNew(SComboBox<TSharedPtr<FCalibratorPointData>>)
		.OptionsSource(&CurrentCalibratorPoints)
		.OnGenerateWidget_Lambda([&](TSharedPtr<FCalibratorPointData> InOption) -> TSharedRef<SWidget>
		{
			return SNew(STextBlock).Text(FText::FromString(*InOption->Name));
		})
		.InitiallySelectedItem(nullptr)
		[
			SNew(STextBlock)
			.Text_Lambda([&]() -> FText
			{
				if (CalibratorPointsComboBox.IsValid() && CalibratorPointsComboBox->GetSelectedItem().IsValid())
				{
					return FText::FromString(CalibratorPointsComboBox->GetSelectedItem()->Name);
				}

				return LOCTEXT("InvalidComboOption", "None");
			})
		];

	// Update combobox from current calibrator
	SetCalibrator(GetCalibrator());

	return CalibratorPointsComboBox.ToSharedRef();
}

TSharedRef<SWidget> UCameraNodalOffsetAlgoPoints::BuildCalibrationPointsTable()
{
	CalibrationListView = SNew(SListView<TSharedPtr<FCalibrationRowData>>)
		.ItemHeight(24)
		.ListItemsSource(&CalibrationRows)
		.OnGenerateRow_Lambda([&](TSharedPtr<FCalibrationRowData> InItem, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow>
		{
			return SNew(CameraNodalOffsetAlgoPoints::SCalibrationRowGenerator, OwnerTable).CalibrationRowData(InItem);
		})
		.SelectionMode(ESelectionMode::Multi)
		.OnKeyDownHandler_Lambda([&](const FGeometry& Geometry, const FKeyEvent& KeyEvent) -> FReply
		{
			if (!CalibrationListView.IsValid())
			{
				return FReply::Unhandled();
			}

			if (KeyEvent.GetKey() == EKeys::Delete)
			{
				// Delete selected items

				const TArray<TSharedPtr<FCalibrationRowData>> SelectedItems = CalibrationListView->GetSelectedItems();

				for (const TSharedPtr<FCalibrationRowData>& SelectedItem : SelectedItems)
				{
					CalibrationRows.Remove(SelectedItem);
				}

				CalibrationListView->RequestListRefresh();
				return FReply::Handled();
			}
			else if (KeyEvent.GetModifierKeys().IsControlDown() && (KeyEvent.GetKey() == EKeys::A))
			{
				// Select all items

				CalibrationListView->SetItemSelection(CalibrationRows, true);
				return FReply::Handled();
			}
			else if (KeyEvent.GetKey() == EKeys::Escape)
			{
				// Deselect all items

				CalibrationListView->ClearSelection();
				return FReply::Handled();
			}

			return FReply::Unhandled();
		})
		.HeaderRow
		(
			SNew(SHeaderRow)

			+ SHeaderRow::Column("Name")
			.DefaultLabel(LOCTEXT("Name", "Name"))

			+ SHeaderRow::Column("Point2D")
			.DefaultLabel(LOCTEXT("Point2D", "Point2D"))

			+ SHeaderRow::Column("Point3D")
			.DefaultLabel(LOCTEXT("Point3D", "Point3D"))
		);

	return CalibrationListView.ToSharedRef();
}

AActor* UCameraNodalOffsetAlgoPoints::FindFirstCalibrator() const
{
	// We find the first UCalibrationPointComponent object and return its actor owner.

	if (!NodalOffsetTool.IsValid())
	{
		return nullptr;
	}

	const FCameraCalibrationStepsController* StepsController = NodalOffsetTool->GetCameraCalibrationStepsController();

	if (!StepsController)
	{
		return nullptr;
	}

	const UWorld* World = StepsController->GetWorld();
	const EObjectFlags ExcludeFlags = RF_ClassDefaultObject; // We don't want the calibrator CDOs.

	for (TObjectIterator<UCalibrationPointComponent> It(ExcludeFlags, true, EInternalObjectFlags::PendingKill); It; ++It)
	{
		AActor* Actor = It->GetOwner();

		if (Actor && (Actor->GetWorld() == World))
		{
			return Actor;
		}
	}

	return nullptr;
}

bool UCameraNodalOffsetAlgoPoints::CalibratorPointCacheFromName(const FString& Name, FCalibratorPointCache& CalibratorPointCache) const
{
	CalibratorPointCache.bIsValid = false;

	if (!Calibrator.IsValid())
	{
		return false;
	}

	TArray<UCalibrationPointComponent*, TInlineAllocator<4>> CalibrationPoints;
	Calibrator->GetComponents<UCalibrationPointComponent, TInlineAllocator<4>>(CalibrationPoints);

	for (const UCalibrationPointComponent* CalibrationPoint : CalibrationPoints)
	{
		if (CalibrationPoint->GetWorldLocation(Name, CalibratorPointCache.Location))
		{
			CalibratorPointCache.bIsValid = true;
			CalibratorPointCache.Name = Name;
			return true;
		}
	}

	return false;
}

void UCameraNodalOffsetAlgoPoints::SetCalibrator(AActor* InCalibrator)
{
	Calibrator = InCalibrator;

	// Update the list of points

	CurrentCalibratorPoints.Empty();

	if (!Calibrator.IsValid())
	{
		return;
	}

	TArray<UCalibrationPointComponent*, TInlineAllocator<4>> CalibrationPoints;
	Calibrator->GetComponents<UCalibrationPointComponent, TInlineAllocator<4>>(CalibrationPoints);

	for (const UCalibrationPointComponent* CalibrationPoint : CalibrationPoints)
	{
		TArray<FString> PointNames;

		CalibrationPoint->GetNamespacedPointNames(PointNames);

		for (FString& PointName : PointNames)
		{
			CurrentCalibratorPoints.Add(MakeShared<FCalibratorPointData>(PointName));
		}
	}

	// Notify combobox

	if (!CalibratorPointsComboBox)
	{
		return;
	}

	CalibratorPointsComboBox->RefreshOptions();

	if (CurrentCalibratorPoints.Num())
	{
		CalibratorPointsComboBox->SetSelectedItem(CurrentCalibratorPoints[0]);
	}
	else
	{
		CalibratorPointsComboBox->SetSelectedItem(nullptr);
	}
}

AActor* UCameraNodalOffsetAlgoPoints::GetCalibrator() const
{
	return Calibrator.Get();
}

void UCameraNodalOffsetAlgoPoints::OnSavedNodalOffset()
{
	// Since the nodal point was saved, there is no further use for the current samples.
	ClearCalibrationRows();
}

void UCameraNodalOffsetAlgoPoints::ClearCalibrationRows()
{
	CalibrationRows.Empty();

	if (CalibrationListView.IsValid())
	{
		CalibrationListView->RequestListRefresh();
	}
}

bool UCameraNodalOffsetAlgoPoints::GetStepsControllerAndLensFile(
	const FCameraCalibrationStepsController** OutStepsController,
	const ULensFile** OutLensFile) const
{
	if (!NodalOffsetTool.IsValid())
	{
		return false;
	}

	if (OutStepsController)
	{
		*OutStepsController = NodalOffsetTool->GetCameraCalibrationStepsController();

		if (!*OutStepsController)
		{
			return false;
		}
	}

	if (OutLensFile)
	{
		if (!OutStepsController)
		{
			return false;
		}

		*OutLensFile = (*OutStepsController)->GetLensFile();

		if (!*OutLensFile)
		{
			return false;
		}
	}

	return true;
}

void UCameraNodalOffsetAlgoPoints::GroupRowsByCameraPose(
	TArray<TSharedPtr<TArray<TSharedPtr<FCalibrationRowData>>>>& OutSamePoseRowGroups,
	const TArray<TSharedPtr<FCalibrationRowData>>& Rows) const
{
	for (const TSharedPtr<FCalibrationRowData>& Row : Rows)
	{
		check(Row.IsValid());

		// Find the group it belongs to based on transform
		TSharedPtr<TArray<TSharedPtr<FCalibrationRowData>>> ClosestGroup;

		for (const auto& SamePoseRowGroup : OutSamePoseRowGroups)
		{
			if (FCameraCalibrationUtils::IsNearlyEqual(Row->CameraData.Pose, (*SamePoseRowGroup)[0]->CameraData.Pose))
			{
				ClosestGroup = SamePoseRowGroup;
				break;
			}
		}

		if (!ClosestGroup.IsValid())
		{
			ClosestGroup = MakeShared<TArray<TSharedPtr<FCalibrationRowData>>>();
			OutSamePoseRowGroups.Add(ClosestGroup);
		}

		ClosestGroup->Add(Row);
	}
}

TSharedRef<SWidget> UCameraNodalOffsetAlgoPoints::BuildHelpWidget()
{
	return SNew(STextBlock)
		.Text(LOCTEXT("NodalOffsetAlgoPointsHelp",
			"This nodal offset algorithm will estimate the camera pose by minimizing the reprojection\n"
			"error of a set of 3d points.\n\n"

			"The 3d points are taken from the calibrator object, which you need to select using the\n"
			"provided picker. All that is required is that the object contains one or more 'Calibration\n'"
			"Point Components'. These 3d calibration points will appear in the provided drop-down.\n\n"

			"To build the table that correlates these 3d points with where they are in the media plate,\n"
			"simply click on the viewport, as accurately as possible, where their physical counterpart\n"
			"appears. You can right-click the viewport to pause it if it helps in accuracy.\n\n"

			"Once the table is built, the algorithm will calculate where the camera must be so that\n"
			"the projection of these 3d points onto the camera plane are as close as possible to their\n"
			"actual 2d location that specified by clicking on the viewport.\n\n"

			"This camera pose information can then be used in the following ways:\n\n"

			"- To calculate the offset between where it currently is and where it should be. This offset\n"
			"  will be added to the lens file when 'Add To Nodal Offset Calibration' is clicked, and will\n"
			"  ultimately be applied to the tracking data so that the camera's position in the CG scene\n"
			"  is accurate. This requires that the position of the calibrator is accurate with respect to\n"
			"  the camera tracking system.\n\n"

			"- To place the calibrator actor, and any actors parented to it, in such a way that it coincides\n"
			"  with its physical counterpart as seen by both the live action camera and the virtual camera.\n"
			"  In this case, it is not required that the calibrator is tracked, and its pose will be\n"
			"  altered directly. In this case, the lens file is not modified, and requires that the camera\n"
			"  nodal offset (i.e. no parallax point) is already calibrated.\n\n"

			"- The same as above, but by offsetting the calibrator's parent. In this case, it is implied\n"
			"  that we are adjusting the calibrator's tracking system origin.\n\n"

			"- Similarly as above, but by offsetting the camera's parent. The camera lens file is not\n"
			"  changed, and it is implied that we are calibrating the camera tracking system origin.\n\n"

			"Notes:\n\n"
			" - This calibration step relies on the camera having a lens distortion calibration.\n"
			" - It requires the camera to not move much from the moment you capture the first\n"
			"   point until you capture the last one.\n"
		));
}

#undef LOCTEXT_NAMESPACE
