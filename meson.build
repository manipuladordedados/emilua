project(
    'emilua', 'cpp',
    default_options : ['cpp_std=c++20'],
    meson_version : '>=1.2.0',
    license : 'BSL-1.0',
    version : '0.6.0',
)

# https://github.com/mesonbuild/meson/issues/2320
subdir('include/emilua')

if get_option('thread_support_level') >= 1
    thread_dep = dependency('threads')
else
    thread_dep = dependency('', required : false)
endif

boost_modules = [
    'nowide',
]

if host_machine.system() == 'linux'
    boost_modules += 'context'
endif

if host_machine.system() == 'freebsd'
    add_project_link_arguments('-ljail', language : 'cpp')
endif

luajit = dependency('luajit', version : '>=2.0.5')
boost = dependency(
    'boost',
    version : '>=1.82',
    modules : boost_modules,
)
fmt = dependency('fmt', version : '>=8.0')
serd = dependency('serd-0')
sord = dependency('sord-0')
trial_protocol = dependency('trial-protocol')
cereal = dependency('cereal')
libdl = meson.get_compiler('cpp').find_library('dl', required : false)
gawk = find_program('gawk')

add_project_arguments(
    '-DBOOST_DLL_USE_STD_FS',
    '-DBOOST_ASIO_DISABLE_CO_AWAIT',
    '-DBOOST_ASIO_DISABLE_STD_COROUTINE',
    '-DBOOST_BEAST_USE_STD_STRING_VIEW',
    language : 'cpp',
)

if meson.get_compiler('cpp').get_id() == 'msvc'
    if host_machine.cpu_family() != 'x86_64'
        error('Full LuaJIT C++ exception interoperability is required')
    endif

    if get_option('cpp_eh') != 'a'
        error('LuaJIT requires `/EHa` to have full C++ exception ' +
              'interoperability')
    endif

    add_project_arguments('/bigobj', language : 'cpp')
endif

if get_option('thread_support_level') == 0
    add_project_arguments('-DBOOST_ASIO_DISABLE_THREADS', language : 'cpp')
endif

if get_option('enable_io_uring')
    liburing = dependency('liburing', version : '>=2.0')
    add_project_arguments('-DBOOST_ASIO_HAS_IO_URING', language : 'cpp')
    add_project_arguments('-DBOOST_ASIO_DISABLE_EPOLL', language : 'cpp')
else
    liburing = dependency('', required : false)
endif

if host_machine.system() == 'linux'
    if get_option('thread_support_level') >= 1
        libcap = [dependency('libcap'), dependency('libpsx')]
    else
        libcap = dependency('libcap')
    endif
else
    libcap = dependency('', required : false)
endif

if get_option('use_wolfssl')
    ssl = dependency('wolfssl')
    add_project_arguments('-DBOOST_ASIO_USE_WOLFSSL', language : 'cpp')
else
    ssl = dependency('openssl', version : '>=1.1.1')
endif

bytecode_gen = generator(
    executable(
        'luabcxxd',
        'tool/luabcxxd.cpp',
        dependencies : [ luajit ],
        implicit_include_directories : false,
    ),
    output: 'bc_@BASENAME@.cpp',
    arguments: ['@INPUT@', '@OUTPUT@']
)

extra_re2c_args = []

if meson.get_compiler('cpp').get_id() == 'gcc'
    extra_re2c_args += '--case-ranges'
endif

re2c_gen = generator(
    find_program('re2c'),
    output: '@BASENAME@.cpp',
    depfile : '@PLAINNAME@.d',
    arguments: [
        '--depfile', '@DEPFILE@',
        '-W', # Turn on all warnings
        '--flex-syntax',
        '--empty-class', 'match-none',
        '--input-encoding', 'ascii',
        extra_re2c_args,
        '-I', '@CURRENT_SOURCE_DIR@/include',
        '@INPUT@', '-o', '@OUTPUT@',
    ],
)

gperf_gen = generator(
    gawk,
    output : 'bc2_@BASENAME@.cpp',
    arguments: [
        '--file', meson.current_source_dir() / 'src' / 'emilua_gperf.awk',
        '--assign', 'GPERF_BIN=' + find_program('gperf').full_path(),
        '@INPUT@', '@OUTPUT@',
    ],
)
install_data(
    'src' / 'emilua_gperf.awk',
    install_dir : get_option('libexecdir') / 'emilua',
    install_mode : 'rwxr-xr-x',
    install_tag : 'devel',
)

