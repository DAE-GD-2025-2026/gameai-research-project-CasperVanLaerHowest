#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LSystemControlWidget.generated.h"

class ATwoDLSystem;
class UButton;
class USpinBox;

UCLASS()
class LSYSTEMS_API ULSystemControlWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "L-System")
	void SetLSystem(ATwoDLSystem* InLSystem);

protected:
	virtual void NativeConstruct() override;

private:
	void FindLSystemIfNeeded();
	void SynchronizeValues();

	UFUNCTION()
	void HandleAngleChanged(float Value);

	UFUNCTION()
	void HandleGenerationChanged(float Value);

	UFUNCTION()
	void HandleSegmentLengthChanged(float Value);

	UFUNCTION()
	void HandleRegenerateClicked();

	UPROPERTY(BlueprintReadOnly, Category = "L-System", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ATwoDLSystem> LSystem;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USpinBox> AngleSpinBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USpinBox> GenerationSpinBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USpinBox> SegmentLengthSpinBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RegenerateButton;

	bool bSynchronizingValues = false;
};
