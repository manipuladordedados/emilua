project(
    'emilua', 'cpp',
    default_options : ['cpp_std=c++20'],
    meson_version : '>=1.3.0',
    license : 'BSL-1.0 OR MIT',
    version : '0.12.0',
)

pkgconfig = import('pkgconfig')
fs = import('fs')

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

if host_machine.system() == 'darwin'
    add_project_arguments('-DMSG_NOSIGNAL=0', language : 'cpp')
endif

luajit = dependency('luajit', version : '>=2.1.0')
boost = dependency(
    'boost',
    version : '>=1.82',
    modules : boost_modules,
)
fmt = dependency('fmt', version : '>=8.0')
serd = dependency('serd-0')
sord = dependency('sord-0')
cereal = dependency('cereal')
libdl = meson.get_compiler('cpp').find_library('dl', required : false)
gawk = find_program('gawk')

if get_option('use_standalone_asio')
    asio = dependency('asio', fallback : ['asio', 'asio_dep'])
else
    asio = dependency('', required : false)
endif

add_project_arguments(
    '-DBOOST_DLL_USE_STD_FS',
    '-DBOOST_BEAST_USE_STD_STRING_VIEW',
    language : 'cpp',
)

if get_option('use_standalone_asio')
    add_project_arguments(
        '-DASIO_DISABLE_CO_AWAIT',
        '-DASIO_DISABLE_STD_COROUTINE',
        language : 'cpp',
    )
else
    add_project_arguments(
        '-DBOOST_ASIO_DISABLE_CO_AWAIT',
        '-DBOOST_ASIO_DISABLE_STD_COROUTINE',
        language : 'cpp',
    )
endif

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
    add_project_arguments('-DBOOST_SYSTEM_DISABLE_THREADS', language : 'cpp')

    if get_option('use_standalone_asio')
        add_project_arguments('-DASIO_DISABLE_THREADS', language : 'cpp')
    else
        add_project_arguments('-DBOOST_ASIO_DISABLE_THREADS', language : 'cpp')
    endif
endif

if get_option('enable_io_uring')
    liburing = dependency('liburing', version : '>=2.0')

    if get_option('use_standalone_asio')
        add_project_arguments('-DASIO_HAS_IO_URING', language : 'cpp')
        add_project_arguments('-DASIO_DISABLE_EPOLL', language : 'cpp')
    else
        add_project_arguments('-DBOOST_ASIO_HAS_IO_URING', language : 'cpp')
        add_project_arguments('-DBOOST_ASIO_DISABLE_EPOLL', language : 'cpp')
    endif
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

    if get_option('use_standalone_asio')
        add_project_arguments('-DASIO_USE_WOLFSSL', language : 'cpp')
    else
        add_project_arguments('-DBOOST_ASIO_USE_WOLFSSL', language : 'cpp')
    endif
else
    ssl = dependency('openssl', version : '>=1.1.1')
endif

emiluabcxxd = executable(
    'luabcxxd',
    'tool/luabcxxd.cpp',
    dependencies : [ luajit ],
    implicit_include_directories : false,
    install : true,
    install_dir : get_option('libexecdir') / 'emilua',
    install_tag : 'devel',
)

bytecode_gen = generator(
    emiluabcxxd,
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

if get_option('disable_color') or host_machine.system() == 'windows'
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
    'src/builtin_module.cpp',
    'src/generic_error.cpp',
    'src/native_module.cpp',
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

    # ip
    'bytecode/ip_connect.lua',
    'bytecode/ip_dial.lua',

    # tls
    'bytecode/tls_dial.lua',

    # unix
    'bytecode/unix_dial.lua',

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
        'src/open_posix_libs.cpp',
        'src/ipc_actor.cpp',
        'src/libc_service.cpp',
        'src/proc_set_libc_service.cpp',
        'src' / host_machine.system() / 'ipc_actor/sys_bindings.cpp',
        'src/ambient_authority.cpp',
    ]
endif

if host_machine.system() == 'linux'
    src += 'src/linux/landlock.cpp'
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
        asio,
        luajit,
        fmt,
        serd,
        sord,
        cereal,
        ssl,
        curses,
        libdl,
        liburing,
        libcap,
    ],
    include_directories : include_directories(incdir),
    implicit_include_directories : false,
    cpp_static_args : ['-DEMILUA_STATIC_BUILD'],
    version : meson.project_version(),
    install : true,
)

