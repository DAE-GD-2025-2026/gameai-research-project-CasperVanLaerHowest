// Fill out your copyright notice in the Description page of Project Settings.


#include "TwoDLSystem.h"


// Sets default values
ATwoDLSystem::ATwoDLSystem()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	SegmentInstances =
		CreateDefaultSubobject<UInstancedStaticMeshComponent>(
			TEXT("SegmentInstances"));

	SetRootComponent(SegmentInstances);
	
	RewriteRules.Add(TEXT("X"), TEXT("F+[[X]-X]-F[-FX]+X"));
	RewriteRules.Add(TEXT("F"), TEXT("FF"));
	
	m_StartAxiom = m_Axiom;
}

// Called when the game starts or when spawned
void ATwoDLSystem::BeginPlay()
{
	Super::BeginPlay();
	
	RerunAllGen();
}

// Called every frame
void ATwoDLSystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ATwoDLSystem::RunGeneration(FString &CurrentString)
{
	FString GeneratedString;

	for (int32 i = 0; i < CurrentString.Len(); ++i)
	{
		FString Letter = CurrentString.Mid(i, 1);
		if (RewriteRules.Contains(Letter))
		{
			GeneratedString += RewriteRules[Letter];
		}
		else
		{
			GeneratedString += Letter;
		}
	}
	
	CurrentString = GeneratedString;
}

void ATwoDLSystem::RerunAllGen()
{
	m_Axiom = m_StartAxiom;
	
	for (uint8 i = 0; i < m_Redos; ++i)
	{
		RunGeneration(m_Axiom);
	}
	
	DrawAxiom();
}

void ATwoDLSystem::DrawAxiom()
{
	SegmentInstances->ClearInstances();

	FVector Position = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;

	struct FTurtleState
	{
		FVector Position;
		FRotator Rotation;
	};

	TArray<FTurtleState> StateStack;

	for (const TCHAR Symbol : m_Axiom)
	{
		switch (Symbol)
		{
		case TEXT('F'):
			{
				const FVector Direction = Rotation.Vector();
				const FVector Center =
					Position + Direction * SegmentLength * 0.5f;

				const FTransform Transform(
					Rotation,
					Center,
					FVector(SegmentLength / 100.0f, 0.1f, 0.1f));
				
				SegmentInstances->AddInstance(Transform, false);

				Position += Direction * SegmentLength;
				break;
			}

		case TEXT('+'):
			Rotation.Yaw += TurnAngle;
			break;

		case TEXT('-'):
			Rotation.Yaw -= TurnAngle;
			break;

		case TEXT('['):
			StateStack.Add({Position, Rotation});
			break;

		case TEXT(']'):
			if (!StateStack.IsEmpty())
			{
				const FTurtleState SavedState = StateStack.Pop();
				Position = SavedState.Position;
				Rotation = SavedState.Rotation;
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("L-System contains an unmatched closing bracket."));
			}
			break;

		default:
			// X is currently ignored while drawing.
			break;
		}
	}

	if (!StateStack.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("L-System contains %d unmatched opening bracket(s)."),
			StateStack.Num());
	}
}

void ATwoDLSystem::SetTurnAngle(float NewAngle)
{
	TurnAngle = FMath::Clamp(NewAngle, 0.0f, 360.0f);
	RerunAllGen();
}

void ATwoDLSystem::SetSegmentLength(float NewLength)
{
	SegmentLength = FMath::Max(NewLength, 1.0f);
	RerunAllGen();
}

void ATwoDLSystem::SetRedoCount(int32 NewCount)
{
	// Keep this limited: L-system size grows extremely quickly.
	m_Redos = static_cast<uint8>(FMath::Clamp(NewCount, 1, 8));
	RerunAllGen();
}
