#include "Namespace.h"
#include "Object.h"
#include "Symbol.h"
#include "PersistentArrayMap.h"
#include "String.h"
#include "Exceptions.h"
#include "Var.h"
#include <stdio.h>

/*
 * Allocates and initializes a new Namespace object with the given symbol as its name.
 * Initializes the mappings and aliases to empty PersistentArrayMaps.
 * Consumes the `name` argument.
 */
Namespace *Namespace_create(Symbol *name) {
  Namespace *self = (Namespace *)allocate(sizeof(Namespace));
  Object_create((Object *)self, namespaceType);
  self->name = name;
  
  // Initialize mappings and aliases with empty PersistentArrayMap
  RTValue emptyMap = RT_boxPtr(PersistentArrayMap_empty()); // Refcount is 1
  atomic_store_explicit(&self->mappings, emptyMap, memory_order_relaxed);
  atomic_store_explicit(&self->aliases, emptyMap, memory_order_relaxed);
  retain(emptyMap); // Retain once more for the second pointer (aliases), making refcount 2
  
  return self;
}

/*
 * Destroys the Namespace object. If `deallocateChildren` is true, it releases
 * the retained references to the namespace name, mappings, and aliases maps.
 */
void Namespace_destroy(Namespace *self, bool deallocateChildren) {
  printf("Namespace_destroy called for %s!\n", String_c_str(self->name->name));
  if (deallocateChildren) {
    if (self->name) {
      Object_release((Object *)self->name);
    }
    RTValue mappings = atomic_load_explicit(&self->mappings, memory_order_acquire);
    release(mappings);
    
    RTValue aliases = atomic_load_explicit(&self->aliases, memory_order_acquire);
    release(aliases);
  }
}

/*
 * Returns the hash code of the namespace, which is equivalent to the hash code of its name symbol.
 */
uword_t Namespace_hash(Namespace *self) {
  return Symbol_hash(self->name);
}

/*
 * Checks equality between two namespaces by comparing their name symbols.
 */
bool Namespace_equals(Namespace *self, Namespace *other) {
  return Symbol_equals(self->name, other->name);
}

/*
 * Generates a string representation of the namespace in the format `#namespace[name]`.
 * Consumes the `self` argument and returns a new String.
 */
String *Namespace_toString(Namespace *self) {
  String *nameStr = Symbol_toString(RT_boxSymbol(self->name));
  String *prefix = String_create("#namespace[");
  String *suffix = String_create("]");
  
  String *res1 = String_concat(prefix, nameStr);
  String *res2 = String_concat(res1, suffix);
  
  return res2;
}

/*
 * Promotes the namespace and its children (name, mappings, aliases) to shared state,
 * allowing safe concurrent access across threads.
 */
void Namespace_promoteToShared(Namespace *self, uword_t count) {
  Object_promoteToSharedShallow((Object *)self, count);
  Object_promoteToShared((Object *)self->name);
  promoteToShared(atomic_load_explicit(&self->mappings, memory_order_acquire));
  promoteToShared(atomic_load_explicit(&self->aliases, memory_order_acquire));
}


/*
 * Retrieves the current dictionary of interned variables and mappings.
 * The caller assumes ownership of the returned map reference (+1 refcount).
 * Consumes the `self` argument.
 */
RTValue Namespace_getMappings(Namespace *self) {
  RTValue m = atomic_load_explicit(&self->mappings, memory_order_acquire);
  retain(m);
  Ptr_release(self);
  return m;
}

/*
 * Retrieves the current dictionary of namespace aliases.
 * The caller assumes ownership of the returned map reference (+1 refcount).
 * Consumes the `self` argument.
 */
RTValue Namespace_getAliases(Namespace *self) {
  RTValue a = atomic_load_explicit(&self->aliases, memory_order_acquire);
  retain(a);
  Ptr_release(self);
  return a;
}

/*
 * Checks if the given object `o` is a Var interned in this specific namespace
 * under the exact symbol `sym`.
 * Consumes `self`, `sym`, and `o` arguments.
 */
bool Namespace_isInternedMapping(Namespace *self, Symbol *sym, RTValue o) {
  bool res = false;
  if (getType(o) == varType) {
    Var *v = (Var *)RT_unboxPtr(o);
    res = (v->ns == self) && Symbol_equals(v->sym, sym);
  }
  Ptr_release(self);
  Ptr_release(sym);
  release(o);
  return res;
}

