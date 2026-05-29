// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <memory>
#include <vector>

#include "CoreMinimal.h"
#include "InputAction.h"
#include "Movement/SteeringBehaviors/Steering/SteeringBehaviors.h"
#include "Shared/Level_Base.h"
#include "Level_SquadCoordination.generated.h"

class ASteeringAgent;

UENUM(BlueprintType)
enum class ESquadRoles : uint8
{
	Leader = 0,
	LeftFlank = 1,
	RightFlank = 2,
	RearSupport = 3
};

UENUM(BlueprintType)
enum class ESquadFormation : uint8
{
	Wedge,
	Column,
	Line
};

USTRUCT(BlueprintType)
struct FSquadAgent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Squad")
	ASteeringAgent* Agent{};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Squad")
	ESquadRoles Role{ESquadRoles::Leader};
};

UCLASS()
class GAMEAIPROG_API ALevel_SquadCoordination : public ALevel_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SquadCoordination|Input")
	UInputAction* SetSquadTargetAction{};

	ALevel_SquadCoordination();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void BindLevelInputActions() override;

private:
	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	int32 SquadSize{4};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	ESquadFormation SquadFormation{ESquadFormation::Wedge};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float FormationSpacing{260.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float SpawnZ{90.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Debug")
	bool bDrawDebug{true};

	UPROPERTY()
	TArray<FSquadAgent> SquadAgents{};

	std::vector<std::unique_ptr<Arrive>> ArriveBehaviors{};
	bool bWasLeftMouseDown{false};

	void SpawnSquad(const FVector& SpawnCenter);
	void AddAgentToSquad(const FVector& SpawnCenter);
	void RemoveAgentFromSquad();
	void SetSquadTargetFromMouse();
	void UpdateSquadTargets();
	void DrawSquadDebug() const;
	void UpdateImGui();
	ESquadRoles GetRoleForAgentIndex(int32 AgentIndex) const;
	FVector2D GetFormationOffset(int32 AgentIndex) const;
};
