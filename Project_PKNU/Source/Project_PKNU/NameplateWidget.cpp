
#include "NameplateWidget.h"
#include "Components/TextBlock.h"

void UNameplateWidget::SetName(const FString& Name)
{
    if (NameText)
    {
        NameText->SetText(FText::FromString(Name));
    }
}
