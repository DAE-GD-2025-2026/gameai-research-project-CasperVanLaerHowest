// Fill out your copyright notice in the Description page of Project Settings.

#include "Level_SquadCoordination.h"

#include "DrawDebugHelpers.h"
#include "InputCoreTypes.h"
#include "imgui.h"

ALevel_SquadCoordination::ALevel_SquadCoordination()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ALevel_SquadCoordination::BeginPlay()
{
	Super::BeginPlay();

	if (TrimWorld)
	{
		TrimWorld->SetTrimWorldSize(3200.f);
		TrimWorld->bShouldTrimWorld = true;
	}

	const FVector SpawnCenter = GetNavMeshBoundsCenter(SpawnZ).value_or(FVector{0.f, 0.f, SpawnZ});
	MouseTarget.Position = FVector2D{SpawnCenter};
	SpawnSquad(SpawnCenter);
	UpdateSquadTargets();
}

void ALevel_SquadCoordination::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (PlayerController)
	{
		const bool bLeftMouseDown = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
		if (bLeftMouseDown && !bWasLeftMouseDown && !ImGui::GetIO().WantCaptureMouse)
		{
			SetSquadTargetFromMouse();
		}
		bWasLeftMouseDown = bLeftMouseDown;
	}

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

	if (!PlayerEnhancedInputComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("SquadCoordination: Enhanced input component missing, using direct LMB fallback."));
		return;
	}

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
	SquadAgents.Reset();
	ArriveBehaviors.clear();

	const int32 ClampedSquadSize = FMath::Max(1, SquadSize);
	SquadAgents.Reserve(ClampedSquadSize);
	ArriveBehaviors.reserve(ClampedSquadSize);

	for (int32 AgentIndex = 0; AgentIndex < ClampedSquadSize; ++AgentIndex)
	{
		const FVector2D Offset = GetFormationOffset(AgentIndex);
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
			continue;
		}

		Agent->SpawnDefaultController();
		Agent->SetDebugRenderingEnabled(bDrawDebug);

		auto ArriveBehavior = std::make_unique<Arrive>();
		Agent->SetSteeringBehavior(ArriveBehavior.get());

		SquadAgents.Add(Agent);
		ArriveBehaviors.emplace_back(std::move(ArriveBehavior));
	}
}

void ALevel_SquadCoordination::SetSquadTargetFromMouse()
{
	if (const auto MouseWorldPos = GetMouseWorldPos(); MouseWorldPos.has_value())
	{
		LatestMouseWorldPos = MouseWorldPos.value();
	}

	MouseTarget.Position = FVector2D{LatestMouseWorldPos};
	UpdateSquadTargets();
}

void ALevel_SquadCoordination::UpdateSquadTargets()
{
	const int32 AgentCount = FMath::Min(SquadAgents.Num(), static_cast<int32>(ArriveBehaviors.size()));
	for (int32 AgentIndex = 0; AgentIndex < AgentCount; ++AgentIndex)
	{
		if (!IsValid(SquadAgents[AgentIndex]) || !ArriveBehaviors[AgentIndex])
		{
			continue;
		}

		FTargetData FormationTarget{};
		FormationTarget.Position = MouseTarget.Position + GetFormationOffset(AgentIndex);
		ArriveBehaviors[AgentIndex]->SetTarget(FormationTarget);
		SquadAgents[AgentIndex]->SetDebugRenderingEnabled(bDrawDebug);
	}
}

void ALevel_SquadCoordination::DrawSquadDebug() const
{
	const float DrawZ = SpawnZ + 15.f;
	const FVector SquadTarget{MouseTarget.Position.X, MouseTarget.Position.Y, DrawZ};
	DrawDebugSphere(GetWorld(), SquadTarget, 70.f, 16, FColor::Cyan, false, -1.f, 0, 3.f);

	for (int32 AgentIndex = 0; AgentIndex < SquadAgents.Num(); ++AgentIndex)
	{
		const ASteeringAgent* Agent = SquadAgents[AgentIndex];
		if (!IsValid(Agent))
		{
			continue;
		}

		const FVector2D Slot2D = MouseTarget.Position + GetFormationOffset(AgentIndex);
		const FVector SlotLocation{Slot2D.X, Slot2D.Y, DrawZ};
		DrawDebugPoint(GetWorld(), SlotLocation, 14.f, FColor::Green, false, -1.f, 0);
		DrawDebugLine(GetWorld(), Agent->GetActorLocation(), SlotLocation, FColor::Green, false, -1.f, 0, 2.f);
	}
}

void ALevel_SquadCoordination::UpdateImGui()
{
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
	ImGui::Text("Target: (%.1f, %.1f)", MouseTarget.Position.X, MouseTarget.Position.Y);
	if (ImGui::Checkbox("Debug Rendering", &bDrawDebug))
	{
		for (ASteeringAgent* Agent : SquadAgents)
		{
			if (IsValid(Agent))
			{
				Agent->SetDebugRenderingEnabled(bDrawDebug);
			}
		}
	}
	ImGui::SliderFloat("Formation spacing", &FormationSpacing, 100.f, 700.f, "%.1f");
	ImGui::Unindent();

	ImGui::End();
}

FVector2D ALevel_SquadCoordination::GetFormationOffset(int32 AgentIndex) const
{
	if (AgentIndex == 0)
	{
		return FVector2D::ZeroVector;
	}

	const int32 RingIndex = (AgentIndex - 1) / 6 + 1;
	const int32 IndexInRing = (AgentIndex - 1) % 6;
	const float Angle = static_cast<float>(IndexInRing) * UE_TWO_PI / 6.f;
	const float Radius = FormationSpacing * static_cast<float>(RingIndex);

	return FVector2D{
		FMath::Cos(Angle) * Radius,
		FMath::Sin(Angle) * Radius
	};
}
