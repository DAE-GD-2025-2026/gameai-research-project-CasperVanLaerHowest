#include "ThreeDLSystemControlWidget.h"

#include "../3d/ThreeDLSystem.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

void UThreeDLSystemControlWidget::NativeConstruct()
{
	Super::NativeConstruct();

	FindLSystemIfNeeded();
	if (LSystem)
	{
		LSystem->OnRegenerated.AddUniqueDynamic(
			this, &UThreeDLSystemControlWidget::SynchronizeStatistics);
	}
	SynchronizeValues();

	if (AngleSpinBox) AngleSpinBox->OnValueChanged.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleAngleChanged);
	if (GenerationSpinBox) GenerationSpinBox->OnValueChanged.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleGenerationChanged);
	if (SegmentLengthSpinBox) SegmentLengthSpinBox->OnValueChanged.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleSegmentLengthChanged);
	if (StartWidthSpinBox) StartWidthSpinBox->OnValueChanged.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleStartWidthChanged);
	if (WidthMultiplierSpinBox) WidthMultiplierSpinBox->OnValueChanged.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleWidthMultiplierChanged);
	if (RandomSeedSpinBox) RandomSeedSpinBox->OnValueChanged.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleRandomSeedChanged);
	if (AngleVariationSpinBox) AngleVariationSpinBox->OnValueChanged.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleAngleVariationChanged);
	if (LeavesCheckBox) LeavesCheckBox->OnCheckStateChanged.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleLeavesChanged);
	if (LeafScaleSpinBox) LeafScaleSpinBox->OnValueChanged.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleLeafScaleChanged);
	if (OrbitCameraCheckBox) OrbitCameraCheckBox->OnCheckStateChanged.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleOrbitChanged);
	if (OrbitSpeedSpinBox) OrbitSpeedSpinBox->OnValueChanged.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleOrbitSpeedChanged);
	if (OrbitDistanceSpinBox) OrbitDistanceSpinBox->OnValueChanged.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleOrbitDistanceChanged);
	if (TreePresetButton) TreePresetButton->OnClicked.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleTreePresetClicked);
	if (BushPresetButton) BushPresetButton->OnClicked.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleBushPresetClicked);
	if (CoralPresetButton) CoralPresetButton->OnClicked.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleCoralPresetClicked);
	if (RegenerateButton) RegenerateButton->OnClicked.AddUniqueDynamic(this, &UThreeDLSystemControlWidget::HandleRegenerateClicked);
}

void UThreeDLSystemControlWidget::NativeDestruct()
{
	if (LSystem)
	{
		LSystem->OnRegenerated.RemoveDynamic(
			this, &UThreeDLSystemControlWidget::SynchronizeStatistics);
	}

	Super::NativeDestruct();
}

void UThreeDLSystemControlWidget::SetLSystem(AThreeDLSystem* InLSystem)
{
	if (LSystem)
	{
		LSystem->OnRegenerated.RemoveDynamic(
			this, &UThreeDLSystemControlWidget::SynchronizeStatistics);
	}

	LSystem = InLSystem;
	if (LSystem)
	{
		LSystem->OnRegenerated.AddUniqueDynamic(
			this, &UThreeDLSystemControlWidget::SynchronizeStatistics);
	}
	SynchronizeValues();
}

void UThreeDLSystemControlWidget::FindLSystemIfNeeded()
{
	if (!LSystem)
	{
		LSystem = Cast<AThreeDLSystem>(UGameplayStatics::GetActorOfClass(this, AThreeDLSystem::StaticClass()));
	}
}

void UThreeDLSystemControlWidget::SynchronizeValues()
{
	if (!LSystem) return;

	bSynchronizingValues = true;
	if (AngleSpinBox) AngleSpinBox->SetValue(LSystem->GetTurnAngle());
	if (GenerationSpinBox) GenerationSpinBox->SetValue(LSystem->GetRedoCount());
	if (SegmentLengthSpinBox) SegmentLengthSpinBox->SetValue(LSystem->GetSegmentLength());
	if (StartWidthSpinBox) StartWidthSpinBox->SetValue(LSystem->GetStartWidth());
	if (WidthMultiplierSpinBox) WidthMultiplierSpinBox->SetValue(LSystem->GetWidthMultiplier());
	if (RandomSeedSpinBox) RandomSeedSpinBox->SetValue(LSystem->GetRandomSeed());
	if (AngleVariationSpinBox) AngleVariationSpinBox->SetValue(LSystem->GetAngleVariationPercent());
	if (LeavesCheckBox) LeavesCheckBox->SetIsChecked(LSystem->AreLeavesEnabled());
	if (LeafScaleSpinBox) LeafScaleSpinBox->SetValue(LSystem->GetLeafScale());
	if (OrbitCameraCheckBox) OrbitCameraCheckBox->SetIsChecked(LSystem->IsCameraOrbitEnabled());
	if (OrbitSpeedSpinBox) OrbitSpeedSpinBox->SetValue(LSystem->GetCameraOrbitSpeed());
	if (OrbitDistanceSpinBox) OrbitDistanceSpinBox->SetValue(LSystem->GetCameraOrbitDistance());
	SynchronizeStatistics();
	bSynchronizingValues = false;
}

