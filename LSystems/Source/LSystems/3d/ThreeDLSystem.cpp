// Fill out your copyright notice in the Description page of Project Settings.

#include "ThreeDLSystem.h"

#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Math/RotationMatrix.h"

// Sets default values
AThreeDLSystem::AThreeDLSystem()
{
	PrimaryActorTick.bCanEverTick = true;

	BranchMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BranchMesh"));
	SetRootComponent(BranchMesh);

	BranchMesh->bUseAsyncCooking = true;

	RewriteRules.Add(TEXT("X"), TEXT("F[+!X][-!X][&!X][^!X]FX"));
	RewriteRules.Add(TEXT("F"), TEXT("FF"));

	FRandomRewriteRule RandomFRule;
	RandomFRule.Symbol = TEXT("F");

	FRandomRewriteOption StraightOption;
	StraightOption.Replacement = TEXT("FF");
	StraightOption.Chance = 0.40f;
	RandomFRule.Options.Add(StraightOption);

	FRandomRewriteOption YawBranchOption;
	YawBranchOption.Replacement = TEXT("F[+F]F[-F]F");
	YawBranchOption.Chance = 0.35f;
	RandomFRule.Options.Add(YawBranchOption);

	FRandomRewriteOption PitchBranchOption;
	PitchBranchOption.Replacement = TEXT("F[&F]F[^F]F");
	PitchBranchOption.Chance = 0.25f;
	RandomFRule.Options.Add(PitchBranchOption);

	RandomRewriteRules.Add(RandomFRule);

	m_StartAxiom = m_Axiom;
	GenerationRandomStream.Initialize(RandomSeed);
	AngleRandomStream.Initialize(RandomSeed ^ 0x5F3759DF);
}

void AThreeDLSystem::BeginPlay()
{
	Super::BeginPlay();

	if (!OrbitCamera)
	{
		OrbitCamera = Cast<ACameraActor>(UGameplayStatics::GetActorOfClass(
			this, ACameraActor::StaticClass()));
	}

	InitializeCameraOrbit();
	RerunAllGen();
}

void AThreeDLSystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bOrbitCamera || !OrbitCamera)
	{
		return;
	}

	CameraOrbitAngleRadians += FMath::DegreesToRadians(CameraOrbitSpeed) * DeltaTime;
	const FVector ActorLocation = GetActorLocation();
	const FVector NewCameraLocation(
		ActorLocation.X + FMath::Cos(CameraOrbitAngleRadians) * CameraOrbitDistance,
		ActorLocation.Y + FMath::Sin(CameraOrbitAngleRadians) * CameraOrbitDistance,
		ActorLocation.Z + CameraOrbitHeight);
	const FVector LookAtLocation = ActorLocation + FVector(0.0f, 0.0f, CameraTargetHeight);

	OrbitCamera->SetActorLocation(NewCameraLocation);
	OrbitCamera->SetActorRotation(
		UKismetMathLibrary::FindLookAtRotation(NewCameraLocation, LookAtLocation));
}

void AThreeDLSystem::InitializeCameraOrbit()
{
	if (!OrbitCamera)
	{
		return;
	}

	const FVector Offset = OrbitCamera->GetActorLocation() - GetActorLocation();
	if (CameraOrbitDistance <= KINDA_SMALL_NUMBER)
	{
		CameraOrbitDistance = FMath::Max(FVector2D(Offset.X, Offset.Y).Length(), 1.0f);
	}
	CameraOrbitHeight = Offset.Z;
	CameraOrbitAngleRadians = FMath::Atan2(Offset.Y, Offset.X);
}

void AThreeDLSystem::RunGeneration(FString& CurrentString)
{
	FString GeneratedString;

	for (int32 i = 0; i < CurrentString.Len(); ++i)
	{
		const FString Letter = CurrentString.Mid(i, 1);
		FString RandomReplacement;
		if (TryGetRandomReplacement(Letter, RandomReplacement))
		{
			GeneratedString += RandomReplacement;
		}
		else if (const FString* Replacement = RewriteRules.Find(Letter))
		{
			GeneratedString += *Replacement;
		}
		else
		{
			GeneratedString += Letter;
		}
	}

	CurrentString = GeneratedString;
}

