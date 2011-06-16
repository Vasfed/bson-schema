desc "measure speed"
task :benchmark do
  require 'bundler/setup'
  require 'bson-schema'
  require 'json-schema'
  require 'benchmark'



  metaschema = {
        "id"=>"http://json-schema.org/draft-03/schema#",
        "$schema"=>"http://json-schema.org/draft-03/schema#",
        #"dependencies"=>{"exclusiveMaximum"=>"maximum", "exclusiveMinimum"=>"minimum"}, # incorrectly handled by stock json-schema
        "default"=>{},
        "type"=>"object",
        "properties"=>
        {"dependencies"=>
          {"default"=>{},
           "additionalProperties"=>
            {"items"=>{"type"=>"string"},
             "type"=>["string", "array", {"$ref"=>"#"}]},
           "type"=>"object"},
         "required"=>{"default"=>false, "type"=>"boolean"},
         "items"=>
          {"items"=>{"$ref"=>"#"}, "default"=>{}, "type"=>[{"$ref"=>"#"}, "array"]},
         "format"=>{"type"=>"string"},
         "maxLength"=>{"type"=>"integer"},
         "title"=>{"type"=>"string"},
         "maximum"=>{"type"=>"number"},
         "extends"=>
          {"items"=>{"$ref"=>"#"}, "default"=>{}, "type"=>[{"$ref"=>"#"}, "array"]},
         "default"=>{"type"=>"any"},
         "enum"=>{"minItems"=>1, "type"=>"array", "uniqueItems"=>true},
         "divisibleBy"=>
          {"default"=>1, "type"=>"number", "minimum"=>0, "exclusiveMinimum"=>true},
         "minLength"=>{"default"=>0, "type"=>"integer", "minimum"=>0},
         "exclusiveMaximum"=>{"default"=>false, "type"=>"boolean"},
         "additionalProperties"=>{"default"=>{}, "type"=>[{"$ref"=>"#"}, "boolean"]},
         "id"=>{"format"=>"uri", "type"=>"string"},
         "disallow"=>
          {"items"=>{"type"=>["string", {"$ref"=>"#"}]},
           "type"=>["string", "array"],
           "uniqueItems"=>true},
         "pattern"=>{"format"=>"regex", "type"=>"string"},
         "minItems"=>{"default"=>0, "type"=>"integer", "minimum"=>0},
         "additionalItems"=>{"default"=>{}, "type"=>[{"$ref"=>"#"}, "boolean"]},
         "patternProperties"=>
          {"default"=>{}, "additionalProperties"=>{"$ref"=>"#"}, "type"=>"object"},
         "type"=>
          {"items"=>{"type"=>["string", {"$ref"=>"#"}]},
           "default"=>"any",
           "type"=>["string", "array"],
           "uniqueItems"=>true},
         "$schema"=>{"format"=>"uri", "type"=>"string"},
         "minimum"=>{"type"=>"number"},
         "$ref"=>{"format"=>"uri", "type"=>"string"},
         "description"=>{"type"=>"string"},
         "uniqueItems"=>{"default"=>false, "type"=>"boolean"},
         "maxItems"=>{"type"=>"integer", "minimum"=>0},
         "exclusiveMinimum"=>{"default"=>false, "type"=>"boolean"},
         "properties"=>
          {"default"=>{}, "additionalProperties"=>{"$ref"=>"#"}, "type"=>"object"}}
        }



  schema = {'properties' => {'a' => {'type' => 'integer'}}}
  data = {'a' => 1}


  schema_more = metaschema
  data_more = metaschema

  require 'bson'
  metaschema_binary = BSON.serialize(metaschema).to_s

   n = 500
   Benchmark.bm(7) do |x|
     x.report("bson simple:")   { n.times{ Bson::Schema.validate!(schema, data); }}
     x.report("json simple:") { n.times { JSON::Validator.validate!(schema, data) }}
     x.report("bson large:")   { n.times{ Bson::Schema.validate!(schema_more, data_more); }}

     #normally each value is converted to bson each time, we may remove this overhead:
     x.report("bson binary:")   { n.times{ Bson::Schema.validate!(metaschema_binary, metaschema_binary); }}
     x.report("json large:") { n.times { JSON::Validator.validate!(schema_more, data_more) }}
   end



end