/*
 * Validates whether an existing mapping (`oldVal`) can be safely replaced by a new mapping (`neuVal`).
 * Throws an IllegalStateException if an attempt is made to overwrite a Var interned in this namespace.
 * Consumes all arguments: `self`, `sym`, `oldVal`, and `neuVal`.
 */
bool Namespace_checkReplacement(Namespace *self, Symbol *sym, RTValue oldVal, RTValue neuVal) {
  bool res = true;
  if (getType(oldVal) == varType) {
    Ptr_retain(self); Ptr_retain(sym); retain(oldVal); // for isInternedMapping which consumes
    if (Namespace_isInternedMapping(self, sym, oldVal)) {
      Ptr_release(self); Ptr_release(sym); release(oldVal); release(neuVal);
      throwIllegalStateException_C("REJECTED: attempt to replace interned var");
      return false;
    }
  }
  Ptr_release(self);
  Ptr_release(sym);
  release(oldVal);
  release(neuVal);
  return res;
}

/*
 * Interns a symbol in this namespace, establishing a new Var if one does not exist,
 * or returning the existing Var if already mapped.
 * 
 * "Interning" in this context refers to the process of associating a symbol 
 * with a canonical, globally unique Var instance within a namespace. This ensures 
 * that all references to the same symbol in this namespace resolve to the exact 
 * same memory location and share the same root binding and metadata.
 * 
 * Performs a lock-free atomic compare-and-swap (CAS) to update the mappings map.
 * Uses Epoch-Based Reclamation (RCU-style) `autorelease` for replacing old maps.
 * Consumes both `self` and `sym` arguments, and returns a retained Var (+1 refcount).
 */
Var *Namespace_intern(Namespace *self, Symbol *sym) {
  if (sym->ns != NULL) {
    Ptr_release(self);
    Ptr_release(sym);
    throwIllegalArgumentException_C("Can't intern namespace-qualified symbol");
  }
  
  RTValue symVal = RT_boxSymbol((Object *)sym);
  Var *v = NULL;
  
  while (true) {
    RTValue mapVal = atomic_load_explicit(&self->mappings, memory_order_acquire);
    PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(mapVal);
    
    Ptr_retain(map); retain(symVal);
    RTValue o = PersistentArrayMap_get(map, symVal);
    if (!RT_isNil(o)) {
      Ptr_retain(self); Ptr_retain(sym); retain(o);
      if (Namespace_isInternedMapping(self, sym, o)) {
        if (v) { release(RT_boxPtr((Object *)v)); }
        Ptr_release(self);
        Ptr_release(sym);
        return (Var *)RT_unboxPtr(o);
      }
      
      if (v == NULL) {
        v = Var_create_interned(self, sym); // v has +1 refcount
      }
      
      Ptr_retain(self); Ptr_retain(sym); retain(o); retain(RT_boxPtr((Object *)v));
      if (Namespace_checkReplacement(self, sym, o, RT_boxPtr((Object *)v))) {
        release(o);
        RTValue newVal = RT_boxPtr((Object *)v);
        Ptr_retain(map); retain(symVal); retain(newVal);
        PersistentArrayMap *newMap = PersistentArrayMap_assoc(map, symVal, newVal);
        RTValue newMapVal = RT_boxPtr((Object *)newMap);
        
        if (atomic_compare_exchange_strong_explicit(&self->mappings, &mapVal, newMapVal, memory_order_acq_rel, memory_order_acquire)) {
          autorelease(mapVal);
          Ptr_release(self);
          Ptr_release(sym);
          // return v without releasing it, so caller owns the ref. Wait, assoc also retained it. So v has +2.
          // We need to return it with +1 to caller. But wait, assoc retained newVal. v was created with +1.
          // So returning v directly leaves it with +2. Wait, caller expects +1. We just drop our reference?
          // If we drop our reference, we must `release(RT_boxPtr(v));` but wait! We can't release it AND return it.
          // We just don't release it! Wait! If it has +2 and we give 1 to caller, we must release the other.
          // But `assoc` took +1. We created it with +1. So +2. Caller takes +1.
          // We don't need to retain. Wait! If `assoc` took +1, we have +1. We give +1 to caller. Perfect.
          return v;
        } else {
          release(newMapVal);
        }
      } else {
        if (v) { release(RT_boxPtr((Object *)v)); }
        Ptr_release(self);
        Ptr_release(sym);
        return (Var *)RT_unboxPtr(o);
      }
    } else {
      if (v == NULL) {
        v = Var_create_interned(self, sym);
      }
      RTValue newVal = RT_boxPtr((Object *)v);
      Ptr_retain(map); retain(symVal); retain(newVal);
        PersistentArrayMap *newMap = PersistentArrayMap_assoc(map, symVal, newVal);
      RTValue newMapVal = RT_boxPtr((Object *)newMap);
      
      if (atomic_compare_exchange_strong_explicit(&self->mappings, &mapVal, newMapVal, memory_order_acq_rel, memory_order_acquire)) {
        autorelease(mapVal);
        Ptr_release(self);
        Ptr_release(sym);
        return v;
      } else {
        release(newMapVal);
      }
    }
  }
}

