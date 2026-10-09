#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"

// Content fingerprints hash the parsed document, not its bytes: whitespace,
// line endings and number spelling (990 vs 990.0) do not change a save's
// compatibility, while any changed value, key or ordering still does.
inline FString SeigeCanonicalJson(const TSharedPtr<FJsonObject>& Object)
{
    FString Text;
    if(Object.IsValid())FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
    return Text;
}
