//#include <mongo/client/dbclient.h>

#include <string>
#include <map>

#include <mongo/bson/bson.h>
#include <pcre.h>


#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>


#include "jsonschema.h"

using namespace std;


string str_type(BSONElement data){
  using namespace mongo;
  switch(data.type()){
     case jstNULL: return "null";
     case NumberDouble: return "number";
     case NumberInt: return "integer";
     case NumberLong: return "long";
     case mongo::String: return "string";
     case Symbol: return "symbol";
     case Object: return "object";
     case mongo::Array: return "array";
     case mongo::Bool: return "boolean";
     case BinData: return "bindata";
     case jstOID: return "oid";
     case mongo::Date: return "date";
     case Timestamp: return "timestamp";
     case RegEx: return "regex";
     case DBRef: return "dbref";
     case Code: return "code";
     case CodeWScope: return "scoped code";
     default:
       return "unknown type";
   }
}

bool is_type(BSONElement data, string type){//helper
 using namespace mongo;

  if(boost::iequals(type, "any"))
    return true;

  switch(data.type()){
    case jstNULL:
      return boost::iequals(type, "null");
    case NumberDouble:
      return boost::iequals(type, "number");
    case NumberInt:
    case NumberLong:
      return boost::iequals(type, "integer") || boost::iequals(type, "number");
    case mongo::String:
    case Symbol:
      return boost::iequals(type, "string");
    case Object:
      return boost::iequals(type, "object");
    case mongo::Array:
      return boost::iequals(type, "array");
    case mongo::Bool:
      return boost::iequals(type, "boolean");


    // TODO: other types? (not covered by json-schema spec)
    case BinData:

    case jstOID:

    case mongo::Date:
    case Timestamp:
        return 0;
    case RegEx:
        return 0;
    case DBRef:
        return 0;
    case Code:
        return 0;
    case CodeWScope:
        return 0;
    default:
      return false;
  }
}

bool is_equal_bson(BSONElement a, BSONElement b){
  if(a.isNumber() && b.isNumber() && a.Number() == b.Number())
    return true;
  if(a.type() != b.type())
    return false;
  switch(a.type()){
    case mongo::Bool:
      return a.Bool() == b.Bool();
    case mongo::String:
      return a.String() == b.String();
    case mongo::Array:{
      //TODO: binary-compare json?
      BSONObj::iterator i = a.Obj().begin(), j = b.Obj().begin();
      while(i.more() && j.more()){
        BSONElement ae = i.next(), be = j.next();
        if(!is_equal_bson(ae, be))
          return false;
      }
      return i.more() == j.more();
      }
    case mongo::Object:{
      // contains the same property names, and each property in the object is equal to the corresponding property in the other object
      if(a.valuesize() != b.valuesize()) // ensures number of fields etc.
        return false;
      for(BSONObj::iterator i = a.Obj().begin(); i.more();){
        BSONElement ae = i.next();
        if(!is_equal_bson(ae, b[ae.fieldName()]))
          return false;
      }
      return true;
    }

    default: break;
  }
  return false;
}


template<bool ThrowOnFail=false>
class CValidatorImpl: public CValidator{
  public:

    CValidatorImpl():CValidator(){}
    CValidatorImpl(const BSONObj schema){
      m_primary_schema_id = add_schema(schema);
    }