bool AThreeDLSystem::TryGetRandomReplacement(
	const FString& Symbol,
	FString& OutReplacement)
{
	const FRandomRewriteRule* MatchingRule = RandomRewriteRules.FindByPredicate(
		[&Symbol](const FRandomRewriteRule& Rule)
		{
			return Rule.Symbol == Symbol;
		});

	if (!MatchingRule)
	{
		return false;
	}

	float TotalChance = 0.0f;
	for (const FRandomRewriteOption& Option : MatchingRule->Options)
	{
		TotalChance += FMath::Max(Option.Chance, 0.0f);
	}

	if (TotalChance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float SelectedChance = GenerationRandomStream.FRandRange(0.0f, TotalChance);
	float AccumulatedChance = 0.0f;
	const FRandomRewriteOption* LastValidOption = nullptr;

	for (const FRandomRewriteOption& Option : MatchingRule->Options)
	{
		if (Option.Chance <= 0.0f)
		{
			continue;
		}

		LastValidOption = &Option;
		AccumulatedChance += Option.Chance;
		if (SelectedChance <= AccumulatedChance)
		{
			OutReplacement = Option.Replacement;
			return true;
		}
	}

	// Protect against floating-point rounding at the upper edge of the range.
	if (LastValidOption)
	{
		OutReplacement = LastValidOption->Replacement;
		return true;
	}

	return false;
}

void AThreeDLSystem::RerunAllGen()
{
	m_Axiom = m_StartAxiom;
	GenerationRandomStream.Initialize(RandomSeed);

	for (uint8 i = 0; i < m_Redos; ++i)
	{
		RunGeneration(m_Axiom);
	}

	DrawAxiom();
}

void AThreeDLSystem::DrawAxiom()
{
	BranchMesh->ClearAllMeshSections();
	AngleRandomStream.Initialize(RandomSeed ^ 0x5F3759DF);

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	FVector Position = FVector::ZeroVector;
	FRotator Rotation = FRotator(90.0f, 0.0f, 0.0f);
	float CurrentWidth = FMath::Max(StartWidth, MinWidth);

	struct FTurtleState3D
	{
		FVector Position;
		FRotator Rotation;
		float Width;
	};

	TArray<FTurtleState3D> StateStack;

	for (const TCHAR Symbol : m_Axiom)
	{
		switch (Symbol)
		{
		case TEXT('F'):
			{
				const FVector Direction = Rotation.Vector();
				const FVector Start = Position;
				const FVector End = Position + Direction * SegmentLength;
				const float EndWidth = FMath::Max(CurrentWidth * WidthMultiplier, MinWidth);

				AddBranchCylinder(
					Start,
					End,
					CurrentWidth * 0.5f,
					EndWidth * 0.5f,
					Vertices,
					Triangles,
					Normals,
					UVs,
					VertexColors,
					Tangents);

				Position = End;
				CurrentWidth = EndWidth;
				break;
			}

		case TEXT('+'):
			Rotation.Yaw += GetRandomTurnAngle();
			break;

		case TEXT('-'):
			Rotation.Yaw -= GetRandomTurnAngle();
			break;

		case TEXT('&'):
			Rotation.Pitch += GetRandomTurnAngle();
			break;

		case TEXT('^'):
			Rotation.Pitch -= GetRandomTurnAngle();
			break;

		case TEXT('\\'):
			Rotation.Roll += GetRandomTurnAngle();
			break;

		case TEXT('/'):
			Rotation.Roll -= GetRandomTurnAngle();
			break;

		case TEXT('!'):
			CurrentWidth = FMath::Max(CurrentWidth * WidthMultiplier, MinWidth);
			break;

		case TEXT('['):
			StateStack.Add({Position, Rotation, CurrentWidth});
			break;

		case TEXT(']'):
			if (!StateStack.IsEmpty())
			{
				const FTurtleState3D SavedState = StateStack.Pop();
				Position = SavedState.Position;
				Rotation = SavedState.Rotation;
				CurrentWidth = SavedState.Width;
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("3D L-System contains an unmatched closing bracket."));
			}
			break;

		default:
			// X and other control symbols are ignored while drawing.
			break;
		}
	}

	if (!StateStack.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("3D L-System contains %d unmatched opening bracket(s)."),
			StateStack.Num());
	}

	if (!Vertices.IsEmpty())
	{
		BranchMesh->CreateMeshSection_LinearColor(
			0,
			Vertices,
			Triangles,
			Normals,
			UVs,
			VertexColors,
			Tangents,
			bCreateCollision);

		if (BranchMaterial)
		{
			BranchMesh->SetMaterial(0, BranchMaterial);
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("3D L-System generated %d symbols and %d branch vertices."),
		m_Axiom.Len(),
		Vertices.Num());
}

