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

UENUM(BlueprintType)
enum class ESquadAgentState : uint8
{
	FollowFormation = 0,
	RejoinSquad = 1,
	LowHealthFallback = 2,
	SupportLowHealthAlly = 3,
	Patrol = 4
};

UENUM(BlueprintType)
enum class ESquadCoordinationMode : uint8
{
	RoleBasedFormation = 0,
	SharedTargetBaseline = 1
};

USTRUCT(BlueprintType)
struct FSquadAgent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Squad")
	ASteeringAgent* Agent{};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Squad")
	ESquadRoles Role{ESquadRoles::Leader};

	float Health{100.f};
	float LowHealthThreshold{35.f};
	ESquadAgentState State{ESquadAgentState::FollowFormation};
	FVector2D LastPosition{};
	FVector2D AssignedSlot{};
	float StuckTimer{0.f};
	bool bUsingRelaxedSlot{false};
};

USTRUCT(BlueprintType)
struct FSquadResearchMetrics
{
	GENERATED_BODY()

	float AverageSlotError{0.f};
	float MaxSlotError{0.f};
	float AverageSpacingError{0.f};
	float TimeToSettle{0.f};
	int32 StuckAgentCount{0};
	int32 RelaxedSlotCount{0};
	bool bFormationSettled{false};
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
	bool bUseAutomaticFormation{false};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float AutoFormationSideProbeDistance{420.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float AutoFormationForwardProbeDistance{520.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float AutoFormationProbeRadius{70.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Research")
	ESquadCoordinationMode CoordinationMode{ESquadCoordinationMode::RoleBasedFormation};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float FormationSpacing{260.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float AgentAvoidanceRadius{75.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float ArriveStopRadius{25.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float RejoinDistance{450.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Research")
	float SettledSlotError{95.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Research")
	float StuckProgressDistance{12.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Research")
	float StuckTimeThreshold{1.5f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float SupportAllyDistance{280.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float LowHealthFallbackDistance{550.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float SupportEnemyDetectionRange{1104.5f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float WoundedSafeDistanceFromEnemy{900.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	FVector PatrolRouteOffset{-1200.f, 614.3f, 0.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Setup")
	float SpawnZ{90.f};

	UPROPERTY(EditAnywhere, Category="SquadCoordination|Debug")
	bool bDrawDebug{true};

	UPROPERTY()
	TArray<FSquadAgent> SquadAgents{};

	UPROPERTY()
	ASteeringAgent* PatrolEnemy{};

	TArray<FVector> PatrolEnemyPoints{};
	int32 PatrolEnemyPointIndex{0};
	std::unique_ptr<Seek> PatrolEnemySeek{};

	std::vector<std::unique_ptr<AvoidanceArrive>> ArriveBehaviors{};
	FSquadResearchMetrics ResearchMetrics{};
	ESquadRoles NewAgentRole{ESquadRoles::RearSupport};
	bool bWasLeftMouseDown{false};
	float TimeSinceTargetSet{0.f};
	float LastSettleTime{0.f};
	bool bHasSettledForTarget{false};
	bool bHasAutomaticFormationForTarget{false};

	void SpawnSquad(const FVector& SpawnCenter);
	void AddAgentToSquad(const FVector& SpawnCenter, ESquadRoles AgentRole);
	void RemoveAgentFromSquad();
	void SpawnPatrolEnemy(const FVector& SpawnCenter);
	void SetSquadTargetFromMouse();
	void ResetTargetEvaluation();
	void UpdateAutomaticFormation();
	void UpdateSquadTargets(float DeltaTime);
	void UpdateResearchMetrics(const TArray<FVector2D>& AssignedSlots);
	void RebuildPatrolRoute();
	void InitSquadBehaviorTree(FSquadAgent& SquadAgent);
	void InitPatrolBehaviorTree(ASteeringAgent& Agent);
	void DrawSquadDebug() const;
	void UpdateImGui();
	static const char* GetStateLabel(ESquadAgentState State);
	
	ESquadRoles GetRoleForAgentIndex(int32 AgentIndex) const;
	
	int32 GetRoleOccurrenceIndex(int32 AgentIndex) const;
	
	FVector2D GetFormationOffset(int32 AgentIndex) const;
	FVector2D GetDesiredSlotForAgent(int32 AgentIndex) const;
	FVector2D RotateFormationOffset(const FVector2D& LocalOffset) const;
	FVector2D GetFormationForward() const;
	FVector2D GetFormationRight() const;
	FVector2D GetWedgeFormationOffsetForRole(ESquadRoles SquadRole, int32 RoleOccurrenceIndex) const;
	
	bool IsNavigationSpaceAvailable(const FVector2D& TestPoint, float ProbeRadius) const;
	bool TryGetValidNavSlot(const FVector2D& DesiredSlot, FVector2D& OutValidSlot) const;
};