    void handle_subschemas(BSONObj schema, string id, int level = 0, string path = "/"){
      if(schema["$schema"].ok()){
        BSONElement s = schema["$schema"];
        if(s.type() != mongo::String)
          throw InvalidSchema(path + "$schema must be a string");
        if(!match_pattern("http://json-schema.org/draft-03/schema#?", s.valuestr())){
          throw InvalidSchema(path + "$schema must be 'http://json-schema.org/draft-03/schema', other are unsupported");
        }
      }

      if(level > 0 && schema["id"].ok()){
        // add all schemas that can be referenced
        add_schema(schema, id); //TODO: check parent schema tracking
      }


      for(BSONObj::iterator i = schema.begin(); i.more();){
        BSONElement e = i.next();

        if(!strcmp("$ref", e.fieldName())){
          if(e.type() != mongo::String)
            throw InvalidSchema(path + ": $ref must be a string");

          //TODO: preload referenced schemas? (and option for that (in ctor))
          //note: at time of loading referenced schema may not exist, so cannot check (ie. {$ref:a, a:{}})
        }

        //items may be schema or an array of schemas
        if(!strcmp("items", e.fieldName())){
          if(e.type() == mongo::Array){
            for(BSONObj::iterator j = e.Obj().begin(); j.more();){
              BSONElement e = j.next();
              handle_subschemas(e.Obj(), id, level+1, path + "items[" + boost::lexical_cast<string>(e.fieldName()) + "]/");
            }
          } else if(e.type() == mongo::Object){
            handle_subschemas(e.Obj(), id, level+1, path + "items/");
          }
          continue;
        }

        // these may be schema, or some elems n array may be schemas
        if(!strcmp("type", e.fieldName()) || !strcmp("disallow", e.fieldName())){
          if(e.type() == mongo::Array){
            for(BSONObj::iterator j = e.Obj().begin(); j.more();){
              BSONElement e = j.next();
              if(e.type() == mongo::Object)
                handle_subschemas(e.Obj(), id, level+1, path + e.fieldName() + "[" + boost::lexical_cast<string>(e.fieldName()) + "]/"); //+2?
            }
          } else if(e.type() == mongo::Object){
            handle_subschemas(e.Obj(), id, level+1, path + e.fieldName() + "/");
          }
          continue;
        }

        // schemas where values in obj are schemas
        if(!strcmp("properties", e.fieldName()) || !strcmp("patternProperties", e.fieldName()) || !strcmp("dependencies", e.fieldName())){
          if(e.type() != mongo::Object && e.fieldName()[0] != 'd') // in dependencies there may be also an array etc.
            throw InvalidSchema(string(e.fieldName()) + " must be a schema");
          for(BSONObj::iterator j = e.Obj().begin(); j.more();){
            BSONElement e = j.next();
            if(e.type() == mongo::Object)
              handle_subschemas(e.Obj(), id, level+1, path + e.fieldName() + "/");
          }
          continue;
        }

        // each of these are schemas
        if(!strcmp("additionalProperties", e.fieldName()) ||
          !strcmp("additionalItems", e.fieldName()) ||
          !strcmp("extends", e.fieldName())){
          if(e.type() == mongo::Object){
            handle_subschemas(e.Obj(), id, level+1, path + e.fieldName() + "/");
          }
        }
      }
    }

    std::string add_schema(BSONObj schema, string parent=""){
      std::string id = generate_schema_id(schema, parent);
      BSONObj our_schema;
      if(m_schemas.find(id) != m_schemas.end()){
        throw InvalidSchema("Schema with id '"+id+"'already exist");
        return id;
      } else {
        if (parent == ""){
          // copy data for top-level schema (just in case it goes out of scope in caller)
          our_schema = schema.getOwned();
        } else {
          // not-owned is ok for sub-schemas, save the ram :)
          our_schema = schema;
        }
        m_schemas.insert(pair<string,BSONObj>(id, our_schema));
      }

      if(parent == "") // we can be called from handle_subschemas itself
        handle_subschemas(our_schema, id);

      //TODO: some optimization? (i.e. pre-compilation etc.)

      return id;
    }

    std::string generate_schema_id(BSONObj schema, string parent=""){
      BSONElement id = schema["id"];
      if(id.type() == mongo::String){
        //FIXME: track parent (for relative ids or subschemas etc.)
        // check if uri is relative and make it absolute via parent base uri
        return id.String();
      } else {
        //TODO: a better generation, GUID-like maybe
        return std::string("_unnamed_schema") + boost::lexical_cast<std::string>(m_schemas.size());
      }
    }

    BSONObj get_schema(string id){
      typeof(m_schemas.begin()) it = m_schemas.find(id);
      if(it == m_schemas.end()){
        throw InvalidSchema("cannot find referenced schema " + id);
      }
      return it->second;
    }