void AThreeDLSystem::SetTurnAngle(float NewAngle)
{
	TurnAngle = FMath::Clamp(NewAngle, 0.0f, 360.0f);
	RerunAllGen();
}

void AThreeDLSystem::SetSegmentLength(float NewLength)
{
	SegmentLength = FMath::Max(NewLength, 1.0f);
	RerunAllGen();
}

void AThreeDLSystem::SetRedoCount(int32 NewCount)
{
	m_Redos = static_cast<uint8>(FMath::Clamp(NewCount, 1, 8));
	RerunAllGen();
}

void AThreeDLSystem::SetStartWidth(float NewWidth)
{
	StartWidth = FMath::Max(NewWidth, MinWidth);
	RerunAllGen();
}

void AThreeDLSystem::SetWidthMultiplier(float NewMultiplier)
{
	WidthMultiplier = FMath::Clamp(NewMultiplier, 0.01f, 1.0f);
	RerunAllGen();
}

void AThreeDLSystem::SetRandomSeed(int32 NewSeed)
{
	RandomSeed = NewSeed;
	RerunAllGen();
}

void AThreeDLSystem::SetAngleVariationPercent(float NewVariationPercent)
{
	AngleVariationPercent = FMath::Clamp(NewVariationPercent, 0.0f, 100.0f);
	RerunAllGen();
}

void AThreeDLSystem::ApplyPreset(EThreeDLSystemPreset NewPreset)
{
	CurrentPreset = NewPreset;
	RewriteRules.Reset();
	RandomRewriteRules.Reset();
	m_StartAxiom = TEXT("X");

	auto AddRandomFOption = [this](
		FRandomRewriteRule& Rule,
		const TCHAR* Replacement,
		float Chance)
	{
		FRandomRewriteOption Option;
		Option.Replacement = Replacement;
		Option.Chance = Chance;
		Rule.Options.Add(Option);
	};

	FRandomRewriteRule RandomFRule;
	RandomFRule.Symbol = TEXT("F");

	switch (NewPreset)
	{
	case EThreeDLSystemPreset::Tree:
		RewriteRules.Add(TEXT("X"), TEXT("F[+!X][-!X][&!X][^!X]FX"));
		RewriteRules.Add(TEXT("F"), TEXT("FF"));
		AddRandomFOption(RandomFRule, TEXT("FF"), 0.40f);
		AddRandomFOption(RandomFRule, TEXT("F[+F]F[-F]F"), 0.35f);
		AddRandomFOption(RandomFRule, TEXT("F[&F]F[^F]F"), 0.25f);
		m_Redos = 3;
		SegmentLength = 50.0f;
		TurnAngle = 25.0f;
		StartWidth = 10.0f;
		WidthMultiplier = 0.75f;
		AngleVariationPercent = 15.0f;
		break;

	case EThreeDLSystemPreset::Bush:
		RewriteRules.Add(TEXT("X"), TEXT("F[+X][-X][&X][^X][+&X][-^X]"));
		RewriteRules.Add(TEXT("F"), TEXT("FF"));
		AddRandomFOption(RandomFRule, TEXT("FF"), 0.30f);
		AddRandomFOption(RandomFRule, TEXT("F[+F][-F]"), 0.35f);
		AddRandomFOption(RandomFRule, TEXT("F[&F][^F]"), 0.35f);
		m_Redos = 3;
		SegmentLength = 35.0f;
		TurnAngle = 32.0f;
		StartWidth = 12.0f;
		WidthMultiplier = 0.70f;
		AngleVariationPercent = 18.0f;
		break;

	case EThreeDLSystemPreset::Coral:
		RewriteRules.Add(TEXT("X"), TEXT("F[+&X][-&X][+^X][-^X][++X][--X]"));
		RewriteRules.Add(TEXT("F"), TEXT("FF"));
		AddRandomFOption(RandomFRule, TEXT("F"), 0.15f);
		AddRandomFOption(RandomFRule, TEXT("FF"), 0.35f);
		AddRandomFOption(RandomFRule, TEXT("F[+F][-F][&F][^F]"), 0.50f);
		m_Redos = 3;
		SegmentLength = 30.0f;
		TurnAngle = 38.0f;
		StartWidth = 14.0f;
		WidthMultiplier = 0.82f;
		AngleVariationPercent = 12.0f;
		break;
	}

	RandomRewriteRules.Add(RandomFRule);
	RerunAllGen();
}

