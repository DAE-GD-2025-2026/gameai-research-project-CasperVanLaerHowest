// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/InstancedStaticMeshComponent.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TwoDLSystem.generated.h"

UCLASS()
class LSYSTEMS_API ATwoDLSystem : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATwoDLSystem();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System")
	TMap<FString, FString> RewriteRules;
	
	UFUNCTION(BlueprintCallable, Category = "L-System")
	void RunGeneration(FString &CurrentString);
	
	UFUNCTION(BlueprintCallable, Category = "L-System")
	void RerunAllGen();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "L-System")
	TObjectPtr<UInstancedStaticMeshComponent> SegmentInstances;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Drawing")
	float SegmentLength = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System|Drawing")
	float TurnAngle = 25.0f;

	UFUNCTION(BlueprintCallable, Category = "L-System")
	void DrawAxiom();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System")
	FString m_StartAxiom;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "L-System")
	uint8 m_Redos{3};

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
private:
	UPROPERTY()
	FString m_Axiom{TEXT("X")};
};
