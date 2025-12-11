
#include "ChatWidget.h"

#include "Components/EditableTextBox.h"
#include <Components/TextBlock.h>

void UChatWidget::FocusOnInput()
{
    if (ChatInputBox)
    {
        // 한 프레임 지연 후 포커스 주기
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
            {
                ChatInputBox->SetKeyboardFocus();
            }, 0.01f, false); // 약간의 지연을 줌
    }
}

void UChatWidget::AddChatMessage(const FString& SenderId, const FString& Message)
{
    if (ChatScrollBox)
    {
        UTextBlock* NewMessage = NewObject<UTextBlock>(ChatScrollBox);
        if (NewMessage)
        {
            // 메시지 형식 지정: "playerName: 메시지"
            FString FullMessage = FString::Printf(TEXT("%s: %s"), *SenderId, *Message);

            // 텍스트 설정 및 추가
            NewMessage->SetText(FText::FromString(FullMessage));
            NewMessage->SetAutoWrapText(true); // 자동 줄바꿈 활성화
            ChatScrollBox->AddChild(NewMessage);
            ChatScrollBox->ScrollToEnd();
        }
    }
}



void UChatWidget::OnChatTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    UE_LOG(LogTemp, Warning, TEXT("OnChatTextCommitted Called! Commit Method: %d"), (int)CommitMethod);

    if (CommitMethod == ETextCommit::OnEnter)
    {
        if (OwnerCharacter && !Text.IsEmpty())
        {
            // 캐릭터에 메시지 전송 요청
            OwnerCharacter->SendChatMessage(Text.ToString());

            // 메시지는 서버로부터 브로드캐스트를 받은 후 AddChatMessage를 통해 추가됩니다.
            // 로컬에서 즉시 추가하지 않습니다.

        }

        // 텍스트박스 비우기
        ChatInputBox->SetText(FText::GetEmpty());

        // 엔터 누른 후도 계속 포커스 유지
        FocusOnInput();
    }
}

void UChatWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (ChatInputBox)
    {
        ChatInputBox->OnTextCommitted.AddDynamic(this, &UChatWidget::OnChatTextCommitted);
    }
}


FReply UChatWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        APlayerController* PC = GetOwningPlayer();
        if (PC)
        {
            // 한 프레임 뒤에 강제로 포커스 제거 + 게임 모드 전환
            FTimerHandle TimerHandle;
            GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateLambda([PC]()
            {
                FInputModeGameOnly GameOnly;
                PC->SetInputMode(GameOnly);
                PC->bShowMouseCursor = false;
                FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);

            }), 0.01f, false); // 1 프레임 지연

            return FReply::Handled();
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}