    BSONObj resolve_schema_fragment(BSONObj schema_root, string id, string fragment){
      if(fragment == "/" || fragment == "")
        return schema_root;

      // сплиттим и сносим лишнее из фрагментов
      vector<string> fragments;
      boost::split(fragments, fragment, boost::is_any_of("/\\"), boost::token_compress_on);
      std::vector<string>::iterator new_end = std::remove_if(fragments.begin(), fragments.end(), std::bind2nd(std::equal_to<string>(), string("")));
      fragments.erase(new_end, fragments.end());
      std::vector<string>::iterator It = fragments.begin();

      BSONElement target = schema_root[*It];
      string target_path = "/" + *It;
      It++;

      for(; It != fragments.end(); ++It){
        switch(target.type()){
          case mongo::Object:
            target = target[*It];
            break;
          case mongo::Array:
            //TODO: check *It to be integer index (inside bson indexes anyway are strings):
            target = target[*It];
            break;
          default:
            throw InvalidSchema("Referenced schema " + id + " at " + target_path + " is not object nor array");
        }
        target_path += "/" + (string)*It;
      }

      if(target.type() != mongo::Object)
        throw InvalidSchema("Referenced subschema is not schema at " + target_path);
      return target.Obj();
    }

    //actually not id, but uri/reference
    BSONObj schema(string id, BSONObj referenced_from){
      //TODO: resolve relative uris agaist parent schema uri and fetch fragment from uri
      //TODO: better uri parsing...
      size_t hash_loc = id.find_first_of("#");
      if(hash_loc == string::npos){ // no hash
        return get_schema(id);
      }

      string id1 = id.substr(0, hash_loc), path = id.substr(hash_loc + 1);

      if(id1 == ""){
        //loopback
        return resolve_schema_fragment(referenced_from, id, path);
      } else {
        return resolve_schema_fragment(get_schema(id1), id, path);
      }
    }


    bool validate(BSONObj data){
      return validate(BSON("wrap" << data)["wrap"]);
    }

    bool validate(BSONElement data){
      BSONObj schema = this->get_schema(m_primary_schema_id);
      return validate(schema, schema, data, "/");
    }

    bool match_pattern(const char* pattern, string str){
      const char *error;
      int erroffset;
       pcre *re = pcre_compile(
        pattern,
        0,                /* default options */
        &error,           /* for error message */
        &erroffset,       /* for error offset */
        NULL);            /* use default character tables */
      int rc = pcre_exec(
            re,                   /* the compiled pattern */
            NULL,                 /* no extra data - we didn't study the pattern */
            str.data(),              /* the subject string */
            (int)str.size(),       /* the length of the subject */
            0,                    /* start at offset 0 in the subject */
            0,                    /* default options */
            NULL,              /* output vector for substring information */
            0);           /* number of elements in the output vector */
       pcre_free(re);
       return rc >= 0;
    }

    bool match_pattern(BSONElement val, string str){
      //TODO: precompile regex on schema load or cache?
      string pattern;
      if(val.type() == mongo::String){
        pattern = val.String();
      } else if(mongo::RegEx){
        pattern = val.value();
        //TODO: regexFlags()
      } else {
        throw InvalidSchema("invalid value for regex");
      }
      return match_pattern(pattern.c_str(), str);
    }



    #define VALIDATOR(validator) bool validate_##validator(BSONObj schema, BSONObj parent_schema, BSONElement validator##_schema, BSONElement data, string path, const char* validator_name = #validator)
    #define VALIDATOR_(validator) bool validate_##validator(BSONObj schema, BSONObj parent_schema, BSONElement val, BSONElement data, string path, const char* validator_name = #validator)
    #define CHAIN_SCHEMA(schema, data, path) validate((schema), parent_schema, (data), (path))

    #define FAIL(msg) { if(ThrowOnFail) throw ValidationError(validator_name, path, msg); return false; }


    //bool validate_type(BSONObj schema, BSONElement type_schema, BSONElement data, string path){
    VALIDATOR(type){
      switch(type_schema.type()){
        case mongo::String:
          if(is_type(data, type_schema.str()))
            return true;
          FAIL(": " + str_type(data) + " is not " + type_schema.str());
        case mongo::Object: return CHAIN_SCHEMA(type_schema.Obj(), data, path + data.fieldName() + "/");
        case mongo::Array:
          for(BSONObj::iterator i = type_schema.Obj().begin(); i.more();){
            BSONElement e = i.next();
            try{
              if(e.type() == mongo::String && is_type(data, e.str()) ||
                 e.type() == mongo::Object && CHAIN_SCHEMA(e.Obj(), data, path))
                 return true;
            } catch(ValidationError){
              continue;
            }
          }
          FAIL(": data did not match any of types/schemas"); //TODO: list types from array in error message?
        default:
          throw InvalidSchema("type should be string, array or schema");
      }
    }

