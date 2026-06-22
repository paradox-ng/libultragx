#include "invoke.h"

// libultragx stub. prism's native-invoke feature lets a shader template call a
// registered native C function with a runtime-determined argument count. The GX
// backend uses fixed-function TEV and never processes shader templates, so this
// code path is unreachable here.
//
// The upstream implementation dispatches by calling a no-prototype function
// pointer `uintptr_t (*)()` with a varying number of arguments, which does not
// compile under devkitPPC's strict C prototypes. Since the feature is dead for
// us, this stub satisfies the declaration without exercising it.
uintptr_t invoke(uintptr_t (*native_code)(), uintptr_t* args, size_t length) {
    (void)native_code;
    (void)args;
    (void)length;
    return (uintptr_t)0;
}
