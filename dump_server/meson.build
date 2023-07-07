
dump_server_includes = include_directories(['../include','../include/stdexec/include/'])
executable(
  'dump_server',
  'dump_server.cpp',
  include_directories: dump_server_includes,
  dependencies:[boost_dep,openssl_dep],
  install: true,
  install_dir: get_option('bindir'),cpp_args : CHAI_ARGS)