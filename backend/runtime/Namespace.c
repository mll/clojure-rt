#include "Namespace.h"
#include "ConcurrentHashMap.h"
#include "Ebr.h"
#include "Exceptions.h"
#include "Object.h"
#include "ObjectProto.h"
#include "PersistentArrayMap.h"
#include "RTValue.h"
#include "String.h"
#include "Symbol.h"
#include "Var.h"
#include <stdio.h>

extern ConcurrentHashMap *namespaces;

/*
 * Allocates and initializes a new Namespace object with the given symbol as its
 * name. Initializes the mappings and aliases to empty PersistentArrayMaps.
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
  retain(emptyMap); // Retain once more for the second pointer (aliases), making
                    // refcount 2

  return self;
}

/*
 * Searches for a Namespace with the given name symbol in the global register.
 * Returns the retained Namespace (+1 refcount) if found, or NULL otherwise.
 * Consumes `name` argument.
 */
RTValue Namespace_find(Symbol *name) {
  RTValue symVal = RT_boxSymbol((Object *)name);
  return ConcurrentHashMap_get_preservesSelf(namespaces, symVal);
}

/*
 * Searches for a Namespace with the given name symbol in the global register.
 * If not found, a new Namespace is created and registered.
 * Returns the Namespace with a +1 refcount.
 * Consumes `name` argument.
 */
Namespace *Namespace_findOrCreate(Symbol *name) {
  Ptr_retain(name);
  RTValue nsVal = Namespace_find(name);
  if (!RT_isNil(nsVal)) {
    Ptr_release(name); // Consume argument
    Namespace *ns = (Namespace *)RT_unboxPtr(nsVal);
    return ns;
  }
  Ptr_retain(name);
  Namespace *newNs = Namespace_create(name);

  Ptr_retain(newNs);
  RTValue existing = ConcurrentHashMap_putIfAbsent_preservesSelf(
      namespaces, RT_boxPtr(name), RT_boxPtr(newNs), false);
  if (!RT_isNil(existing)) {
    Ptr_release(newNs);
    return (Namespace *)RT_unboxPtr(existing);
  }

  return newNs;
}

/*
 * Returns the name symbol of the Namespace.
 * Consumes the `self` argument.
 */
Symbol *Namespace_getName(Namespace *self) {
  Symbol *name = self->name;
  Ptr_retain(name);
  Ptr_release(self);
  return name;
}

/*
 * Destroys the Namespace object. If `deallocateChildren` is true, it releases
 * the retained references to the namespace name, mappings, and aliases maps.
 */
void Namespace_destroy(Namespace *self, bool deallocateChildren) {
  if (deallocateChildren) {
    if (self->name) {
      Object_release((Object *)self->name);
    }
    RTValue mappings =
        atomic_load_explicit(&self->mappings, memory_order_acquire);
    autorelease(mappings);

    RTValue aliases =
        atomic_load_explicit(&self->aliases, memory_order_acquire);
    autorelease(aliases);
  }
}

/*
 * Returns the hash code of the namespace, which is equivalent to the hash code
 * of its name symbol.
 */
uword_t Namespace_hash(Namespace *self) { return Symbol_hash(self->name); }

/*
 * Checks equality between two namespaces by comparing their name symbols.
 */
bool Namespace_equals(Namespace *self, Namespace *other) {
  return Symbol_equals(self->name, other->name);
}

/*
 * Generates a string representation of the namespace in the format
 * `#namespace[name]`. Consumes the `self` argument and returns a new String.
 */
String *Namespace_toString(Namespace *self) {
  Ptr_retain(self->name);
  String *nameStr = toString(RT_boxPtr(self->name));
  String *prefix = String_create("#namespace[");
  String *suffix = String_create("]");

  String *res1 = String_concat(prefix, nameStr);
  String *res2 = String_concat(res1, suffix);
  Ptr_release(self);
  return res2;
}

