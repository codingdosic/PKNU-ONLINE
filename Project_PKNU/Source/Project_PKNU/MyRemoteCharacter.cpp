// Fill out your copyright notice in the Description page of Project Settings.


#include "MyRemoteCharacter.h"
#include "Components/WidgetComponent.h"
#include "NameplateWidget.h"

#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
AMyRemoteCharacter::AMyRemoteCharacter()
{
	PrimaryActorTick.bCanEverTick = true; // Tick 활성화 (중요!)
	bReplicates = true; // 원격 플레이어이므로 복제 필요
	bAlwaysRelevant = true; // 항상 클라이언트에게 관련성 있도록 설정

    // Disable the character movement component since we are manually setting the position
    GetCharacterMovement()->SetMovementMode(MOVE_None);

    NameplateComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameplateComponent"));
    NameplateComponent->SetupAttachment(RootComponent);
    NameplateComponent->SetWidgetSpace(EWidgetSpace::Screen);
    NameplateComponent->SetDrawSize(FVector2D(200, 30)); // 위젯 크기 조절
}

void AMyRemoteCharacter::SetName(const FString& Name)
{
    UNameplateWidget* NameplateWidget = Cast<UNameplateWidget>(NameplateComponent->GetUserWidgetObject());
    if (NameplateWidget)
    {
        NameplateWidget->SetName(Name);
    }
}

// Called when the game starts or when spawned
void AMyRemoteCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void AMyRemoteCharacter::AddTransformSnapshot(const FVector& NewLocation, const FRotator& NewRotation, float NewSpeed, bool bNewIsFalling, double NewTimestamp)
{
    // 애니메이션 상태는 즉시 업데이트
    CurrentSpeed = NewSpeed;
    bIsFalling = bNewIsFalling;

    // 버퍼에 새 스냅샷 추가
    FTransformSnapshot Snapshot;
    Snapshot.Location = NewLocation;
    Snapshot.Rotation = NewRotation;
    Snapshot.Timestamp = NewTimestamp;
    TransformBuffer.Add(Snapshot);

    // 버퍼가 너무 커지지 않도록 오래된 데이터 정리 (예: 2초 이상 된 데이터)
    const double OldestTime = GetWorld()->GetTimeSeconds() - 2.0; // Use GetTimeSeconds() for local client time
    TransformBuffer.RemoveAll([&](const FTransformSnapshot& s) {
        return s.Timestamp < OldestTime;
    });

    // 버퍼가 정렬되어 있지 않을 수 있으므로, 타임스탬프 기준으로 정렬
    // (서버에서 순서대로 보내지만, 네트워크 지연으로 인해 순서가 바뀔 수도 있으므로 안전장치)
    TransformBuffer.Sort([](const FTransformSnapshot& A, const FTransformSnapshot& B) {
        return A.Timestamp < B.Timestamp;
    });
}


// Called every frame
void AMyRemoteCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 보간할 스냅샷이 2개 미만이면 아무것도 하지 않음 (혹은 마지막 스냅샷 위치 유지)
    if (TransformBuffer.Num() < 2)
    {
        if (TransformBuffer.Num() == 1)
        {
            // 데이터가 하나뿐이면 해당 위치로 이동
            SetActorLocationAndRotation(TransformBuffer[0].Location, TransformBuffer[0].Rotation);
        }
        return;
    }

    // 보간의 기준이 될 과거 시간 계산
    // GetWorld()->GetTimeSeconds()는 로컬 클라이언트의 시간입니다.
    const double RenderTime = GetWorld()->GetTimeSeconds() - InterpolationDelay;

    // RenderTime을 기준으로 보간할 두 개의 스냅샷 찾기
    FTransformSnapshot* From = nullptr;
    FTransformSnapshot* To = nullptr;

    for (int32 i = 0; i < TransformBuffer.Num() - 1; ++i)
    {
        if (TransformBuffer[i].Timestamp <= RenderTime && RenderTime < TransformBuffer[i+1].Timestamp)
        {
            From = &TransformBuffer[i];
            To = &TransformBuffer[i+1];
            break;
        }
    }

    // 적절한 스냅샷을 찾았으면 보간 수행
    if (From && To)
    {
        const double TimeBetweenSnapshots = To->Timestamp - From->Timestamp;
        // 두 스냅샷 사이에서 RenderTime이 얼마나 진행되었는지 비율 계산 (0.0 ~ 1.0)
        const float InterpAlpha = (TimeBetweenSnapshots > 0.0) ? (float)((RenderTime - From->Timestamp) / TimeBetweenSnapshots) : 0.0f; // TimeBetweenSnapshots가 0이면 0.0f
        
        // 위치와 회전을 선형 보간 (Lerp)
        FVector NewLocation = FMath::Lerp(From->Location, To->Location, InterpAlpha);
        FRotator NewRotation = FMath::Lerp(From->Rotation, To->Rotation, InterpAlpha);
        
        SetActorLocationAndRotation(NewLocation, NewRotation);
    }
    else 
    {
        // RenderTime이 현재 버퍼의 모든 스냅샷보다 최신이거나 (네트워크 지연이 심한 경우)
        // 가장 마지막 스냅샷 위치로 이동 (외삽/Extrapolation을 추가할 수도 있음)
        if(TransformBuffer.Num() > 0 && RenderTime > TransformBuffer.Last().Timestamp)
        {
             const FTransformSnapshot& LastSnapshot = TransformBuffer.Last();
             SetActorLocationAndRotation(LastSnapshot.Location, LastSnapshot.Rotation);
        }
    }
}