    //bool validate_properties(BSONObj schema, BSONElement props, BSONElement data, string path){
    VALIDATOR(properties){
      if(data.type() != mongo::Object) return true;

      for(BSONObj::iterator i = properties_schema.Obj().begin(); i.more();){
        BSONElement prop_schema = i.next(), e;
        if((e = data[prop_schema.fieldName()]).ok()){
          if(!CHAIN_SCHEMA(prop_schema.Obj(), e, path + prop_schema.fieldName() + "/"))
            FAIL("");
        } else {
          if((e = prop_schema["required"]).ok() && e.trueValue())
            FAIL(string(": required property '") + prop_schema.fieldName() + "' is missing");
        }
      }
      return true;
    }

    //bool validate_additionalProperties(BSONObj schema, BSONElement add_props, BSONElement data, string path){
    VALIDATOR(additionalProperties){
      //check properties and patternProperties
      if(additionalProperties_schema.type() == mongo::Bool && additionalProperties_schema.boolean())
        return true;

      if(data.type() == mongo::Object){
        BSONElement props = schema["properties"], pattern = schema["patternProperties"];
        for(BSONObj::iterator i = data.Obj().begin(); i.more();){
          BSONElement e = i.next();
          const char* name = e.fieldName();
          if(props.type() == mongo::Object && props[name].ok()){
            continue;
          }

          bool valid = false;
          if(pattern.type() == mongo::Object){
            for(BSONObj::iterator pi = pattern.Obj().begin(); pi.more();){
              BSONElement patt = pi.next();
              if(match_pattern(patt.fieldName(), e.fieldName())){
                valid = true;
                break;
              }
            }
          }
          if(!valid){
            if((additionalProperties_schema.type() == mongo::Object) && CHAIN_SCHEMA(additionalProperties_schema.Obj(), e, path + e.fieldName() + "/"))
              continue;
            FAIL(string(": field '") + e.fieldName() +"' is not matched by neither properties not patternProperties");
          }
        }
      }
      return true;
    }

    //bool validate_items(BSONObj schema, BSONElement items, BSONElement data, string path){
    VALIDATOR(items){
      if(data.type() == mongo::Array){
        int num = 0;
        if(items_schema.type() == mongo::Object){
          for(BSONObj::iterator i = data.Obj().begin(); i.more();){
            BSONElement e = i.next();
            if(!CHAIN_SCHEMA(items_schema.Obj(), e, path + "[" + boost::lexical_cast<std::string>(num) + "]"))
              FAIL("");
            num++;
          }
        } else if(items_schema.type() == mongo::Array){
          BSONObj::iterator i = data.Obj().begin(),
            s = items_schema.Obj().begin();
          while(i.more() && s.more()){
            BSONElement e = i.next(), is = s.next();
            if(is.type() != mongo::Object)
              throw InvalidSchema("items in items array should be schemas");
            if(!CHAIN_SCHEMA(is.Obj(), e, path + "[" + boost::lexical_cast<std::string>(num) + "]"))
              FAIL("");
            num++;
          }
          //additionalItems check
          if(i.more()){
            BSONElement additional = schema["additionalItems"];
            switch(additional.type()){
              case mongo::Bool:
                if(additional.boolean())
                  return true;
                FAIL(string(": additional items are not allowed, viotaing item is at ") + i.next().fieldName());
              case mongo::Object:
                while(i.more()){
                  BSONElement e = i.next();
                  if(!CHAIN_SCHEMA(additional.Obj(), e, path + "[" + boost::lexical_cast<std::string>(num) + "]"))
                    FAIL("");
                  num++;
                }
                break;
              case mongo::EOO: return true;
              default:
                throw InvalidSchema("additionalItems may only be boolean or schema");
            }
          }
        }
      }
      return true;
    }


