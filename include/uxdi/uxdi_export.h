#pragma once

// DLL export macros for Windows platform
// Allows building uxdi components as shared libraries (DLLs)

#ifdef UXDI_STATIC_DEFINE
#  define UXDI_API
#  define UXDI_NO_EXPORT
#else
#  ifndef UXDI_API
#    ifdef uxdi_core_EXPORTS
       /* We are building this library */
#      define UXDI_API __declspec(dllexport)
#    else
       /* We are using this library */
#      define UXDI_API __declspec(dllimport)
#    endif
#  endif

#  ifndef UXDI_NO_EXPORT
#    define UXDI_NO_EXPORT
#  endif
#endif

#ifndef UXDI_DEPRECATED
#  define UXDI_DEPRECATED __declspec(deprecated)
#endif

#ifndef UXDI_DEPRECATED_EXPORT
#  define UXDI_DEPRECATED_EXPORT UXDI_API UXDI_DEPRECATED
#endif

#ifndef UXDI_DEPRECATED_NO_EXPORT
#  define UXDI_DEPRECATED_NO_EXPORT UXDI_NO_EXPORT UXDI_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef UXDI_NO_DEPRECATED
#    define UXDI_NO_DEPRECATED
#  endif
#endif

/* Compile-time deprecation warnings for Visual Studio */
#if defined(_MSC_VER)
#  define UXDI_DEPRECATED_MSG(msg) __declspec(deprecated(msg))
#elif defined(__GNUC__)
#  define UXDI_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
#else
#  define UXDI_DEPRECATED_MSG(msg) UXDI_DEPRECATED
#endif