void UThreeDLSystemControlWidget::SynchronizeStatistics()
{
	if (!LSystem)
	{
		return;
	}

	if (GrammarTextBlock)
	{
		GrammarTextBlock->SetText(FText::FromString(LSystem->GetCurrentGrammar()));
	}
	if (SymbolCountTextBlock)
	{
		SymbolCountTextBlock->SetText(FText::AsNumber(LSystem->GetSymbolCount()));
	}
	if (TriangleCountTextBlock)
	{
		TriangleCountTextBlock->SetText(FText::AsNumber(LSystem->GetTriangleCount()));
	}
}

void UThreeDLSystemControlWidget::HandleAngleChanged(float Value) { if (LSystem && !bSynchronizingValues) { LSystem->SetTurnAngle(Value); SynchronizeStatistics(); } }
void UThreeDLSystemControlWidget::HandleGenerationChanged(float Value) { if (LSystem && !bSynchronizingValues) { LSystem->SetRedoCount(FMath::RoundToInt(Value)); SynchronizeStatistics(); } }
void UThreeDLSystemControlWidget::HandleSegmentLengthChanged(float Value) { if (LSystem && !bSynchronizingValues) { LSystem->SetSegmentLength(Value); SynchronizeStatistics(); } }
void UThreeDLSystemControlWidget::HandleStartWidthChanged(float Value) { if (LSystem && !bSynchronizingValues) { LSystem->SetStartWidth(Value); SynchronizeStatistics(); } }
void UThreeDLSystemControlWidget::HandleWidthMultiplierChanged(float Value) { if (LSystem && !bSynchronizingValues) { LSystem->SetWidthMultiplier(Value); SynchronizeStatistics(); } }
void UThreeDLSystemControlWidget::HandleRandomSeedChanged(float Value) { if (LSystem && !bSynchronizingValues) { LSystem->SetRandomSeed(FMath::RoundToInt(Value)); SynchronizeStatistics(); } }
void UThreeDLSystemControlWidget::HandleAngleVariationChanged(float Value) { if (LSystem && !bSynchronizingValues) { LSystem->SetAngleVariationPercent(Value); SynchronizeStatistics(); } }
void UThreeDLSystemControlWidget::HandleLeavesChanged(bool bIsChecked) { if (LSystem && !bSynchronizingValues) { LSystem->SetLeavesEnabled(bIsChecked); SynchronizeStatistics(); } }
void UThreeDLSystemControlWidget::HandleLeafScaleChanged(float Value) { if (LSystem && !bSynchronizingValues) { LSystem->SetLeafScale(Value); SynchronizeStatistics(); } }
void UThreeDLSystemControlWidget::HandleOrbitChanged(bool bIsChecked) { if (LSystem && !bSynchronizingValues) LSystem->SetCameraOrbitEnabled(bIsChecked); }
void UThreeDLSystemControlWidget::HandleOrbitSpeedChanged(float Value) { if (LSystem && !bSynchronizingValues) LSystem->SetCameraOrbitSpeed(Value); }
void UThreeDLSystemControlWidget::HandleOrbitDistanceChanged(float Value) { if (LSystem && !bSynchronizingValues) LSystem->SetCameraOrbitDistance(Value); }
void UThreeDLSystemControlWidget::HandleTreePresetClicked() { if (LSystem) { LSystem->ApplyPreset(EThreeDLSystemPreset::Tree); SynchronizeValues(); } }
void UThreeDLSystemControlWidget::HandleBushPresetClicked() { if (LSystem) { LSystem->ApplyPreset(EThreeDLSystemPreset::Bush); SynchronizeValues(); } }
void UThreeDLSystemControlWidget::HandleCoralPresetClicked() { if (LSystem) { LSystem->ApplyPreset(EThreeDLSystemPreset::Coral); SynchronizeValues(); } }
void UThreeDLSystemControlWidget::HandleRegenerateClicked() { if (LSystem) { LSystem->RerunAllGen(); SynchronizeStatistics(); } }
