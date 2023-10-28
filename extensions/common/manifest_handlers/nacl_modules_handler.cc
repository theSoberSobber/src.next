// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/nacl_modules_handler.h"

#include <stddef.h>

#include <memory>

#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

struct NaClModuleData : Extension::ManifestData {
  // Optional list of NaCl modules and associated properties.
  NaClModuleInfo::List nacl_modules_;
};

}  // namespace

// static
const NaClModuleInfo::List* NaClModuleInfo::GetNaClModules(
    const Extension* extension) {
  NaClModuleData* data = static_cast<NaClModuleData*>(
      extension->GetManifestData(keys::kNaClModules));
  return data ? &data->nacl_modules_ : NULL;
}

NaClModulesHandler::NaClModulesHandler() {
}

NaClModulesHandler::~NaClModulesHandler() {
}

bool NaClModulesHandler::Parse(Extension* extension, std::u16string* error) {
  const base::Value* list_value = nullptr;
  if (!extension->manifest()->GetList(keys::kNaClModules, &list_value)) {
    *error = base::ASCIIToUTF16(errors::kInvalidNaClModules);
    return false;
  }

  std::unique_ptr<NaClModuleData> nacl_module_data(new NaClModuleData);

  base::Value::ConstListView list_view = list_value->GetList();
  for (size_t i = 0; i < list_view.size(); ++i) {
    if (!list_view[i].is_dict()) {
      *error = base::ASCIIToUTF16(errors::kInvalidNaClModules);
      return false;
    }

    // Get nacl_modules[i].path.
    const base::Value* path_str = list_view[i].FindKeyOfType(
        keys::kNaClModulesPath, base::Value::Type::STRING);
    if (path_str == nullptr) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidNaClModulesPath, base::NumberToString(i));
      return false;
    }

    // Get nacl_modules[i].mime_type.
    const base::Value* mime_type = list_view[i].FindKeyOfType(
        keys::kNaClModulesMIMEType, base::Value::Type::STRING);
    if (mime_type == nullptr) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidNaClModulesMIMEType, base::NumberToString(i));
      return false;
    }

    nacl_module_data->nacl_modules_.push_back(NaClModuleInfo());
    nacl_module_data->nacl_modules_.back().url =
        extension->GetResourceURL(path_str->GetString());
    nacl_module_data->nacl_modules_.back().mime_type = mime_type->GetString();
  }

  extension->SetManifestData(keys::kNaClModules, std::move(nacl_module_data));
  return true;
}

base::span<const char* const> NaClModulesHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kNaClModules};
  return kKeys;
}

}  // namespace extensions
