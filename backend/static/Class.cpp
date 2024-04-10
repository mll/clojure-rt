#include <string>
#include <unordered_map>

extern "C" {
  #include "runtime/Class.h"
  #include "runtime/String.h"
}

std::unordered_map<std::string, Class *> getStaticClasses() {
    std::unordered_map<std::string, Class *> classes;
    
    char* s = (char *) "java.lang.Class";
    String* name = String_createDynamicStr(s);
    String* className = String_createDynamicStr(s);
    Class *_class = Class_create(name, className, 0, NULL);
    classes.insert({{s}, _class});
    
    return classes;
}
