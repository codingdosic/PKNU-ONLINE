#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "IWebSocket.h"
#include "WebSocketManager.generated.h"

class AMyWebSocketCharacter;
class AMyRemoteCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInitialStateSynced);

UCLASS(Blueprintable)
class PROJECT_PKNU_API UWebSocketManager : public UObject, public FTickableGameObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "WebSocket")
    FOnInitialStateSynced OnInitialStateSynced;

    UPROPERTY()
    TMap<FString, TWeakObjectPtr<AActor>> TrackedWorldObjects; // ObjectID -> Actor (Weak Ptr)

    UPROPERTY()
    TMap<FString, FTransform> LastSentTransforms; // ObjectID -> 마지막 전송 Transform

    UPROPERTY()
    TMap<FString, FTransform> WorldObjectTargetTransforms; // 월드 오브젝트의 목표 Transform
    UPROPERTY()
    TMap<FString, float> WorldObjectInterpSpeeds; // 월드 오브젝트별 보간 속도
    UPROPERTY()
    TMap<FString, double> WorldObjectLastReceiveTime; // 월드 오브젝트의 마지막 업데이트 수신 시간

    UWebSocketManager();

    void Initialize(UClass* InRemoteCharacterClass, UWorld* InWorld);

    void Connect(const FString& InUsername);
    void Close();

    void RegisterPlayerCharacter(AMyWebSocketCharacter* InCharacter);
    void UnregisterPlayerCharacter();

    // 메시지 전송
    void SendRegisterCharacter(); // 캐릭터 등록
    void SendUpdate(const FString& EntityType, const FString& ID, const FTransform& Transform, float Speed, bool bIsFalling);
    void SendChatMessage(const FString& Message); // 채팅 메시지 전송
    void SendTransformData(); // 초기 Transform 전송

    // Tick
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    // 수신 메시지 처리
    void OnWebSocketMessage(const FString& Message);

    UFUNCTION()
    void SendInitialWorldObjects();

    UFUNCTION(BlueprintCallable, Category = "WebSocket")
    void SendWorldObjectTransform(const FString& ObjectID, const FTransform& Transform);


    // 임계값 (조정 가능)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network|WorldObjects")
    float WorldPosThreshold = 15.0f; // cm 단위: 위치 변화가 이보다 커야 전송

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network|WorldObjects")
    float WorldRotThreshold = 15.0f; // degree: 회전 변화가 이보다 커야 전송

    // (선택) 월드 오브젝트 전송 전용 타이머 (플레이어 전송과 분리)
    float TimeSinceLastWorldSend = 0.0f;

protected:
    void SpawnOrUpdateWorldObject(const FString& ObjectID, const FTransform& Transform, bool bIsLocalUpdate);
    void SpawnOrUpdateRemoteCharacter(const FString& PlayerID, const FTransform& Transform, float Speed, bool bIsFalling, const FString& InPlayerName, double Timestamp);

protected:
    TSharedPtr<IWebSocket> WebSocket;
    UClass* RemoteCharacterClass;
    UWorld* World;
    AMyWebSocketCharacter* OwnerCharacter;

    TMap<FString, AMyRemoteCharacter*> OtherPlayersMap; // Remote players
    TMap<FString, AActor*> RemoteWorldObjectsMap;      // Remote/world objects

    // 이전 위치/회전/상태 비교용
    FVector LastSentLocation;
    FRotator LastSentRotation;
    float LastSentSpeed;
    bool bLastSentIsFalling;
    bool bHasSentInitialTransform;

    // 전송 관련
    float SendInterval;
    float TimeSinceLastSend;

    FString MyPlayerId; // Client-generated unique ID
    FString MyPlayerName; // Player's chosen name
};
