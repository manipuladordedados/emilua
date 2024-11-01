// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/core.hpp>

extern char** environ;

namespace emilua::main {
int main(int argc, char *argv[], char *envp[]);
} // namespace emilua::main

int main(int argc, char *argv[], char *envp[])
{
#if BOOST_OS_UNIX
    // FreeBSD has an unfortunate bug:
    // https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=265008
    //
    // Until it's fixed, Emilua will force this workaround on every UNIX
    // platform just to keep things a little simpler with less ifdefs required.
    emilua::app_context::environp = &environ;
#endif // BOOST_OS_UNIX

    return emilua::main::main(argc, argv, envp);
}
