#!/usr/bin/env -S gawk --file
#
# emilua_gperf.awk
#
# Written in 2023 by Vinícius dos Santos Oliveira <vini.ipsmaker@gmail.com>
#
# To the extent possible under law, the author(s) have dedicated all copyright
# and related and neighboring rights to this software to the public domain
# worldwide. This software is distributed without any warranty.
#
# You should have received a copy of the CC0 Public Domain Dedication along with
# this software. If not, see
# <http://creativecommons.org/publicdomain/zero/1.0/>.

function count_lfs(str    , pieces) {
    if (index(str, "\n") == 0) {
        return 0
    }
    return split(str, pieces, "\n") - 1
}

function append_header(str) {
    output_header = output_header "\n#line " linenrdst " \"" ARGV[ARGC - 1] \
        "\"\n" str
    linenrdst += count_lfs(str) + 2
}

function append_body(str, is_unprocessed_original_source    , pp) {
    if (is_unprocessed_original_source) {
        pp = "#line " linenrsrc " \"" FILENAME "\"\n"
        linenrsrc += count_lfs(str)
    } else {
        # we patch the correct value later
        pp = "#line 0 \"" ARGV[ARGC - 1] "\"\n"
    }

    if ( \
        length(output_body) > 0 && \
        substr(output_body, length(output_body)) != "\n" \
    ) {
        pp = "\n" pp
    }
    output_body = output_body pp str
}

function fix_pp_line0(    linenr, out2, i, n) {
    linenr = count_lfs(output_header) + 2

    n = split(output_body, out2, "#line 0 ")
    output_body = ""
    for (i = 1 ; i < n ; ++i) {
        linenr += count_lfs(out2[i])
        output_body = output_body out2[i] "#line " linenr " "
    }
    output_body = output_body out2[n]
}

function gperf(context_index    , i, proc, input, out) {
    proc = GPERF_BIN " --language=C++ --enum --readonly-tables --struct-type " \
        "--initializer-suffix=,{} --class-name=Perfect_Hash_" context_index
    input = sprintf( \
        "struct word_type_%i { const char* name; %s; };\n%%%%\n",
        context_index, context[context_index, "param"])

    for (i in context[context_index, "pairs"]) {
        input = input i ", EMILUA_GPERF_DETAIL_VALUE" \
            context[context_index, "pairs"][i] "_\n"
    }

    input = input "%%\n"

    printf "%s", input |& proc
    close(proc, "to")
    proc |& getline out
    if (close(proc) != 0) {
        print "gperf exited with failure" >"/dev/stderr"
        exit 1
    }
    if (out !~ /duplicates = 0/) {
        print "WARNING: Duplicates found" >"/dev/stderr"
    }

    for (i in context[context_index, "pairs"]) {
        sub("EMILUA_GPERF_DETAIL_VALUE" context[context_index, "pairs"][i] "_",
            context["pairs"][context[context_index, "pairs"][i]], out)
    }

    if (length(context[context_index, "ppguard"]) > 0) {
        append_header( \
            "#if " context[context_index, "ppguard"] "\n" \
            "namespace emilua::gperf::detail {\nnamespace {\n" out \
            "} // namespace\n} // namespace emilua::gperf::detail\n" \
            "#endif // " context[context_index, "ppguard"] "\n")
    } else {
        append_header( \
            "namespace emilua::gperf::detail {\nnamespace {\n" out \
            "} // namespace\n} // namespace emilua::gperf::detail\n")
    }

    if (length(context[context_index, "default_value"]) > 0) {
        return sprintf( \
            " ::emilua::gperf::detail::value_or(::emilua::gperf::detail::" \
            "Perfect_Hash_%1$i::" \
            "in_word_set((%2$s).data(), (%2$s).size()), %3$s)",
            context_index, context[context_index, "symbol"],
            context[context_index, "default_value"])
    } else {
        return sprintf( \
            " ::emilua::gperf::detail::make_optional(::emilua::gperf::" \
            "detail::Perfect_Hash_%1$i::" \
            "in_word_set((%2$s).data(), (%2$s).size()))",
            context_index, context[context_index, "symbol"])
    }
}

function process_arguments(    i, levels, ret, ch) {
    if (substr($0, 1, 1) != "(") {
        printf "Open parens expected. Got: `%s`\n", substr($0, 1, 1) \
            >"/dev/stderr"
        exit 1
    }

    levels = 1
    for (i = 2 ; levels != 0 ; ++i) {
        ch = substr($0, i, 1)
        if (ch == "(") {
            ++levels
        } else if (ch == ")") {
            --levels
        } else if (ch == "\n") {
            ++linenrsrc
        }
    }
    ret = substr($0, 2, i - 3)
    $0 = substr($0, i)
    return ret
}

function process_param(context_index) {
    context[context_index, "param"] = process_arguments()
}