libemilua_dep = declare_dependency(
    dependencies : [
        thread_dep,
        boost,
        asio,
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

libemilua_main = library(
    'emilua-main',
    gperf_gen.process(['src/lib_main.cpp']),
    dependencies : [
        thread_dep,
        boost,
        asio,
        luajit,
        fmt,
        curses,
        liburing,
    ],
    link_with : libemilua,
    include_directories : include_directories(incdir),
    implicit_include_directories : false,
    cpp_static_args : ['-DEMILUA_STATIC_BUILD'],
    version : meson.project_version(),
    install : true,
)

libemilua_main_dep = declare_dependency(
    dependencies : [ libemilua_dep ],
    link_with : [libemilua_main],
)

libc_service_src = []

if host_machine.system() == 'linux'
    if (
        meson.get_compiler('cpp').has_define(
            '__GLIBC__', prefix : '#include <cstddef>') or
        meson.get_compiler('cpp').has_define(
            '__GNU_LIBRARY__', prefix : '#include <cstddef>')
    )
        libc_service_src += 'src/linux/glibc/libc_service.cpp'
    endif
elif host_machine.system() == 'freebsd'
    libc_service_src += 'src/freebsd/libc_service.cpp'
elif host_machine.system() != 'windows'
    libc_service_src += 'src/posix/libc_service.cpp'
endif

if libc_service_src.length() == 0
    libemilua_libc_service_dep = dependency('', required : false)
else
    libemilua_libc_service = library(
        'emilua-libc-service',
        libc_service_src,
        override_options : ['b_lundef=false'],
        dependencies : [ boost ],
        include_directories : include_directories(incdir),
        implicit_include_directories : false,
        version : meson.project_version(),
        install : true,
    )

    libemilua_libc_service_dep = declare_dependency(
        dependencies : [ libemilua_dep ],
        objects : libemilua_libc_service.extract_all_objects(),
    )

    if not ('-static' in get_option('cpp_link_args'))
        preload_libc_src = [
            'src/open_posix_libs.cpp',
            'src/preload_libc.cpp',
        ]

        preload_libc_gperf_src = [
            'src' / host_machine.system() / 'ipc_actor/sys_bindings.cpp',
        ]

        if host_machine.system() == 'linux'
            preload_libc_gperf_src += 'src/linux/landlock.cpp'
        endif

        libemilua_preload_libc = shared_library(
            'emilua-preload-libc',
            libc_service_src,
            preload_libc_src,
            gperf_gen.process(preload_libc_gperf_src),
            dependencies : [
                cereal,
                libcap,
                luajit,
                boost,
                fmt,
            ],
            include_directories : include_directories(incdir),
            implicit_include_directories : false,
            version : meson.project_version(),
            install : true,
        )
    endif
endif

emilua_bin = executable(
    'emilua',
    re2c_gen.process('src/main.ypp'),
    dependencies : [
        # libc_service must be linked first to override glibc functions
        libemilua_libc_service_dep,

        libemilua_dep,
        libemilua_main_dep,
    ],
    export_dynamic : get_option('enable_plugins'),
    include_directories : include_directories(incdir),
    implicit_include_directories : false,
    install : true,
)

genmain_bin = executable(
    'emilua-genmain' + meson.project_version().split('.')[0],
    'tool/genmain.cpp',
    dependencies : [ libemilua_dep, libemilua_main_dep ],
    include_directories : include_directories(incdir),
    implicit_include_directories : false,
    install : true,
    install_dir : get_option('libexecdir') / 'emilua',
)

genmods_bin = executable(
    'emilua-genmods' + meson.project_version().split('.')[0],
    'tool/genmods.cpp',
    dependencies : [ libemilua_dep, libemilua_main_dep ],
    include_directories : include_directories(incdir),
    implicit_include_directories : false,
    install : true,
    install_dir : get_option('libexecdir') / 'emilua',
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
    'use_standalone_asio=' + get_option('use_standalone_asio').to_string(),
]
pc_extra_cflags = []

if get_option('thread_support_level') == 0
    pc_extra_cflags += '-DBOOST_SYSTEM_DISABLE_THREADS'

    if get_option('use_standalone_asio')
        pc_extra_cflags += '-DASIO_DISABLE_THREADS'
    else
        pc_extra_cflags += '-DBOOST_ASIO_DISABLE_THREADS'
    endif
endif

if get_option('enable_io_uring')
    if get_option('use_standalone_asio')
        pc_extra_cflags += [
            '-DASIO_HAS_IO_URING',
            '-DASIO_DISABLE_EPOLL',
        ]
    else
        pc_extra_cflags += [
            '-DBOOST_ASIO_HAS_IO_URING',
            '-DBOOST_ASIO_DISABLE_EPOLL',
        ]
    endif
endif

if get_option('use_wolfssl')
    if get_option('use_standalone_asio')
        pc_extra_cflags += '-DASIO_USE_WOLFSSL'
    else
        pc_extra_cflags += '-DBOOST_ASIO_USE_WOLFSSL'
    endif
endif

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
    install : true,
    install_dir : get_option('libexecdir') / 'emilua',
    install_mode : 'rwxr-xr-x',
    install_tag : 'devel',
)

normalize_path_bin = executable(
    'normalize_path',
    'tool/normalize_path.cpp',
    dependencies : [
        boost,
        asio,
        luajit,
    ],
    implicit_include_directories : false,
    install : true,
    install_dir : get_option('libexecdir') / 'emilua',
    install_tag : 'devel',
)

install_data(
    'test' / 'run_test.sh',
    install_dir : get_option('libexecdir') / 'emilua',
    install_mode : 'rwxr-xr-x',
    install_tag : 'devel',
)

install_data(
    'test' / 'run_test.awk',
    install_dir : get_option('libexecdir') / 'emilua',
    install_mode : 'rwxr-xr-x',
    install_tag : 'devel',
)

pkgconfig.generate(
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
        'emiluabcxxd=${libexecdir}' / 'emilua' / 'luabcxxd',
        'emilua_errnos_awk=${libexecdir}' / 'emilua' / 'errnos.awk',
        'emilua_normalize_path=${libexecdir}' / 'emilua' / 'normalize_path',
        'emilua_run_test_sh=${libexecdir}' / 'emilua' / 'run_test.sh',
        'emilua_run_test_awk=${libexecdir}' / 'emilua' / 'run_test.awk',
        'emilua_genmain=${libexecdir}' / 'emilua' / 'emilua-genmain' +
            meson.project_version().split('.')[0],
        'emilua_genmods=${libexecdir}' / 'emilua' / 'emilua-genmods' +
            meson.project_version().split('.')[0],
    ],
    uninstalled_variables : pc_base_variables + [
        'pluginsdir=' + meson.project_build_root() / 'plugins',
        'emilua_bin=' + emilua_bin.full_path(),
        'emilua_gperf=${srcdir}' / 'src' / 'emilua_gperf.awk',
        'emiluabcxxd=' + emiluabcxxd.full_path(),
        'emilua_errnos_awk=' + errno_sed.full_path(),
        'emilua_normalize_path=' + normalize_path_bin.full_path(),
        'emilua_run_test_sh=${srcdir}' / 'test' / 'run_test.sh',
        'emilua_run_test_awk=${srcdir}' / 'test' / 'run_test.awk',
        'emilua_genmain=' + genmain_bin.full_path(),
        'emilua_genmods=' + genmods_bin.full_path(),
    ],
    extra_cflags : pc_extra_cflags,
)

