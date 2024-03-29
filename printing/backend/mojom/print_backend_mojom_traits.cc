// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/mojom/print_backend_mojom_traits.h"

#include <map>

#include "base/logging.h"
#include "build/chromeos_buildflags.h"
#include "ui/gfx/geometry/mojom/geometry.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"

// Implementations of std::less<> here are for purposes of detecting duplicate
// entries in arrays.  They do not require strict checks of all fields, but
// instead focus on identifying attributes that would be used to clearly
// distinguish properties to a user.  E.g., if two entries have the same
// displayable name but different corresponding values, consider that to be a
// duplicate for these purposes.
namespace std {

template <>
struct less<::gfx::Size> {
  bool operator()(const ::gfx::Size& lhs, const ::gfx::Size& rhs) const {
    if (lhs.width() < rhs.width())
      return true;
    return lhs.height() < rhs.height();
  }
};

template <>
struct less<::printing::PrinterSemanticCapsAndDefaults::Paper> {
  bool operator()(
      const ::printing::PrinterSemanticCapsAndDefaults::Paper& lhs,
      const ::printing::PrinterSemanticCapsAndDefaults::Paper& rhs) const {
    if (lhs.display_name < rhs.display_name)
      return true;
    return lhs.vendor_id < rhs.vendor_id;
  }
};

#if defined(OS_CHROMEOS)
template <>
struct less<::printing::AdvancedCapability> {
  bool operator()(const ::printing::AdvancedCapability& lhs,
                  const ::printing::AdvancedCapability& rhs) const {
    if (lhs.name < rhs.name)
      return true;
    return lhs.display_name < rhs.display_name;
  }
};
#endif  // defined(OS_CHROMEOS)

}  // namespace std

