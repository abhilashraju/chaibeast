
proxy_includes = include_directories(['../include','include'])
jsondep=dependency('nlohmann_json')
executable(
  'reverseproxy',
  'reverseproxy.cpp',
  include_directories: proxy_includes,
  dependencies:[openssl_dep,boost_dep,jsondep,chaibeast_dep],
  install: true,
  install_dir: get_option('bindir'))