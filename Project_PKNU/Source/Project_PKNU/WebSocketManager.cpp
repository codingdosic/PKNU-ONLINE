#include "WebSocketManager.h"
#include "MyWebSocketCharacter.h"
#include "MyRemoteCharacter.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Engine/World.h"
#include "EngineUtils.h" // TActorIterator 사용
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ChatWidget.h" // For handling chat UI

UWebSocketManager::UWebSocketManager()
    : RemoteCharacterClass(nullptr)
    , World(nullptr)
    , OwnerCharacter(nullptr)
    , LastSentSpeed(0.0f)
    , bLastSentIsFalling(false)
    , bHasSentInitialTransform(false)
    , SendInterval(0.1f) // 30ms 주기 (권장: 0.02 ~ 0.05)
    , TimeSinceLastSend(0.0f)
{
}

void UWebSocketManager::Initialize(UClass* InRemoteCharacterClass, UWorld* InWorld)
{
    RemoteCharacterClass = InRemoteCharacterClass;
    World = InWorld;
    OwnerCharacter = nullptr;
}

void UWebSocketManager::Connect(const FString& InUsername)
{
    MyPlayerName = InUsername;
    if (!GEngine) return;

    FString ServerURL = TEXT("ws://localhost:8080");
    // FString ServerURL = TEXT("wss://19013cdb7bef.ngrok-free.app");

    WebSocket = FWebSocketsModule::Get().CreateWebSocket(ServerURL);

    WebSocket->OnConnected().AddLambda([this]() {
        // UE_LOG(LogTemp, Warning, TEXT("WebSocket connected"));

        SendInitialWorldObjects();

        if (OwnerCharacter)
        {
            SendRegisterCharacter();
            SendTransformData();
        }
        });

    WebSocket->OnMessage().AddLambda([this](const FString& Msg) {
        //UE_LOG(LogTemp, Warning, TEXT("[WEBSOCKET RAW MESSAGE] %s"), *Msg);
        OnWebSocketMessage(Msg);
        });

    WebSocket->OnClosed().AddLambda([this](int32 StatusCode, const FString& Reason, bool bWasClean) {
        UE_LOG(LogTemp, Warning, TEXT("WebSocket disconnected. Status Code: %d, Reason: %s, WasClean: %s"), StatusCode, *Reason, (bWasClean ? TEXT("true") : TEXT("false")));
        UnregisterPlayerCharacter(); // 로컬 플레이어 캐릭터 정리
        });

    //WebSocket->OnError().AddLambda([this](const FString& Error) {
    //    UE_LOG(LogTemp, Error, TEXT("WebSocket error: %s"), *Error);
    //    // 에러 발생 시에도 정리 로직을 호출할 수 있습니다.
    //    UnregisterPlayerCharacter();
    //    });

    WebSocket->Connect();
}

void UWebSocketManager::Close()
{
    if (WebSocket.IsValid() && WebSocket->IsConnected())
        WebSocket->Close();
}

void UWebSocketManager::RegisterPlayerCharacter(AMyWebSocketCharacter* InCharacter)
{
    OwnerCharacter = InCharacter;
    // The character is now registered only after the WebSocket connection is established.
    // See the OnConnected lambda in the Connect() function.
}

void UWebSocketManager::UnregisterPlayerCharacter()
{
    if (OwnerCharacter)
    {
        // OwnerCharacter = nullptr; // OwnerCharacter는 게임 인스턴스에서 관리되므로 여기서는 nullptr로 만들지 않습니다.
        // 대신, OwnerCharacter가 더 이상 유효하지 않음을 표시하거나,
        // 게임 모드/스테이트에 연결이 끊어졌음을 알리는 로직을 추가할 수 있습니다.
        UE_LOG(LogTemp, Warning, TEXT("Local player character unregistered due to WebSocket disconnection."));
    }
    MyPlayerId.Empty(); // 플레이어 ID 초기화
    bHasSentInitialTransform = false; // 초기 트랜스폼 전송 상태 초기화
}