float AThreeDLSystem::GetRandomTurnAngle()
{
	const float Variation = FMath::Clamp(AngleVariationPercent, 0.0f, 100.0f) * 0.01f;
	return TurnAngle * AngleRandomStream.FRandRange(1.0f - Variation, 1.0f + Variation);
}

void AThreeDLSystem::SetCameraOrbitEnabled(bool bEnabled)
{
	if (bEnabled && !bOrbitCamera)
	{
		InitializeCameraOrbit();
	}

	bOrbitCamera = bEnabled;
}

void AThreeDLSystem::SetCameraOrbitSpeed(float NewSpeed)
{
	CameraOrbitSpeed = FMath::Clamp(NewSpeed, -360.0f, 360.0f);
}

void AThreeDLSystem::SetCameraOrbitDistance(float NewDistance)
{
	CameraOrbitDistance = FMath::Max(NewDistance, 1.0f);

	if (!OrbitCamera)
	{
		return;
	}

	const FVector ActorLocation = GetActorLocation();
	const FVector CurrentOffset = OrbitCamera->GetActorLocation() - ActorLocation;
	FVector2D HorizontalDirection(CurrentOffset.X, CurrentOffset.Y);
	if (!HorizontalDirection.Normalize())
	{
		HorizontalDirection = FVector2D(
			FMath::Cos(CameraOrbitAngleRadians),
			FMath::Sin(CameraOrbitAngleRadians));
	}

	const FVector NewCameraLocation(
		ActorLocation.X + HorizontalDirection.X * CameraOrbitDistance,
		ActorLocation.Y + HorizontalDirection.Y * CameraOrbitDistance,
		OrbitCamera->GetActorLocation().Z);
	const FVector LookAtLocation = ActorLocation + FVector(0.0f, 0.0f, CameraTargetHeight);

	OrbitCamera->SetActorLocation(NewCameraLocation);
	OrbitCamera->SetActorRotation(
		UKismetMathLibrary::FindLookAtRotation(NewCameraLocation, LookAtLocation));
}

void AThreeDLSystem::AddBranchCylinder(
	const FVector& Start,
	const FVector& End,
	float StartRadius,
	float EndRadius,
	TArray<FVector>& Vertices,
	TArray<int32>& Triangles,
	TArray<FVector>& Normals,
	TArray<FVector2D>& UVs,
	TArray<FLinearColor>& VertexColors,
	TArray<FProcMeshTangent>& Tangents) const
{
	const FVector Axis = End - Start;
	const float Length = Axis.Length();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const int32 SideCount = FMath::Max(BranchSides, 3);
	const FVector Direction = Axis / Length;
	const FMatrix Basis = FRotationMatrix::MakeFromX(Direction);
	const FVector Right = Basis.GetUnitAxis(EAxis::Y);
	const FVector Up = Basis.GetUnitAxis(EAxis::Z);
	const int32 FirstVertexIndex = Vertices.Num();

	for (int32 SideIndex = 0; SideIndex < SideCount; ++SideIndex)
	{
		const float AngleRadians = 2.0f * PI * static_cast<float>(SideIndex) / static_cast<float>(SideCount);
		const FVector RadialDirection =
			Right * FMath::Cos(AngleRadians) +
			Up * FMath::Sin(AngleRadians);

		Vertices.Add(Start + RadialDirection * StartRadius);
		Vertices.Add(End + RadialDirection * EndRadius);

		Normals.Add(RadialDirection);
		Normals.Add(RadialDirection);

		const float U = static_cast<float>(SideIndex) / static_cast<float>(SideCount);
		UVs.Add(FVector2D(U, 0.0f));
		UVs.Add(FVector2D(U, 1.0f));

		VertexColors.Add(FLinearColor::White);
		VertexColors.Add(FLinearColor::White);

		const FProcMeshTangent Tangent(Direction, false);
		Tangents.Add(Tangent);
		Tangents.Add(Tangent);
	}

	for (int32 SideIndex = 0; SideIndex < SideCount; ++SideIndex)
	{
		const int32 Current = FirstVertexIndex + SideIndex * 2;
		const int32 Next = FirstVertexIndex + ((SideIndex + 1) % SideCount) * 2;

		const int32 StartCurrent = Current;
		const int32 EndCurrent = Current + 1;
		const int32 StartNext = Next;
		const int32 EndNext = Next + 1;

		Triangles.Add(StartCurrent);
		Triangles.Add(EndCurrent);
		Triangles.Add(StartNext);

		Triangles.Add(StartNext);
		Triangles.Add(EndCurrent);
		Triangles.Add(EndNext);
	}
}
