
fileserver_includes = include_directories(['../include'])
executable(
  'fileserver',
  'fileserver.cpp',
  include_directories: fileserver_includes,
  dependencies:[openssl_dep,boost_dep],
  install: true,
  install_dir: get_option('bindir'),cpp_args : '-DSSL=0')