void UWebSocketManager::SendRegisterCharacter()
{
    if (!OwnerCharacter) return;

    // 서버가 ID를 할당하므로 클라이언트는 ID를 생성하지 않음
    // MyPlayerId는 서버로부터 'id' 메시지를 수신했을 때 설정됨
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("type"), TEXT("register_character"));
    Root->SetStringField(TEXT("playerID"), MyPlayerId); // MyPlayerId는 아직 비어있음

    TSharedPtr<FJsonObject> MetaObject = MakeShared<FJsonObject>();
    MetaObject->SetStringField(TEXT("playerName"), MyPlayerName);
    Root->SetObjectField(TEXT("meta"), MetaObject);

    FVector Loc = OwnerCharacter->GetActorLocation();
    FRotator Rot = OwnerCharacter->GetActorRotation();

    TSharedPtr<FJsonObject> Pos = MakeShared<FJsonObject>();
    Pos->SetNumberField(TEXT("x"), Loc.X);
    Pos->SetNumberField(TEXT("y"), Loc.Y);
    Pos->SetNumberField(TEXT("z"), Loc.Z);

    TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
    RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
    RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
    RotObj->SetNumberField(TEXT("roll"), Rot.Roll);

    Root->SetObjectField(TEXT("position"), Pos);
    Root->SetObjectField(TEXT("rotation"), RotObj);

    FString OutString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutString);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    WebSocket->Send(OutString);
}

void UWebSocketManager::SendTransformData()
{
    if (!OwnerCharacter || MyPlayerId.IsEmpty() || !WebSocket.IsValid() || !WebSocket->IsConnected()) return;

    FTransform CurrentTransform = OwnerCharacter->GetActorTransform();
    float Speed = OwnerCharacter->GetVelocity().Size();
    bool bIsFalling = OwnerCharacter->GetCharacterMovement()->IsFalling();

    SendUpdate(TEXT("player"), MyPlayerId, CurrentTransform, Speed, bIsFalling);
}

// SendUpdate: 기존 JSON에 타임스탬프(ts, ms)를 추가
void UWebSocketManager::SendUpdate(const FString& EntityType, const FString& ID, const FTransform& Transform, float Speed, bool bIsFalling)
{
    if (!WebSocket.IsValid() || !WebSocket->IsConnected()) return;

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("type"), TEXT("transform"));
    Root->SetStringField(TEXT("id"), ID);

    FVector Loc = Transform.GetLocation();
    FRotator Rot = Transform.GetRotation().Rotator();

    Root->SetNumberField(TEXT("x"), Loc.X);
    Root->SetNumberField(TEXT("y"), Loc.Y);
    Root->SetNumberField(TEXT("z"), Loc.Z);
    Root->SetNumberField(TEXT("pitch"), Rot.Pitch);
    Root->SetNumberField(TEXT("yaw"), Rot.Yaw);
    Root->SetNumberField(TEXT("roll"), Rot.Roll);
    Root->SetNumberField(TEXT("speed"), Speed);
    Root->SetBoolField(TEXT("isFalling"), bIsFalling);

    // --- 추가: 밀리초 단위 타임스탬프 (UTC) ---
    FDateTime UtcNow = FDateTime::UtcNow();
    int64 Millis = (UtcNow.ToUnixTimestamp() * 1000LL) + (UtcNow.GetMillisecond());
    Root->SetNumberField(TEXT("ts"), static_cast<double>(Millis));
    // --------------------------------------------

    FString OutString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutString);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    WebSocket->Send(OutString);
}


void UWebSocketManager::SendChatMessage(const FString& Message)
{
    if (!OwnerCharacter || !WebSocket.IsValid() || !WebSocket->IsConnected()) return;

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("type"), TEXT("chat"));
    Root->SetStringField(TEXT("playerID"), MyPlayerId);
    Root->SetStringField(TEXT("message"), Message);

    FString OutString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutString);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    UE_LOG(LogTemp, Warning, TEXT("Sending chat message: PlayerID=%s, Message=%s, JSON=%s"), *MyPlayerId, *Message, *OutString);

    WebSocket->Send(OutString);
}

