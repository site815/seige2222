#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
namespace SeigeCombatJson
{
using O=TSharedPtr<FJsonObject>;
inline bool Num(const O& V,const TCHAR* K,double& D,double Min=0,double Max=1.e12){return V&&V->TryGetNumberField(K,D)&&FMath::IsFinite(D)&&D>=Min&&D<=Max;}
inline bool Int(const O& V,const TCHAR* K,int32& D,int32 Min=0,int32 Max=MAX_int32){double N=0;if(!Num(V,K,N,Min,Max)||N!=FMath::FloorToDouble(N))return false;D=int32(N);return true;}
inline bool Str(const O& V,const TCHAR* K,FString& S){return V&&V->TryGetStringField(K,S);}
inline bool List(const O& V,const TCHAR* K,TArray<FString>& A){const TArray<TSharedPtr<FJsonValue>>* L=nullptr;if(!V||!V->TryGetArrayField(K,L)||L->Num()>128)return false;A.Reset();for(const auto& X:*L){FString S;if(!X->TryGetString(S))return false;A.Add(S);}return true;}
inline void PutList(const O& V,const TCHAR* K,const TArray<FString>& L){TArray<TSharedPtr<FJsonValue>> A;for(const auto& X:L)A.Add(MakeShared<FJsonValueString>(X));V->SetArrayField(K,A);}
inline void Point(const O& V,const TCHAR* K,FVector2D P){V->SetArrayField(K,{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y)});}
inline bool Point(const O& V,const TCHAR* K,FVector2D& P,double Bound){const TArray<TSharedPtr<FJsonValue>>* A=nullptr;double X,Y;if(!V||!V->TryGetArrayField(K,A)||A->Num()!=2||!(*A)[0]->TryGetNumber(X)||!(*A)[1]->TryGetNumber(Y)||!FMath::IsFinite(X)||!FMath::IsFinite(Y)||FMath::Abs(X)>Bound||FMath::Abs(Y)>Bound)return false;P=FVector2D(X,Y);return true;}
inline void Numbers(const O& V,const TCHAR* K,const TArray<double>& L){TArray<TSharedPtr<FJsonValue>> A;for(double X:L)A.Add(MakeShared<FJsonValueNumber>(X));V->SetArrayField(K,A);}
inline bool Numbers(const O& V,const TCHAR* K,TArray<double>& L){const TArray<TSharedPtr<FJsonValue>>* A=nullptr;if(!V||!V->TryGetArrayField(K,A)||A->Num()>128)return false;L.Reset();for(const auto& X:*A){double N;if(!X->TryGetNumber(N)||!FMath::IsFinite(N)||N<0||N>1.e9)return false;L.Add(N);}return true;}
inline bool OneOf(const FString& S,std::initializer_list<const TCHAR*> Values){for(const auto* V:Values)if(S==V)return true;return false;}
inline void PutAmounts(const O& V,const TCHAR* K,const TMap<FString,double>& M){O A=MakeShared<FJsonObject>();TArray<FString> Keys;M.GetKeys(Keys);Keys.Sort();for(const auto& Key:Keys)A->SetNumberField(Key,M[Key]);V->SetObjectField(K,A);}
}
