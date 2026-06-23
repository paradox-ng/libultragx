#pragma once

// Lean stub. The adopted Fast3D interpreter (#include) carries this header but
// uses no symbol from it: prism shader-template resources drive the desktop
// shader backends, whereas libultragx renders fixed-function GX/TEV and loads no
// Shader resources. Kept empty so the include resolves without pulling in LUS's
// ResourceFactoryBinary / Shader machinery. If a real Shader factory is ever
// needed, replace this with the full declaration.
