project(
  'chai',
  'cpp',
  version: '0.1',
  meson_version: '>=0.58.0',
  default_options: [
    'warning_level=3',
    'cpp_std=c++20',
  ])

#CHAI_ARGS=['-DSSL_ON']
CHAI_ARGS=[]
openssl_dep = dependency('openssl', version: '>=1.1.1')
boost_dep = dependency('boost',modules: ['coroutine'])
chaibeast_includes = include_directories(['./include','./include/stdexec/include/','include'])
chaibeast_dep = declare_dependency(
	include_directories : chaibeast_includes,
	dependencies : [dependency('threads'),openssl_dep,boost_dep]
)
#boost_lib_path = boost_dep.get_pkgconfig_variable('libdir')
#boost_lib_path='/opt/homebrew/Cellar/boost/1.82.0/lib'
subdir('reverseproxy')
#subdir('fileserver')
subdir('dump_server')