void UWebSocketManager::OnWebSocketMessage(const FString& Message)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject)) return;

    FString Type = JsonObject->GetStringField(TEXT("type"));

    if (Type == TEXT("id"))
    {
                  MyPlayerId = JsonObject->GetStringField(TEXT("id"));        if (OwnerCharacter)
            OwnerCharacter->SetName(MyPlayerName); // Use the chosen player name
    }
    else if (Type == TEXT("state_sync"))
    {
        OnInitialStateSynced.Broadcast();

        // Handle initial player characters
        const TArray<TSharedPtr<FJsonValue>>* PlayerArr;
        if (JsonObject->TryGetArrayField(TEXT("playerCharacters"), PlayerArr))
        {
            for (const auto& Val : *PlayerArr)
            {
                const TSharedPtr<FJsonObject>* PlayerObject;
                if (Val->TryGetObject(PlayerObject))
                {
                    FString PlayerID = (*PlayerObject)->GetStringField(TEXT("playerID"));
                    const TSharedPtr<FJsonObject>* StateObject;
                    if ((*PlayerObject)->TryGetObjectField(TEXT("state"), StateObject))
                    {
                        const TSharedPtr<FJsonObject>* PosObject = nullptr;
                        const TSharedPtr<FJsonObject>* RotObject = nullptr;
                        if ((*StateObject)->TryGetObjectField(TEXT("position"), PosObject) && (*StateObject)->TryGetObjectField(TEXT("rotation"), RotObject))
                        {
                            FVector Location((*PosObject)->GetNumberField(TEXT("x")), (*PosObject)->GetNumberField(TEXT("y")), (*PosObject)->GetNumberField(TEXT("z")));
                            FRotator Rotation((*RotObject)->GetNumberField(TEXT("pitch")), (*RotObject)->GetNumberField(TEXT("yaw")), (*RotObject)->GetNumberField(TEXT("roll")));

                            float Speed = (*StateObject)->GetNumberField(TEXT("speed"));
                            bool bIsFalling = (*StateObject)->GetBoolField(TEXT("isFalling"));

                            FString PlayerName = PlayerID; // Default to PlayerID
                            const TSharedPtr<FJsonObject>* MetaObject;
                            if ((*StateObject)->TryGetObjectField(TEXT("meta"), MetaObject))
                            {
                                if ((*MetaObject)->HasField(TEXT("playerName")))
                                {
                                    PlayerName = (*MetaObject)->GetStringField(TEXT("playerName"));
                                }
                            }
                            
                            // 도착 시간을 기준으로 타임스탬프 생성
                            const double Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
                            SpawnOrUpdateRemoteCharacter(PlayerID, FTransform(Rotation, Location), Speed, bIsFalling, PlayerName, Timestamp);
                        }
                    }
                }
            }
        }
    }
    else if (Type == TEXT("render_update"))
    {
        FString Action = JsonObject->GetStringField(TEXT("action"));

        // ✅ ✅ [월드 오브젝트 업데이트]
        if (Action == TEXT("add_object") || (Action == TEXT("update") && JsonObject->GetBoolField(TEXT("isObject"))))
        {
            UE_LOG(LogTemp, Warning, TEXT("서버로부터 수신"));
            FString ObjectID = JsonObject->GetStringField(TEXT("id"));
            const TSharedPtr<FJsonObject>* StateObject;

            if (JsonObject->TryGetObjectField(TEXT("state"), StateObject))
            {
                const TSharedPtr<FJsonObject>* PosObject = nullptr;
                const TSharedPtr<FJsonObject>* RotObject = nullptr;

                if ((*StateObject)->TryGetObjectField(TEXT("position"), PosObject) &&
                    (*StateObject)->TryGetObjectField(TEXT("rotation"), RotObject))
                {
                    FVector Location((*PosObject)->GetNumberField(TEXT("x")),
                        (*PosObject)->GetNumberField(TEXT("y")),
                        (*PosObject)->GetNumberField(TEXT("z")));
                    FRotator Rotation((*RotObject)->GetNumberField(TEXT("pitch")),
                        (*RotObject)->GetNumberField(TEXT("yaw")),
                        (*RotObject)->GetNumberField(TEXT("roll")));

                    // 서버에서 받은 transform → 절대 재전송 금지
                    SpawnOrUpdateWorldObject(ObjectID, FTransform(Rotation, Location), false);
                }
            }
        }
        else if (Action == TEXT("add_character")) // Handle new character addition
        {
            FString PlayerID = JsonObject->GetStringField(TEXT("playerID"));
            const TSharedPtr<FJsonObject>* StateObject;
            if (JsonObject->TryGetObjectField(TEXT("state"), StateObject))
            {
                const TSharedPtr<FJsonObject>* PosObject = nullptr;
                const TSharedPtr<FJsonObject>* RotObject = nullptr;
                if ((*StateObject)->TryGetObjectField(TEXT("position"), PosObject) && (*StateObject)->TryGetObjectField(TEXT("rotation"), RotObject))
                {
                    FVector Location((*PosObject)->GetNumberField(TEXT("x")), (*PosObject)->GetNumberField(TEXT("y")), (*PosObject)->GetNumberField(TEXT("z")));
                    FRotator Rotation((*RotObject)->GetNumberField(TEXT("pitch")), (*RotObject)->GetNumberField(TEXT("yaw")), (*RotObject)->GetNumberField(TEXT("roll")));

                    float Speed = (*StateObject)->GetNumberField(TEXT("speed"));
                    bool bIsFalling = (*StateObject)->GetBoolField(TEXT("isFalling"));

                    FString PlayerName = PlayerID; // Default to PlayerID
                    const TSharedPtr<FJsonObject>* MetaObject;
                    if ((*StateObject)->TryGetObjectField(TEXT("meta"), MetaObject))
                    {
                        if ((*MetaObject)->HasField(TEXT("playerName")))
                        {
                            PlayerName = (*MetaObject)->GetStringField(TEXT("playerName"));
                        }
                    }

                    // 도착 시간을 기준으로 타임스탬프 생성
                    const double Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
                    SpawnOrUpdateRemoteCharacter(PlayerID, FTransform(Rotation, Location), Speed, bIsFalling, PlayerName, Timestamp);
                }
            }
        }


        else if (Action == TEXT("transform")) // 서버에서 보내는 transform 처리
        {
            const TSharedPtr<FJsonObject>* StateObject;
            if (JsonObject->TryGetObjectField(TEXT("state"), StateObject))
            {
                const TSharedPtr<FJsonObject>* PosObject = nullptr;
                const TSharedPtr<FJsonObject>* RotObject = nullptr; // 반드시 nullptr로 초기화

                if ((*StateObject)->TryGetObjectField(TEXT("position"), PosObject) &&
                    (*StateObject)->TryGetObjectField(TEXT("rotation"), RotObject))
                {
                    FVector Location(
                        (*PosObject)->GetNumberField(TEXT("x")),
                        (*PosObject)->GetNumberField(TEXT("y")),
                        (*PosObject)->GetNumberField(TEXT("z"))
                    );
                    FRotator Rotation(
                        (*RotObject)->GetNumberField(TEXT("pitch")),
                        (*RotObject)->GetNumberField(TEXT("yaw")),
                        (*RotObject)->GetNumberField(TEXT("roll"))
                    );

                    float Speed = (*StateObject)->GetNumberField(TEXT("speed"));
                    bool bIsFalling = (*StateObject)->GetBoolField(TEXT("isFalling"));
                    FString PlayerID = JsonObject->GetStringField(TEXT("playerID"));

                    FString PlayerName = PlayerID; // Default to PlayerID
                    const TSharedPtr<FJsonObject>* MetaObject;
                    if ((*StateObject)->TryGetObjectField(TEXT("meta"), MetaObject))
                    {
                        if ((*MetaObject)->HasField(TEXT("playerName")))
                        {
                            PlayerName = (*MetaObject)->GetStringField(TEXT("playerName"));
                        }
                    }

                    // 도착 시간을 기준으로 타임스탬프 생성
                    const double Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
                    SpawnOrUpdateRemoteCharacter(PlayerID, FTransform(Rotation, Location), Speed, bIsFalling, PlayerName, Timestamp);
                }
            }
        }
        else if (Action == TEXT("remove_character"))
        {
            FString PlayerID = JsonObject->GetStringField(TEXT("playerID"));
            if (OtherPlayersMap.Contains(PlayerID))
            {
                AMyRemoteCharacter* CharToRemove = OtherPlayersMap[PlayerID];
                if (CharToRemove)
                {
                    CharToRemove->Destroy();
                }
                OtherPlayersMap.Remove(PlayerID);
            }
        }
        else if (Action == TEXT("new_chat")) // Handle incoming chat messages from server
        {
            FString SenderName = JsonObject->GetStringField(TEXT("playerID")); // Server now sends playerName in playerID field
            FString MessageText = JsonObject->GetStringField(TEXT("message"));
            // No need to check if SenderId == MyPlayerId here, as the server should handle not sending back to sender,
            // or the ChatWidgetInstance can handle it if needed.
            // For now, just add the message.
            if (OwnerCharacter && OwnerCharacter->ChatWidgetInstance)
            {
                OwnerCharacter->ChatWidgetInstance->AddChatMessage(SenderName, MessageText);
            }
        }
        else if (Action == TEXT("update_batch")) // New block for update_batch
        {
            const TArray<TSharedPtr<FJsonValue>>* PlayersArray;
            if (JsonObject->TryGetArrayField(TEXT("players"), PlayersArray))
            {
                // 도착 시간을 기준으로 타임스탬프 생성 (배치 내 모든 플레이어에게 동일하게 적용)
                const double Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

                for (const auto& PlayerValue : *PlayersArray)
                {
                    const TSharedPtr<FJsonObject>* PlayerObject;
                    if (PlayerValue->TryGetObject(PlayerObject))
                    {
                        FString PlayerID = (*PlayerObject)->GetStringField(TEXT("playerID"));
                        const TSharedPtr<FJsonObject>* StateObject;
                        if ((*PlayerObject)->TryGetObjectField(TEXT("state"), StateObject))
                        {
                            const TSharedPtr<FJsonObject>* PosObject = nullptr;
                            const TSharedPtr<FJsonObject>* RotObject = nullptr;
                            if ((*StateObject)->TryGetObjectField(TEXT("position"), PosObject) && (*StateObject)->TryGetObjectField(TEXT("rotation"), RotObject))
                            {
                                FVector Location((*PosObject)->GetNumberField(TEXT("x")), (*PosObject)->GetNumberField(TEXT("y")), (*PosObject)->GetNumberField(TEXT("z")));
                                FRotator Rotation((*RotObject)->GetNumberField(TEXT("pitch")), (*RotObject)->GetNumberField(TEXT("yaw")), (*RotObject)->GetNumberField(TEXT("roll")));

                                float Speed = (*StateObject)->GetNumberField(TEXT("speed"));
                                bool bIsFalling = (*StateObject)->GetBoolField(TEXT("isFalling"));

                                FString PlayerName = PlayerID; // Default to PlayerID
                                const TSharedPtr<FJsonObject>* MetaObject;
                                if ((*StateObject)->TryGetObjectField(TEXT("meta"), MetaObject))
                                {
                                    if ((*MetaObject)->HasField(TEXT("playerName")))
                                    {
                                        PlayerName = (*MetaObject)->GetStringField(TEXT("playerName"));
                                    }
                                }
                                
                                SpawnOrUpdateRemoteCharacter(PlayerID, FTransform(Rotation, Location), Speed, bIsFalling, PlayerName, Timestamp);
                            }
                        }
                    }
                }
            }
        }
    }
    else if (Type == TEXT("transform"))
    {
        FString SenderId = JsonObject->GetStringField(TEXT("id"));
        // UE_LOG(LogTemp, Warning, TEXT("[CLIENT RECEIVED TRANSFORM] SenderId=%s, MyPlayerId=%s"), *SenderId, *MyPlayerId);

        if (!MyPlayerId.IsEmpty() && SenderId == MyPlayerId)
        {
            // UE_LOG(LogTemp, Warning, TEXT("Ignoring own transform message."));
            return;
        }

        FVector NewLoc(JsonObject->GetNumberField(TEXT("x")),
            JsonObject->GetNumberField(TEXT("y")),
            JsonObject->GetNumberField(TEXT("z")));
        FRotator NewRot(JsonObject->GetNumberField(TEXT("pitch")),
            JsonObject->GetNumberField(TEXT("yaw")),
            JsonObject->GetNumberField(TEXT("roll")));

        float Speed = JsonObject->GetNumberField(TEXT("speed"));
        bool bIsFalling = JsonObject->GetBoolField(TEXT("isFalling"));

        FString PlayerName = SenderId; // Default to SenderId
        const TSharedPtr<FJsonObject>* StateObject;
        if (JsonObject->TryGetObjectField(TEXT("state"), StateObject))
        {
            const TSharedPtr<FJsonObject>* MetaObject;
            if ((*StateObject)->TryGetObjectField(TEXT("meta"), MetaObject))
            {
                if ((*MetaObject)->HasField(TEXT("playerName")))
                {
                    PlayerName = (*MetaObject)->GetStringField(TEXT("playerName"));
                }
            }
        }

        // 도착 시간을 기준으로 타임스탬프 생성
        const double Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        SpawnOrUpdateRemoteCharacter(SenderId, FTransform(NewRot, NewLoc), Speed, bIsFalling, PlayerName, Timestamp);
    }
}

