// Fill out your copyright notice in the Description page of Project Settings.


#include "GameAIController.h"

#include "DecisionMaking/BehaviorTree/GameAIBehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "GameFramework/Actor.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	bool HasBlackboardKey(const UBlackboardData& BlackboardData, const FName KeyName)
	{
		for (const FBlackboardEntry& Entry : BlackboardData.Keys)
		{
			if (Entry.EntryName == KeyName)
			{
				return true;
			}
		}

		return false;
	}

	void EnsureObjectKey(UBlackboardData& BlackboardData, const FName KeyName, UClass* BaseClass)
	{
		if (HasBlackboardKey(BlackboardData, KeyName))
		{
			return;
		}

		FBlackboardEntry Entry;
		Entry.EntryName = KeyName;

		UBlackboardKeyType_Object* KeyType = NewObject<UBlackboardKeyType_Object>(&BlackboardData);
		KeyType->BaseClass = BaseClass;
		Entry.KeyType = KeyType;

		BlackboardData.Keys.Add(Entry);
	}

	template <typename KeyType>
	void EnsureKey(UBlackboardData& BlackboardData, const FName KeyName)
	{
		if (HasBlackboardKey(BlackboardData, KeyName))
		{
			return;
		}

		FBlackboardEntry Entry;
		Entry.EntryName = KeyName;
		Entry.KeyType = NewObject<KeyType>(&BlackboardData);

		BlackboardData.Keys.Add(Entry);
	}

	void EnsureSquadBlackboardKeys(UBlackboardData& BlackboardData)
	{
		EnsureObjectKey(BlackboardData, TEXT("SquadLeader"), AActor::StaticClass());
		EnsureObjectKey(BlackboardData, TEXT("LowHealthAlly"), AActor::StaticClass());
		EnsureKey<UBlackboardKeyType_Vector>(BlackboardData, TEXT("SquadSlotLocation"));
		EnsureKey<UBlackboardKeyType_Vector>(BlackboardData, TEXT("SafeLocation"));
		EnsureKey<UBlackboardKeyType_Vector>(BlackboardData, TEXT("PatrolLocation"));
		EnsureKey<UBlackboardKeyType_Float>(BlackboardData, TEXT("Health"));
		EnsureKey<UBlackboardKeyType_Float>(BlackboardData, TEXT("LowHealthThreshold"));
		EnsureKey<UBlackboardKeyType_Float>(BlackboardData, TEXT("DistanceToSlot"));
		EnsureKey<UBlackboardKeyType_Float>(BlackboardData, TEXT("FormationRole"));
		EnsureKey<UBlackboardKeyType_Float>(BlackboardData, TEXT("AgentState"));
		EnsureKey<UBlackboardKeyType_Bool>(BlackboardData, TEXT("IsLowHealth"));
		EnsureKey<UBlackboardKeyType_Bool>(BlackboardData, TEXT("IsEnemyInRange"));
		EnsureKey<UBlackboardKeyType_Bool>(BlackboardData, TEXT("IsUnsafeFromEnemy"));
		EnsureKey<UBlackboardKeyType_Bool>(BlackboardData, TEXT("IsLowHealthAllyUnsafe"));
		EnsureKey<UBlackboardKeyType_Bool>(BlackboardData, TEXT("HasLowHealthAlly"));
		EnsureKey<UBlackboardKeyType_Bool>(BlackboardData, TEXT("IsTooFarFromFormation"));
		EnsureKey<UBlackboardKeyType_Bool>(BlackboardData, TEXT("IsPatrolOnly"));
	}
}


// Sets default values
AGameAIController::AGameAIController()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	BrainComponent = CreateDefaultSubobject<UGameAIBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"));

	static ConstructorHelpers::FObjectFinder<UBlackboardData> DefaultBlackboardAsset(
		TEXT("/Game/DecisionMaking/BB_TEST.BB_TEST"));
	if (DefaultBlackboardAsset.Succeeded())
	{
		BehaviorTreeBlackboardAsset = DefaultBlackboardAsset.Object;
	}
}

// Called when the game starts or when spawned
void AGameAIController::BeginPlay()
{
	Super::BeginPlay();
	
	// Create Blackboard if need be
	InitBehaviorTree();
}

// Called every frame
void AGameAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGameAIController::InitBehaviorTree()
{
	if (!BehaviorTreeBlackboardAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("BehaviorTree: No blackboard asset assigned on %s."), *GetName());
		return;
	}

	EnsureSquadBlackboardKeys(*BehaviorTreeBlackboardAsset);

	UBlackboardComponent* BlackboardComp = Blackboard;
	if (!UseBlackboard(BehaviorTreeBlackboardAsset, BlackboardComp))
	{
		UE_LOG(LogTemp, Error, TEXT("BehaviorTree: Failed to initialize blackboard asset %s on %s."),
			*BehaviorTreeBlackboardAsset->GetName(),
			*GetName());
		return;
	}

	Blackboard = BlackboardComp;
}

void AGameAIController::RunBehaviorTreeLogic()
{
	UGameAIBehaviorTreeComponent* BehaviorTreeComp = FindComponentByClass<UGameAIBehaviorTreeComponent>();
	if (ensure(BehaviorTreeComp))
	{
		BehaviorTreeComp->StartLogic();
	}
}



