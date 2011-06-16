# -*- encoding: utf-8 -*-
$:.push File.expand_path("../lib", __FILE__)
require "bson-schema/version"

Gem::Specification.new do |s|
  s.name        = "bson-schema"
  s.version     = Bson::Schema::VERSION
  s.authors     = ["Vasily Fedoseyev"]
  s.email       = ["vasilyfedoseyev@gmail.com"]
  s.homepage    = ""
  s.summary     = %q{Json-Schema validation for BSON}
  s.description = %q{Almost draft-3 compatible json-schema validation over BSON with native extensions}

  s.rubyforge_project = "bson-schema"

  #s.rdoc_options = ["--main", "README.rdoc"]
  s.require_paths = ["lib", "ext/bson-schema"]

  s.files         = `git ls-files`.split("\n")
  s.test_files    = `git ls-files -- {test,spec,features}/*`.split("\n")
  s.executables   = `git ls-files -- bin/*`.split("\n").map{ |f| File.basename(f) }
  s.require_paths = ["lib"]

  s.extensions = ["ext/at_fork/extconf.rb"]
  s.extra_rdoc_files = []

  s.add_development_dependency('rspec', '~>2.6.0')
end
