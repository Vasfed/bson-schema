require 'rubygems'
require 'bundler/gem_tasks'

require 'rspec/core/rake_task'

RSpec::Core::RakeTask.new('spec')

Dir['lib/tasks/**/*.rake'].each { |t| load t }

task :default => ['extconf:compile', :spec]