#include <ruby.h>
#include "jsonschema.h"


typedef VALUE(*rubyfunc)(ANYARGS);

static VALUE method_validate(VALUE self, VALUE schema, VALUE data){
  mongo::BSONObj data_bson(RSTRING_PTR(data)),
    schema_bson(RSTRING_PTR(schema));

  if(CValidator::validate(schema_bson, data_bson))
    return Qtrue;

  return Qfalse;
}

static VALUE method_validate_throw(VALUE self, VALUE schema, VALUE data){
  mongo::BSONObj data_bson(RSTRING_PTR(data)),
    schema_bson(RSTRING_PTR(schema));

  //printf("Validating '%s' with '%s'\n", data_bson.toString().c_str(), schema_bson.toString().c_str());

  try{
    CValidator::validate_throw(schema_bson, data_bson);
  } catch(const InvalidSchema& e){
    printf("invalid schema raised: %s\n", e.what());
    //TODO: raise ruby exception
    return Qfalse;
  } catch(const ValidationError& e){
    printf("validation error raised: %s\n", e.what());
    return Qfalse;
  }

  return Qtrue;
}

extern "C" {
  void Init_bson_schema() {
    //printf("bsonschema ext included\n");
    VALUE topmodule = rb_define_module("Bson");
    VALUE module = rb_define_module_under(topmodule, "SchemaExt");
  	rb_define_singleton_method(module, "validate", (rubyfunc)method_validate, 2);
  	rb_define_singleton_method(module, "validate!", (rubyfunc)method_validate_throw, 2);
  }
}
