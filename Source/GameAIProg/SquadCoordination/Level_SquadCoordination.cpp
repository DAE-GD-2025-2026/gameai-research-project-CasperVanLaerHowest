// Fill out your copyright notice in the Description page of Project Settings.

#include "Level_SquadCoordination.h"

#include <memory>

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DecisionMaking/BehaviorTree/BT.h"
#include "DecisionMaking/BehaviorTree/GameAIBehaviorTreeComponent.h"
#include "DecisionMaking/GameAIController.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "InputCoreTypes.h"
#include "imgui.h"

namespace
{
	ESquadAgentState GetStateFromBlackboard(UBlackboardComponent* Blackboard)
	{
		if (!Blackboard)
		{
			return ESquadAgentState::FollowFormation;
		}

		const int32 StateValue = FMath::RoundToInt(Blackboard->GetValueAsFloat(TEXT("AgentState")));
		if (StateValue < static_cast<int32>(ESquadAgentState::FollowFormation)
			|| StateValue > static_cast<int32>(ESquadAgentState::Patrol))
		{
			return ESquadAgentState::FollowFormation;
		}

		return static_cast<ESquadAgentState>(StateValue);
	}

	const char* GetCoordinationModeLabel(const ESquadCoordinationMode Mode)
	{
		switch (Mode)
		{
		case ESquadCoordinationMode::RoleBasedFormation:
			return "Role-based formation";
		case ESquadCoordinationMode::SharedTargetBaseline:
			return "Shared target baseline";
		default:
			return "Unknown";
		}
	}
}

ALevel_SquadCoordination::ALevel_SquadCoordination()
{
	// Let Unreal call Tick every frame so the level can react to mouse input,
	// update squad destinations, draw debug helpers, and refresh the ImGui panel.
	PrimaryActorTick.bCanEverTick = true;
}

void ALevel_SquadCoordination::BeginPlay()
{
	Super::BeginPlay();

	// Keep the playable area focused around this level so agents do not wander
	// too far outside the navmesh/world area during the demo.
	if (TrimWorld)
	{
		TrimWorld->SetTrimWorldSize(3200.f);
		TrimWorld->bShouldTrimWorld = true;
	}

	// Start the squad near the center of the navmesh when possible. If the
	// navmesh center cannot be found, fall back to world origin at SpawnZ.
	const FVector SpawnCenter = GetNavMeshBoundsCenter(SpawnZ).value_or(FVector{0.f, 0.f, SpawnZ});
	MouseTarget.Position = FVector2D{SpawnCenter};
	SpawnSquad(SpawnCenter);
	SpawnPatrolEnemy(SpawnCenter + FVector{900.f, 900.f, 0.f});
	UpdateSquadTargets(0.f);
}

void ALevel_SquadCoordination::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// The Enhanced Input binding handles normal clicks, but this direct mouse
	// check acts as a fallback when the input action or component is missing.
	if (PlayerController)
	{
		const bool bLeftMouseDown = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
		// Only respond on the first frame of a click, and ignore clicks that
		// ImGui wants to use for sliders, checkboxes, or other UI controls.
		if (bLeftMouseDown && !bWasLeftMouseDown && !ImGui::GetIO().WantCaptureMouse)
		{
			SetSquadTargetFromMouse();
		}
		bWasLeftMouseDown = bLeftMouseDown;
	}

	// Recalculate targets every frame so formation spacing/debug changes in the
	// UI immediately affect the squad without needing another click.
	TimeSinceTargetSet += DeltaTime;
	UpdateSquadTargets(DeltaTime);

	if (bDrawDebug)
	{
		DrawSquadDebug();
	}

	UpdateImGui();
}

void ALevel_SquadCoordination::BindLevelInputActions()
{
	Super::BindLevelInputActions();

	// If Enhanced Input is not available, Tick still checks left mouse directly
	// so the level remains usable.
	if (!PlayerEnhancedInputComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("SquadCoordination: Enhanced input component missing, using direct LMB fallback."));
		return;
	}

	// Bind the assigned input action to the same function used by the fallback
	// mouse code, keeping both input paths in sync.
	if (SetSquadTargetAction)
	{
		PlayerEnhancedInputComponent->BindAction(
			SetSquadTargetAction,
			ETriggerEvent::Started,
			this,
			&ALevel_SquadCoordination::SetSquadTargetFromMouse);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SquadCoordination: SetSquadTargetAction not assigned, using direct LMB fallback."));
	}
}

void ALevel_SquadCoordination::SpawnSquad(const FVector& SpawnCenter)
{
	// Clear any previous squad data before spawning a fresh set of agents.
	SquadAgents.Reset();
	ArriveBehaviors.clear();

	// Make sure there is always at least one agent, even if the editable value
	// in the details panel is accidentally set to zero or below.
	const int32 ClampedSquadSize = FMath::Max(1, SquadSize);
	SquadAgents.Reserve(ClampedSquadSize);
	ArriveBehaviors.reserve(ClampedSquadSize);

	for (int32 AgentIndex = 0; AgentIndex < ClampedSquadSize; ++AgentIndex)
	{
		AddAgentToSquad(SpawnCenter, GetRoleForAgentIndex(AgentIndex));
	}

	SquadSize = SquadAgents.Num();
}

void ALevel_SquadCoordination::AddAgentToSquad(const FVector& SpawnCenter, ESquadRoles AgentRole)
{
	const int32 AgentIndex = SquadAgents.Num();

	// Spawn the agent directly into its current formation slot so newly added
	// squad members join the layout cleanly instead of appearing on top of the leader.
	int32 RoleOccurrenceIndex = 0;
	for (const FSquadAgent& SquadAgent : SquadAgents)
	{
		if (SquadAgent.Role == AgentRole)
		{
			++RoleOccurrenceIndex;
		}
	}
	FVector2D Offset = FVector2D::ZeroVector;
	switch (SquadFormation)
	{
	case ESquadFormation::Wedge:
		Offset = GetWedgeFormationOffsetForRole(AgentRole, RoleOccurrenceIndex);
		break;
	case ESquadFormation::Column:
		Offset = RotateFormationOffset(FVector2D{-FormationSpacing * static_cast<float>(AgentIndex), 0.f});
		break;
	case ESquadFormation::Line:
	{
		const int32 NewAgentCount = AgentIndex + 1;
		const float CenteredIndex = static_cast<float>(AgentIndex) - (static_cast<float>(NewAgentCount - 1) * 0.5f);
		Offset = RotateFormationOffset(FVector2D{0.f, FormationSpacing * CenteredIndex});
		break;
	}
	default:
		Offset = FVector2D::ZeroVector;
		break;
	}
	const FVector SpawnLocation{
		SpawnCenter.X + Offset.X,
		SpawnCenter.Y + Offset.Y,
		SpawnZ
	};

	ASteeringAgent* Agent = GetWorld()->SpawnActor<ASteeringAgent>(
		SteeringAgentClass,
		SpawnLocation,
		FRotator::ZeroRotator);

	if (!IsValid(Agent))
	{
		UE_LOG(LogTemp, Error, TEXT("SquadCoordination: Failed to spawn squad agent %d."), AgentIndex);
		return;
	}

	if (AController* ExistingController = Agent->GetController();
		ExistingController && !ExistingController->IsA(AGameAIController::StaticClass()))
	{
		ExistingController->UnPossess();
		ExistingController->Destroy();
	}
	Agent->AIControllerClass = AGameAIController::StaticClass();
	Agent->SpawnDefaultController();
	Agent->SetDebugRenderingEnabled(bDrawDebug);

	// Each agent gets its own avoidance-aware arrive behavior. The level keeps ownership of
	// these behaviors in ArriveBehaviors so the raw pointer given to the
	// steering agent stays valid for the lifetime of the squad.
	auto ArriveBehavior = std::make_unique<AvoidanceArrive>();
	ArriveBehavior->SetAvoidanceRadius(AgentAvoidanceRadius);
	Agent->SetSteeringBehavior(ArriveBehavior.get());

	FSquadAgent SquadAgent{};
	SquadAgent.Agent = Agent;
	SquadAgent.Role = AgentRole;
	SquadAgent.Health = AgentRole == ESquadRoles::RearSupport ? 25.f : 100.f;
	SquadAgent.LastPosition = Agent->GetPosition();
	SquadAgent.AssignedSlot = FVector2D{SpawnLocation};
	SquadAgents.Add(SquadAgent);
	ArriveBehaviors.emplace_back(std::move(ArriveBehavior));
	SquadSize = SquadAgents.Num();

	InitSquadBehaviorTree(SquadAgents.Last());
	UpdateSquadTargets(0.f);
}

