#if !defined (__MVCOMMS_H)
#define __MVCOMMS_H


// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the MVCOMMS_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// MVCOMMS_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef MVCOMMS_EXPORTS
#ifdef _WIN32
#define MVCOMMS_API __declspec(dllexport)
#else
#define MVCOMMS_API __attribute__((visibility("default")))
#endif
#else
#ifdef _WIN32
#define MVCOMMS_API __declspec(dllimport)
#else
#define MVCOMMS_API __attribute__((visibility("hidden")))
#endif
#endif

//#define WIN32_LEAN_AND_MEAN
//#include <windows.h>

#endif /* __MVCOMMS_H */


