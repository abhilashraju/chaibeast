
dump_server_includes = include_directories(['../include','../include/stdexec/include/'])
executable(
  'dump_server',
  'dump_server.cpp',
  include_directories: dump_server_includes,
  dependencies:[openssl_dep,boost_dep],
  install: true,
  install_dir: get_option('bindir'))