    //bool validate_dependencies(BSONObj schema, BSONElement dependencies, BSONElement data, string path){
    VALIDATOR(dependencies){
      if(data.type() == mongo::Object){
        for(BSONObj::iterator i = dependencies_schema.Obj().begin(); i.more();){
          BSONElement dependency = i.next();
          const char* dep_key = dependency.fieldName();
          if(data[dep_key].ok()){
            switch(dependency.type()){
              case mongo::String:
                if(data[dependency.str()].ok())
                  return true;
                FAIL(": missing field " + dependency.str());
              case mongo::Array:
                for(BSONObj::iterator depi = dependency.Obj().begin(); depi.more();){
                  BSONElement dep = depi.next();
                  if(dep.type() != mongo::String)
                    throw InvalidSchema("values in dependency array must be strings with field names");
                  if(!data[dep.str()].ok())
                    FAIL(string(": Required field '") + dep.str() + "' is missing");
                  }
                return true;
              case mongo::Object:
                return CHAIN_SCHEMA(dependency.Obj(), data, path);
              default:
                throw InvalidSchema("Dependency must be a string, array or schema");
            }
          }
        }
      }
      return true;
    }

    //bool validate_minLength(BSONObj schema, BSONElement val, BSONElement data, string path){
    VALIDATOR_(minLength){
      if(data.type() == mongo::String){
        if(val.type() != mongo::NumberInt) //TODO: NumberLong (the same for maxLength and divisibleBy)
          throw InvalidSchema("minLength must be a integer");

        //FIXME: this counts bytes instead of unicode chars!
        if(data.String().length() < (size_t)val.Int())
          FAIL(string(": length is under ") + boost::lexical_cast<string>(val.Int()));
      }
      return true;
    }

    //bool validate_maxLength(BSONObj schema, BSONElement val, BSONElement data, string path){
    VALIDATOR_(maxLength){
      if(data.type() == mongo::String){
        if(val.type() != mongo::NumberInt)
          throw InvalidSchema("maxLength must be a integer");
        if(data.String().length() > (size_t)val.Int())
          FAIL(string(": length is over ") + boost::lexical_cast<string>(val.Int()));
      }
      return true;
    }

    //bool validate_divisibleBy(BSONObj schema, BSONElement val, BSONElement data, string path){
    VALIDATOR_(divisibleBy){
      if(val.type() != mongo::NumberInt || !val.Int())
        throw InvalidSchema("divisibleBy must be a non-zero integer");
      switch(data.type()){
        case mongo::NumberInt:
          if ((data.Int() % val.Int()) == 0)
            return true;
          break;
        case mongo::NumberLong:
          if((data.Long() % val.Int()) == 0)
            return true;
          break;
        case mongo::NumberDouble:
          if(((long long)data.Number() % val.Int()) == 0) //TODO: more clean division of doubles?...
            return true;
          break;
        default: // for other types- assume not divisible (no mention for that in std)
          break;
      }
      FAIL(string(": value is not integer or not divisible by ") + boost::lexical_cast<string>(val.Int()));
    }

    //bool validate_maximum(BSONObj schema, BSONElement val, BSONElement data, string path){
    VALIDATOR_(maximum){
      if(data.isNumber()){
        if(!val.isNumber())
          throw InvalidSchema("maximum must be a number");
        if(schema["exclusiveMaximum"].trueValue() ? data.Number() >= val.Number() : data.Number() > val.Number())
          FAIL(": value is over " + boost::lexical_cast<string>(val.Number()));
      }
      return true;
    }

    //bool validate_minimum(BSONObj schema, BSONElement val, BSONElement data, string path){
    VALIDATOR_(minimum){
      if(data.isNumber()){
        if(!val.isNumber())
          throw InvalidSchema("minimum must be a number");
        if(schema["exclusiveMinimum"].trueValue() ? data.Number() <= val.Number() : data.Number() < val.Number())
          FAIL(": value is under " + boost::lexical_cast<string>(val.Number()));
      }
      return true;
    }

    //bool validate_enum(BSONObj schema, BSONElement val, BSONElement data, string path){
    VALIDATOR_(enum){
      if(val.type() != mongo::Array)
        throw InvalidSchema("enum must be an array");

      for(BSONObj::iterator i = val.Obj().begin(); i.more();){
        BSONElement v = i.next();
        if(is_equal_bson(v, data))
          return true;
      }
      FAIL("");
    }

    //bool validate_uniqueItems(BSONObj schema, BSONElement val, BSONElement data, string path){
    VALIDATOR_(uniqueItems){
      if(data.type() == mongo::Array && val.trueValue()){ //TODO: this can be applied to objects too (not stated in std)
        for(BSONObj::iterator i = data.Obj().begin(); i.more();){
          BSONElement v = i.next();
          for(BSONObj::iterator j = i; j.more();){
            if(is_equal_bson(v, j.next()))
              FAIL(string(": not unique item at ") + v.fieldName());
          }
        }
      }
      return true;
    }