void UWebSocketManager::SpawnOrUpdateWorldObject(const FString& ObjectID, const FTransform& TargetTransform, bool bIsLocalUpdate /*= false*/)
{
    if (!World) return;

    AActor* Actor = nullptr;

    // 1) 기존 Actor 가져오기
    if (RemoteWorldObjectsMap.Contains(ObjectID))
    {
        Actor = RemoteWorldObjectsMap[ObjectID];
    }
    else
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* ExistingActor = *It;
            if (ExistingActor && ExistingActor->GetName() == ObjectID)
            {
                Actor = ExistingActor;
                RemoteWorldObjectsMap.Add(ObjectID, Actor);
                break;
            }
        }
    }

    // 2) Actor가 존재하면 위치 보간 적용
    if (Actor)
    {
        if (!bIsLocalUpdate) // 서버에서 받은 Transform
        {
            // 목표 Transform 저장
            WorldObjectTargetTransforms.Add(ObjectID, TargetTransform);
            WorldObjectLastReceiveTime.Add(ObjectID, FDateTime::UtcNow().ToUnixTimestamp() + FDateTime::UtcNow().GetMillisecond() / 1000.0);
            // 기본 보간 속도 설정 (필요시 조절)
            if (!WorldObjectInterpSpeeds.Contains(ObjectID))
            {
                WorldObjectInterpSpeeds.Add(ObjectID, 1.f); // 기본 보간 속도
            }
        }
        else // 로컬에서 직접 이동
        {
            Actor->SetActorLocationAndRotation(TargetTransform.GetLocation(), TargetTransform.GetRotation());
            SendWorldObjectTransform(ObjectID, TargetTransform);
            LastSentTransforms.Add(ObjectID, TargetTransform);
        }
        return;
    }

    // 3) Actor가 없으면 새로 스폰
    FActorSpawnParameters Params;
    Actor = World->SpawnActor<AActor>(AActor::StaticClass(), TargetTransform, Params);
    if (Actor)
    {
        RemoteWorldObjectsMap.Add(ObjectID, Actor);
        // 스폰된 액터의 초기 Transform을 목표로 설정
        WorldObjectTargetTransforms.Add(ObjectID, TargetTransform);
        WorldObjectLastReceiveTime.Add(ObjectID, FDateTime::UtcNow().ToUnixTimestamp() + FDateTime::UtcNow().GetMillisecond() / 1000.0);
        if (!WorldObjectInterpSpeeds.Contains(ObjectID))
        {
            WorldObjectInterpSpeeds.Add(ObjectID, 1.f); // 기본 보간 속도
        }
    }

    if (bIsLocalUpdate)
    {
        SendWorldObjectTransform(ObjectID, TargetTransform);
        LastSentTransforms.Add(ObjectID, TargetTransform);
    }
}






