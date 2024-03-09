
proxy_includes = include_directories([])
jsondep=dependency('nlohmann_json')
executable(
  'server_theme',
  'server_theme.cpp',
  include_directories: proxy_includes,
  dependencies:[openssl_dep,boost_dep,jsondep,chaibeast_dep],
  install: true,
  install_dir: get_option('bindir'))