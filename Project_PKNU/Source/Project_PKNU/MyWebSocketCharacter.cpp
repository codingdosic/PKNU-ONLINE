// Fill out your copyright notice in the Description page of Project Settings.


#include "MyWebSocketCharacter.h"
#include "Components/WidgetComponent.h"
#include "NameplateWidget.h"
#include "PknuGameInstance.h"
#include "WebSocketManager.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "ChatWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"

// 생성자
AMyWebSocketCharacter::AMyWebSocketCharacter()
{
    // 매 프레임마다 Tick 수행 여부
    PrimaryActorTick.bCanEverTick = true;

    NameplateComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameplateComponent"));
    NameplateComponent->SetupAttachment(RootComponent);
    NameplateComponent->SetWidgetSpace(EWidgetSpace::Screen);
    NameplateComponent->SetDrawSize(FVector2D(200, 30));

}

void AMyWebSocketCharacter::SetName(const FString& Name)
{
    UNameplateWidget* NameplateWidget = Cast<UNameplateWidget>(NameplateComponent->GetUserWidgetObject());
    if (NameplateWidget)
    {
        NameplateWidget->SetName(Name);
    }
}

// 캐릭터 생성 시 자동으로 호출
void AMyWebSocketCharacter::BeginPlay()
{
    // 부모의 메서드 호출
    Super::BeginPlay();

    // Add Input Mapping Context
    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(WebSocketCharacterMappingContext, 0);
        }
    }

    // GameInstance에서 WebSocketManager 가져오기
    UPknuGameInstance* GameInstance = GetGameInstance<UPknuGameInstance>();
    if (GameInstance)
    {
        UE_LOG(LogTemp, Warning, TEXT("AMyWebSocketCharacter::BeginPlay - GameInstance is Valid."));
        WebSocketManager = GameInstance->GetWebSocketManager();
        if (WebSocketManager)
        {
            UE_LOG(LogTemp, Warning, TEXT("AMyWebSocketCharacter::BeginPlay - WebSocketManager retrieved. Registering character."));
            // WebSocketManager에 이 캐릭터를 등록
            WebSocketManager->RegisterPlayerCharacter(this);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("AMyWebSocketCharacter::BeginPlay - Failed to retrieve WebSocketManager from GameInstance."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("AMyWebSocketCharacter::BeginPlay - Failed to get PknuGameInstance."));
    }
}

// 액터 제거나 게임 종료시 호출
void AMyWebSocketCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // WebSocketManager에서 이 캐릭터 등록 해제
    if (WebSocketManager)
    {
        WebSocketManager->UnregisterPlayerCharacter();
    }

    // 부모의 메서드 호출
    Super::EndPlay(EndPlayReason);
}

// 매 프레임마다 호출되는 함수
void AMyWebSocketCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (WebSocketManager)
    {
        WebSocketManager->Tick(DeltaTime);
    }
}

void AMyWebSocketCharacter::ToggleChatInput()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    // 위젯이 없으면 생성
    if (!ChatWidgetInstance)
    {
        if (ChatWidgetClass)
        {
            ChatWidgetInstance = CreateWidget<UChatWidget>(GetWorld(), ChatWidgetClass);
            if (ChatWidgetInstance)
            {
                ChatWidgetInstance->AddToViewport();
                ChatWidgetInstance->SetOwnerCharacter(this);
            }
        }
    }

    // 위젯이 유효하고 뷰포트에 있다면 로직 실행
    if (ChatWidgetInstance && ChatWidgetInstance->IsInViewport())
    {
        TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetUserFocusedWidget(0);
        const bool bHasFocus = IsValid(ChatWidgetInstance->ChatInputBox) && (FocusedWidget == ChatWidgetInstance->ChatInputBox->TakeWidget());

        if (!bHasFocus)
        {
            // 채팅 입력창에 포커스 주기
            FInputModeGameAndUI InputMode;
            InputMode.SetWidgetToFocus(ChatWidgetInstance->ChatInputBox->TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(InputMode);
            PC->bShowMouseCursor = true;
            ChatWidgetInstance->FocusOnInput();
        }
        else
        {
            // 게임 모드로 돌아가기
            FInputModeGameOnly GameOnly;
            PC->SetInputMode(GameOnly);
            PC->bShowMouseCursor = false;
            FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
        }
    }
}


// 조작 설정
void AMyWebSocketCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    // 부모의 메서드 호출
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Enhanced Input 사용
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		//ToggleChat
		EnhancedInputComponent->BindAction(ToggleChatAction, ETriggerEvent::Started, this, &AMyWebSocketCharacter::ToggleChatInput);
	}
}

void AMyWebSocketCharacter::SendChatMessage(const FString& Message)
{
    if (WebSocketManager)
    {
        WebSocketManager->SendChatMessage(Message);
    }
}

FString AMyWebSocketCharacter::GetMySocketId() const
{
    // In the new architecture, the client defines its own ID, which is its actor name.
    return GetName();
}