void UWebSocketManager::SpawnOrUpdateRemoteCharacter(const FString& PlayerID, const FTransform& Transform, float Speed, bool bIsFalling, const FString& InPlayerName, double Timestamp)






{






    // Use the generated unique ID for the self-check.






    if (!MyPlayerId.IsEmpty() && PlayerID == MyPlayerId)






    {






        // It's me, don't spawn a remote character for myself.






        return;






    }













    // Try to find an existing character






    AMyRemoteCharacter* ExistingChar = OtherPlayersMap.FindRef(PlayerID);













    if (ExistingChar)






    {






        ExistingChar->AddTransformSnapshot(Transform.GetLocation(), Transform.GetRotation().Rotator(), Speed, bIsFalling, Timestamp);






        return;






    }













    if (!World || !RemoteCharacterClass) return;













    FActorSpawnParameters Params;






    // It is better to let the engine handle the name to avoid conflicts, we track by PlayerID in our map.






    // Params.Name = FName(*PlayerID);






    AMyRemoteCharacter* NewChar = World->SpawnActor<AMyRemoteCharacter>(RemoteCharacterClass, Transform, Params); // Initial spawn transform






    if (NewChar)






    {






        NewChar->SetName(InPlayerName); // Set the nameplate text.






        // Add initial transform to the snapshot buffer with the provided timestamp






        NewChar->AddTransformSnapshot(Transform.GetLocation(), Transform.GetRotation().Rotator(), Speed, bIsFalling, Timestamp);






        OtherPlayersMap.Add(PlayerID, NewChar);






    }






}








