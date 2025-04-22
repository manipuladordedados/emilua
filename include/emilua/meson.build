# https://github.com/mesonbuild/meson/issues/2320

conf = configuration_data()

conf.set('VERSION_MAJOR', meson.project_version().split('.')[0].to_int())
conf.set('VERSION_MINOR', meson.project_version().split('.')[1].to_int())
conf.set('VERSION_PATCH', meson.project_version().split('.')[2].to_int())
conf.set_quoted(
    'VERSION_STRING',
    meson.project_version() + get_option('version_suffix')
)
conf.set_quoted('LIBROOTDIR', get_option('prefix') / get_option('libdir'))
conf.set10('ENABLE_PLUGINS', get_option('enable_plugins'))
conf.set10('ENABLE_FILE_IO', get_option('enable_file_io'))
conf.set10('USE_STANDALONE_ASIO', get_option('use_standalone_asio'))
conf.set('THREAD_SUPPORT_LEVEL',  get_option('thread_support_level'))
conf.set('IPC_ACTOR_MSG_MAX_MEMBERS_NUMBER',
         get_option('ipc_actor_msg_max_members_number'))
conf.set('IPC_ACTOR_MAX_CONFIG_MSG_SIZE',
         get_option('ipc_actor_max_config_msg_size'))

if host_machine.system() == 'windows'
    conf.set10('ENABLE_COLOR', false)
    conf.set('EINTR_RTSIGNO', 0)
else
    conf.set10('ENABLE_COLOR', not get_option('disable_color'))
    conf.set('EINTR_RTSIGNO', get_option('eintr_rtsigno'))
endif

if meson.get_compiler('cpp').compiles('''
    #include <pthread.h>
    #include <csignal>

    int main()
    {
        sigval value;
        pthread_sigqueue(pthread_self(), 0, value);
    }
''')
    conf.set10('HAVE_PTHREAD_SIGQUEUE', true)
else
    conf.set10('HAVE_PTHREAD_SIGQUEUE', false)
endif

if meson.get_compiler('cpp').compiles('''
    #include <link.h>

    int main()
    {
        int result = rtld_set_var("LIBRARY_PATH_FDS", "10");
        (void)result;
    }
''')
    conf.set10('HAVE_RTLD_SET_VAR', true)
else
    conf.set10('HAVE_RTLD_SET_VAR', false)
endif

configure_file(
    input : 'config.h.in',
    output : 'config.h',
    configuration : conf,
    install_dir : get_option('includedir') / 'emilua',
    install_tag : 'devel',
)
