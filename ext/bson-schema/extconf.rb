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
dir_config('mongoclient', HEADER_DIRS, LIB_DIRS)
dir_config('boost', HEADER_DIRS, LIB_DIRS)
dir_config('pcre', HEADER_DIRS, LIB_DIRS)

unless find_header('pcre.h')
  abort 'This extension requires PCRE'
end

# unless find_header('mongo/bson/bson.h')
#   abort 'This extension requires mongocpp driver'
# end

# unless find_header('boost/algorithm/string.hpp') && have_header('boost/lexical_cast.hpp')
#   abort 'This extension requires BOOST'
# end


have_library('boost_system')
have_library('pcre')
have_library('mongoclient')

create_makefile("bson_schema")