// Tick: 이전의 "변화가 있을 때만 전송" 논리를 보전하면서,
// 매 SendInterval 마다 전송하도록 변경.
// (변화가 거의 없으면 동일 데이터가 계속 전송되므로 서버 부하 우려가 있다면
//  아래에서 추가적인 'idle' 전송 스킵 로직을 넣어도 됨)
// Tick에서 월드 오브젝트 전송
// Tick에서 월드 오브젝트 전송
void UWebSocketManager::Tick(float DeltaTime)
{
    TimeSinceLastSend += DeltaTime;
    TimeSinceLastWorldSend += DeltaTime;

    if (!OwnerCharacter || MyPlayerId.IsEmpty() || !WebSocket.IsValid() || !WebSocket->IsConnected()) return;

    // 1) 플레이어 위치 전송
    if (TimeSinceLastSend >= SendInterval)
    {
        FTransform CurrentTransform = OwnerCharacter->GetActorTransform();
        float Speed = OwnerCharacter->GetVelocity().Size();
        bool bIsFalling = OwnerCharacter->GetCharacterMovement()->IsFalling();

        bool bPlayerChanged =
            !bHasSentInitialTransform ||
            !CurrentTransform.GetLocation().Equals(LastSentLocation, 1.0f) || // 1cm 이상 변화일 때
            !CurrentTransform.GetRotation().Rotator().Equals(LastSentRotation, 1.0f) ||
            FMath::Abs(Speed - LastSentSpeed) > 1.f ||
            bIsFalling != bLastSentIsFalling;

        if (bPlayerChanged)
        {
            SendUpdate(TEXT("player"), MyPlayerId, CurrentTransform, Speed, bIsFalling);

            LastSentLocation = CurrentTransform.GetLocation();
            LastSentRotation = CurrentTransform.GetRotation().Rotator();
            LastSentSpeed = Speed;
            bLastSentIsFalling = bIsFalling;
            bHasSentInitialTransform = true;
        }

        TimeSinceLastSend = 0.f;
    }

    // 2) 로컬에서 움직인 월드 오브젝트만 전송
    const float WorldSendInterval = SendInterval+0.2f;
    if (TimeSinceLastWorldSend >= WorldSendInterval)
    {
        for (auto& Elem : TrackedWorldObjects)
        {
            const FString& ObjectID = Elem.Key;
            TWeakObjectPtr<AActor> ActorPtr = Elem.Value;
            if (!ActorPtr.IsValid()) continue; // 유효성 검사

            AActor* Actor = ActorPtr.Get(); // 실제 포인터 가져오기
            if (!Actor) continue;

            FTransform CurrentTransform = Actor->GetActorTransform();
            bool bSignificantChange = true;

            if (LastSentTransforms.Contains(ObjectID))
            {
                const FTransform& LastTransform = LastSentTransforms[ObjectID];
                FVector DeltaLoc = CurrentTransform.GetLocation() - LastTransform.GetLocation();
                float AngleDeg = FMath::RadiansToDegrees(CurrentTransform.GetRotation().AngularDistance(LastTransform.GetRotation()));
                bSignificantChange = (DeltaLoc.Size() > WorldPosThreshold) || (AngleDeg > WorldRotThreshold);
            }

            if (bSignificantChange)
            {
                SendWorldObjectTransform(ObjectID, CurrentTransform);
                LastSentTransforms.Add(ObjectID, CurrentTransform);
            }
        }

        TimeSinceLastWorldSend = 0.f;
    }

    // 3) 월드 오브젝트 보간 처리
    for (auto It = WorldObjectTargetTransforms.CreateIterator(); It; ++It)
    {
        const FString& ObjectID = It.Key();
        const FTransform& TargetTransform = It.Value();

        AActor** ActorPtr = RemoteWorldObjectsMap.Find(ObjectID);
        if (ActorPtr && *ActorPtr)
        {
            AActor* Actor = *ActorPtr;
            float InterpSpeed = WorldObjectInterpSpeeds.Contains(ObjectID) ? WorldObjectInterpSpeeds[ObjectID] : 25.0f;

            FVector NewLocation = FMath::VInterpTo(Actor->GetActorLocation(), TargetTransform.GetLocation(), DeltaTime, InterpSpeed);
            FRotator NewRotation = FMath::RInterpTo(Actor->GetActorRotation(), TargetTransform.GetRotation().Rotator(), DeltaTime, InterpSpeed);

            Actor->SetActorLocationAndRotation(NewLocation, NewRotation);

            // 목표에 거의 도달했으면 맵에서 제거 (선택 사항: 불필요한 처리 방지)
            if (Actor->GetActorLocation().Equals(TargetTransform.GetLocation(), 0.1f) &&
                Actor->GetActorRotation().Equals(TargetTransform.GetRotation().Rotator(), 0.1f))
            {
                // It.RemoveCurrent(); // 현재는 계속 보간하도록 유지
            }
        }
        else
        {
            // 액터가 유효하지 않으면 맵에서 제거
            It.RemoveCurrent();
        }
    }
}