pkgconfig.generate(
    libemilua_main,
    filebase : 'emilua_main',
    name : 'Emilua\'s default main()',
    description : 'A default main() for the Emilua runtime with some hooks',
    requires : ['emilua'],
)

if libc_service_src.length() != 0
    pkgconfig.generate(
        libemilua_libc_service,
        filebase : 'emilua_libc_service',
        name : 'Emilua\'s default libc overriders for ambient authority',
        description : 'in sandboxed mode, we forward requests to process ' +
            'outside the sandbox',
        requires : ['emilua'],
    )

    if not ('-static' in get_option('cpp_link_args'))
        pkgconfig.generate(
            filebase : 'emilua_preload_libc',
            name : 'Emilua\'s LD_PRELOAD for ambient authority',
            description : 'in sandboxed mode, we forward requests to process ' +
                'outside the sandbox',
            variables : [
                'libpath=' + get_option('prefix') / get_option('libdir') /
                    fs.name(libemilua_preload_libc.full_path()),
            ],
            uninstalled_variables : [
                'libpath=' + libemilua_preload_libc.full_path(),
            ],
        )
    endif
endif

if get_option('enable_tests')
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
    else
        if not ('-static' in get_option('cpp_link_args'))
            tests_env += ('EMILUA_PRELOAD_LIBC_PATH=' +
                libemilua_preload_libc.full_path())
        endif
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
            'actor31',
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

            # first
            'byte_span20',
            'byte_span21',
            'byte_span22',
            'byte_span23',
            'byte_span24',

            # last
            'byte_span25',
            'byte_span26',
            'byte_span27',
            'byte_span28',
            'byte_span29',
            'byte_span30',

            # lower/upper
            'byte_span31',
        ],
        'primserdes' : [
            'primserdes_u16be',
            'primserdes_u16le',
            'primserdes_u24be',
            'primserdes_u24le',
            'primserdes_u32be',
            'primserdes_u32le',
            'primserdes_u40be',
            'primserdes_u40le',
            'primserdes_u48be',
            'primserdes_u48le',
            'primserdes_i8',
            'primserdes_i16be',
            'primserdes_i16le',
            'primserdes_i24be',
            'primserdes_i24le',
            'primserdes_i32be',
            'primserdes_i32le',
            'primserdes_i40be',
            'primserdes_i40le',
            'primserdes_i48be',
            'primserdes_i48le',
            'primserdes_f32be',
            'primserdes_f32le',
            'primserdes_f64be',
            'primserdes_f64le',
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

    if get_option('thread_support_level') >= 1
        tests +=  {
            'actor' : tests['actor'] + [
                'actor32',
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
                'ipc_actor_1_57',
                'ipc_actor_1_58',
            ],
            'ipc_actor3' : [
                # module path resolution
                'ipc_actor_3_1',
            ],
            'ipc_actor_cache' : [
                # Lua source code cache
                'ipc_actor_cache1',
                'ipc_actor_cache2',
                'ipc_actor_cache3',
            ],
            'system' : [
                'get_lowfd1',
                'get_lowfd2',
                'get_lowfd3',
                'get_lowfd4',
                'get_lowfd5',
                'get_lowfd6',
                'spawn_with_pid_on_env',
            ],
            'libc_service' : [
                'libc_service_getaddrinfo',
                'libc_service_getaddrinfo2',
                'libc_service_getaddrinfo3',
                'libc_service_getaddrinfo4',
                'libc_service_stat1',
                'libc_service_stat2',
                'libc_service_stat3',
                'libc_service_lstat1',
                'libc_service_lstat2',
                'libc_service_lstat3',
            ] +
            (('-static' in get_option('cpp_link_args')) ? [] : [
                'libc_service_cat',
            ]) +
            (get_option('enable_io_uring') ? [] : [
                'libc_service_connect',
                'libc_service_connect2',
                'libc_service_connect3',
                'libc_service_bind',
                'libc_service_bind2',
                'libc_service_bind3',
            ]),
            'is_socket' : [
                'is_socket1',
                'is_socket2',
                'is_socket3',
                'is_socket4',
                'is_socket5',
                'is_socket6',
                'is_socket7',
                'is_socket8',
                'is_socket9',
                'is_socket10',
                'is_socket11',
                'is_socket12',
            ],
        }

        test(
            'libc_service_read_dev_null', executable(
                'libc_service_read_dev_null',
                'test/libc_service_read_dev_null.cpp',
                dependencies : [
                    # libc_service must be linked first to override glibc
                    # functions
                    libemilua_libc_service_dep,

                    libemilua_dep,
                    libemilua_main_dep,
                ],
                include_directories : include_directories(incdir),
                implicit_include_directories : false,
            ),
            suite : 'libc_service',
            protocol : 'tap',
        )
    endif

    test(
        'builtin_modules1', executable(
            'builtin_modules1',
            'test/builtin_modules1.cpp',
            dependencies : [libemilua_dep, libemilua_main_dep],
            include_directories : include_directories(incdir),
            implicit_include_directories : false,
        ),
        suite : 'builtin_modules',
    )

    test(
        'builtin_modules2', executable(
            'builtin_modules2',
            'test/builtin_modules2.cpp',
            dependencies : [libemilua_dep, libemilua_main_dep],
            include_directories : include_directories(incdir),
            implicit_include_directories : false,
        ),
        suite : 'builtin_modules',
    )

    if host_machine.system() != 'windows' and get_option('enable_plugins')
        ipc_actor_badinjector = shared_module(
            'ipc_actor_badinjector',
            ['test/ipc_actor_badinjector.cpp'],
            dependencies : [
                thread_dep,
                boost,
                asio,
                luajit,
                fmt,
            ],
            include_directories : include_directories(incdir),
            implicit_include_directories : false,
            gnu_symbol_visibility : 'hidden',
        )

        plugin_hello = shared_module(
            'hello',
            ['test/hello.cpp'],
            dependencies : [
                thread_dep,
                boost,
                asio,
                luajit,
                fmt,
            ],
            include_directories : include_directories(incdir),
            implicit_include_directories : false,
            gnu_symbol_visibility : 'hidden',
        )

        plugin_hello2 = shared_module(
            'hello2',
            ['test/hello2.cpp'],
            dependencies : [
                thread_dep,
                boost,
                asio,
                luajit,
                fmt,
            ],
            include_directories : include_directories(incdir),
            implicit_include_directories : false,
            gnu_symbol_visibility : 'hidden',
        )

        # hello and hello2 are probably in the same directory anyway, so we
        # can't write all tests that we should be writing
        tests_env += (
            'EMILUA_PATH=' + fs.parent(ipc_actor_badinjector.full_path()) +
                ':' + fs.parent(plugin_hello.full_path()) +
                ':' + fs.parent(plugin_hello2.full_path())
        )

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
            ],
            'ipc_actor4' : [
                # flat
                'ipc_actor_4_1',
                'ipc_actor_4_2',
                'ipc_actor_4_3',
                'ipc_actor_4_4',
                'ipc_actor_4_5',
                'ipc_actor_4_6',

                # nested
                'ipc_actor_4_7',
                'ipc_actor_4_8',
                'ipc_actor_4_9',
                'ipc_actor_4_10',
                'ipc_actor_4_11',
                'ipc_actor_4_12',
            ],
        }
    endif

    have_rtld_set_var = meson.get_compiler('cpp').compiles('''
        #include <link.h>

        int main()
        {
            int result = rtld_set_var("LIBRARY_PATH_FDS", "10");
            (void)result;
        }
    ''')

    if have_rtld_set_var and get_option('enable_plugins')
        sample_lib_for_plugin = shared_library(
            'sample_lib_for_plugin',
            ['test/sample_lib_for_plugin.cpp'],
            dependencies : [ boost ],
            implicit_include_directories : false,
        )
        sample_plugin_for_lib = shared_module(
            'sample_plugin_for_lib',
            ['test/sample_plugin_for_lib.cpp'],
            dependencies : [
                thread_dep,
                boost,
                asio,
                luajit,
                fmt,
            ],
            link_with : sample_lib_for_plugin,
            include_directories : include_directories(incdir),
            implicit_include_directories : false,
            gnu_symbol_visibility : 'hidden',
        )

        tests_env += (
            'SAMPLE_LIB_FOR_PLUGIN_PATH=' +
                fs.parent(sample_lib_for_plugin.full_path())
        )

        tests +=  {
            'ipc_actor5' : [
                'ipc_actor5_1',
                'ipc_actor5_2',
            ],
        }
    endif

    if host_machine.system() == 'linux'
        tests +=  {
            'module_system2' : [
                # EIO on /proc/self/mem is a Linux trick
                'module8',
            ]
        }

        if get_option('thread_support_level') >= 1
            test(
                'libpsx1', shell, suite : 'libpsx',
                args : [
                    meson.current_source_dir() / 'test' / 'run_test.sh',
                    meson.current_source_dir() / 'test' / 'run_test.awk',
                    meson.current_source_dir() / 'test' / 'libpsx1',
                ],
                workdir : meson.current_source_dir() / 'test',
                env : tests_env,

                # there's a libpsx bug for C++ projects that was only solved in
                # 2.72: https://bugzilla.kernel.org/show_bug.cgi?id=218607
                should_fail : libcap[1].version().version_compare('<2.72'),
             )
        endif
    endif

    foreach suite, t : tests
        foreach t : t
            test(t, shell, suite : suite,
                 args : [
                     meson.current_source_dir() / 'test' / 'run_test.sh',
                     meson.current_source_dir() / 'test' / 'run_test.awk',
                     meson.current_source_dir() / 'test' / t,
                 ],
                 workdir : meson.current_source_dir() / 'test',
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