void ALevel_SquadCoordination::RemoveAgentFromSquad()
{
	// Keep at least the leader alive so the formation still has a stable anchor.
	if (SquadAgents.Num() <= 1)
	{
		return;
	}

	const int32 AgentIndex = SquadAgents.Num() - 1;
	ASteeringAgent* Agent = SquadAgents[AgentIndex].Agent;
	if (IsValid(Agent))
	{
		Agent->SetSteeringBehavior(nullptr);
		Agent->Destroy();
	}

	SquadAgents.RemoveAt(AgentIndex);
	if (ArriveBehaviors.size() > static_cast<size_t>(AgentIndex))
	{
		ArriveBehaviors.erase(ArriveBehaviors.begin() + AgentIndex);
	}

	SquadSize = SquadAgents.Num();
	UpdateSquadTargets(0.f);
}

void ALevel_SquadCoordination::SpawnPatrolEnemy(const FVector& SpawnCenter)
{
	PatrolEnemy = GetWorld()->SpawnActor<ASteeringAgent>(
		SteeringAgentClass,
		SpawnCenter,
		FRotator::ZeroRotator);

	if (!IsValid(PatrolEnemy))
	{
		UE_LOG(LogTemp, Error, TEXT("SquadCoordination: Failed to spawn patrol enemy."));
		return;
	}

	if (AController* ExistingController = PatrolEnemy->GetController();
		ExistingController && !ExistingController->IsA(AGameAIController::StaticClass()))
	{
		ExistingController->UnPossess();
		ExistingController->Destroy();
	}
	PatrolEnemy->AIControllerClass = AGameAIController::StaticClass();
	PatrolEnemy->SpawnDefaultController();
	PatrolEnemy->SetDebugRenderingEnabled(bDrawDebug);
	PatrolEnemy->SetActorLocation(PatrolEnemy->GetActorLocation() + PatrolRouteOffset);
	InitPatrolBehaviorTree(*PatrolEnemy);
}

