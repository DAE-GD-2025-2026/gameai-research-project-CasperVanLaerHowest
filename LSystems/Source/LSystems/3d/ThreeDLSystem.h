// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "ThreeDLSystem.generated.h"

class ACameraActor;

UCLASS()
class LSYSTEMS_API AThreeDLSystem : public AActor
{
	GENERATED_BODY()

public:
	AThreeDLSystem();

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

	UFUNCTION(BlueprintCallable, Category = "L-System|Camera")
	void SetCameraOrbitEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "L-System|Camera")
	void SetCameraOrbitSpeed(float NewSpeed);

	float GetTurnAngle() const { return TurnAngle; }
	float GetSegmentLength() const { return SegmentLength; }
	int32 GetRedoCount() const { return m_Redos; }
	float GetStartWidth() const { return StartWidth; }
	float GetWidthMultiplier() const { return WidthMultiplier; }
	int32 GetRandomSeed() const { return RandomSeed; }
	bool IsCameraOrbitEnabled() const { return bOrbitCamera; }
	float GetCameraOrbitSpeed() const { return CameraOrbitSpeed; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L-System")
	TObjectPtr<UProceduralMeshComponent> BranchMesh;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Random")
	int32 RandomSeed = 12345;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "L-System|Camera")
	TObjectPtr<ACameraActor> OrbitCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Camera")
	bool bOrbitCamera = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Camera", meta = (Units = "deg/s"))
	float CameraOrbitSpeed = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Camera")
	float CameraTargetHeight = 150.0f;

	UFUNCTION(BlueprintCallable, Category = "L-System")
	void RunGeneration(FString& CurrentString);

	UFUNCTION(BlueprintCallable, Category = "L-System")
	void DrawAxiom();

private:
	void InitializeCameraOrbit();

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

	float CameraOrbitAngleRadians = 0.0f;
	float CameraOrbitRadius = 1.0f;
	float CameraOrbitHeight = 0.0f;
};
