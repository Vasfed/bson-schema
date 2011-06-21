require 'mkmf'

LIBDIR = Config::CONFIG['libdir']
INCLUDEDIR  = Config::CONFIG['includedir']

HEADER_DIRS = [
  # First search /opt/local for macports
  '/opt/local/include',

  # Then search /usr/local for people that installed from source
  '/usr/local/include',

  # Check the ruby install locations
  INCLUDEDIR,

  # Finally fall back to /usr
  '/usr/include',
]

LIB_DIRS = [
  '/opt/local/lib',
  '/usr/local/lib',
  LIBDIR,
  '/usr/lib',
]

dir_config("bson-schema")
#dir_config('boost', HEADER_DIRS, LIB_DIRS)
dir_config('pcre', HEADER_DIRS, LIB_DIRS)

unless have_header('pcre.h')
  puts "Warning: NO PCRE"
end

have_library('pcre')

create_header('extconf.h')
create_makefile("bson_schema")
