#pragma once // (c) 2020 Lukas Brunner

#define BRWL_NS namespace BRWL {
	
#define BRWL_NS_END }

#define BRWL_PAL_NS BRWL_NS \
namespace PAL {

#define BRWL_PAL_NS_END BRWL_NS_END\
}



#if defined(_WIN32) && !defined(BRWL_PLATFORM_WINDOWS)
#define BRWL_PLATFORM_WINDOWS
#endif

#define BRWL_USE_DEAR_IM_GUI
#if defined(BRWL_USE_DEAR_IM_GUI) && !defined(_WIN64)
#error Dear ImGui is not supported on 32bit.
#endif

#include <cstdint>
#include <memory>
#include <string>
#include <cmath>
#include <mutex>
#include <vector>
#include <algorithm>


#ifdef BRWL_PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX


#include <Windows.h>
// DX 12
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#define ENABLE_PIX 1
#define FORCE_ENABLE_DRED 1

// if debugging with pix, don't touch the debug interfaces
#if ENABLE_PIX || defined(SUBMISSION)
	#define ENABLE_GRAPHICS_DEBUG_FEATURES 0
#else
	#define ENABLE_GRAPHICS_DEBUG_FEATURES 1
#endif


#if ENABLE_GRAPHICS_DEBUG_FEATURES
#include <dxgidebug.h>
#endif

// undefine annoying macros from windows headers
#undef near
#undef far
#ifdef ERROR
#undef ERROR
#endif

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#endif // BRWL_PLATFORM_WINDOWS

#pragma warning(error:4003) // not enough actual parameters for macro
#pragma warning(error:4715) // not all control paths return a value

#ifdef UNICODE
#define BRWL_CHAR wchar_t
//TODO: rename to something shorter. And it's actually a string
#define BRWL_CHAR_LITERAL(LITERAL) L##LITERAL
#define BRWL_STRLEN wcslen
#define BRWL_STR ::std::wstring
#define BRWL_SNPRINTF swprintf
#define BRWL_STD_COUT ::std::wcout
#define BRWL_STD_CIN ::std::wcin
#define BRWL_STRCMP ::std::wcscmp
#else
#define BRWL_CHAR char
#define BRWL_CHAR_LITERAL(LITERAL) LITERAL
#define BRWL_STRLEN strlen
#define BRWL_STR ::std::string
#define BRWL_SNPRINTF snprintf
#define BRWL_STD_CIN ::std::cin
#define BRWL_STRCMP ::std::strcmp
#endif

#ifdef BRWL_PLATFORM_WINDOWS
#define BRWL_NEWLINE BRWL_CHAR_LITERAL("\r\n")
#endif

#if defined(BRWL_PLATFORM_WINDOWS) && ENABLE_PIX
#define USE_PIX
#include "pix3.h"
#define SCOPED_CPU_EVENT(r, g, b, label, ...) PIXScopedEvent(PIX_COLOR(r, g, b), BRWL_CHAR_LITERAL(label), ## __VA_ARGS__)
#define SCOPED_GPU_EVENT(pContext, r, g, b, label, ...) PIXScopedEvent(pContext, PIX_COLOR(r, g, b), BRWL_CHAR_LITERAL(label), ##  __VA_ARGS__)
#else
#define SCOPED_CPU_EVENT(...)
#define SCOPED_GPU_EVENT(...)
#endif

#ifndef SUBMISSION
#define BRWL_USE_DEBUG_SYMBOLS
#endif

#include "Utilities.h"
#include "Exception.h"

#include "Logger.h"
#include "BrwlMath.h"

#include "MacroUtils.h"

BRWL_NS

// this function should be called as early as possible during startup of the program
void earlyStaticInit();
// this function should be called as late as possible during destruction of the program
void lateStaticDestroy();

BRWL_NS_END