function process_default_value(context_index    , aux, value) {
    value = process_arguments()
    aux = $0
    $0 = value
    value = ""
    while (match($0, /EMILUA_GPERF_BEGIN/)) {
        value = value substr($0, 1, RSTART - 1)
        $0 = substr($0, RSTART + RLENGTH)
        value = value process_gperf_block()
    }
    value = value $0
    $0 = aux
    context[context_index, "default_value"] = value
}

function process_pair(context_index    , saved_input, value, matches) {
    value = process_arguments()
    match(value, /"([^"]*)"/, matches)
    value = substr(value, index(value, ",") + 1)

    if (matches[1] in context[context_index, "pairs"]) {
        printf("ERROR: Duplicate value found: `%s`\n", matches[1]) \
            >"/dev/stderr"
        exit 1
    }

    saved_input = $0
    $0 = value
    value = ""
    while (match($0, /EMILUA_GPERF_BEGIN/)) {
        value = value substr($0, 1, RSTART - 1)
        $0 = substr($0, RSTART + RLENGTH)
        value = value process_gperf_block()
    }
    value = value $0
    $0 = saved_input

    gsub(/&/, "\\\\&", value)
    context[context_index, "pairs"][matches[1]] = context["next_value"]
    context["pairs"][context["next_value"]] = value
    ++context["next_value"]
}

function process_ppguard(context_index) {
    context[context_index, "ppguard"] = process_arguments()
}

function process_decls_block(    symbol, idx, saved_input, value, matches) {
    symbol = "EMILUA_GPERF_DECLS_END(" process_arguments() ")"
    idx = index($0, symbol)
    value = substr($0, 1, idx - 1)
    linenrsrc += count_lfs(value)
    $0 = substr($0, idx + length(symbol))

    saved_input = $0
    $0 = value
    value = ""
    while (match($0, /EMILUA_GPERF_BEGIN/)) {
        value = value substr($0, 1, RSTART - 1)
        $0 = substr($0, RSTART + RLENGTH)
        value = value process_gperf_block()
    }
    value = value $0
    $0 = saved_input

    match(value, /EMILUA_GPERF_NAMESPACE\(([[:alpha:]_][[:alnum:]_:]*)?\)/,
          matches)
    gsub(/EMILUA_GPERF_NAMESPACE\(([[:alpha:]_][[:alnum:]_:]*)?\)/, "", value)
    if (matches[1, "length"] > 0) {
        append_header(sprintf( \
            "namespace %1$s {\n%2$s\n} // namespace %1$s\n",
            matches[1], value))
    } else {
        append_header(value)
    }
}

function process_gperf_block(    context_index, symbol, matches) {
    context_index = context["next"]++

    # initialize array
    context[context_index, "pairs"][0] = 0
    delete context[context_index, "pairs"][0]

    symbol = process_arguments()
    if (symbol !~ /^([[:alpha:]_][[:alnum:]_]*)?$/) {
        printf "Bad name for block: `%s`\n", symbol >"/dev/stderr"
        exit 1
    }
    context[context_index, "symbol"] = symbol

    while (match( \
        $0, /EMILUA_GPERF_(BEGIN|END|PARAM|DEFAULT_VALUE|PAIR|PPGUARD)/, \
        matches \
    )) {
        linenrsrc += count_lfs(substr($0, 1, RSTART + RLENGTH - 1))
        $0 = substr($0, RSTART + RLENGTH)
        if (matches[1] == "BEGIN") {
            print "GPERF block cannot appear here" >"/dev/stderr"
            exit 1
        } else if (matches[1] == "END") {
            if (process_arguments() != symbol) {
                printf "Mismatched blocks. Expected `%s`\n", symbol \
                    >"/dev/stderr"
                exit 1
            }
            return gperf(context_index)
        } else if (matches[1] == "PARAM") {
            process_param(context_index)
        } else if (matches[1] == "DEFAULT_VALUE") {
            process_default_value(context_index)
        } else if (matches[1] == "PAIR") {
            process_pair(context_index)
        } else if (matches[1] == "PPGUARD") {
            process_ppguard(context_index)
        }
    }
}

BEGIN {
    RS = "^$"
    FS = "\0"
    getline
    linenrsrc = 1
    linenrdst = 9
    output_header = "#line 2 \"" ARGV[ARGC - 1] "\"\n\
#include <cstring>\n\
\n\
namespace emilua::gperf::detail { using std::size_t; using std::strcmp; }\n\
#line 6 \"" ARGV[ARGC - 1] "\"\n\
\n"
    output_body = ""
    context["next"] = 1
    context["next_value"] = 1
    while (match($0, /EMILUA_GPERF_(BEGIN|DECLS_BEGIN)/)) {
        append_body(substr($0, 1, RSTART - 1), 1)
        if (substr($0, RSTART, RLENGTH) == "EMILUA_GPERF_DECLS_BEGIN") {
            $0 = substr($0, RSTART + RLENGTH)
            process_decls_block()
        } else {
            $0 = substr($0, RSTART + RLENGTH)
            append_body(process_gperf_block())
        }
    }
    append_body($0, 1)
    $0 = ""
    fix_pp_line0()
    printf("%s%s", output_header, output_body) >ARGV[ARGC - 1]
}