void ALevel_SquadCoordination::InitSquadBehaviorTree(FSquadAgent& SquadAgent)
{
	AGameAIController* AIController = SquadAgent.Agent ? Cast<AGameAIController>(SquadAgent.Agent->GetController()) : nullptr;
	UGameAIBehaviorTreeComponent* BehaviorTreeComp = AIController ? AIController->FindComponentByClass<UGameAIBehaviorTreeComponent>() : nullptr;
	UBlackboardComponent* Blackboard = AIController ? AIController->GetBlackboardComponent() : nullptr;
	if (!AIController || !BehaviorTreeComp || !Blackboard)
	{
		UE_LOG(LogTemp, Warning, TEXT("SquadCoordination: Could not initialize squad behavior tree."));
		return;
	}

	Blackboard->SetValueAsFloat(TEXT("Health"), SquadAgent.Health);
	Blackboard->SetValueAsFloat(TEXT("LowHealthThreshold"), SquadAgent.LowHealthThreshold);
	Blackboard->SetValueAsBool(TEXT("IsPatrolOnly"), false);

	auto MoveToBlackboardVector = [this](AAIController& Controller, const FName KeyName, ESquadAgentState State, float AcceptanceRadius)
	{
		UBlackboardComponent* BlackboardComp = Controller.GetBlackboardComponent();
		if (!BlackboardComp)
		{
			return GameAI::BT::ENodeResult::Failed;
		}

		BlackboardComp->SetValueAsFloat(TEXT("AgentState"), static_cast<float>(State));
		const FVector TargetLocation = BlackboardComp->GetValueAsVector(KeyName);
		Controller.MoveToLocation(
			TargetLocation,
			AcceptanceRadius,
			true,
			true,
			true,
			false);
		return GameAI::BT::ENodeResult::Running;
	};

	auto Tree = std::make_unique<GameAI::BT::BehaviorTree>();
	auto Root = std::make_unique<GameAI::BT::Selector>("Squad State Selector");

	auto LowHealthSequence = std::make_unique<GameAI::BT::Sequence>("Low Health -> Fall Back");
	LowHealthSequence->AddChild(std::make_unique<GameAI::BT::Action>("Enemy In Range",
		[](AAIController& Controller, float)
		{
			UBlackboardComponent* BlackboardComp = Controller.GetBlackboardComponent();
			return BlackboardComp && BlackboardComp->GetValueAsBool(TEXT("IsEnemyInRange"))
				? GameAI::BT::ENodeResult::Succeeded
				: GameAI::BT::ENodeResult::Failed;
		}));
	LowHealthSequence->AddChild(std::make_unique<GameAI::BT::Action>("Is Low Health",
		[](AAIController& Controller, float)
		{
			UBlackboardComponent* BlackboardComp = Controller.GetBlackboardComponent();
			return BlackboardComp && BlackboardComp->GetValueAsBool(TEXT("IsLowHealth"))
				? GameAI::BT::ENodeResult::Succeeded
				: GameAI::BT::ENodeResult::Failed;
		}));
	LowHealthSequence->AddChild(std::make_unique<GameAI::BT::Action>("Still Unsafe",
		[](AAIController& Controller, float)
		{
			UBlackboardComponent* BlackboardComp = Controller.GetBlackboardComponent();
			return BlackboardComp && BlackboardComp->GetValueAsBool(TEXT("IsUnsafeFromEnemy"))
				? GameAI::BT::ENodeResult::Succeeded
				: GameAI::BT::ENodeResult::Failed;
		}));
	LowHealthSequence->AddChild(std::make_unique<GameAI::BT::Action>("Fallback To Safe Location",
		[MoveToBlackboardVector](AAIController& Controller, float)
		{
			return MoveToBlackboardVector(Controller, TEXT("SafeLocation"), ESquadAgentState::LowHealthFallback, 80.f);
		}));
	Root->AddChild(std::move(LowHealthSequence));

	auto SupportSequence = std::make_unique<GameAI::BT::Sequence>("Ally Low Health -> Support");
	SupportSequence->AddChild(std::make_unique<GameAI::BT::Action>("Enemy In Range",
		[](AAIController& Controller, float)
		{
			UBlackboardComponent* BlackboardComp = Controller.GetBlackboardComponent();
			return BlackboardComp && BlackboardComp->GetValueAsBool(TEXT("IsEnemyInRange"))
				? GameAI::BT::ENodeResult::Succeeded
				: GameAI::BT::ENodeResult::Failed;
		}));
	SupportSequence->AddChild(std::make_unique<GameAI::BT::Action>("Has Low Health Ally",
		[](AAIController& Controller, float)
		{
			UBlackboardComponent* BlackboardComp = Controller.GetBlackboardComponent();
			return BlackboardComp && BlackboardComp->GetValueAsBool(TEXT("HasLowHealthAlly"))
				? GameAI::BT::ENodeResult::Succeeded
				: GameAI::BT::ENodeResult::Failed;
		}));
	SupportSequence->AddChild(std::make_unique<GameAI::BT::Action>("Low Health Ally Still Unsafe",
		[](AAIController& Controller, float)
		{
			UBlackboardComponent* BlackboardComp = Controller.GetBlackboardComponent();
			return BlackboardComp && BlackboardComp->GetValueAsBool(TEXT("IsLowHealthAllyUnsafe"))
				? GameAI::BT::ENodeResult::Succeeded
				: GameAI::BT::ENodeResult::Failed;
		}));
	SupportSequence->AddChild(std::make_unique<GameAI::BT::Action>("Hold And Attack Enemy",
		[this](AAIController& Controller, float)
		{
			UBlackboardComponent* BlackboardComp = Controller.GetBlackboardComponent();
			ASteeringAgent* Agent = Cast<ASteeringAgent>(Controller.GetPawn());
			if (!BlackboardComp || !IsValid(Agent) || !IsValid(PatrolEnemy))
			{
				return GameAI::BT::ENodeResult::Failed;
			}

			BlackboardComp->SetValueAsFloat(TEXT("AgentState"), static_cast<float>(ESquadAgentState::SupportLowHealthAlly));
			Controller.StopMovement();
			const FVector ToEnemy = PatrolEnemy->GetActorLocation() - Agent->GetActorLocation();
			if (!ToEnemy.IsNearlyZero())
			{
				Agent->SetActorRotation(ToEnemy.Rotation());
			}
			return GameAI::BT::ENodeResult::Running;
		}));
	Root->AddChild(std::move(SupportSequence));

	auto RejoinSequence = std::make_unique<GameAI::BT::Sequence>("Too Far -> Rejoin Squad");
	RejoinSequence->AddChild(std::make_unique<GameAI::BT::Action>("Is Too Far From Formation",
		[](AAIController& Controller, float)
		{
			UBlackboardComponent* BlackboardComp = Controller.GetBlackboardComponent();
			return BlackboardComp && BlackboardComp->GetValueAsBool(TEXT("IsTooFarFromFormation"))
				? GameAI::BT::ENodeResult::Succeeded
				: GameAI::BT::ENodeResult::Failed;
		}));
	RejoinSequence->AddChild(std::make_unique<GameAI::BT::Action>("Rejoin Formation Slot",
		[MoveToBlackboardVector, this](AAIController& Controller, float)
		{
			return MoveToBlackboardVector(Controller, TEXT("SquadSlotLocation"), ESquadAgentState::RejoinSquad, ArriveStopRadius);
		}));
	Root->AddChild(std::move(RejoinSequence));

	Root->AddChild(std::make_unique<GameAI::BT::Action>("Follow Formation",
		[MoveToBlackboardVector, this](AAIController& Controller, float)
		{
			return MoveToBlackboardVector(Controller, TEXT("SquadSlotLocation"), ESquadAgentState::FollowFormation, ArriveStopRadius);
		}));

	Tree->SetRoot(std::move(Root));
	BehaviorTreeComp->SetTree(std::move(Tree));
	AIController->RunBehaviorTreeLogic();
}

void ALevel_SquadCoordination::InitPatrolBehaviorTree(ASteeringAgent& Agent)
{
	AGameAIController* AIController = Cast<AGameAIController>(Agent.GetController());
	UGameAIBehaviorTreeComponent* BehaviorTreeComp = AIController ? AIController->FindComponentByClass<UGameAIBehaviorTreeComponent>() : nullptr;
	UBlackboardComponent* Blackboard = AIController ? AIController->GetBlackboardComponent() : nullptr;
	if (!AIController || !BehaviorTreeComp || !Blackboard)
	{
		UE_LOG(LogTemp, Warning, TEXT("SquadCoordination: Could not initialize patrol behavior tree."));
		return;
	}

	Blackboard->SetValueAsBool(TEXT("IsPatrolOnly"), true);
	Blackboard->SetValueAsFloat(TEXT("AgentState"), static_cast<float>(ESquadAgentState::Patrol));

	if (!PatrolEnemySeek)
	{
		PatrolEnemySeek = std::make_unique<Seek>();
	}

	PatrolEnemyPoints.Reset();
	PatrolEnemyPointIndex = 0;
	RebuildPatrolRoute();

	auto Tree = std::make_unique<GameAI::BT::BehaviorTree>();
	auto Root = std::make_unique<GameAI::BT::Action>("Patrol Forever",
		[this](AAIController& Controller, float)
		{
			UBlackboardComponent* BlackboardComp = Controller.GetBlackboardComponent();
			ASteeringAgent* Agent = Cast<ASteeringAgent>(Controller.GetPawn());
			if (!BlackboardComp || !IsValid(Agent) || !PatrolEnemySeek || PatrolEnemyPoints.IsEmpty())
			{
				return GameAI::BT::ENodeResult::Failed;
			}

			PatrolEnemyPointIndex = PatrolEnemyPoints.IsValidIndex(PatrolEnemyPointIndex) ? PatrolEnemyPointIndex : 0;
			const FVector CurrentTarget = PatrolEnemyPoints[PatrolEnemyPointIndex];
			BlackboardComp->SetValueAsVector(TEXT("PatrolLocation"), CurrentTarget);
			BlackboardComp->SetValueAsFloat(TEXT("AgentState"), static_cast<float>(ESquadAgentState::Patrol));

			const FVector2D ToPoint = FVector2D{CurrentTarget} - Agent->GetPosition();
			const float ReachDistance = FMath::Max(Agent->GetCapsuleRadius() * 1.5f, 80.f);
			if (ToPoint.SizeSquared() <= ReachDistance * ReachDistance)
			{
				PatrolEnemyPointIndex = (PatrolEnemyPointIndex + 1) % PatrolEnemyPoints.Num();
			}

			const FVector Target = PatrolEnemyPoints[PatrolEnemyPointIndex];
			PatrolEnemySeek->SetTarget(FTargetData{FVector2D{Target}});
			Agent->SetSteeringBehavior(PatrolEnemySeek.get());
			return GameAI::BT::ENodeResult::Running;
		});

	Tree->SetRoot(std::move(Root));
	BehaviorTreeComp->SetTree(std::move(Tree));
	AIController->RunBehaviorTreeLogic();
}