/*
 * Establishes a reference (mapping) from the symbol `sym` to the given value `val` within this namespace.
 * Throws an exception if the symbol is already mapped to an interned Var in this namespace.
 * Uses lock-free atomic updates (CAS) for map replacement.
 * Consumes `self`, `sym`, and `val` arguments. The returned value is retained (+1 refcount).
 */
RTValue Namespace_reference(Namespace *self, Symbol *sym, RTValue val) {
  if (sym->ns != NULL) {
    Ptr_release(self); Ptr_release(sym); release(val);
    throwIllegalArgumentException_C("Can't intern namespace-qualified symbol");
  }
  
  RTValue symVal = RT_boxSymbol((Object *)sym);
  
  while (true) {
    RTValue mapVal = atomic_load_explicit(&self->mappings, memory_order_acquire);
    PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(mapVal);
    
    Ptr_retain(map); retain(symVal);
    RTValue o = PersistentArrayMap_get(map, symVal);
    if (!RT_isNil(o)) {
      if (equals(o, val)) {
        Ptr_release(self); Ptr_release(sym); release(val);
        return o;
      }
      Ptr_retain(self); Ptr_retain(sym); retain(o);
      if (Namespace_isInternedMapping(self, sym, o)) {
        Ptr_release(self); Ptr_release(sym); release(val); release(o);
        throwIllegalStateException_C("Cannot reference into an interned mapping");
      }
      Ptr_retain(self); Ptr_retain(sym); retain(o); retain(val);
      if (!Namespace_checkReplacement(self, sym, o, val)) {
        Ptr_release(self); Ptr_release(sym); release(val);
        return o;
      }
      release(o);
    }
    
    Ptr_retain(map); retain(symVal); retain(val);
    PersistentArrayMap *newMap = PersistentArrayMap_assoc(map, symVal, val);
    RTValue newMapVal = RT_boxPtr((Object *)newMap);
    
    if (atomic_compare_exchange_strong_explicit(&self->mappings, &mapVal, newMapVal, memory_order_acq_rel, memory_order_acquire)) {
      autorelease(mapVal);
      Ptr_release(self);
      Ptr_release(sym);
      // We don't release `val` because we return it to the caller!
      // But wait! assoc retained `val`. So `val` has +2 refcount. 
      // If we return `val` to caller, caller takes +1. The map holds +1.
      // So we MUST release `val` if we want to give caller the same `val`? No!
      // If we received `val` with +1, we give `val` with +1 to caller. The map took +1.
      // Wait! `assoc` called `retain(val)`. So `val` refcount is +2. 
      // We own +1 (from argument). We give +1 to caller. So it balances out. We do NOT release `val`.
      return val;
    } else {
      release(newMapVal);
    }
  }
}

/*
 * A convenience wrapper for `Namespace_reference` specifically designed to map a symbol to a Class.
 * Consumes `self`, `sym`, and `val`.
 */
RTValue Namespace_referenceClass(Namespace *self, Symbol *sym, Class *val) {
  // val is a Class pointer, it consumes val.
  // Namespace_reference consumes self, sym, and RTValue. So we don't need to release self/sym here.
  return Namespace_reference(self, sym, RT_boxPtr((Object *)val));
}

