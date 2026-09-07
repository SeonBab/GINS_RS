// Fill out your copyright notice in the Description page of Project Settings.


#include "RSDamageNumberWidget.h"

#include "Components/TextBlock.h"

void URSDamageNumberWidget::SetDamageText(const FText& InDamageText)
{
	if (TextBlock_Damage)
	{
		TextBlock_Damage->SetText(InDamageText);
	}
}