void ALevel_SquadCoordination::SetSquadTargetFromMouse()
{
	// Store the latest valid mouse world position. If the current frame cannot
	// produce one, the squad keeps using the last known position instead.
	if (const auto MouseWorldPos = GetMouseWorldPos(); MouseWorldPos.has_value())
	{
		LatestMouseWorldPos = MouseWorldPos.value();
	}

	// MouseTarget is the squad's center point; individual agents offset from it
	// to form the surrounding pattern.
	MouseTarget.Position = FVector2D{LatestMouseWorldPos};
	ResetTargetEvaluation();
	UpdateAutomaticFormation();
	UpdateSquadTargets(0.f);
}

void ALevel_SquadCoordination::ResetTargetEvaluation()
{
	TimeSinceTargetSet = 0.f;
	LastSettleTime = 0.f;
	bHasSettledForTarget = false;
	bHasAutomaticFormationForTarget = false;
	ResearchMetrics.bFormationSettled = false;
	for (FSquadAgent& SquadAgent : SquadAgents)
	{
		SquadAgent.StuckTimer = 0.f;
		SquadAgent.bUsingRelaxedSlot = false;
	}
}

void ALevel_SquadCoordination::UpdateAutomaticFormation()
{
	if (!bUseAutomaticFormation
		|| bHasAutomaticFormationForTarget
		|| CoordinationMode == ESquadCoordinationMode::SharedTargetBaseline)
	{
		return;
	}

	const FVector2D Center = MouseTarget.Position;
	const FVector2D Forward = GetFormationForward();
	const FVector2D Right = GetFormationRight();

	const bool bLeftOpen = IsNavigationSpaceAvailable(Center - Right * AutoFormationSideProbeDistance, AutoFormationProbeRadius);
	const bool bRightOpen = IsNavigationSpaceAvailable(Center + Right * AutoFormationSideProbeDistance, AutoFormationProbeRadius);
	const bool bForwardOpen = IsNavigationSpaceAvailable(Center + Forward * AutoFormationForwardProbeDistance, AutoFormationProbeRadius);

	ESquadFormation DesiredFormation = ESquadFormation::Wedge;
	if (!bLeftOpen || !bRightOpen)
	{
		DesiredFormation = ESquadFormation::Column;
	}
	else if (!bForwardOpen)
	{
		DesiredFormation = ESquadFormation::Line;
	}

	if (SquadFormation != DesiredFormation)
	{
		SquadFormation = DesiredFormation;
	}
	bHasAutomaticFormationForTarget = true;
}

void ALevel_SquadCoordination::RebuildPatrolRoute()
{
	PatrolEnemyPoints.Reset();
	PatrolEnemyPointIndex = 0;

	if (!IsValid(PatrolEnemy))
	{
		return;
	}

	const FVector RouteCenter = PatrolEnemy->GetActorLocation();
	const FVector PatrolOffsets[] = {
		FVector{-520.f, -520.f, 0.f},
		FVector{520.f, -520.f, 0.f},
		FVector{520.f, 520.f, 0.f},
		FVector{-520.f, 520.f, 0.f}
	};

	for (const FVector& Offset : PatrolOffsets)
	{
		FVector2D ProjectedPoint{};
		const FVector DesiredPoint = RouteCenter + Offset;
		if (TryGetValidNavSlot(FVector2D{DesiredPoint}, ProjectedPoint))
		{
			PatrolEnemyPoints.Add(FVector{ProjectedPoint.X, ProjectedPoint.Y, SpawnZ});
		}
	}

	if (PatrolEnemyPoints.Num() < 2)
	{
		PatrolEnemyPoints.Add(RouteCenter);
		PatrolEnemyPoints.Add(RouteCenter + FVector{450.f, 0.f, 0.f});
	}
}

