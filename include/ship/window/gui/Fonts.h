#pragma once

// Upstream this holds the base85-compressed Font Awesome webfont (~150 KB) that the
// ImGui GUI loads into its font atlas. The console build strips the ImGui GUI and
// cannot afford the atlas, so the data is empty; the symbol exists only so the
// (no-op) font-setup code compiles and links.
static const char fontawesome_compressed_data_base85[] = "";