/*
 * Removes the mapping for the given symbol from the namespace's mappings.
 * Uses lock-free atomic CAS to replace the mappings map with the symbol dissociated.
 * Consumes `self` and `sym` arguments.
 */
void Namespace_unmap(Namespace *self, Symbol *sym) {
  if (sym->ns != NULL) {
    Ptr_release(self); Ptr_release(sym);
    throwIllegalArgumentException_C("Can't unmap namespace-qualified symbol");
  }
  RTValue symVal = RT_boxSymbol((Object *)sym);
  
  while (true) {
    RTValue mapVal = atomic_load_explicit(&self->mappings, memory_order_acquire);
    PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(mapVal);
    
    Ptr_retain(map); retain(symVal);
    RTValue existing = PersistentArrayMap_get(map, symVal);
    if (RT_isNil(existing)) {
      Ptr_release(self); Ptr_release(sym);
      return;
    }
    release(existing);
    
    Ptr_retain(map); retain(symVal);
    PersistentArrayMap *newMap = PersistentArrayMap_dissoc(map, symVal);
    RTValue newMapVal = RT_boxPtr((Object *)newMap);
    
    if (atomic_compare_exchange_strong_explicit(&self->mappings, &mapVal, newMapVal, memory_order_acq_rel, memory_order_acquire)) {
      autorelease(mapVal);
      Ptr_release(self); Ptr_release(sym);
      return;
    } else {
      release(newMapVal);
    }
  }
}

/*
 * Imports a Class into the namespace under the given symbol, making it accessible.
 * Wraps `Namespace_referenceClass` and unboxes the result.
 * Consumes `self`, `sym`, and `c` arguments.
 */
Class *Namespace_importClassSym(Namespace *self, Symbol *sym, Class *c) {
  return (Class *)RT_unboxPtr(Namespace_referenceClass(self, sym, c));
}

/*
 * Refers a symbol to a specific Var object in this namespace.
 * This is typically used to import functions or values from other namespaces.
 * Consumes `self`, `sym`, and `var` arguments.
 */
Var *Namespace_refer(Namespace *self, Symbol *sym, Var *var) {
  return (Var *)RT_unboxPtr(Namespace_reference(self, sym, RT_boxPtr((Object *)var)));
}

/*
 * Looks up and retrieves the mapping for the given symbol name in this namespace.
 * Returns the mapped value with a retained (+1) refcount, or nil if not found.
 * Consumes `self` and `name` arguments.
 */
RTValue Namespace_getMapping(Namespace *self, Symbol *name) {
  RTValue mapVal = atomic_load_explicit(&self->mappings, memory_order_acquire);
  Ptr_retain(RT_unboxPtr(mapVal)); Ptr_retain(name);
  RTValue res = PersistentArrayMap_get((PersistentArrayMap *)RT_unboxPtr(mapVal), RT_boxSymbol((Object *)name));
  Ptr_release(self); Ptr_release(name);
  return res;
}

/*
 * Searches for a Var that is explicitly interned within this namespace under the given symbol.
 * Returns the retained Var (+1 refcount) if found and owned by this namespace, or NULL otherwise.
 * Consumes `self` and `symbol` arguments.
 */
Var *Namespace_findInternedVar(Namespace *self, Symbol *symbol) {
  Ptr_retain(self); Ptr_retain(symbol);
  RTValue o = Namespace_getMapping(self, symbol);
  if (!RT_isNil(o) && getType(o) == varType) {
    Var *v = (Var *)RT_unboxPtr(o);
    if (v->ns == self) {
      Ptr_release(self); Ptr_release(symbol);
      // we already retained `o` inside getMapping. It returns +1 to caller.
      return v; 
    }
  }
  release(o);
  Ptr_release(self); Ptr_release(symbol);
  return NULL;
}

/*
 * Resolves an alias symbol to its corresponding Namespace object.
 * Returns the mapped Namespace pointer with a retained (+1) refcount, or NULL if not found.
 * Consumes `self` and `alias` arguments.
 */