void ALevel_SquadCoordination::UpdateSquadTargets(float DeltaTime)
{
	// Only update pairs that exist in both arrays. This protects against failed
	// spawns or any future change that could leave the arrays out of sync.
	const int32 AgentCount = FMath::Min(SquadAgents.Num(), static_cast<int32>(ArriveBehaviors.size()));
	TArray<ASteeringAgent*> ActiveAgents{};
	ActiveAgents.Reserve(AgentCount);
	TArray<FVector2D> AssignedSlots{};
	AssignedSlots.Init(FVector2D::ZeroVector, AgentCount);
	for (int32 AgentIndex = 0; AgentIndex < AgentCount; ++AgentIndex)
	{
		if (IsValid(SquadAgents[AgentIndex].Agent))
		{
			ActiveAgents.Add(SquadAgents[AgentIndex].Agent);
		}
	}

	for (int32 AgentIndex = 0; AgentIndex < AgentCount; ++AgentIndex)
	{
		FSquadAgent& SquadAgent = SquadAgents[AgentIndex];

		if (!IsValid(SquadAgent.Agent) || !ArriveBehaviors[AgentIndex])
		{
			continue;
		}

		const FVector2D DesiredSlot = GetDesiredSlotForAgent(AgentIndex);
		FVector2D ValidSlot{};
		if (!TryGetValidNavSlot(DesiredSlot, ValidSlot))
		{
			// If this formation slot is outside the NavMesh, fall back to the
			// squad center so the agent still moves somewhere valid.
			if (!TryGetValidNavSlot(MouseTarget.Position, ValidSlot))
			{
				// If even the squad center is invalid, keep the agent near its
				// current valid area instead of leaving it with a stale target.
				const FVector AgentLocation = SquadAgent.Agent->GetActorLocation();
				const FVector2D AgentSlot{AgentLocation.X, AgentLocation.Y};
				if (!TryGetValidNavSlot(AgentSlot, ValidSlot))
				{
					continue;
				}
			}
		}

		const FVector2D CurrentPosition = SquadAgent.Agent->GetPosition();
		const float SlotDistance = FVector2D::Distance(CurrentPosition, ValidSlot);
		const float AssignedSlotDistance = FVector2D::Distance(CurrentPosition, SquadAgent.AssignedSlot);
		const float ProgressDistance = FVector2D::Distance(CurrentPosition, SquadAgent.LastPosition);
		if (SquadAgent.bUsingRelaxedSlot && AssignedSlotDistance <= SettledSlotError)
		{
			SquadAgent.StuckTimer = 0.f;
			SquadAgent.bUsingRelaxedSlot = false;
		}
		else if (DeltaTime > 0.f && SlotDistance > SettledSlotError && ProgressDistance < StuckProgressDistance)
		{
			SquadAgent.StuckTimer += DeltaTime;
		}
		else if (SlotDistance <= SettledSlotError || ProgressDistance >= StuckProgressDistance)
		{
			SquadAgent.StuckTimer = 0.f;
			SquadAgent.bUsingRelaxedSlot = false;
		}

		SquadAgent.LastPosition = CurrentPosition;
		if (!SquadAgent.bUsingRelaxedSlot)
		{
			SquadAgent.bUsingRelaxedSlot = SquadAgent.StuckTimer >= StuckTimeThreshold;
		}

		if (SquadAgent.bUsingRelaxedSlot && CoordinationMode == ESquadCoordinationMode::RoleBasedFormation)
		{
			FVector2D RelaxedSlot{};
			const FVector2D FormationOffset = DesiredSlot - MouseTarget.Position;
			const FVector2D OffsetDirection = FormationOffset.SizeSquared() > KINDA_SMALL_NUMBER
				? FormationOffset.GetSafeNormal()
				: FVector2D{1.f, 0.f};
			const FVector2D RelaxedTarget = DesiredSlot + OffsetDirection * FMath::Max(AgentAvoidanceRadius, 80.f);

			if (TryGetValidNavSlot(RelaxedTarget, RelaxedSlot))
			{
				ValidSlot = RelaxedSlot;
			}
		}

		AssignedSlots[AgentIndex] = ValidSlot;
		SquadAgent.AssignedSlot = ValidSlot;

		ASteeringAgent* LowHealthAlly = nullptr;
		for (const FSquadAgent& OtherSquadAgent : SquadAgents)
		{
			if (OtherSquadAgent.Agent != SquadAgent.Agent
				&& IsValid(OtherSquadAgent.Agent)
				&& OtherSquadAgent.Health <= OtherSquadAgent.LowHealthThreshold)
			{
				LowHealthAlly = OtherSquadAgent.Agent;
				break;
			}
		}

		FVector2D SafeSlot = MouseTarget.Position + RotateFormationOffset(FVector2D{-LowHealthFallbackDistance, 0.f});
		TryGetValidNavSlot(SafeSlot, SafeSlot);

		const bool bEnemyInRange = IsValid(PatrolEnemy)
			&& FVector::DistSquared2D(SquadAgent.Agent->GetActorLocation(), PatrolEnemy->GetActorLocation())
				<= FMath::Square(SupportEnemyDetectionRange);
		const bool bUnsafeFromEnemy = IsValid(PatrolEnemy)
			&& FVector::DistSquared2D(SquadAgent.Agent->GetActorLocation(), PatrolEnemy->GetActorLocation())
				<= FMath::Square(WoundedSafeDistanceFromEnemy);
		const bool bLowHealthAllyUnsafe = IsValid(PatrolEnemy)
			&& IsValid(LowHealthAlly)
			&& FVector::DistSquared2D(LowHealthAlly->GetActorLocation(), PatrolEnemy->GetActorLocation())
				<= FMath::Square(WoundedSafeDistanceFromEnemy);

		const FVector MoveTarget{ValidSlot.X, ValidSlot.Y, SpawnZ};
		const FVector SafeTarget{SafeSlot.X, SafeSlot.Y, SpawnZ};
		if (AAIController* AIController = Cast<AAIController>(SquadAgent.Agent->GetController()))
		{
			if (UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent())
			{
				Blackboard->SetValueAsVector(TEXT("SquadSlotLocation"), MoveTarget);
				Blackboard->SetValueAsVector(TEXT("SafeLocation"), SafeTarget);
				Blackboard->SetValueAsFloat(TEXT("DistanceToSlot"), FVector2D::Distance(SquadAgent.Agent->GetPosition(), ValidSlot));
				Blackboard->SetValueAsFloat(TEXT("FormationRole"), static_cast<float>(SquadAgent.Role));
				Blackboard->SetValueAsFloat(TEXT("Health"), SquadAgent.Health);
				Blackboard->SetValueAsFloat(TEXT("LowHealthThreshold"), SquadAgent.LowHealthThreshold);
				Blackboard->SetValueAsBool(TEXT("IsLowHealth"), SquadAgent.Health <= SquadAgent.LowHealthThreshold);
				Blackboard->SetValueAsBool(TEXT("IsEnemyInRange"), bEnemyInRange);
				Blackboard->SetValueAsBool(TEXT("IsUnsafeFromEnemy"), bUnsafeFromEnemy);
				Blackboard->SetValueAsBool(TEXT("IsLowHealthAllyUnsafe"), bLowHealthAllyUnsafe);
				Blackboard->SetValueAsBool(TEXT("IsTooFarFromFormation"),
					FVector2D::DistSquared(CurrentPosition, ValidSlot) > FMath::Square(RejoinDistance)
					|| SquadAgent.bUsingRelaxedSlot);
				Blackboard->SetValueAsBool(TEXT("IsSlotBlocked"), SquadAgent.bUsingRelaxedSlot);
				Blackboard->SetValueAsObject(TEXT("LowHealthAlly"), LowHealthAlly);
				Blackboard->SetValueAsBool(TEXT("HasLowHealthAlly"), IsValid(LowHealthAlly));
				SquadAgent.State = GetStateFromBlackboard(Blackboard);

				if (!SquadAgents.IsEmpty() && IsValid(SquadAgents[0].Agent))
				{
					Blackboard->SetValueAsObject(TEXT("SquadLeader"), SquadAgents[0].Agent);
				}
			}
		}

		FTargetData FormationTarget{};
		// Unreal's path follower handles the real destination and cornering.
		// The steering layer stays local so agents can still make room for each other.
		FormationTarget.Position = SquadAgent.Agent->GetPosition();
		ArriveBehaviors[AgentIndex]->ClearAgentsToAvoid();
		for (ASteeringAgent* ActiveAgent : ActiveAgents)
		{
			ArriveBehaviors[AgentIndex]->AddAgentToAvoid(ActiveAgent);
		}
		ArriveBehaviors[AgentIndex]->SetAvoidanceRadius(AgentAvoidanceRadius);
		ArriveBehaviors[AgentIndex]->SetRadiusNear(ArriveStopRadius);
		ArriveBehaviors[AgentIndex]->SetRadiusFar(FMath::Max(ArriveStopRadius + 1.f, 300.f));
		ArriveBehaviors[AgentIndex]->SetTarget(FormationTarget);
		SquadAgent.Agent->SetDebugRenderingEnabled(bDrawDebug);
	}

	UpdateResearchMetrics(AssignedSlots);
}