    //bool validate_minItems(BSONObj schema, BSONElement val, BSONElement data, string path){
    VALIDATOR_(minItems){
      if(val.type() != mongo::NumberInt)
        throw InvalidSchema("minItems must be an integer");

      if(data.type() == mongo::Array){
        int nfields = data.Obj().nFields();
        if(nfields < val.Int())
          FAIL(": array has " + boost::lexical_cast<string>(nfields) + " while required at least " + boost::lexical_cast<string>(val.Int()));
      }
      return true;
    }

    //bool validate_maxItems(BSONObj schema, BSONElement val, BSONElement data, string path){
    VALIDATOR_(maxItems){
      if(val.type() != mongo::NumberInt)
        throw InvalidSchema("maxItems must be an integer");

      if(data.type() == mongo::Array){
        int nfields = data.Obj().nFields();
        if(nfields > val.Int())
          FAIL(": array has " + boost::lexical_cast<string>(nfields) + " while required at most " + boost::lexical_cast<string>(val.Int()));
      }
      return true;
    }

    //bool validate_extends(BSONObj schema, BSONElement val, BSONElement data, string path){
    VALIDATOR_(extends){
      switch(val.type()){
        case mongo::String:{
          BSONObj schema = this->schema(val.String(), parent_schema);
          //check if schema is subschema:
          BSONObj new_root = schema.isOwned() ? schema : parent_schema;
          return validate(schema, new_root, data, path);
          }
        case mongo::Object:
          return CHAIN_SCHEMA(val.Obj(), data, path);
        case mongo::Array:{
          for(BSONObj::iterator i = val.Obj().begin(); i.more();){
            BSONElement v = i.next();

            switch(v.type()){
              case mongo::Object:
                if(!CHAIN_SCHEMA(v.Obj(), data, path))
                  FAIL("");
                break;
              case mongo::String:{
                BSONObj schema = this->schema(v.String(), parent_schema);
                if(!validate(schema, schema.isOwned() ? schema : parent_schema, data, path))
                  FAIL("");
                }
                break;
              default:
                throw InvalidSchema("Values in extends array may only be strings or schemas");
                break;
              }
            }
            return true;
        }
        default:
          throw InvalidSchema("Value in extends may only be string or schema");
          break;
      }
    }

    //bool validate_schema_reference(BSONObj schema, BSONElement val, BSONElement data, string path){
    VALIDATOR_(schema_reference){
      BSONObj sc = this->schema(val.String(), parent_schema);
      return validate(sc, (sc.isOwned()? sc : parent_schema), data, path);
    }

    //bool validate_pattern(BSONObj schema, BSONElement val, BSONElement data, string path){
    VALIDATOR_(pattern){
      if(data.type() == mongo::String){
        if(!match_pattern(val, data.String()))
          FAIL("");
      }
      return true;
    }

    //bool validate_format(BSONObj schema, BSONElement val, BSONElement data, string path){
    VALIDATOR_(format){  //Validators MAY (but are not required to) validate that the
      //   instance values conform to a format

      //TODO: validation of formats? (required as 'may' in std, but usable)

      return true;
    }


    //bool validate_patternProperties(BSONObj schema, BSONElement props, BSONElement data, string path){
    VALIDATOR(patternProperties){
      if(data.type() != mongo::Object) return true;

      for(BSONObj::iterator i = patternProperties_schema.Obj().begin(); i.more();){
        BSONElement prop_schema = i.next(), e;
        for(BSONObj::iterator i = data.Obj().begin(); i.more();){
          BSONElement e = i.next();
          if(match_pattern(prop_schema.fieldName(), e.fieldName())){
            if(!CHAIN_SCHEMA(prop_schema.Obj(), e, path + prop_schema.fieldName() + "/"))
              FAIL("");
            break; //next pattern
          } else {
             if((e = prop_schema["required"]).ok() && e.trueValue())
               FAIL(string(": missing a field with name matching ") + prop_schema.fieldName());
          }
        }
      }
      return true;
    }

