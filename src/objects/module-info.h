// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MODULE_INFO_H_
#define V8_OBJECTS_MODULE_INFO_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;
class Isolate;
class ModuleDescriptor;
class ModuleInfoEntry;
class String;
class Zone;

// ModuleInfo is to ModuleDescriptor what ScopeInfo is to Scope.
class ModuleInfo : public FixedArray {
 public:
  DECLARE_CAST(ModuleInfo)

  static Handle<ModuleInfo> New(Isolate* isolate, Zone* zone,
                                ModuleDescriptor* descr);

  inline FixedArray* module_requests() const {
    return FixedArray::cast(get(kModuleRequestsIndex));
  }

  inline FixedArray* special_exports() const {
    return FixedArray::cast(get(kSpecialExportsIndex));
  }

  inline FixedArray* regular_exports() const {
    return FixedArray::cast(get(kRegularExportsIndex));
  }

  inline FixedArray* regular_imports() const {
    return FixedArray::cast(get(kRegularImportsIndex));
  }

  inline FixedArray* namespace_imports() const {
    return FixedArray::cast(get(kNamespaceImportsIndex));
  }

  // Accessors for [regular_exports].
  int RegularExportCount() const;
  String* RegularExportLocalName(int i) const;
  int RegularExportCellIndex(int i) const;
  FixedArray* RegularExportExportNames(int i) const;

  static Handle<ModuleInfoEntry> LookupRegularImport(Handle<ModuleInfo> info,
                                                     Handle<String> local_name);

#ifdef DEBUG
  inline bool Equals(ModuleInfo* other) const {
    return regular_exports() == other->regular_exports() &&
           regular_imports() == other->regular_imports() &&
           special_exports() == other->special_exports() &&
           namespace_imports() == other->namespace_imports();
  }
#endif

 private:
  friend class Factory;
  friend class ModuleDescriptor;
  enum {
    kModuleRequestsIndex,
    kSpecialExportsIndex,
    kRegularExportsIndex,
    kNamespaceImportsIndex,
    kRegularImportsIndex,
    kLength
  };
  enum {
    kRegularExportLocalNameOffset,
    kRegularExportCellIndexOffset,
    kRegularExportExportNamesOffset,
    kRegularExportLength
  };
  DISALLOW_IMPLICIT_CONSTRUCTORS(ModuleInfo);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MODULE_INFO_H_