void ALevel_SquadCoordination::UpdateResearchMetrics(const TArray<FVector2D>& AssignedSlots)
{
	ResearchMetrics = FSquadResearchMetrics{};

	const int32 AgentCount = FMath::Min(SquadAgents.Num(), AssignedSlots.Num());
	if (AgentCount <= 0)
	{
		return;
	}

	float TotalSlotError = 0.f;
	float TotalSpacingError = 0.f;
	int32 SpacingPairCount = 0;

	for (int32 AgentIndex = 0; AgentIndex < AgentCount; ++AgentIndex)
	{
		const FSquadAgent& SquadAgent = SquadAgents[AgentIndex];
		if (!IsValid(SquadAgent.Agent))
		{
			continue;
		}

		const float SlotError = FVector2D::Distance(SquadAgent.Agent->GetPosition(), AssignedSlots[AgentIndex]);
		TotalSlotError += SlotError;
		ResearchMetrics.MaxSlotError = FMath::Max(ResearchMetrics.MaxSlotError, SlotError);

		if (SquadAgent.StuckTimer >= StuckTimeThreshold)
		{
			++ResearchMetrics.StuckAgentCount;
		}
		if (SquadAgent.bUsingRelaxedSlot)
		{
			++ResearchMetrics.RelaxedSlotCount;
		}

		for (int32 OtherAgentIndex = AgentIndex + 1; OtherAgentIndex < AgentCount; ++OtherAgentIndex)
		{
			const FSquadAgent& OtherSquadAgent = SquadAgents[OtherAgentIndex];
			if (!IsValid(OtherSquadAgent.Agent))
			{
				continue;
			}

			const float CurrentDistance = FVector2D::Distance(SquadAgent.Agent->GetPosition(), OtherSquadAgent.Agent->GetPosition());
			const float DesiredDistance = FVector2D::Distance(AssignedSlots[AgentIndex], AssignedSlots[OtherAgentIndex]);
			TotalSpacingError += FMath::Abs(CurrentDistance - DesiredDistance);
			++SpacingPairCount;
		}
	}

	ResearchMetrics.AverageSlotError = TotalSlotError / static_cast<float>(AgentCount);
	ResearchMetrics.AverageSpacingError = SpacingPairCount > 0
		? TotalSpacingError / static_cast<float>(SpacingPairCount)
		: 0.f;
	ResearchMetrics.bFormationSettled = ResearchMetrics.MaxSlotError <= SettledSlotError;
	if (ResearchMetrics.bFormationSettled && !bHasSettledForTarget)
	{
		LastSettleTime = TimeSinceTargetSet;
		bHasSettledForTarget = true;
	}
	ResearchMetrics.TimeToSettle = bHasSettledForTarget ? LastSettleTime : 0.f;
}

void ALevel_SquadCoordination::DrawSquadDebug() const
{
	// Draw the shared squad target slightly above the ground so it is visible.
	const float DrawZ = SpawnZ + 15.f;
	const FVector SquadTarget{MouseTarget.Position.X, MouseTarget.Position.Y, DrawZ};
	DrawDebugSphere(GetWorld(), SquadTarget, 70.f, 16, FColor::Cyan, false, -1.f, 0, 3.f);

	for (int32 AgentIndex = 0; AgentIndex < SquadAgents.Num(); ++AgentIndex)
	{
		ASteeringAgent* Agent = SquadAgents[AgentIndex].Agent;
		if (!IsValid(Agent))
		{
			continue;
		}

		// Green points/lines show the current slot handed to Unreal's path follower.
		const FVector2D Slot2D = SquadAgents[AgentIndex].AssignedSlot;
		FVector2D DebugTarget = Slot2D;

		const FVector SlotLocation{DebugTarget.X, DebugTarget.Y, DrawZ};
		const FColor SlotColor = SquadAgents[AgentIndex].bUsingRelaxedSlot ? FColor::Orange : FColor::Green;
		DrawDebugPoint(GetWorld(), SlotLocation, 14.f, SlotColor, false, -1.f, 0);
		DrawDebugLine(GetWorld(), Agent->GetActorLocation(), SlotLocation, SlotColor, false, -1.f, 0, 2.f);

		const FSquadAgent& SquadAgent = SquadAgents[AgentIndex];
		const bool bIsLowHealth = SquadAgent.Health <= SquadAgent.LowHealthThreshold;
		const FColor HealthColor = bIsLowHealth ? FColor::Red : FColor::Green;
		const FVector AgentDebugLocation = Agent->GetActorLocation() + FVector{0.f, 0.f, 130.f};
		const FString DebugLabel = FString::Printf(
			TEXT("Agent %d | %s\nHP %.0f/%.0f | %s"),
			AgentIndex + 1,
			ANSI_TO_TCHAR(GetStateLabel(SquadAgent.State)),
			SquadAgent.Health,
			SquadAgent.LowHealthThreshold,
			bIsLowHealth ? TEXT("LOW HEALTH") : TEXT("Healthy"));

		DrawDebugSphere(GetWorld(), Agent->GetActorLocation() + FVector{0.f, 0.f, 55.f}, 42.f, 12, HealthColor, false, -1.f, 0, 3.f);
		DrawDebugString(GetWorld(), AgentDebugLocation, DebugLabel, nullptr, HealthColor, 0.f, true, 1.1f);
	}

	if (IsValid(PatrolEnemy))
	{
		for (int32 PointIndex = 0; PointIndex < PatrolEnemyPoints.Num(); ++PointIndex)
		{
			const FVector& PatrolPoint = PatrolEnemyPoints[PointIndex];
			const bool bCurrentPoint = PointIndex == PatrolEnemyPointIndex;
			const FColor PointColor = bCurrentPoint ? FColor::Yellow : FColor::Blue;
			DrawDebugSphere(GetWorld(), PatrolPoint + FVector{0.f, 0.f, 20.f}, bCurrentPoint ? 55.f : 35.f, 12, PointColor, false, -1.f, 0, 3.f);

			if (PatrolEnemyPoints.IsValidIndex(PointIndex + 1))
			{
				DrawDebugLine(GetWorld(), PatrolPoint, PatrolEnemyPoints[PointIndex + 1], FColor::Blue, false, -1.f, 0, 2.f);
			}
			else if (PatrolEnemyPoints.Num() > 1)
			{
				DrawDebugLine(GetWorld(), PatrolPoint, PatrolEnemyPoints[0], FColor::Blue, false, -1.f, 0, 2.f);
			}
		}

		const FVector PatrolDebugLocation = PatrolEnemy->GetActorLocation() + FVector{0.f, 0.f, 130.f};
		DrawDebugSphere(GetWorld(), PatrolEnemy->GetActorLocation() + FVector{0.f, 0.f, 55.f}, 42.f, 12, FColor::Blue, false, -1.f, 0, 3.f);
		DrawDebugString(GetWorld(), PatrolDebugLocation, TEXT("Patrol Enemy\nState: Patrol"), nullptr, FColor::Blue, 0.f, true, 1.1f);
	}
}