if get_option('enable_manpages')
    asciidoctor = find_program('asciidoctor', native : true)
    custom_target(
        'manpages',
        build_by_default : true,
        command : [
            asciidoctor,
            '--backend=manpage',
            '--attribute=VERSION=' + meson.project_version() +
            get_option('version_suffix'),
            '--out-file=@OUTPUT@',
            '@INPUT@',
        ],
        output : '@BASENAME@',
        input : 'doc/man/emilua.1.adoc',
        install : true,
        install_dir : get_option('mandir') / 'man1',
        install_tag : 'man',
    )

    subdir('doc')
endif

if get_option('disable_color')
    curses = dependency('', required : false)
else
    curses = dependency('curses', required : true)
endif

if get_option('enable_tests')
    if host_machine.system() == 'windows'
        shell = find_program(
            'run_test',
            dirs : meson.current_source_dir() / 'test',
        )
    else
        # GNU Coreutils binaries are not specified explicitly, but expected too
        shell = find_program('bash', 'zsh', 'ksh93', 'ksh')
    endif
endif

incdir = [
    'include'
]

src = [
    'src/file_descriptor.cpp',
    'src/recursive_mutex.cpp',
    'src/generic_error.cpp',
    'src/scope_cleanup.cpp',
    'src/serial_port.cpp',
    'src/async_base.cpp',
    'src/asio_error.cpp',
    'src/filesystem.cpp',
    'src/byte_span.cpp',
    'src/lua_shim.cpp',
    'src/future.cpp',
    'src/stream.cpp',
    'src/system.cpp',
    'src/fiber.cpp',
    'src/mutex.cpp',
    'src/regex.cpp',
    'src/time.cpp',
    'src/condition_variable.cpp',
    'src/core.cpp',
    'src/json.cpp',
    'src/pipe.cpp',
    'src/tls.cpp',
    'src/ip.cpp',

    'src' / host_machine.system() / 'system/spawn.cpp',
]

bytecode_src = [
    # async base
    'bytecode/var_args__retval1_to_error_.lua',
    'bytecode/var_args__retval1_to_error__fwd_retval2_.lua',
    'bytecode/var_args__retval1_to_error__fwd_retval23_.lua',
    'bytecode/var_args__retval1_to_error__fwd_retval234_.lua',

    # lua shim
    'bytecode/coroutine_create.lua',
    'bytecode/coroutine_resume.lua',
    'bytecode/coroutine_wrap.lua',
    'bytecode/pcall.lua',
    'bytecode/xpcall.lua',

    # stream
    'bytecode/read_all.lua',
    'bytecode/read_at_least.lua',
    'bytecode/write_all.lua',
    'bytecode/write_at_least.lua',
    'bytecode/scanner_get_line.lua',
    'bytecode/scanner_buffered_line.lua',
    'bytecode/scanner_buffer.lua',
    'bytecode/scanner_new.lua',
    'bytecode/scanner_remove_line.lua',
    'bytecode/scanner_set_buffer.lua',
    'bytecode/scanner_with_awk_defaults.lua',

    # actor
    'bytecode/chan_op.lua',

    # fiber
    'bytecode/spawn_start_fn.lua',
    'bytecode/fiber_join.lua',

    # scope
    'bytecode/scope_cleanup_pop.lua',
    'bytecode/scope.lua',

    # state
    'bytecode/start_fn.lua',
    'bytecode/module_start_fn.lua',
    'bytecode/require.lua',

    # condition_variable
    'bytecode/cond_wait.lua',

    # file
    'bytecode/read_all_at.lua',
    'bytecode/read_at_least_at.lua',
    'bytecode/write_all_at.lua',
    'bytecode/write_at_least_at.lua',

    # json
    'bytecode/json_encode.lua',

    # ip
    'bytecode/ip_connect.lua',

    # filesystem
    'bytecode/mode.lua',
]

re2c_src = [
    'src/actor.ypp',
    'src/state.ypp',
]

if host_machine.system() != 'windows'
    src += [
        'src/unix.cpp',
        'src/ipc_actor.cpp',
        'src' / host_machine.system() / 'ipc_actor/sys_bindings.cpp',
    ]
endif

if get_option('enable_plugins')
    src += 'src/plugin.cpp'
endif

if get_option('enable_file_io')
    src += 'src/file.cpp'
endif

