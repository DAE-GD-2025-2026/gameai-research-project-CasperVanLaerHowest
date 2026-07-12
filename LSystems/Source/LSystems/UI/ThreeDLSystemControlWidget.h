#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ThreeDLSystemControlWidget.generated.h"

class AThreeDLSystem;
class UButton;
class UCheckBox;
class USpinBox;

UCLASS()
class LSYSTEMS_API UThreeDLSystemControlWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "L-System")
	void SetLSystem(AThreeDLSystem* InLSystem);

protected:
	virtual void NativeConstruct() override;

private:
	void FindLSystemIfNeeded();
	void SynchronizeValues();

	UFUNCTION() void HandleAngleChanged(float Value);
	UFUNCTION() void HandleGenerationChanged(float Value);
	UFUNCTION() void HandleSegmentLengthChanged(float Value);
	UFUNCTION() void HandleStartWidthChanged(float Value);
	UFUNCTION() void HandleWidthMultiplierChanged(float Value);
	UFUNCTION() void HandleRandomSeedChanged(float Value);
	UFUNCTION() void HandleOrbitChanged(bool bIsChecked);
	UFUNCTION() void HandleOrbitSpeedChanged(float Value);
	UFUNCTION() void HandleRegenerateClicked();

	UPROPERTY(BlueprintReadOnly, Category = "L-System", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AThreeDLSystem> LSystem;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USpinBox> AngleSpinBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USpinBox> GenerationSpinBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USpinBox> SegmentLengthSpinBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USpinBox> StartWidthSpinBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USpinBox> WidthMultiplierSpinBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USpinBox> RandomSeedSpinBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCheckBox> OrbitCameraCheckBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USpinBox> OrbitSpeedSpinBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> RegenerateButton;

	bool bSynchronizingValues = false;
};
