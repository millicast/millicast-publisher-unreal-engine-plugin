// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include <string>

namespace Millicast::Publisher
{
	inline std::string to_string(const FString& Str)
	{
		auto Ansi = StringCast<ANSICHAR>(*Str, Str.Len());
		std::string Res{ Ansi.Get(), static_cast<SIZE_T>(Ansi.Length()) };
		return Res;
	}

	inline FString ToString(const std::string& Str)
	{
		auto Conv = StringCast<TCHAR>(Str.c_str(), Str.size());
		FString Res{ Conv.Length(), Conv.Get() };
		return Res;
	}

        template<typename T>
        bool IsEmpty(const TArray<T>& Array)
        {
        #if ENGINE_MAJOR_VERSION < 5
          return Array.Num() == 0;
        #else
          return Array.IsEmpty();
        #endif
        }

}
