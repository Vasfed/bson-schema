#ifndef __JSON_BSON_SCHEMA_VALIDATOR__
#define __JSON_BSON_SCHEMA_VALIDATOR__
#pragma once

#include <stdexcept>
#include <string>
#include <mongo/bson/bson.h>

//TODO: выпилить bson из монгодрайвера чтобы убрать зависимость
// typedef mongo::BSONObj BSONObj;
// typedef mongo::BSONElement BSONElement;


//exceptions:
struct InvalidSchema:public std::runtime_error{ InvalidSchema(const string& str):runtime_error(str){} };
struct ValidationError:public std::runtime_error{ ValidationError(const string& validator, const string& path, const string& str):runtime_error(validator + " validation failed at " + path + str){} };

class CValidator {
public:

  //these just return if data validates (or throw InvalidSchema)
  static bool validate(mongo::BSONObj schema, mongo::BSONElement data);
  static bool validate(mongo::BSONObj schema, mongo::BSONObj data);

  //these return true or throw ValidationError
  static bool validate_throw(mongo::BSONObj schema, mongo::BSONElement data);
  static bool validate_throw(mongo::BSONObj schema, mongo::BSONObj data);



  explicit CValidator(){}

  //FIXME: some interface and factory
};

#endif //#ifndef __JSON_BSON_SCHEMA_VALIDATOR__