void UWebSocketManager::SendInitialWorldObjects()
{
    if (!World || !WebSocket.IsValid() || !WebSocket->IsConnected()) return;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor) continue;

        // 태그 필터링: "WorldObject" 태그가 있어야 전송
        if (!Actor->ActorHasTag(FName(TEXT("WorldObject")))) continue;

        FString ObjectID = Actor->GetName();
        FTransform Transform = Actor->GetActorTransform();

        // 트래킹 맵에 추가 (초기 상태 기준)
        TrackedWorldObjects.Add(ObjectID, Actor);
        LastSentTransforms.Add(ObjectID, Transform);

        // 서버로 전송 (update 메시지)
        TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("type"), TEXT("update"));
        Root->SetStringField(TEXT("entityType"), TEXT("world"));
        Root->SetStringField(TEXT("id"), ObjectID);

        FVector Loc = Transform.GetLocation();
        FRotator Rot = Transform.GetRotation().Rotator();

        TSharedPtr<FJsonObject> Pos = MakeShared<FJsonObject>();
        Pos->SetNumberField(TEXT("x"), Loc.X);
        Pos->SetNumberField(TEXT("y"), Loc.Y);
        Pos->SetNumberField(TEXT("z"), Loc.Z);

        TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
        RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
        RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
        RotObj->SetNumberField(TEXT("roll"), Rot.Roll);

        TSharedPtr<FJsonObject> State = MakeShared<FJsonObject>();
        State->SetObjectField(TEXT("position"), Pos);
        State->SetObjectField(TEXT("rotation"), RotObj);

        Root->SetObjectField(TEXT("state"), State);
        Root->SetBoolField(TEXT("isObject"), true);

        FString OutString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutString);
        FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

        WebSocket->Send(OutString);

        // UE_LOG(LogTemp, Warning, TEXT("[SEND INITIAL WORLD OBJECT] %s"), *ObjectID);
    }
}