Namespace *Namespace_lookupAlias(Namespace *self, Symbol *alias) {
  RTValue mapVal = atomic_load_explicit(&self->aliases, memory_order_acquire);
  Ptr_retain(RT_unboxPtr(mapVal)); Ptr_retain(alias);
  RTValue res = PersistentArrayMap_get((PersistentArrayMap *)RT_unboxPtr(mapVal), RT_boxSymbol((Object *)alias));
  Namespace *ns = NULL;
  if (!RT_isNil(res)) {
    ns = (Namespace *)RT_unboxPtr(res);
  }
  Ptr_release(self); Ptr_release(alias);
  return ns; 
}

/*
 * Registers a namespace alias, linking the given symbol to the target Namespace `ns`.
 * Throws an exception if the alias is already mapped to a different namespace.
 * Uses lock-free atomic CAS to update the aliases map.
 * Consumes `self`, `alias`, and `ns` arguments.
 */
void Namespace_addAlias(Namespace *self, Symbol *alias, Namespace *ns) {
  if (alias == NULL || ns == NULL) {
    if(alias) Ptr_release(alias);
    if(ns) Ptr_release(ns);
    Ptr_release(self);
    throwIllegalArgumentException_C("Null pointer passed");
  }
  if (alias->ns != NULL) {
    Ptr_release(alias); Ptr_release(ns); Ptr_release(self);
    throwIllegalArgumentException_C("Cannot alias namespace-qualified symbol");
  }
  
  RTValue aliasVal = RT_boxSymbol((Object *)alias);
  RTValue nsVal = RT_boxPtr((Object *)ns);
  
  while (true) {
    RTValue mapVal = atomic_load_explicit(&self->aliases, memory_order_acquire);
    PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(mapVal);
    
    Ptr_retain(map); retain(aliasVal);
    RTValue oldNs = PersistentArrayMap_get(map, aliasVal);
    if (!RT_isNil(oldNs)) {
      if (!equals(oldNs, nsVal)) {
        Ptr_release(alias); Ptr_release(ns); Ptr_release(self);
        release(oldNs);
        throwIllegalStateException_C("Alias already exists");
      }
      Ptr_release(alias); Ptr_release(ns); Ptr_release(self);
      release(oldNs);
      return;
    }
    
    Ptr_retain(map); retain(aliasVal); retain(nsVal);
    PersistentArrayMap *newMap = PersistentArrayMap_assoc(map, aliasVal, nsVal);
    RTValue newMapVal = RT_boxPtr((Object *)newMap);
    
    if (atomic_compare_exchange_strong_explicit(&self->aliases, &mapVal, newMapVal, memory_order_acq_rel, memory_order_acquire)) {
      autorelease(mapVal);
      Ptr_release(alias); Ptr_release(ns); Ptr_release(self);
      return;
    } else {
      release(newMapVal);
    }
  }
}

/*
 * Removes a previously registered namespace alias from the aliases dictionary.
 * Uses lock-free atomic CAS to replace the aliases map with the alias dissociated.
 * Consumes `self` and `alias` arguments.
 */
void Namespace_removeAlias(Namespace *self, Symbol *alias) {
  if (alias->ns != NULL) {
    Ptr_release(self); Ptr_release(alias);
    throwIllegalArgumentException_C("Cannot alias namespace-qualified symbol");
  }
  RTValue aliasVal = RT_boxSymbol((Object *)alias);
  
  while (true) {
    RTValue mapVal = atomic_load_explicit(&self->aliases, memory_order_acquire);
    PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(mapVal);
    
    Ptr_retain(map); retain(aliasVal);
    RTValue existing = PersistentArrayMap_get(map, aliasVal);
    if (RT_isNil(existing)) {
      Ptr_release(self); Ptr_release(alias);
      return;
    }
    release(existing);
    
    Ptr_retain(map); retain(aliasVal);
    PersistentArrayMap *newMap = PersistentArrayMap_dissoc(map, aliasVal);
    RTValue newMapVal = RT_boxPtr((Object *)newMap);
    
    if (atomic_compare_exchange_strong_explicit(&self->aliases, &mapVal, newMapVal, memory_order_acq_rel, memory_order_acquire)) {
      autorelease(mapVal);
      Ptr_release(self); Ptr_release(alias);
      return;
    } else {
      release(newMapVal);
    }
  }
}
