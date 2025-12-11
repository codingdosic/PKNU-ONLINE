// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MyRemoteCharacter.generated.h"

// 위치와 시간을 함께 저장할 구조체 정의
USTRUCT()
struct FTransformSnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    FVector Location;

    UPROPERTY()
    FRotator Rotation;

    UPROPERTY()
    double Timestamp; // 서버에서 받은 시간을 저장
};

class UWidgetComponent;

UCLASS()
class PROJECT_PKNU_API AMyRemoteCharacter : public ACharacter
{
	GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    UWidgetComponent* NameplateComponent;

    void SetName(const FString& Name);

    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    float CurrentSpeed;

    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    bool bIsFalling;

    AMyRemoteCharacter();

    // 기존 함수를 타임스탬프를 받도록 수정
    void AddTransformSnapshot(const FVector& NewLocation, const FRotator& NewRotation, float NewSpeed, bool bNewIsFalling, double NewTimestamp);

    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

private:
    // 트랜스폼 스냅샷을 저장할 버퍼
    TArray<FTransformSnapshot> TransformBuffer;

    // 얼마만큼의 지연을 둘 것인지 (네트워크 상태에 따라 조절)
    UPROPERTY(EditAnywhere, Category = "Network Interpolation")
    float InterpolationDelay = 0.1f; // 100ms
};
