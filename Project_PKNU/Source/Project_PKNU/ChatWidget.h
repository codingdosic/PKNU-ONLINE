// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "MyWebSocketCharacter.h"
#include "ChatWidget.generated.h"


UCLASS()
class PROJECT_PKNU_API UChatWidget : public UUserWidget
{
	GENERATED_BODY()

public:

    // 캐릭터 참조 변수 추가
    UPROPERTY()
    AMyWebSocketCharacter* OwnerCharacter;

    // 캐릭터 참조를 세팅하는 함수
    void SetOwnerCharacter(AMyWebSocketCharacter* InCharacter) { OwnerCharacter = InCharacter; }

    UPROPERTY(meta = (BindWidget))
    class UEditableTextBox* ChatInputBox;

    UPROPERTY(meta = (BindWidget))
    class UScrollBox* ChatScrollBox;

    UFUNCTION(BlueprintCallable)
    void FocusOnInput();

    UFUNCTION(BlueprintCallable)
    void AddChatMessage(const FString& SenderId, const FString& Message);


    UFUNCTION()
    void OnChatTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

    
        
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
        
    virtual void NativeOnInitialized() override;


};
