#pragma once
// Minimal ImGui config stand-in for the no-ImGui console build. Fast3D's
// rendering API references ImTextureID (normally from imgui.h); libultragx ships
// no ImGui, so we provide only the type. The framebuffer-as-ImGui-texture path
// is unused on GameCube/Wii (we render direct to the screen).
#ifndef LUGX_IMTEXTUREID_DEFINED
#define LUGX_IMTEXTUREID_DEFINED
typedef void* ImTextureID;
#endif