namespace mojo {

namespace {
template <class Key>
class DuplicateChecker {
 public:
  bool HasDuplicates(const std::vector<Key>& items) {
    std::map<Key, bool> items_encountered;
    for (auto it = items.begin(); it != items.end(); ++it) {
      auto found = items_encountered.find(*it);
      if (found != items_encountered.end())
        return true;
      items_encountered[*it] = true;
    }
    return false;
  }
};

}  // namespace

// static
bool StructTraits<printing::mojom::PrinterBasicInfoDataView,
                  printing::PrinterBasicInfo>::
    Read(printing::mojom::PrinterBasicInfoDataView data,
         printing::PrinterBasicInfo* out) {
  if (!data.ReadPrinterName(&out->printer_name) ||
      !data.ReadDisplayName(&out->display_name) ||
      !data.ReadPrinterDescription(&out->printer_description)) {
    return false;
  }
  out->printer_status = data.printer_status();
  out->is_default = data.is_default();
  if (!data.ReadOptions(&out->options))
    return false;

  // There should be a non-empty value for `printer_name` since it needs to
  // uniquely identify the printer with the operating system among multiple
  // possible destinations.
  if (out->printer_name.empty()) {
    DLOG(ERROR) << "The printer name must not be empty.";
    return false;
  }
  // There should be a non-empty value for `display_name` since it needs to
  // uniquely identify the printer in user dialogs among multiple possible
  // destinations.
  if (out->display_name.empty()) {
    DLOG(ERROR) << "The printer's display name must not be empty.";
    return false;
  }

  return true;
}

// static
bool StructTraits<printing::mojom::PaperDataView,
                  printing::PrinterSemanticCapsAndDefaults::Paper>::
    Read(printing::mojom::PaperDataView data,
         printing::PrinterSemanticCapsAndDefaults::Paper* out) {
  return data.ReadDisplayName(&out->display_name) &&
         data.ReadVendorId(&out->vendor_id) && data.ReadSizeUm(&out->size_um);
}

#if defined(OS_CHROMEOS)
// static
printing::mojom::AdvancedCapabilityType
EnumTraits<printing::mojom::AdvancedCapabilityType,
           ::printing::AdvancedCapability::Type>::
    ToMojom(::printing::AdvancedCapability::Type input) {
  switch (input) {
    case ::printing::AdvancedCapability::Type::kBoolean:
      return printing::mojom::AdvancedCapabilityType::kBoolean;
    case ::printing::AdvancedCapability::Type::kFloat:
      return printing::mojom::AdvancedCapabilityType::kFloat;
    case ::printing::AdvancedCapability::Type::kInteger:
      return printing::mojom::AdvancedCapabilityType::kInteger;
    case ::printing::AdvancedCapability::Type::kString:
      return printing::mojom::AdvancedCapabilityType::kString;
  }
  NOTREACHED();
  return printing::mojom::AdvancedCapabilityType::kString;
}

// static
bool EnumTraits<printing::mojom::AdvancedCapabilityType,
                ::printing::AdvancedCapability::Type>::
    FromMojom(printing::mojom::AdvancedCapabilityType input,
              ::printing::AdvancedCapability::Type* output) {
  switch (input) {
    case printing::mojom::AdvancedCapabilityType::kBoolean:
      *output = ::printing::AdvancedCapability::Type::kBoolean;
      return true;
    case printing::mojom::AdvancedCapabilityType::kFloat:
      *output = ::printing::AdvancedCapability::Type::kFloat;
      return true;
    case printing::mojom::AdvancedCapabilityType::kInteger:
      *output = ::printing::AdvancedCapability::Type::kInteger;
      return true;
    case printing::mojom::AdvancedCapabilityType::kString:
      *output = ::printing::AdvancedCapability::Type::kString;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<printing::mojom::AdvancedCapabilityValueDataView,
                  ::printing::AdvancedCapabilityValue>::
    Read(printing::mojom::AdvancedCapabilityValueDataView data,
         ::printing::AdvancedCapabilityValue* out) {
  return data.ReadName(&out->name) && data.ReadDisplayName(&out->display_name);
}

// static
bool StructTraits<printing::mojom::AdvancedCapabilityDataView,
                  ::printing::AdvancedCapability>::
    Read(printing::mojom::AdvancedCapabilityDataView data,
         ::printing::AdvancedCapability* out) {
  return data.ReadName(&out->name) &&
         data.ReadDisplayName(&out->display_name) &&
         data.ReadType(&out->type) &&
         data.ReadDefaultValue(&out->default_value) &&
         data.ReadValues(&out->values);
}
#endif  // defined(OS_CHROMEOS)

// static
bool StructTraits<printing::mojom::PrinterSemanticCapsAndDefaultsDataView,
                  printing::PrinterSemanticCapsAndDefaults>::
    Read(printing::mojom::PrinterSemanticCapsAndDefaultsDataView data,
         printing::PrinterSemanticCapsAndDefaults* out) {
  out->collate_capable = data.collate_capable();
  out->collate_default = data.collate_default();
  out->copies_max = data.copies_max();
  if (!data.ReadDuplexModes(&out->duplex_modes) ||
      !data.ReadDuplexDefault(&out->duplex_default)) {
    return false;
  }
  out->color_changeable = data.color_changeable();
  out->color_default = data.color_default();
  if (!data.ReadColorModel(&out->color_model) ||
      !data.ReadBwModel(&out->bw_model) || !data.ReadPapers(&out->papers) ||
      !data.ReadUserDefinedPapers(&out->user_defined_papers) ||
      !data.ReadDefaultPaper(&out->default_paper) ||
      !data.ReadDpis(&out->dpis) || !data.ReadDefaultDpi(&out->default_dpi)) {
    return false;
  }

#if defined(OS_CHROMEOS)
  out->pin_supported = data.pin_supported();
  if (!data.ReadAdvancedCapabilities(&out->advanced_capabilities))
    return false;
#endif  // defined(OS_CHROMEOS)

  // Extra validity checks.

  // Can not have less than one copy.
  if (out->copies_max < 1) {
    DLOG(ERROR) << "Must have copies_max greater than zero.";
    return false;
  }

  // There should be at least one item in `papers`.
  if (out->papers.empty()) {
    DLOG(ERROR) << "The available papers must not be empty.";
    return false;
  }

  // There should not be duplicates in any of the arrays.
  DuplicateChecker<printing::mojom::DuplexMode> duplex_modes_dup_checker;
  if (duplex_modes_dup_checker.HasDuplicates(out->duplex_modes)) {
    DLOG(ERROR) << "Duplicate duplex_modes detected.";
    return false;
  }

  DuplicateChecker<printing::PrinterSemanticCapsAndDefaults::Paper>
      papers_dup_checker;
  if (papers_dup_checker.HasDuplicates(out->papers)) {
    DLOG(ERROR) << "Duplicate papers detected.";
    return false;
  }

  DuplicateChecker<printing::PrinterSemanticCapsAndDefaults::Paper>
      user_defined_papers_dup_checker;
  if (user_defined_papers_dup_checker.HasDuplicates(out->user_defined_papers)) {
    DLOG(ERROR) << "Duplicate user_defined_papers detected.";
    return false;
  }

  DuplicateChecker<gfx::Size> dpis_dup_checker;
  if (dpis_dup_checker.HasDuplicates(out->dpis)) {
    DLOG(ERROR) << "Duplicate dpis detected.";
    return false;
  }

#if defined(OS_CHROMEOS)
  DuplicateChecker<printing::AdvancedCapability>
      advanced_capabilities_dup_checker;
  if (advanced_capabilities_dup_checker.HasDuplicates(
          out->advanced_capabilities)) {
    DLOG(ERROR) << "Duplicate advanced_capabilities detected.";
    return false;
  }
#endif  // defined(OS_CHROMEOS)

  return true;
}

}  // namespace mojo