    VALIDATOR(valueDependencies){
      if(data.type() == mongo::Object){
        for(BSONObj::iterator i = valueDependencies_schema.Obj().begin(); i.more();){
          BSONElement dependency_field = i.next();
          const char* dep_key = dependency_field.fieldName();

          BSONElement dep_val = data[dep_key];
          if(dep_val.type() == mongo::String){ //not eoo, + non-strings cannot be checked because field keys are strings
            BSONElement dependency = dependency_field[dep_val.String()];
            switch(dependency.type()){
              case mongo::String:
                if(!data[dependency.str()].ok())
                  FAIL(string(": missing field '") + dependency.str() + "'");
                break;
              case mongo::Array:
                for(BSONObj::iterator depi = dependency.Obj().begin(); depi.more();){
                  BSONElement dep = depi.next();
                  if(dep.type() != mongo::String)
                    throw InvalidSchema("values in valueDependency array must be strings with field names");
                  if(!data[dep.str()].ok())
                    FAIL(string(": missing field '") + dep.str() + "'");
                  }
                return true;
              case mongo::Object:
                return CHAIN_SCHEMA(dependency.Obj(), data, path);
              default:
                throw InvalidSchema("valueDependency must be a string, array or schema");
            }
          }
        }
      }
      return true;
    }

    #undef VALIDATOR

    bool validate(BSONObj schema, BSONObj root_schema, BSONElement data, string path){
      for(BSONObj::iterator i = schema.begin(); i.more();){
        BSONElement e = i.next();
        //assuming schema is valid (but still some checks that may throw InvalidSchema ;)
        // valuestr()+strncmp is faster, but no check etc.

        #define CALL_VALIDATOR(validator) validate_##validator(schema, root_schema, e, data, path)
        #define VALIDATOR(validator) if(!strcmp(#validator, e.fieldName())) { if(!CALL_VALIDATOR(validator)) return false;}

      // Draft-3 validators (see http://tools.ietf.org/html/draft-zyp-json-schema-03):
      //TODO: implement Draft-2 and Draft-1 specs?

        VALIDATOR(type);
        VALIDATOR(properties);
        VALIDATOR(patternProperties)
        VALIDATOR(additionalProperties);
        VALIDATOR(items);
        // additionalItems handled in items handler
        // required handled in properties and additionalProperties
        VALIDATOR(dependencies);
        VALIDATOR(minimum)
        VALIDATOR(maximum)
        //exclusiveMinimum handled in minimum
        //exclusiveMaximum handled in maximum
        VALIDATOR(minItems)
        VALIDATOR(maxItems)
        VALIDATOR(uniqueItems);
        VALIDATOR(pattern);
        VALIDATOR(minLength)
        VALIDATOR(maxLength)
        VALIDATOR(enum)
        //TODO: default values (this requires input data to be modificable)
        //title attribute is not validated
        //description attribute is not validated
        VALIDATOR(format)
        VALIDATOR(divisibleBy)
        if(!strcmp("disallow", e.fieldName())) { if(CALL_VALIDATOR(type)) return false;}
        VALIDATOR(extends)
        //id attr handled on schema load
        if(!strcmp("$ref", e.fieldName())) { if(!CALL_VALIDATOR(schema_reference)) return false;}
        //$schema attr handled on schema load

      // additional validators(not in draft-3):
        VALIDATOR(valueDependencies)

        #undef VALIDATOR
        //TODO: hyperschemas?

      }
      return true;
    }


  protected:

    std::map<std::string, BSONObj> m_schemas;
    std::string m_primary_schema_id;


  public:
    static bool validate(BSONObj schema, BSONElement data){
      return CValidatorImpl(schema).validate(data);
    }
    static bool validate(BSONObj schema, BSONObj data){
      return CValidatorImpl(schema).validate(data);
    }
};

bool CValidator::validate(BSONObj schema, BSONElement data){
  return CValidatorImpl<false>(schema).validate(data);
}
bool CValidator::validate(BSONObj schema, BSONObj data){
  return CValidatorImpl<false>(schema).validate(data);
}
bool CValidator::validate_throw(BSONObj schema, BSONElement data){
  return CValidatorImpl<true>(schema).validate(data);
}
bool CValidator::validate_throw(BSONObj schema, BSONObj data){
  return CValidatorImpl<true>(schema).validate(data);
}