void ALevel_SquadCoordination::UpdateImGui()
{
	// Build a small fixed ImGui panel for controls, performance stats, and live
	// tuning values used by this level.
	ImGui::SetNextWindowPos(WindowPos);
	ImGui::SetNextWindowSize(WindowSize);
	ImGui::Begin("Gameplay Programming", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

	ImGui::Text("CONTROLS");
	ImGui::Indent();
	ImGui::Text("LMB: set squad target");
	ImGui::Unindent();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Text("STATS");
	ImGui::Indent();
	ImGui::Text("%.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
	ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
	ImGui::Unindent();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::Text("Squad Coordination");
	ImGui::Indent();
	ImGui::Text("Agents: %d", SquadAgents.Num());
	ImGui::Text("Mode: %s", GetCoordinationModeLabel(CoordinationMode));
	const char* ModeLabels[] = {"Role-based formation", "Shared target baseline"};
	int CurrentMode = static_cast<int>(CoordinationMode);
	if (ImGui::Combo("Research mode", &CurrentMode, ModeLabels, IM_ARRAYSIZE(ModeLabels)))
	{
		CoordinationMode = static_cast<ESquadCoordinationMode>(CurrentMode);
		ResetTargetEvaluation();
		UpdateAutomaticFormation();
		for (FSquadAgent& SquadAgent : SquadAgents)
		{
			if (IsValid(SquadAgent.Agent))
			{
				SquadAgent.LastPosition = SquadAgent.Agent->GetPosition();
			}
		}
	}
	ImGui::Text("Average slot error: %.1f", ResearchMetrics.AverageSlotError);
	ImGui::Text("Max slot error: %.1f", ResearchMetrics.MaxSlotError);
	ImGui::Text("Average spacing error: %.1f", ResearchMetrics.AverageSpacingError);
	ImGui::Text("Stuck agents: %d | Relaxed slots: %d", ResearchMetrics.StuckAgentCount, ResearchMetrics.RelaxedSlotCount);
	ImGui::Text("Settled: %s | Time since target: %.2fs | Settle time: %.2fs",
		ResearchMetrics.bFormationSettled ? "yes" : "no",
		TimeSinceTargetSet,
		ResearchMetrics.TimeToSettle);
	ImGui::SliderFloat("Settled slot error", &SettledSlotError, 25.f, 300.f, "%.1f");
	ImGui::SliderFloat("Stuck progress distance", &StuckProgressDistance, 1.f, 80.f, "%.1f");
	ImGui::SliderFloat("Stuck time threshold", &StuckTimeThreshold, 0.25f, 5.f, "%.2f");
	bool bAutoFormationSettingsChanged = false;
	bAutoFormationSettingsChanged |= ImGui::Checkbox("Automatic formation", &bUseAutomaticFormation);
	bAutoFormationSettingsChanged |= ImGui::SliderFloat("Auto side probe distance", &AutoFormationSideProbeDistance, 150.f, 900.f, "%.1f");
	bAutoFormationSettingsChanged |= ImGui::SliderFloat("Auto forward probe distance", &AutoFormationForwardProbeDistance, 150.f, 1100.f, "%.1f");
	bAutoFormationSettingsChanged |= ImGui::SliderFloat("Auto probe radius", &AutoFormationProbeRadius, 20.f, 180.f, "%.1f");
	if (bAutoFormationSettingsChanged)
	{
		ResetTargetEvaluation();
		UpdateAutomaticFormation();
	}
	const char* RoleLabels[] = {"Leader", "Left Flank", "Right Flank", "Rear Support"};
	int NewAgentRoleIndex = static_cast<int>(NewAgentRole);
	if (ImGui::Combo("New agent role", &NewAgentRoleIndex, RoleLabels, IM_ARRAYSIZE(RoleLabels)))
	{
		NewAgentRole = static_cast<ESquadRoles>(NewAgentRoleIndex);
	}
	if (ImGui::Button("Add agent"))
	{
		const FVector SpawnCenter{
			MouseTarget.Position.X,
			MouseTarget.Position.Y,
			SpawnZ
		};
		AddAgentToSquad(SpawnCenter, NewAgentRole);
	}
	ImGui::SameLine();
	const bool bCanRemoveAgent = SquadAgents.Num() > 1;
	if (!bCanRemoveAgent)
	{
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Remove agent"))
	{
		RemoveAgentFromSquad();
	}
	if (!bCanRemoveAgent)
	{
		ImGui::EndDisabled();
	}
	ImGui::Text("Target: (%.1f, %.1f)", MouseTarget.Position.X, MouseTarget.Position.Y);
	if (ImGui::Checkbox("Debug Rendering", &bDrawDebug))
	{
		// Apply the debug toggle immediately to existing agents instead of
		// waiting for the next spawn.
		for (FSquadAgent& SquadAgent : SquadAgents)
		{
			if (IsValid(SquadAgent.Agent))
			{
				SquadAgent.Agent->SetDebugRenderingEnabled(bDrawDebug);
			}
		}
		if (IsValid(PatrolEnemy))
		{
			PatrolEnemy->SetDebugRenderingEnabled(bDrawDebug);
		}
	}
	ImGui::SliderFloat("Formation spacing", &FormationSpacing, 100.f, 700.f, "%.1f");
	ImGui::SliderFloat("Avoidance radius", &AgentAvoidanceRadius, 0.f, 900.f, "%.1f");
	ImGui::SliderFloat("Arrive stop radius", &ArriveStopRadius, 0.f, 150.f, "%.1f");
	ImGui::SliderFloat("Rejoin distance", &RejoinDistance, 100.f, 1200.f, "%.1f");
	ImGui::SliderFloat("Support ally distance", &SupportAllyDistance, 100.f, 700.f, "%.1f");
	ImGui::SliderFloat("Low health fallback distance", &LowHealthFallbackDistance, 150.f, 1200.f, "%.1f");
	ImGui::SliderFloat("Enemy range for support", &SupportEnemyDetectionRange, 100.f, 2000.f, "%.1f");
	ImGui::SliderFloat("Wounded safe distance", &WoundedSafeDistanceFromEnemy, 100.f, 2000.f, "%.1f");
	float PatrolRouteOffsetValues[2] = {
		static_cast<float>(PatrolRouteOffset.X),
		static_cast<float>(PatrolRouteOffset.Y)
	};
	if (ImGui::SliderFloat2("Patrol route offset", PatrolRouteOffsetValues, -1200.f, 1200.f, "%.1f"))
	{
		PatrolRouteOffset.X = PatrolRouteOffsetValues[0];
		PatrolRouteOffset.Y = PatrolRouteOffsetValues[1];

		if (IsValid(PatrolEnemy))
		{
			PatrolEnemy->SetActorLocation(FVector{
				MouseTarget.Position.X + 900.f + PatrolRouteOffset.X,
				MouseTarget.Position.Y + 900.f + PatrolRouteOffset.Y,
				SpawnZ
			});
			RebuildPatrolRoute();
		}
	}

	const char* FormationLabels[] = {"Wedge", "Column", "Line"};
	int CurrentFormation = static_cast<int>(SquadFormation);
	if (bUseAutomaticFormation)
	{
		ImGui::BeginDisabled();
	}
	if (ImGui::Combo("Formation", &CurrentFormation, FormationLabels, IM_ARRAYSIZE(FormationLabels)))
	{
		SquadFormation = static_cast<ESquadFormation>(CurrentFormation);
		ResetTargetEvaluation();
	}
	if (bUseAutomaticFormation)
	{
		ImGui::EndDisabled();
	}

	for (int32 AgentIndex = 0; AgentIndex < SquadAgents.Num(); ++AgentIndex)
	{
		FSquadAgent& SquadAgent = SquadAgents[AgentIndex];
		ImGui::PushID(AgentIndex);
		ImGui::Separator();
		const bool bIsLowHealth = SquadAgent.Health <= SquadAgent.LowHealthThreshold;
		ImGui::Text("Agent %d", AgentIndex + 1);
		ImGui::SameLine();
		ImGui::TextColored(
			bIsLowHealth ? ImVec4{1.f, 0.2f, 0.2f, 1.f} : ImVec4{0.2f, 1.f, 0.35f, 1.f},
			bIsLowHealth ? "LOW HEALTH" : "Healthy");
		ImGui::Text("Current state: %s", GetStateLabel(SquadAgent.State));

		int CurrentRole = static_cast<int>(SquadAgent.Role);
		if (ImGui::Combo("Role", &CurrentRole, RoleLabels, IM_ARRAYSIZE(RoleLabels)))
		{
			SquadAgent.Role = static_cast<ESquadRoles>(CurrentRole);
		}
		ImGui::SliderFloat("Health", &SquadAgent.Health, 0.f, 100.f, "%.1f");
		ImGui::SliderFloat("Low health threshold", &SquadAgent.LowHealthThreshold, 0.f, 100.f, "%.1f");
		ImGui::PopID();
	}

	if (IsValid(PatrolEnemy))
	{
		AAIController* PatrolController = Cast<AAIController>(PatrolEnemy->GetController());
		UBlackboardComponent* PatrolBlackboard = PatrolController
			? PatrolController->GetBlackboardComponent()
			: nullptr;
		ImGui::Text("Patrol enemy state: %s", GetStateLabel(GetStateFromBlackboard(PatrolBlackboard)));
		ImGui::Text("Patrol points: %d | Current: %d", PatrolEnemyPoints.Num(), PatrolEnemyPointIndex + 1);
	}
	ImGui::Unindent();

	ImGui::End();
}

const char* ALevel_SquadCoordination::GetStateLabel(ESquadAgentState State)
{
	switch (State)
	{
	case ESquadAgentState::FollowFormation:
		return "Follow Formation";
	case ESquadAgentState::RejoinSquad:
		return "Rejoin Squad";
	case ESquadAgentState::LowHealthFallback:
		return "Low Health Fallback";
	case ESquadAgentState::SupportLowHealthAlly:
		return "Support Low Health Ally";
	case ESquadAgentState::Patrol:
		return "Patrol";
	default:
		return "Unknown";
	}
}

ESquadRoles ALevel_SquadCoordination::GetRoleForAgentIndex(int32 AgentIndex) const
{
	switch (AgentIndex)
	{
	case 0:
		return ESquadRoles::Leader;
	case 1:
		return ESquadRoles::LeftFlank;
	case 2:
		return ESquadRoles::RightFlank;
	default:
		return ESquadRoles::RearSupport;
	}
}

int32 ALevel_SquadCoordination::GetRoleOccurrenceIndex(int32 AgentIndex) const
{
	if (!SquadAgents.IsValidIndex(AgentIndex))
	{
		return 0;
	}

	const ESquadRoles SquadRole = SquadAgents[AgentIndex].Role;
	int32 OccurrenceIndex = 0;
	for (int32 OtherAgentIndex = 0; OtherAgentIndex < AgentIndex; ++OtherAgentIndex)
	{
		if (SquadAgents[OtherAgentIndex].Role == SquadRole)
		{
			++OccurrenceIndex;
		}
	}

	return OccurrenceIndex;
}

FVector2D ALevel_SquadCoordination::GetFormationOffset(int32 AgentIndex) const
{
	if (!SquadAgents.IsValidIndex(AgentIndex))
	{
		return FVector2D::ZeroVector;
	}

	switch (SquadFormation)
	{
	case ESquadFormation::Wedge:
		return GetWedgeFormationOffsetForRole(SquadAgents[AgentIndex].Role, GetRoleOccurrenceIndex(AgentIndex));

	case ESquadFormation::Column:
		return RotateFormationOffset(FVector2D{-FormationSpacing * static_cast<float>(AgentIndex), 0.f});

	case ESquadFormation::Line:
	{
		const float CenteredIndex = static_cast<float>(AgentIndex) - (static_cast<float>(SquadAgents.Num() - 1) * 0.5f);
		return RotateFormationOffset(FVector2D{0.f, FormationSpacing * CenteredIndex});
	}

	default:
		return FVector2D::ZeroVector;
	}
}

FVector2D ALevel_SquadCoordination::GetDesiredSlotForAgent(int32 AgentIndex) const
{
	if (CoordinationMode == ESquadCoordinationMode::SharedTargetBaseline)
	{
		return MouseTarget.Position;
	}

	return MouseTarget.Position + GetFormationOffset(AgentIndex);
}

FVector2D ALevel_SquadCoordination::RotateFormationOffset(const FVector2D& LocalOffset) const
{
	const FVector2D Forward = GetFormationForward();
	const FVector2D Right = GetFormationRight();
	return Forward * LocalOffset.X + Right * LocalOffset.Y;
}

FVector2D ALevel_SquadCoordination::GetFormationForward() const
{
	float FormationYawRadians = 0.f;
	if (!SquadAgents.IsEmpty() && IsValid(SquadAgents[0].Agent))
	{
		FormationYawRadians = FMath::DegreesToRadians(SquadAgents[0].Agent->GetActorRotation().Yaw);
	}

	return FVector2D{FMath::Cos(FormationYawRadians), FMath::Sin(FormationYawRadians)};
}

FVector2D ALevel_SquadCoordination::GetFormationRight() const
{
	const FVector2D Forward = GetFormationForward();
	return FVector2D{-Forward.Y, Forward.X};
}

FVector2D ALevel_SquadCoordination::GetWedgeFormationOffsetForRole(ESquadRoles SquadRole, int32 RoleOccurrenceIndex) const
{
	FVector2D LocalOffset = FVector2D::ZeroVector;
	const float RoleRankOffset = static_cast<float>(RoleOccurrenceIndex);

	switch (SquadRole)
	{
	case ESquadRoles::Leader:
		LocalOffset = FVector2D{-FormationSpacing * RoleRankOffset, 0.f};
		break;
	case ESquadRoles::LeftFlank:
		LocalOffset = FVector2D{
			-FormationSpacing * (RoleRankOffset + 1.f),
			-FormationSpacing * (RoleRankOffset + 1.f)
		};
		break;
	case ESquadRoles::RightFlank:
		LocalOffset = FVector2D{
			-FormationSpacing * (RoleRankOffset + 1.f),
			FormationSpacing * (RoleRankOffset + 1.f)
		};
		break;
	case ESquadRoles::RearSupport:
		LocalOffset = FVector2D{-FormationSpacing * (RoleRankOffset + 2.f), 0.f};
		break;
	default:
		return FVector2D::ZeroVector;
	}

	return RotateFormationOffset(LocalOffset);
}

bool ALevel_SquadCoordination::TryGetValidNavSlot(const FVector2D& DesiredSlot, FVector2D& OutValidSlot) const
{
	const FVector DesiredLocation{DesiredSlot.X, DesiredSlot.Y, SpawnZ};

	FNavLocation ProjectedLocation{};
	const FVector QueryExtent{150.f, 150.f, 300.f};

	if (const UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		if (NavSystem->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, QueryExtent))
		{
			OutValidSlot = FVector2D{ProjectedLocation.Location.X, ProjectedLocation.Location.Y};
			return true;
		}
	}

	return false;
}

bool ALevel_SquadCoordination::IsNavigationSpaceAvailable(const FVector2D& TestPoint, float ProbeRadius) const
{
	const FVector DesiredLocation{TestPoint.X, TestPoint.Y, SpawnZ};
	FNavLocation ProjectedLocation{};
	const FVector QueryExtent{
		FMath::Max(1.f, ProbeRadius),
		FMath::Max(1.f, ProbeRadius),
		300.f
	};

	if (const UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		return NavSystem->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, QueryExtent);
	}

	return false;
}
