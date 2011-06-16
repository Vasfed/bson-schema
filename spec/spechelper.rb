require 'rubygems'
require 'bundler/setup'


#require 'spork'

#Spork.prefork do
  Bundler.require
  require 'bson-schema'

  RSpec.configure do |config|
    config.mock_with :rspec
    # Dir[File.expand_path(File.join(File.dirname(__FILE__),'support','**','*.rb'))].each {|f| require f}
  end
#end