/*
 * Promotes the namespace and its children (name, mappings, aliases) to shared
 * state, allowing safe concurrent access across threads.
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
bool Namespace_isInternedMapping(Namespace *self, Symbol *sym, RTValue var) {
  bool res = false;
  if (getType(var) == varType) {
    Var *v = (Var *)RT_unboxPtr(var);
    res = (v->ns == self) && Symbol_equals(v->sym, sym);
  }
  Ptr_release(self);
  Ptr_release(sym);
  release(var);
  return res;
}

/*
 * Interns a symbol in this namespace, establishing a new Var if one does not
 * exist, or returning the existing Var if already mapped.
 *
 * "Interning" in this context refers to the process of associating a symbol
 * with a canonical, globally unique Var instance within a namespace. This
 * ensures that all references to the same symbol in this namespace resolve to
 * the exact same memory location and share the same root binding and metadata.
 *
 * Performs a lock-free atomic compare-and-swap (CAS) to update the mappings
 * map. Uses Epoch-Based Reclamation (RCU-style) `autorelease` for replacing old
 * maps. Consumes both `self` and `sym` arguments, and returns a retained Var
 * (+1 refcount).
 */
Var *Namespace_intern(Namespace *self, Symbol *sym) {
  if (sym->ns != NULL) {
    Ptr_release(self);
    Ptr_release(sym);
    throwIllegalArgumentException_C("Can't intern namespace-qualified symbol");
  }

  RTValue mapVal = atomic_load_explicit(&self->mappings, memory_order_acquire);
  PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(mapVal); // 0

  Var *v = NULL;

  Ptr_retain(map); // 1
  Ptr_retain(sym);
  RTValue o = PersistentArrayMap_get(map, RT_boxPtr(sym)); // 0

  while (RT_isNil(o)) {
    if (v == NULL) {
      Ptr_retain(self);
      Ptr_retain(sym);
      v = Var_create_interned(self, sym);
    }

    Ptr_retain(v);
    Ptr_retain(map); // 1
    Ptr_retain(sym);
    PersistentArrayMap *newMap =
        PersistentArrayMap_assoc(map, RT_boxPtr(sym), RT_boxPtr(v)); // 0

    bool replaced = atomic_compare_exchange_strong_explicit(
        &self->mappings, &mapVal, RT_boxPtr(newMap), memory_order_acq_rel,
        memory_order_acquire);
    if (replaced) {
      autorelease(mapVal); // -1
    } else {
      Ptr_release(newMap); // 0
    }
    mapVal = atomic_load_explicit(&self->mappings, memory_order_acquire);
    map = (PersistentArrayMap *)RT_unboxPtr(mapVal); // 0

    Ptr_retain(map); // 1
    Ptr_retain(sym);
    o = PersistentArrayMap_get(map, RT_boxPtr(sym)); // 0
  }

  Ptr_retain(self);
  Ptr_retain(sym);
  retain(o);
  if (Namespace_isInternedMapping(self, sym, o)) {
    Ptr_release(self);
    Ptr_release(sym);
    if (v != NULL) {
      Ptr_release(v);
    }
    return (Var *)RT_unboxPtr(o);
  }

  if (v == NULL) {
    Ptr_retain(self);
    Ptr_retain(sym);
    v = Var_create_interned(self, sym);
  }

  Ptr_retain(self);
  Ptr_retain(sym);
  retain(o);
  Ptr_retain(v);
  if (Namespace_checkReplacement(self, sym, o, RT_boxPtr(v))) {
    bool replaced = false;
    while (!replaced) {
      Ptr_retain(map); // 1
      Ptr_retain(sym);
      Ptr_retain(v);
      PersistentArrayMap *newMap =
          PersistentArrayMap_assoc(map, RT_boxPtr(sym), RT_boxPtr(v)); // 0
      printf("Trying to replace %lld with %lld\n", RT_boxPtr(map),
             RT_boxPtr(newMap));

      replaced = atomic_compare_exchange_strong_explicit(
          &self->mappings, &mapVal, RT_boxPtr(newMap), memory_order_acq_rel,
          memory_order_acquire);
      if (replaced) {
        autorelease(mapVal); // -1
      } else {
        Ptr_release(newMap); // 0
      }
      mapVal = atomic_load_explicit(&self->mappings, memory_order_acquire);
      map = (PersistentArrayMap *)RT_unboxPtr(mapVal); // 0
    }

    Ptr_release(self);
    Ptr_release(sym);
    release(o);
    return v;
  }
  Ptr_release(self);
  Ptr_release(sym);

  if (v != NULL) {
    Ptr_release(v);
  }
  return (Var *)RT_unboxPtr(o);
}

/*
 This method checks if a namespace's mapping is applicable and warns on
problematic cases. It will return a boolean indicating if a mapping is
replaceable. The semantics of what constitutes a legal replacement mapping is
summarized as follows:

| classification | in namespace ns        | newval = anything other than ns/name
| newval = ns/name                    |
|----------------+------------------------+--------------------------------------+-------------------------------------|
| native mapping | name -> ns/name        | no replace, warn-if newval not-core
| no replace, warn-if newval not-core | | alias mapping  | name ->
other/whatever | warn + replace                       | warn + replace |
*/

