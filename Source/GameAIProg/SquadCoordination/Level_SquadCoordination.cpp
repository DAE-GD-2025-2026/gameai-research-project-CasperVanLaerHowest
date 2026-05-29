// Fill out your copyright notice in the Description page of Project Settings.

#include "Level_SquadCoordination.h"

#include "DrawDebugHelpers.h"
#include "InputCoreTypes.h"
#include "imgui.h"

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
	UpdateSquadTargets();
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
	UpdateSquadTargets();

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
	SquadAgents.Add(SquadAgent);
	ArriveBehaviors.emplace_back(std::move(ArriveBehavior));
	SquadSize = SquadAgents.Num();

	UpdateSquadTargets();
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
	UpdateSquadTargets();
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
	UpdateSquadTargets();
}

void ALevel_SquadCoordination::UpdateSquadTargets()
{
	// Only update pairs that exist in both arrays. This protects against failed
	// spawns or any future change that could leave the arrays out of sync.
	const int32 AgentCount = FMath::Min(SquadAgents.Num(), static_cast<int32>(ArriveBehaviors.size()));
	TArray<ASteeringAgent*> ActiveAgents{};
	ActiveAgents.Reserve(AgentCount);
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

		// Move each agent toward its own formation slot relative to the current
		// squad target, rather than sending every agent to the exact same point.
		FTargetData FormationTarget{};
		FormationTarget.Position = MouseTarget.Position + GetFormationOffset(AgentIndex);
		ArriveBehaviors[AgentIndex]->ClearAgentsToAvoid();
		for (ASteeringAgent* ActiveAgent : ActiveAgents)
		{
			ArriveBehaviors[AgentIndex]->AddAgentToAvoid(ActiveAgent);
		}
		ArriveBehaviors[AgentIndex]->SetAvoidanceRadius(AgentAvoidanceRadius);
		ArriveBehaviors[AgentIndex]->SetTarget(FormationTarget);
		SquadAgent.Agent->SetDebugRenderingEnabled(bDrawDebug);
	}
}

void ALevel_SquadCoordination::DrawSquadDebug() const
{
	// Draw the shared squad target slightly above the ground so it is visible.
	const float DrawZ = SpawnZ + 15.f;
	const FVector SquadTarget{MouseTarget.Position.X, MouseTarget.Position.Y, DrawZ};
	DrawDebugSphere(GetWorld(), SquadTarget, 70.f, 16, FColor::Cyan, false, -1.f, 0, 3.f);

	for (int32 AgentIndex = 0; AgentIndex < SquadAgents.Num(); ++AgentIndex)
	{
		const ASteeringAgent* Agent = SquadAgents[AgentIndex].Agent;
		if (!IsValid(Agent))
		{
			continue;
		}

		// Green points show the exact slot each agent is trying to reach, and
		// green lines make it easy to see which agent is assigned to each slot.
		const FVector2D Slot2D = MouseTarget.Position + GetFormationOffset(AgentIndex);
		const FVector SlotLocation{Slot2D.X, Slot2D.Y, DrawZ};
		DrawDebugPoint(GetWorld(), SlotLocation, 14.f, FColor::Green, false, -1.f, 0);
		DrawDebugLine(GetWorld(), Agent->GetActorLocation(), SlotLocation, FColor::Green, false, -1.f, 0, 2.f);
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
	}
	ImGui::SliderFloat("Formation spacing", &FormationSpacing, 100.f, 700.f, "%.1f");
	ImGui::SliderFloat("Avoidance radius", &AgentAvoidanceRadius, 0.f, 900.f, "%.1f");

	const char* FormationLabels[] = {"Wedge", "Column", "Line"};
	int CurrentFormation = static_cast<int>(SquadFormation);
	if (ImGui::Combo("Formation", &CurrentFormation, FormationLabels, IM_ARRAYSIZE(FormationLabels)))
	{
		SquadFormation = static_cast<ESquadFormation>(CurrentFormation);
	}

	for (int32 AgentIndex = 0; AgentIndex < SquadAgents.Num(); ++AgentIndex)
	{
		FSquadAgent& SquadAgent = SquadAgents[AgentIndex];
		ImGui::PushID(AgentIndex);
		int CurrentRole = static_cast<int>(SquadAgent.Role);
		if (ImGui::Combo("Role", &CurrentRole, RoleLabels, IM_ARRAYSIZE(RoleLabels)))
		{
			SquadAgent.Role = static_cast<ESquadRoles>(CurrentRole);
		}
		ImGui::SameLine();
		ImGui::Text("Agent %d", AgentIndex + 1);
		ImGui::PopID();
	}
	ImGui::Unindent();

	ImGui::End();
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

FVector2D ALevel_SquadCoordination::RotateFormationOffset(const FVector2D& LocalOffset) const
{
	float FormationYawRadians = 0.f;
	if (!SquadAgents.IsEmpty() && IsValid(SquadAgents[0].Agent))
	{
		FormationYawRadians = FMath::DegreesToRadians(SquadAgents[0].Agent->GetActorRotation().Yaw);
	}

	const float CosYaw = FMath::Cos(FormationYawRadians);
	const float SinYaw = FMath::Sin(FormationYawRadians);
	return FVector2D{
		LocalOffset.X * CosYaw - LocalOffset.Y * SinYaw,
		LocalOffset.X * SinYaw + LocalOffset.Y * CosYaw
	};
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
