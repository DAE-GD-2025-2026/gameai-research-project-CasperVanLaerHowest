#include "LSystemControlWidget.h"

#include "../2d/TwoDLSystem.h"
#include "Components/Button.h"
#include "Components/SpinBox.h"
#include "Kismet/GameplayStatics.h"

void ULSystemControlWidget::NativeConstruct()
{
	Super::NativeConstruct();

	FindLSystemIfNeeded();
	SynchronizeValues();

	if (AngleSpinBox)
	{
		AngleSpinBox->OnValueChanged.AddUniqueDynamic(
			this, &ULSystemControlWidget::HandleAngleChanged);
	}

	if (GenerationSpinBox)
	{
		GenerationSpinBox->OnValueChanged.AddUniqueDynamic(
			this, &ULSystemControlWidget::HandleGenerationChanged);
	}

	if (SegmentLengthSpinBox)
	{
		SegmentLengthSpinBox->OnValueChanged.AddUniqueDynamic(
			this, &ULSystemControlWidget::HandleSegmentLengthChanged);
	}

	if (RegenerateButton)
	{
		RegenerateButton->OnClicked.AddUniqueDynamic(
			this, &ULSystemControlWidget::HandleRegenerateClicked);
	}
}

void ULSystemControlWidget::SetLSystem(ATwoDLSystem* InLSystem)
{
	LSystem = InLSystem;
	SynchronizeValues();
}

void ULSystemControlWidget::FindLSystemIfNeeded()
{
	if (!LSystem)
	{
		LSystem = Cast<ATwoDLSystem>(UGameplayStatics::GetActorOfClass(
			this, ATwoDLSystem::StaticClass()));
	}
}

void ULSystemControlWidget::SynchronizeValues()
{
	if (!LSystem)
	{
		return;
	}

	bSynchronizingValues = true;

	if (AngleSpinBox)
	{
		AngleSpinBox->SetValue(LSystem->GetTurnAngle());
	}

	if (GenerationSpinBox)
	{
		GenerationSpinBox->SetValue(LSystem->GetRedoCount());
	}

	if (SegmentLengthSpinBox)
	{
		SegmentLengthSpinBox->SetValue(LSystem->GetSegmentLength());
	}

	bSynchronizingValues = false;
}

void ULSystemControlWidget::HandleAngleChanged(float Value)
{
	if (LSystem && !bSynchronizingValues)
	{
		LSystem->SetTurnAngle(Value);
	}
}

void ULSystemControlWidget::HandleGenerationChanged(float Value)
{
	if (LSystem && !bSynchronizingValues)
	{
		LSystem->SetRedoCount(FMath::RoundToInt(Value));
	}
}

void ULSystemControlWidget::HandleSegmentLengthChanged(float Value)
{
	if (LSystem && !bSynchronizingValues)
	{
		LSystem->SetSegmentLength(Value);
	}
}

void ULSystemControlWidget::HandleRegenerateClicked()
{
	if (LSystem)
	{
		LSystem->RerunAllGen();
	}
}