bool Namespace_checkReplacement(Namespace *self, Symbol *sym, RTValue old,
                                RTValue neu) {

  if (getType(old) == varType) {
    // Namespace *ons = ((Var *)RT_unboxPtr(old))->ns;
    Namespace *nns =
        (getType(neu) == varType) ? ((Var *)RT_unboxPtr(neu))->ns : NULL;

    String *s = String_create("clojure.core");
    bool isCore = (nns != NULL) && String_equals(nns->name->name, s);
    Ptr_release(s);

    if (neu) {
      release(neu);
    }

    if (Namespace_isInternedMapping(self, sym, old)) {
      if (!isCore) {
        // TODO: print to screen
        // RT.errPrintWriter().println(
        //     "REJECTED: attempt to replace interned var " + old + " with " +
        //     neu + " in " + name + ", you must ns-unmap first");
        return false;
      } else
        return false;
    }
  } else {
    release(old);
    Ptr_release(sym);
    Ptr_release(self);
    release(neu);
  }

  // TODO: print to screen
  // RT.errPrintWriter().println(
  //     "WARNING: " + sym + " already refers to: " + old +
  //     " in namespace: " + name + ", being replaced by: " + neu);
  return true;
}

/*
 * Establishes a reference (mapping) from the symbol `sym` to the given value
 * `val` within this namespace. Throws an exception if the symbol is already
 * mapped to an interned Var in this namespace. Uses lock-free atomic updates
 * (CAS) for map replacement. Consumes `self`, `sym`, and `val` arguments. The
 * returned value is retained (+1 refcount).
 */
RTValue Namespace_reference(Namespace *self, Symbol *sym, RTValue val) {
  if (sym->ns != NULL) {
    Ptr_release(self);
    Ptr_release(sym);
    release(val);
    throwIllegalArgumentException_C("Can't intern namespace-qualified symbol");
  }

  RTValue symVal = RT_boxSymbol((Object *)sym);

  while (true) {
    RTValue mapVal =
        atomic_load_explicit(&self->mappings, memory_order_acquire);
    PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(mapVal);

    Ptr_retain(map);
    retain(symVal);
    RTValue o = PersistentArrayMap_get(map, symVal);
    if (!RT_isNil(o)) {
      if (equals(o, val)) {
        Ptr_release(self);
        Ptr_release(sym);
        release(val);
        return o;
      }
      Ptr_retain(self);
      Ptr_retain(sym);
      retain(o);
      retain(val);
      if (!Namespace_checkReplacement(self, sym, o, val)) {
        retain(o);
        String *oStr = toString(o);

        Ptr_retain(oStr);
        String *flatOStr = String_compactify(oStr);

        char buf[512];
        snprintf(buf, sizeof(buf), "%s already refers to: %s in namespace: %s",
                 String_c_str(sym->name), String_c_str(flatOStr),
                 String_c_str(self->name->name));

        Ptr_release(flatOStr);
        Ptr_release(oStr);

        release(o);
        release(val);
        Ptr_release(sym);
        Ptr_release(self);
        release(o);
        Ptr_release(self);
        Ptr_release(sym);
        release(val);
        throwIllegalStateException_C(buf);
      }
      release(o);
    }

    Ptr_retain(map);
    retain(symVal);
    retain(val);
    PersistentArrayMap *newMap = PersistentArrayMap_assoc(map, symVal, val);
    RTValue newMapVal = RT_boxPtr((Object *)newMap);

    if (atomic_compare_exchange_strong_explicit(&self->mappings, &mapVal,
                                                newMapVal, memory_order_acq_rel,
                                                memory_order_acquire)) {
      autorelease(mapVal);
      Ptr_release(self);
      Ptr_release(sym);
      return val;
    } else {
      release(newMapVal);
    }
  }
}

/*
 * A convenience wrapper for `Namespace_reference` specifically designed to
 * map a symbol to a Class. Consumes `self`, `sym`, and `val`.
 */
RTValue Namespace_referenceClass(Namespace *self, Symbol *sym, Class *val) {
  // val is a Class pointer, it consumes val.
  // Namespace_reference consumes self, sym, and RTValue. So we don't need to
  // release self/sym here.
  return Namespace_reference(self, sym, RT_boxPtr((Object *)val));
}

