// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

namespace emilua {

class app_context;

namespace main {
int main(int argc, char *argv[], char *envp[]);
void parse_emilua_bin_args(int argc, char *argv[], app_context& appctx);

void parse_args(int argc, char *argv[], app_context& appctx)
{
    return parse_emilua_bin_args(argc, argv, appctx);
}

} // namespace main
} // namespace emilua

int main(int argc, char *argv[], char *envp[])
{
    return emilua::main::main(argc, argv, envp);
}
