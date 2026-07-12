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

	m_StartAxiom = m_Axiom;
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
		ActorLocation.X + FMath::Cos(CameraOrbitAngleRadians) * CameraOrbitRadius,
		ActorLocation.Y + FMath::Sin(CameraOrbitAngleRadians) * CameraOrbitRadius,
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
	CameraOrbitRadius = FMath::Max(FVector2D(Offset.X, Offset.Y).Length(), 1.0f);
	CameraOrbitHeight = Offset.Z;
	CameraOrbitAngleRadians = FMath::Atan2(Offset.Y, Offset.X);
}

void AThreeDLSystem::RunGeneration(FString& CurrentString)
{
	FString GeneratedString;

	for (int32 i = 0; i < CurrentString.Len(); ++i)
	{
		const FString Letter = CurrentString.Mid(i, 1);
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

void AThreeDLSystem::RerunAllGen()
{
	m_Axiom = m_StartAxiom;

	for (uint8 i = 0; i < m_Redos; ++i)
	{
		RunGeneration(m_Axiom);
	}

	DrawAxiom();
}

void AThreeDLSystem::DrawAxiom()
{
	BranchMesh->ClearAllMeshSections();

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
			Rotation.Yaw += TurnAngle;
			break;

		case TEXT('-'):
			Rotation.Yaw -= TurnAngle;
			break;

		case TEXT('&'):
			Rotation.Pitch += TurnAngle;
			break;

		case TEXT('^'):
			Rotation.Pitch -= TurnAngle;
			break;

		case TEXT('\\'):
			Rotation.Roll += TurnAngle;
			break;

		case TEXT('/'):
			Rotation.Roll -= TurnAngle;
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
