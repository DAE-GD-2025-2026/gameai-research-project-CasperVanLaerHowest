// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "ThreeDLSystem.generated.h"

class ACameraActor;
class UInstancedStaticMeshComponent;
class UStaticMesh;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnThreeDLSystemRegenerated);

UENUM(BlueprintType)
enum class EThreeDLSystemPreset : uint8
{
	Tree UMETA(DisplayName = "Tree"),
	Bush UMETA(DisplayName = "Bush"),
	Coral UMETA(DisplayName = "Coral")
};

USTRUCT(BlueprintType)
struct FRandomRewriteOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Rewrite")
	FString Replacement;

	// Relative probability. The options do not need to add up to 1.0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Rewrite", meta = (ClampMin = "0.0"))
	float Chance = 1.0f;
};

USTRUCT(BlueprintType)
struct FRandomRewriteRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Rewrite")
	FString Symbol;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random Rewrite")
	TArray<FRandomRewriteOption> Options;
};

UCLASS()
class LSYSTEMS_API AThreeDLSystem : public AActor
{
	GENERATED_BODY()

public:
	AThreeDLSystem();

	UPROPERTY(BlueprintAssignable, Category = "L-System|Statistics")
	FOnThreeDLSystemRegenerated OnRegenerated;

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "L-System")
	void RerunAllGen();

	UFUNCTION(BlueprintCallable, Category = "L-System|Settings")
	void SetTurnAngle(float NewAngle);

	UFUNCTION(BlueprintCallable, Category = "L-System|Settings")
	void SetSegmentLength(float NewLength);

	UFUNCTION(BlueprintCallable, Category = "L-System|Settings")
	void SetRedoCount(int32 NewCount);

	UFUNCTION(BlueprintCallable, Category = "L-System|Settings")
	void SetStartWidth(float NewWidth);

	UFUNCTION(BlueprintCallable, Category = "L-System|Settings")
	void SetWidthMultiplier(float NewMultiplier);

	UFUNCTION(BlueprintCallable, Category = "L-System|Settings")
	void SetRandomSeed(int32 NewSeed);

	UFUNCTION(BlueprintCallable, Category = "L-System|Settings")
	void SetAngleVariationPercent(float NewVariationPercent);

	UFUNCTION(BlueprintCallable, Category = "L-System|Leaves")
	void SetLeavesEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "L-System|Leaves")
	void SetLeafScale(float NewScale);

	UFUNCTION(BlueprintCallable, Category = "L-System|Presets")
	void ApplyPreset(EThreeDLSystemPreset NewPreset);

	UFUNCTION(BlueprintCallable, Category = "L-System|Camera")
	void SetCameraOrbitEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "L-System|Camera")
	void SetCameraOrbitSpeed(float NewSpeed);

	UFUNCTION(BlueprintCallable, Category = "L-System|Camera")
	void SetCameraOrbitDistance(float NewDistance);

	float GetTurnAngle() const { return TurnAngle; }
	float GetSegmentLength() const { return SegmentLength; }
	int32 GetRedoCount() const { return m_Redos; }
	float GetStartWidth() const { return StartWidth; }
	float GetWidthMultiplier() const { return WidthMultiplier; }
	int32 GetRandomSeed() const { return RandomSeed; }
	float GetAngleVariationPercent() const { return AngleVariationPercent; }
	bool AreLeavesEnabled() const { return bGenerateLeaves; }
	float GetLeafScale() const { return LeafScale; }
	EThreeDLSystemPreset GetCurrentPreset() const { return CurrentPreset; }
	bool IsCameraOrbitEnabled() const { return bOrbitCamera; }
	float GetCameraOrbitSpeed() const { return CameraOrbitSpeed; }
	float GetCameraOrbitDistance() const { return CameraOrbitDistance; }

	UFUNCTION(BlueprintPure, Category = "L-System|Statistics")
	FString GetCurrentGrammar() const { return m_Axiom; }

	UFUNCTION(BlueprintPure, Category = "L-System|Statistics")
	int32 GetSymbolCount() const { return m_Axiom.Len(); }

	UFUNCTION(BlueprintPure, Category = "L-System|Statistics")
	int32 GetTriangleCount() const { return LastTriangleCount; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L-System")
	TObjectPtr<UProceduralMeshComponent> BranchMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L-System|Leaves")
	TObjectPtr<UInstancedStaticMeshComponent> LeafInstances;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System")
	TMap<FString, FString> RewriteRules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System")
	FString m_StartAxiom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System")
	uint8 m_Redos{3};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Drawing")
	float SegmentLength = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Drawing")
	float TurnAngle = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Drawing")
	float StartWidth = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Drawing")
	float WidthMultiplier = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Drawing")
	float MinWidth = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Drawing")
	int32 BranchSides = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Drawing")
	bool bCreateCollision = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Drawing", meta = (DisplayName = "Branch Material"))
	TObjectPtr<UMaterialInterface> BranchMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Leaves")
	bool bGenerateLeaves = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Leaves")
	TObjectPtr<UStaticMesh> LeafMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Leaves")
	TObjectPtr<UMaterialInterface> LeafMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Leaves", meta = (ClampMin = "0.01"))
	float LeafScale = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Leaves")
	FRotator LeafRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Random")
	int32 RandomSeed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Random", meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "Percent"))
	float AngleVariationPercent = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Random")
	TArray<FRandomRewriteRule> RandomRewriteRules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Presets")
	EThreeDLSystemPreset StartingPreset = EThreeDLSystemPreset::Bush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Presets")
	bool bApplyStartingPresetOnBeginPlay = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L-System|Presets")
	EThreeDLSystemPreset CurrentPreset = EThreeDLSystemPreset::Bush;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "L-System|Camera")
	TObjectPtr<ACameraActor> OrbitCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Camera")
	bool bOrbitCamera = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Camera", meta = (Units = "deg/s"))
	float CameraOrbitSpeed = 20.0f;

	// A value of 0 uses the placed camera's horizontal distance when play starts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Camera", meta = (ClampMin = "0.0", Units = "cm", DisplayName = "Orbit Distance"))
	float CameraOrbitDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Camera")
	float CameraTargetHeight = 150.0f;

	UFUNCTION(BlueprintCallable, Category = "L-System")
	void RunGeneration(FString& CurrentString);

	UFUNCTION(BlueprintCallable, Category = "L-System")
	void DrawAxiom();

private:
	void InitializeCameraOrbit();
	void UpdateOrbitCameraTransform();
	void ConfigurePreset(EThreeDLSystemPreset NewPreset);
	bool TryGetRandomReplacement(const FString& Symbol, FString& OutReplacement);
	float GetRandomTurnAngle();

	void AddBranchCylinder(
		const FVector& Start,
		const FVector& End,
		float StartRadius,
		float EndRadius,
		TArray<FVector>& Vertices,
		TArray<int32>& Triangles,
		TArray<FVector>& Normals,
		TArray<FVector2D>& UVs,
		TArray<FLinearColor>& VertexColors,
		TArray<FProcMeshTangent>& Tangents) const;

	UPROPERTY()
	FString m_Axiom{TEXT("X")};

	UPROPERTY(VisibleAnywhere, Category = "L-System|Statistics")
	int32 LastTriangleCount = 0;

	float CameraOrbitAngleRadians = 0.0f;
	float CameraOrbitHeight = 0.0f;
	FRandomStream GenerationRandomStream;
	FRandomStream AngleRandomStream;
};
