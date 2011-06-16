require "bson-schema/version"

ext_dir = File.dirname(__FILE__) + "/../ext/bson-schema"
$:.unshift ext_dir unless $:.include?(ext_dir) || $:.include?(File.expand_path(ext_dir))
require 'bson_schema'

begin
  require 'bson'
rescue
end

module Bson
  module Schema
    def self.validate! schema, data
      schema = ::BSON.serialize(schema).to_s unless schema.is_a?(String)
      data = ::BSON.serialize(data).to_s unless data.is_a?(String)
      return SchemaExt.validate!(schema, data)
    end

    def self.validate schema, data
      schema = ::BSON.serialize(schema).to_s unless schema.is_a?(String)
      data = ::BSON.serialize(data).to_s unless data.is_a?(String)
      return SchemaExt.validate(schema, data)
    end

  end
end