/*
 * Removes the mapping for the given symbol from the namespace's mappings.
 * Uses lock-free atomic CAS to replace the mappings map with the symbol
 * dissociated. Consumes `self` and `sym` arguments.
 */
void Namespace_unmap(Namespace *self, Symbol *sym) {
  if (sym->ns != NULL) {
    Ptr_release(self);
    Ptr_release(sym);
    throwIllegalArgumentException_C("Can't unmap namespace-qualified symbol");
  }
  RTValue symVal = RT_boxSymbol((Object *)sym);

  while (true) {
    RTValue mapVal =
        atomic_load_explicit(&self->mappings, memory_order_acquire);
    PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(mapVal);

    Ptr_retain(map);
    retain(symVal);
    RTValue existing = PersistentArrayMap_get(map, symVal);
    if (RT_isNil(existing)) {
      Ptr_release(self);
      Ptr_release(sym);
      return;
    }
    release(existing);

    Ptr_retain(map);
    retain(symVal);
    PersistentArrayMap *newMap = PersistentArrayMap_dissoc(map, symVal);
    RTValue newMapVal = RT_boxPtr((Object *)newMap);

    if (atomic_compare_exchange_strong_explicit(&self->mappings, &mapVal,
                                                newMapVal, memory_order_acq_rel,
                                                memory_order_acquire)) {
      autorelease(mapVal);
      Ptr_release(self);
      Ptr_release(sym);
      return;
    } else {
      release(newMapVal);
    }
  }
}

/*
 * Imports a Class into the namespace under the given symbol, making it
 * accessible. Wraps `Namespace_referenceClass` and unboxes the result.
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
  return (Var *)RT_unboxPtr(
      Namespace_reference(self, sym, RT_boxPtr((Object *)var)));
}

/*
 * Looks up and retrieves the mapping for the given symbol name in this
 * namespace. Returns the mapped value with a retained (+1) refcount, or nil
 * if not found. Consumes `self` and `name` arguments.
 */
RTValue Namespace_getMapping(Namespace *self, Symbol *name) {
  RTValue mapVal = atomic_load_explicit(&self->mappings, memory_order_acquire);
  Ptr_retain(RT_unboxPtr(mapVal));
  Ptr_retain(name);
  RTValue res = PersistentArrayMap_get(
      (PersistentArrayMap *)RT_unboxPtr(mapVal), RT_boxSymbol((Object *)name));
  Ptr_release(self);
  Ptr_release(name);
  return res;
}

/*
 * Searches for a Var that is explicitly interned within this namespace under
 * the given symbol. Returns the retained Var (+1 refcount) if found and owned
 * by this namespace, or NULL otherwise. Consumes `self` and `symbol`
 * arguments.
 */
RTValue Namespace_findInternedVar(Namespace *self, Symbol *symbol) {
  Ptr_retain(self);
  Ptr_retain(symbol);
  RTValue o = Namespace_getMapping(self, symbol);
  if (!RT_isNil(o) && getType(o) == varType) {
    Var *v = (Var *)RT_unboxPtr(o);
    if (v->ns == self) {
      Ptr_release(self);
      Ptr_release(symbol);
      // we already retained `o` inside getMapping. It returns +1 to caller.
      return o;
    }
  }
  release(o);
  Ptr_release(self);
  Ptr_release(symbol);
  return RT_boxNil();
}

/*
 * Resolves an alias symbol to its corresponding Namespace object.
 * Returns the mapped Namespace pointer as RTValue with a retained (+1)
 * refcount, or RT_boxNil() if not found. Consumes `self` and `alias`
 * arguments.
 */
RTValue Namespace_lookupAlias(Namespace *self, Symbol *alias) {
  RTValue mapVal = atomic_load_explicit(&self->aliases, memory_order_acquire);
  Ptr_retain(RT_unboxPtr(mapVal));
  Ptr_release(self);
  RTValue res = PersistentArrayMap_get(
      (PersistentArrayMap *)RT_unboxPtr(mapVal), RT_boxSymbol((Object *)alias));
  return res;
}

/*
 * Registers a namespace alias, linking the given symbol to the target
 * Namespace `ns`. Throws an exception if the alias is already mapped to a
 * different namespace. Uses lock-free atomic CAS to update the aliases map.
 * Consumes `self`, `alias`, and `ns` arguments.
 */
