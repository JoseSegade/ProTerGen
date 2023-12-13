#pragma once

#include <string>
#include <wrl/client.h>
#include <cstdint>
#include <stdexcept>
#include <comdef.h>

class HrException
{
public: 
   explicit HrException(HRESULT hr, const std::wstring& functionName, const std::wstring& fileName, int32_t lineNumber)
      : ErrorCode(hr), FunctionName(functionName), FileName(fileName), LineNumber(lineNumber) {};

   inline std::wstring ToString()
   {
      _com_error err(ErrorCode);
      std::wstring msg = err.ErrorMessage();

      return FunctionName + L" failed in " + FileName + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
   }

   HRESULT ErrorCode;
   std::wstring FunctionName;
   std::wstring FileName;
   int32_t LineNumber;



};


#ifndef ThrowIfFailed
#define WIDE2(x) L##x
#define WIDE1(x) WIDE2(x)
#define WFILE WIDE1(__FILE__)
#define ThrowIfFailed(x)                            \
{                                                   \
   HRESULT _hr = x;                                 \
   if (FAILED(_hr))                                 \
   {                                                \
      throw HrException(_hr, L#x, WFILE, __LINE__); \
   }                                                \
}
#endif