libemilua = library(
    'emilua',
    gperf_gen.process(src),
    bytecode_gen.process(bytecode_src),
    gperf_gen.process(re2c_gen.process(re2c_src)),
    cpp_pch : 'pch/pchheader.hpp',
    dependencies : [
        thread_dep,
        boost,
        luajit,
        fmt,
        serd,
        sord,
        trial_protocol,
        cereal,
        ssl,
        curses,
        libdl,
        liburing,
        libcap,
    ],
    include_directories : include_directories(incdir),
    implicit_include_directories : false,
    version : meson.project_version(),
    install : true,
)

libemilua_dep = declare_dependency(
    dependencies : [
        thread_dep,
        boost,
        luajit,
        fmt,
        ssl,
        curses,
        liburing,
    ],
    link_whole :
        (
            get_option('default_library') == 'static' and
            get_option('enable_plugins')
        ) ?
        [libemilua] : [],
    link_with :
        (
            get_option('default_library') == 'static' and
            get_option('enable_plugins')
        ) ?
        [] : [libemilua],
)

emilua_bin = executable(
    'emilua',
    re2c_gen.process('src/main.ypp'),
    dependencies : libemilua_dep,
    export_dynamic : get_option('enable_plugins'),
    include_directories : include_directories(incdir),
    implicit_include_directories : false,
    install : true,
)

install_subdir(
    'include/emilua',
    install_dir : get_option('includedir'),
    install_tag : 'devel',
    exclude_files : [
        'config.h.in',
        'meson.build',
    ],
)

pc_base_variables = [
    'thread_support_level=' + get_option('thread_support_level').to_string(),
    'file_io_enabled=' + get_option('enable_file_io').to_string(),
]
pc_extra_cflags = []

if get_option('thread_support_level') == 0
    pc_extra_cflags += '-DBOOST_ASIO_DISABLE_THREADS'
endif

if get_option('enable_io_uring')
    pc_extra_cflags += [
        '-DBOOST_ASIO_HAS_IO_URING',
        '-DBOOST_ASIO_DISABLE_EPOLL',
    ]
endif

if get_option('use_wolfssl')
    pc_extra_cflags += '-DBOOST_ASIO_USE_WOLFSSL'
endif

import('pkgconfig').generate(
    libemilua,
    filebase : 'emilua',
    name : 'Emilua Plugin API',
    description : 'Lua execution engine',
    requires : [
        luajit.name(),
        fmt.name(),
        ssl.name(),
    ] +
    (get_option('enable_io_uring') ? [liburing.name()] : []),
    variables : pc_base_variables + [
        'pluginsdir=${libdir}' / 'emilua-' +
        meson.project_version().split('.')[0] + '.' +
        meson.project_version().split('.')[1],

        'emilua_bin=${bindir}' / 'emilua',
        'emilua_gperf=${libexecdir}' / 'emilua' / 'emilua_gperf.awk',
    ],
    uninstalled_variables : pc_base_variables + [
        'pluginsdir=' + meson.project_build_root() / 'plugins',
        'emilua_bin=' + emilua_bin.full_path(),
        'emilua_gperf=${srcdir}' / 'src' / 'emilua_gperf.awk',
    ],
    extra_cflags : pc_extra_cflags,
)