void UWebSocketManager::SendWorldObjectTransform(const FString& ObjectID, const FTransform& Transform)
{
    if (!WebSocket.IsValid() || !WebSocket->IsConnected()) return;

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("type"), TEXT("update"));
    Root->SetStringField(TEXT("entityType"), TEXT("world"));
    Root->SetStringField(TEXT("id"), ObjectID);

    FVector Loc = Transform.GetLocation();
    FRotator Rot = Transform.GetRotation().Rotator();

    TSharedPtr<FJsonObject> Pos = MakeShared<FJsonObject>();
    Pos->SetNumberField(TEXT("x"), Loc.X);
    Pos->SetNumberField(TEXT("y"), Loc.Y);
    Pos->SetNumberField(TEXT("z"), Loc.Z);

    TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
    RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
    RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
    RotObj->SetNumberField(TEXT("roll"), Rot.Roll);

    TSharedPtr<FJsonObject> State = MakeShared<FJsonObject>();
    State->SetObjectField(TEXT("position"), Pos);
    State->SetObjectField(TEXT("rotation"), RotObj);

    Root->SetObjectField(TEXT("state"), State);
    Root->SetBoolField(TEXT("isObject"), true);

    FString OutString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutString);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    WebSocket->Send(OutString);

    // UE_LOG(LogTemp, Warning, TEXT("[SEND WORLD OBJECT TRANSFORM] %s"), *ObjectID);
}

TStatId UWebSocketManager::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UWebSocketManager, STATGROUP_Tickables);
}
