#pragma once

// API export/import macros. On GameCube/Wii there are no shared libraries, so
// these reduce to C linkage; the _WIN32 dllexport path is kept only for source
// compatibility with code that includes this header.
#ifdef __cplusplus
#define API_EXTERN extern "C"
#else
#define API_EXTERN extern
#endif

#ifdef _WIN32
#ifndef __DLL__
#define API_EXPORT API_EXTERN __declspec(dllexport)
#else
#define API_EXPORT API_EXTERN __declspec(dllimport)
#endif
#else
#define API_EXPORT API_EXTERN
#endif