if get_option('enable_tests')
    normalize_path_bin = executable(
        'normalize_path',
        'tool/normalize_path.cpp',
        dependencies : [
            boost,
            luajit,
        ],
        implicit_include_directories : false,
    )

    errno_sed = custom_target(
        'errnos.awk',
        build_by_default : true,
        command : [
            executable(
                'errno_sed',
                'tool/errno_sed.cpp',
                implicit_include_directories : false,
            ),
        ],
        capture : true,
        output : 'errnos.awk',
    )

    tests_env = [
        # Test must override this env so non-colored terminal output will be
        # auto-detected (this is, in itself, another test)
        'EMILUA_COLORS=',

        'EMILUA_BIN=' + emilua_bin.full_path(),
        'NORMALIZE_PATH_BIN=' + normalize_path_bin.full_path(),
        'ERRNO_SED=' + errno_sed.full_path(),
        'AWK_BIN=' + gawk.full_path(),

        # Make output strings for perror(), strerror(), and others predictable
        'LANG=',
        'LC_ALL=',
        'LC_MESSAGES=',
    ]

    if host_machine.system() == 'windows'
        tests_env += ('APPVEYOR_FIXUP=' +
            meson.current_source_dir() / 'test' / 'force_windows.awk')
    endif

    tests = {
        'fiber' : [
            'detach1',
            'detach2',
            'detach3',
            'detach4',
            'detach5',
            'detach6',
            'join1',
            'join2',
            'join3',
            'join4',
            'join5',
            'join6',
            'join7',
            'join8',
            'join9',
            'yield',
            'local_storage',
            'forbid_suspend_setup1',
            'forbid_suspend_setup2',
            'forbid_suspend_setup3',
            'forbid_suspend_join',
            'forbid_suspend_yield',
            'forbid_suspend_sleep_for',
            'interrupt1',
            'interrupt2',
            'interrupt3',
            'interrupt4',
            'interrupt5',
            'interrupt6',
            'interrupt7',
            'interrupt8',
            'interrupt9',
            'interrupt10',
            'interrupt11',
            'interrupt12',
            'interrupt13',
            'interrupt14',
            'interrupt15',
            'interrupt16',
            'interrupt17',
            'interrupt18',
            'non-portable/interrupt1',
        ],
        'sync' : [
            'mutex1',
            'mutex2',
            'mutex3',
            'mutex4',
            'mutex5',
            'recursive_mutex1',
            'recursive_mutex2',
            'cond1',
            'cond2',
            'cond3',
            'cond4',
            'cond5',
            'cond6',
            'cond7',
            'cond8',
            'future1',
            'future2',
            'future3',
            'future4',
            'future5',
            'future6',
            'future7',
            'future8',
            'future9',
            'future10',
            'non-portable/mutex1',
        ],
        'scope' : [
            'scope1',
            'scope2',
            'scope3',
            'scope4',
            'scope5',
            'scope6',
            'scope7',
            'scope8',
            'scope9',
            'scope_pcall1',
            'scope_xpcall1',
            'scope_nested1',
            'scope_nested2',
        ],
        'lua_shim' : [
            'coroutine_running1',
            'coroutine_running2',
            'coroutine_yield1',
            'coroutine_yield2',
            'coroutine_yield3',
            'coroutine_resume1',
            'coroutine_resume2',
            'coroutine_resume3',
            'coroutine_resume4',
            'coroutine_resume5',
            'coroutine_resume6',
            'coroutine_resume7',
            'pcall1',
            'xpcall1',
        ],
        'module_system' : [
            'module1',
            'module2',
            'module3',
            'module4',
            'module5',
            'module6',
            'module7',
            'module9',
            'module10',
            'module11',
            'module12',
            'module13',
            'module14',
            'module15',
            'module16',
            'module17',
            'module18',
            'module19',
            'module20',
            'module21',
        ],
        'actor' : [
            'actor1',
            'actor2',
            'actor3',
            'actor4',
            'actor5',
            'actor6',
            'actor7',
            'actor8',
            'actor9',
            'actor10',
            'actor11',
            'actor12',
            'actor13',
            'actor14',
            'actor15',
            'actor16',
            'actor17',
            'actor18',
            'actor19',
            'actor20',
            'actor21',
            'actor22',
            'actor23',
            'actor24',
            'actor25',
            'actor26',
            'actor27',
            'actor28',
            'actor30',
        ],
        'json' : [
            'json1',
            'json2',
            'json3',
            'json4',
            'json5',
            'json6',
            'json7',
            'json8',
            'json9',
            'json10',
            'json11',
            'json12',
            'json13',
            'json14',
        ],
        'byte_span' : [
            # new(), __len(), capacity
            'byte_span1',
            'byte_span2',
            'byte_span3',
            'byte_span4',
            'byte_span5',
            'byte_span6',

            # slice(), __index(), __newindex(), __eq()
            'byte_span7',
            'byte_span8',
            'byte_span9',
            'byte_span10',
            'byte_span11',
            'byte_span12',
            'byte_span13',
            'byte_span14',

            # copy(),  __tostring()
            'byte_span15',

            # append()
            'byte_span16',

            # string algorithms
            'byte_span17',
            'byte_span18',
            'byte_span19',
        ],
        'regex' : [
            'regex1',
            'regex2',
            'regex3',
            'regex4',
            'regex5',
            'regex6',
            'regex7',
        ],
    }

    if get_option('thread_support_level') >= 2
        tests +=  {
            'actor' : tests['actor'] + [
                'actor29',
            ]
        }
    endif

    if host_machine.system() != 'windows' # POSIX systems
        tests +=  {
            'ipc_actor1' : [
                # serialization for good objects
                'ipc_actor_1_1',
                'ipc_actor_1_2',
                'ipc_actor_1_3',
                'ipc_actor_1_4',
                'ipc_actor_1_5',
                'ipc_actor_1_6',
                'ipc_actor_1_7',
                'ipc_actor_1_8',
                'ipc_actor_1_9',
                'ipc_actor_1_10',
                'ipc_actor_1_11',
                'ipc_actor_1_12',
                'ipc_actor_1_13',
                'ipc_actor_1_14',
                'ipc_actor_1_15',
                'ipc_actor_1_16',
                'ipc_actor_1_17',
                'ipc_actor_1_18',
                'ipc_actor_1_19',
                'ipc_actor_1_20',
                'ipc_actor_1_21',
                'ipc_actor_1_22',
                'ipc_actor_1_23',
                'ipc_actor_1_24',
                'ipc_actor_1_25',
                'ipc_actor_1_26',
                'ipc_actor_1_27',
                'ipc_actor_1_28',
                'ipc_actor_1_29',
                'ipc_actor_1_30',
                'ipc_actor_1_31',
                'ipc_actor_1_32',
                'ipc_actor_1_33',
                'ipc_actor_1_34',
                'ipc_actor_1_35',
                'ipc_actor_1_36',
                'ipc_actor_1_37',
                'ipc_actor_1_38',
                'ipc_actor_1_39',
                'ipc_actor_1_40',
                'ipc_actor_1_41',
                'ipc_actor_1_42',
                'ipc_actor_1_43',
                'ipc_actor_1_44',

                # bad input on send()
                'ipc_actor_1_45',
                'ipc_actor_1_46',
                'ipc_actor_1_47',
                'ipc_actor_1_48',
                'ipc_actor_1_49',
                'ipc_actor_1_50',
                'ipc_actor_1_51',
                'ipc_actor_1_52',
                'ipc_actor_1_53',
                'ipc_actor_1_54',
                'ipc_actor_1_55',

                # misc
                'ipc_actor_1_56',
            ]
        }
    endif

    if host_machine.system() != 'windows' and get_option('enable_plugins')
        ipc_actor_badinjector = shared_module(
            'ipc_actor_badinjector',
            ['test/ipc_actor_badinjector.cpp'],
            dependencies : [
                thread_dep,
                boost,
                luajit,
            ],
            include_directories : include_directories(incdir),
            implicit_include_directories : false,
            gnu_symbol_visibility : 'hidden',
        )

        tests_env += (
            'EMILUA_PATH=' + fs.parent(ipc_actor_badinjector.full_path()))

        tests +=  {
            'ipc_actor2' : [
                'ipc_actor_2_1',
                'ipc_actor_2_2',
                'ipc_actor_2_3',
                'ipc_actor_2_4',
                'ipc_actor_2_5',
                'ipc_actor_2_6',
                'ipc_actor_2_7',
                'ipc_actor_2_8',
                'ipc_actor_2_9',
                'ipc_actor_2_10',
                'ipc_actor_2_11',
                'ipc_actor_2_12',
                'ipc_actor_2_13',
                'ipc_actor_2_14',
                'ipc_actor_2_15',
                'ipc_actor_2_16',
                'ipc_actor_2_17',
                'ipc_actor_2_18',
                'ipc_actor_2_19',
                'ipc_actor_2_20',
                'ipc_actor_2_21',
                'ipc_actor_2_22',

                'ipc_actor_fuzzer1',
                'ipc_actor_fuzzer2',
            ]
        }
    endif

    if host_machine.system() == 'linux'
        tests +=  {
            'module_system2' : [
                # EIO on /proc/self/mem is a Linux trick
                'module8',
            ]
        }
    endif

    foreach suite, t : tests
        foreach t : t
            test(t, shell, suite : suite,
                 args : [
                     meson.current_source_dir() / 'test' / 'run_test.sh',
                     meson.current_source_dir() / 'test' / 'run_test.awk',
                     meson.current_source_dir() / 'test' / t,
                 ],
                 env : tests_env)
        endforeach
    endforeach
endif

if get_option('enable_gperf_tests')
    gperf_tests = [
        'gperf_basic',
        'gperf_nested',
        'gperf_nested2',
        'gperf_nested3',
    ]
    foreach t : gperf_tests
        test(
            t, executable(
                t,
                gperf_gen.process('test' / t + '.cpp'),
                dependencies : [ fmt ],
                include_directories : include_directories(
                    'test', #< https://github.com/mesonbuild/meson/issues/9219
                ),
                implicit_include_directories : false,
            ),
            suite : 'gperf',
            protocol : 'tap',
        )
    endforeach
endif