void Namespace_addAlias(Namespace *self, Symbol *alias, Namespace *ns) {
  if (alias == NULL || ns == NULL) {
    if (alias)
      Ptr_release(alias);
    if (ns)
      Ptr_release(ns);
    Ptr_release(self);
    throwIllegalArgumentException_C("Null pointer passed");
  }
  if (alias->ns != NULL) {
    Ptr_release(alias);
    Ptr_release(ns);
    Ptr_release(self);
    throwIllegalArgumentException_C("Cannot alias namespace-qualified symbol");
  }

  RTValue aliasVal = RT_boxSymbol((Object *)alias);
  RTValue nsVal = RT_boxPtr((Object *)ns);

  while (true) {
    RTValue mapVal = atomic_load_explicit(&self->aliases, memory_order_acquire);
    PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(mapVal);

    Ptr_retain(map);
    retain(aliasVal);
    RTValue oldNs = PersistentArrayMap_get(map, aliasVal);
    if (!RT_isNil(oldNs)) {
      if (!equals(oldNs, nsVal)) {
        Ptr_release(alias);
        Ptr_release(ns);
        Ptr_release(self);
        release(oldNs);
        throwIllegalStateException_C("Alias already exists");
      }
      Ptr_release(alias);
      Ptr_release(ns);
      Ptr_release(self);
      release(oldNs);
      return;
    }

    Ptr_retain(map);
    retain(aliasVal);
    retain(nsVal);
    PersistentArrayMap *newMap = PersistentArrayMap_assoc(map, aliasVal, nsVal);
    RTValue newMapVal = RT_boxPtr((Object *)newMap);

    if (atomic_compare_exchange_strong_explicit(&self->aliases, &mapVal,
                                                newMapVal, memory_order_acq_rel,
                                                memory_order_acquire)) {
      autorelease(mapVal);
      Ptr_release(alias);
      Ptr_release(ns);
      Ptr_release(self);
      return;
    } else {
      release(newMapVal);
    }
  }
}

/*
 * Removes a previously registered namespace alias from the aliases
 * dictionary. Uses lock-free atomic CAS to replace the aliases map with the
 * alias dissociated. Consumes `self` and `alias` arguments.
 */
void Namespace_removeAlias(Namespace *self, Symbol *alias) {
  if (alias->ns != NULL) {
    Ptr_release(self);
    Ptr_release(alias);
    throwIllegalArgumentException_C("Cannot alias namespace-qualified symbol");
  }
  RTValue aliasVal = RT_boxSymbol((Object *)alias);

  while (true) {
    RTValue mapVal = atomic_load_explicit(&self->aliases, memory_order_acquire);
    PersistentArrayMap *map = (PersistentArrayMap *)RT_unboxPtr(mapVal);

    Ptr_retain(map);
    retain(aliasVal);
    RTValue existing = PersistentArrayMap_get(map, aliasVal);
    if (RT_isNil(existing)) {
      Ptr_release(self);
      Ptr_release(alias);
      return;
    }
    release(existing);

    Ptr_retain(map);
    retain(aliasVal);
    PersistentArrayMap *newMap = PersistentArrayMap_dissoc(map, aliasVal);
    RTValue newMapVal = RT_boxPtr((Object *)newMap);

    if (atomic_compare_exchange_strong_explicit(&self->aliases, &mapVal,
                                                newMapVal, memory_order_acq_rel,
                                                memory_order_acquire)) {
      autorelease(mapVal);
      Ptr_release(self);
      Ptr_release(alias);
      return;
    } else {
      release(newMapVal);
    }
  }
}

RTValue RT_inNs(__attribute__((swift_context)) struct ExecutionContext *ctx,
                Symbol *nsName) __attribute__((swiftcall)) {
  Namespace *ns = Namespace_findOrCreate(nsName);
  RTValue coreNsVal =
      Namespace_find(Symbol_create(String_create("clojure.core")));
  assert(!RT_isNil(coreNsVal) && "clojure.core namespace not found");
  Namespace *coreNs = (Namespace *)RT_unboxPtr(coreNsVal);

  RTValue nsVarVal =
      Namespace_getMapping(coreNs, Symbol_create(String_create("*ns*")));
  assert(!RT_isNil(nsVarVal) && "clojure.core/*ns* Var not found");
  Var *nsVar = (Var *)RT_unboxPtr(nsVarVal);

  return Var_set(ctx, nsVar, RT_boxPtr(ns));
}
