// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/address_field.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {

bool SetFieldAndAdvanceCursor(AutofillScanner* scanner, AutofillField** field) {
  *field = scanner->Cursor();
  scanner->Advance();
  return true;
}

}  // namespace

// Some sites use type="tel" for zip fields (to get a numerical input).
// http://crbug.com/426958
const int AddressField::kZipCodeMatchType =
    MATCH_DEFAULT | MATCH_TELEPHONE | MATCH_NUMBER;

const int AddressField::kDependentLocalityMatchType =
    MATCH_DEFAULT | MATCH_SELECT | MATCH_SEARCH;

// Select fields are allowed here.  This occurs on top-100 site rediff.com.
const int AddressField::kCityMatchType =
    MATCH_DEFAULT | MATCH_SELECT | MATCH_SEARCH;

const int AddressField::kStateMatchType =
    MATCH_DEFAULT | MATCH_SELECT | MATCH_SEARCH;

std::unique_ptr<FormField> AddressField::Parse(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    LogManager* log_manager) {
  if (scanner->IsEnd())
    return nullptr;

  std::unique_ptr<AddressField> address_field(new AddressField(log_manager));
  const AutofillField* const initial_field = scanner->Cursor();
  size_t saved_cursor = scanner->SaveCursor();

  const std::vector<MatchingPattern>& email_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("EMAIL_ADDRESS",
                                                      page_language);

  const std::vector<MatchingPattern>& address_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("ADDRESS_LOOKUP",
                                                      page_language);

  const std::vector<MatchingPattern>& address_ignore_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("ADDRESS_NAME_IGNORED",
                                                      page_language);

  const std::vector<MatchingPattern>& attention_ignore_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("ATTENTION_IGNORED",
                                                      page_language);

  const std::vector<MatchingPattern>& region_ignore_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("REGION_IGNORED",
                                                      page_language);

  // Allow address fields to appear in any order.
  size_t begin_trailing_non_labeled_fields = 0;
  bool has_trailing_non_labeled_fields = false;
  while (!scanner->IsEnd()) {
    const size_t cursor = scanner->SaveCursor();
    // Ignore "Address Lookup" field. http://crbug.com/427622
    if (ParseField(scanner, kAddressLookupRe, address_patterns, nullptr,
                   {log_manager, "kAddressLookupRe"}) ||
        ParseField(scanner, kAddressNameIgnoredRe, address_ignore_patterns,
                   nullptr, {log_manager, "kAddressNameIgnoreRe"})) {
      continue;
      // Ignore email addresses.
    } else if (ParseFieldSpecifics(
                   scanner, kEmailRe, MATCH_DEFAULT | MATCH_TEXT_AREA,
                   email_patterns, nullptr, {log_manager, "kEmailRe"},
                   {.augment_types = MATCH_TEXT_AREA})) {
      continue;
    } else if (address_field->ParseAddress(scanner, page_language) ||
               address_field->ParseDependentLocalityCityStateCountryZipCode(
                   scanner, page_language) ||
               address_field->ParseCompany(scanner, page_language)) {
      has_trailing_non_labeled_fields = false;
      continue;
    } else if (ParseField(scanner, kAttentionIgnoredRe,
                          attention_ignore_patterns, nullptr,
                          {log_manager, "kAttentionIgnoredRe"}) ||
               ParseField(scanner, kRegionIgnoredRe, region_ignore_patterns,
                          nullptr, {log_manager, "kRegionIgnoredRe"})) {
      // We ignore the following:
      // * Attention.
      // * Province/Region/Other.
      continue;
    } else if (scanner->Cursor() != initial_field &&
               ParseEmptyLabel(scanner, nullptr)) {
      // Ignore non-labeled fields within an address; the page
      // MapQuest Driving Directions North America.html contains such a field.
      // We only ignore such fields after we've parsed at least one other field;
      // otherwise we'd effectively parse address fields before other field
      // types after any non-labeled fields, and we want email address fields to
      // have precedence since some pages contain fields labeled
      // "Email address".
      if (!has_trailing_non_labeled_fields) {
        has_trailing_non_labeled_fields = true;
        begin_trailing_non_labeled_fields = cursor;
      }

      continue;
    } else {
      // No field found.
      break;
    }
  }

  // If we have identified any address fields in this field then it should be
  // added to the list of fields.
  if (address_field->company_ || address_field->address1_ ||
      address_field->address2_ || address_field->address3_ ||
      address_field->street_address_ || address_field->city_ ||
      address_field->state_ || address_field->zip_ || address_field->zip4_ ||
      address_field->street_name_ || address_field->house_number_ ||
      address_field->country_ || address_field->apartment_number_ ||
      address_field->dependent_locality_) {
    // Don't slurp non-labeled fields at the end into the address.
    if (has_trailing_non_labeled_fields)
      scanner->RewindTo(begin_trailing_non_labeled_fields);
    return std::move(address_field);
  }

  scanner->RewindTo(saved_cursor);
  return nullptr;
}

AddressField::AddressField(LogManager* log_manager)
    : log_manager_(log_manager) {}

void AddressField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
  // The page can request the address lines as a single textarea input or as
  // multiple text fields (or not at all), but it shouldn't be possible to
  // request both.
  DCHECK(!(address1_ && street_address_));
  DCHECK(!(address2_ && street_address_));
  DCHECK(!(address3_ && street_address_));

  AddClassification(company_, COMPANY_NAME, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(address1_, ADDRESS_HOME_LINE1, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(address2_, ADDRESS_HOME_LINE2, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(address3_, ADDRESS_HOME_LINE3, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(street_address_, ADDRESS_HOME_STREET_ADDRESS,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(dependent_locality_, ADDRESS_HOME_DEPENDENT_LOCALITY,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(city_, ADDRESS_HOME_CITY, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(state_, ADDRESS_HOME_STATE, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(zip_, ADDRESS_HOME_ZIP, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(country_, ADDRESS_HOME_COUNTRY, kBaseAddressParserScore,
                    field_candidates);
  AddClassification(house_number_, ADDRESS_HOME_HOUSE_NUMBER,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(street_name_, ADDRESS_HOME_STREET_NAME,
                    kBaseAddressParserScore, field_candidates);
  AddClassification(apartment_number_, ADDRESS_HOME_APT_NUM,
                    kBaseAddressParserScore, field_candidates);
}

bool AddressField::ParseCompany(AutofillScanner* scanner,
                                const LanguageCode& page_language) {
  if (company_)
    return false;

  const std::vector<MatchingPattern>& company_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("COMPANY_NAME",
                                                      page_language);

  return ParseField(scanner, kCompanyRe, company_patterns, &company_,
                    {log_manager_, "kCompanyRe"});
}

bool AddressField::ParseAddressFieldSequence(
    AutofillScanner* scanner,
    const LanguageCode& page_language) {
  // Search for a sequence of a street name field followed by a house number
  // field. Only if both are found in an abitrary order, the parsing is
  // considered successful.

  // TODO(crbug.com/1125978): Remove once launched.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForMoreStructureInAddresses)) {
    return false;
  }

  const size_t cursor_position = scanner->CursorPosition();

  const std::vector<MatchingPattern>& street_name_patterns =
      PatternProvider::GetInstance().GetMatchPatterns(ADDRESS_HOME_STREET_NAME,
                                                      page_language);

  const std::vector<MatchingPattern>& house_number_patterns =
      PatternProvider::GetInstance().GetMatchPatterns(ADDRESS_HOME_HOUSE_NUMBER,
                                                      page_language);
  const std::vector<MatchingPattern>& apartment_number_patterns =
      PatternProvider::GetInstance().GetMatchPatterns(ADDRESS_HOME_APT_NUM,
                                                      page_language);

  while (!scanner->IsEnd()) {
    if (!street_name_ &&
        ParseFieldSpecifics(scanner, kStreetNameRe, MATCH_DEFAULT,
                            street_name_patterns, &street_name_,
                            {log_manager_, "kStreetNameRe"})) {
      continue;
    }
    if (!house_number_ &&
        ParseFieldSpecifics(scanner, kHouseNumberRe,
                            MATCH_DEFAULT | MATCH_NUMBER | MATCH_TELEPHONE,
                            house_number_patterns, &house_number_,
                            {log_manager_, "kHouseNumberRe"})) {
      continue;
    }

    // TODO(crbug.com/1153715): Remove finch guard once launched.
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForApartmentNumbers) &&
        !apartment_number_ &&
        ParseFieldSpecifics(scanner, kApartmentNumberRe,
                            MATCH_DEFAULT | MATCH_NUMBER | MATCH_TELEPHONE,
                            apartment_number_patterns, &apartment_number_,
                            {log_manager_, "kApartmentNumberRe"})) {
      continue;
    }

    break;
  }

  // The street name and house number are non-optional.
  if (street_name_ && house_number_)
    return true;

  // Reset all fields if the non-optional requirements could not be met.
  street_name_ = nullptr;
  house_number_ = nullptr;
  apartment_number_ = nullptr;

  scanner->RewindTo(cursor_position);
  return false;
}

bool AddressField::ParseAddress(AutofillScanner* scanner,
                                const LanguageCode& page_language) {
  if (street_name_ && house_number_) {
    return false;
  }
  return ParseAddressFieldSequence(scanner, page_language) ||
         ParseAddressLines(scanner, page_language);
}

bool AddressField::ParseAddressLines(AutofillScanner* scanner,
                                     const LanguageCode& page_language) {
  // We only match the string "address" in page text, not in element names,
  // because sometimes every element in a group of address fields will have
  // a name containing the string "address"; for example, on the page
  // Kohl's - Register Billing Address.html the text element labeled "city"
  // has the name "BILL_TO_ADDRESS<>city".  We do match address labels
  // such as "address1", which appear as element names on various pages (eg
  // AmericanGirl-Registration.html, BloomingdalesBilling.html,
  // EBay Registration Enter Information.html).
  if (address1_ || street_address_)
    return false;

  std::u16string pattern = kAddressLine1Re;
  std::u16string label_pattern = kAddressLine1LabelRe;

  const std::vector<MatchingPattern>& address_line1_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("ADDRESS_LINE_1",
                                                      page_language);

  if (!ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT,
                           address_line1_patterns, &address1_,
                           {log_manager_, "kAddressLine1Re"}) &&
      !ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                           address_line1_patterns, &address1_,
                           {log_manager_, "kAddressLine1LabelRe"}) &&
      !ParseFieldSpecifics(scanner, pattern, MATCH_DEFAULT | MATCH_TEXT_AREA,
                           address_line1_patterns, &street_address_,
                           {log_manager_, "kAddressLine1Re"},
                           {.augment_types = MATCH_TEXT_AREA}) &&
      !ParseFieldSpecifics(scanner, label_pattern,
                           MATCH_LABEL | MATCH_TEXT_AREA,
                           address_line1_patterns, &street_address_,
                           {log_manager_, "kAddressLine1LabelRe"},
                           {.augment_types = MATCH_TEXT_AREA}))
    return false;

  if (street_address_)
    return true;

  // This code may not pick up pages that have an address field consisting of a
  // sequence of unlabeled address fields. If we need to add this, see
  // discussion on https://codereview.chromium.org/741493003/
  pattern = kAddressLine2Re;
  label_pattern = kAddressLine2LabelRe;

  const std::vector<MatchingPattern>& address_line2_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("ADDRESS_LINE_2",
                                                      page_language);

  if (!ParseField(scanner, pattern, address_line2_patterns, &address2_,
                  {log_manager_, "kAddressLine2Re"}) &&
      !ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                           address_line2_patterns, &address2_,
                           {log_manager_, "kAddressLine2LabelRe"}))
    return true;

  const std::vector<MatchingPattern>& address_line_extra_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("ADDRESS_LINE_EXTRA",
                                                      page_language);

  // Optionally parse address line 3. This uses the same label regexp as
  // address 2 above.
  pattern = kAddressLinesExtraRe;
  if (!ParseField(scanner, pattern, address_line_extra_patterns, &address3_,
                  {log_manager_, "kAddressLinesExtraRe"}) &&
      !ParseFieldSpecifics(scanner, label_pattern, MATCH_LABEL | MATCH_TEXT,
                           address_line2_patterns, &address3_,
                           {log_manager_, "kAddressLine2LabelRe"}))
    return true;

  // Try for surplus lines, which we will promptly discard. Some pages have 4
  // address lines (e.g. uk/ShoesDirect2.html)!
  //
  // Since these are rare, don't bother considering unlabeled lines as extra
  // address lines.
  pattern = kAddressLinesExtraRe;
  while (ParseField(scanner, pattern, address_line_extra_patterns, nullptr,
                    {log_manager_, "kAddressLinesExtraRe"})) {
    // Consumed a surplus line, try for another.
  }
  return true;
}

bool AddressField::ParseCountry(AutofillScanner* scanner,
                                const LanguageCode& page_language) {
  if (country_)
    return false;

  const std::vector<MatchingPattern>& country_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("COUNTRY", page_language);
  const std::vector<MatchingPattern>& country_patternsl =
      PatternProvider::GetInstance().GetMatchPatterns("COUNTRY_LOCATION",
                                                      page_language);

  scanner->SaveCursor();
  if (ParseFieldSpecifics(
          scanner, kCountryRe, MATCH_DEFAULT | MATCH_SELECT | MATCH_SEARCH,
          country_patterns, &country_, {log_manager_, "kCountryRe"})) {
    return true;
  }

  // The occasional page (e.g. google account registration page) calls this a
  // "location". However, this only makes sense for select tags.
  scanner->Rewind();
  return ParseFieldSpecifics(
      scanner, kCountryLocationRe,
      MATCH_LABEL | MATCH_NAME | MATCH_SELECT | MATCH_SEARCH, country_patternsl,
      &country_, {log_manager_, "kCountryLocationRe"});
}

bool AddressField::ParseZipCode(AutofillScanner* scanner,
                                const LanguageCode& page_language) {
  if (zip_)
    return false;

  const std::vector<MatchingPattern>& zip_code_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("ZIP_CODE",
                                                      page_language);

  const std::vector<MatchingPattern>& four_digit_zip_code_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("ZIP_4", page_language);
  if (!ParseFieldSpecifics(scanner, kZipCodeRe, kZipCodeMatchType,
                           zip_code_patterns, &zip_,
                           {log_manager_, "kZipCodeRe"})) {
    return false;
  }

  // Look for a zip+4, whose field name will also often contain
  // the substring "zip".
  ParseFieldSpecifics(scanner, kZip4Re, kZipCodeMatchType,
                      four_digit_zip_code_patterns, &zip4_,
                      {log_manager_, "kZip4Re"});
  return true;
}

bool AddressField::ParseDependentLocality(AutofillScanner* scanner,
                                          const LanguageCode& page_language) {
  const bool is_enabled_dependent_locality_parsing =
      base::FeatureList::IsEnabled(
          features::kAutofillEnableDependentLocalityParsing);
  // TODO(crbug.com/1157405) Remove feature check when launched.
  if (dependent_locality_ || !is_enabled_dependent_locality_parsing)
    return false;

  const std::vector<MatchingPattern>& dependent_locality_patterns =
      PatternProvider::GetInstance().GetMatchPatterns(
          "ADDRESS_HOME_DEPENDENT_LOCALITY", page_language);
  return ParseFieldSpecifics(scanner, kDependentLocalityRe,
                             kDependentLocalityMatchType,
                             dependent_locality_patterns, &dependent_locality_,
                             {log_manager_, "kDependentLocalityRe"});
}

bool AddressField::ParseCity(AutofillScanner* scanner,
                             const LanguageCode& page_language) {
  if (city_)
    return false;

  const std::vector<MatchingPattern>& city_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("CITY", page_language);
  return ParseFieldSpecifics(scanner, kCityRe, kCityMatchType, city_patterns,
                             &city_, {log_manager_, "kCityRe"});
}

bool AddressField::ParseState(AutofillScanner* scanner,
                              const LanguageCode& page_language) {
  if (state_)
    return false;

  const std::vector<MatchingPattern>& patterns_state =
      PatternProvider::GetInstance().GetMatchPatterns("STATE", page_language);
  return ParseFieldSpecifics(scanner, kStateRe, kStateMatchType, patterns_state,
                             &state_, {log_manager_, "kStateRe"});
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelSeparately(
    AutofillScanner* scanner,
    const std::u16string& pattern,
    int match_type,
    const std::vector<MatchingPattern>& patterns,
    AutofillField** match,
    const RegExLogging& logging) {
  if (scanner->IsEnd())
    return RESULT_MATCH_NONE;

  AutofillField* cur_match = nullptr;
  size_t saved_cursor = scanner->SaveCursor();
  bool parsed_name = ParseFieldSpecifics(
      scanner, pattern, match_type & ~MATCH_LABEL, patterns, &cur_match,
      logging, {.restrict_attributes = MATCH_NAME});
  scanner->RewindTo(saved_cursor);
  bool parsed_label = ParseFieldSpecifics(
      scanner, pattern, match_type & ~MATCH_NAME, patterns, &cur_match, logging,
      {.restrict_attributes = MATCH_LABEL});
  if (parsed_name && parsed_label) {
    if (match)
      *match = cur_match;
    return RESULT_MATCH_NAME_LABEL;
  }

  scanner->RewindTo(saved_cursor);
  if (parsed_name)
    return RESULT_MATCH_NAME;
  if (parsed_label)
    return RESULT_MATCH_LABEL;
  return RESULT_MATCH_NONE;
}

bool AddressField::ParseDependentLocalityCityStateCountryZipCode(
    AutofillScanner* scanner,
    const LanguageCode& page_language) {
  // The |scanner| is not pointing at a field.
  if (scanner->IsEnd())
    return false;

  int num_of_missing_types = 0;
  for (const auto* field :
       {dependent_locality_, city_, state_, country_, zip_}) {
    if (!field)
      ++num_of_missing_types;
  }

  // All the field types have already been detected.
  if (num_of_missing_types == 0)
    return false;

  // Exactly one field type is missing.
  if (num_of_missing_types == 1) {
    if (!dependent_locality_)
      return ParseDependentLocality(scanner, page_language);
    if (!city_)
      return ParseCity(scanner, page_language);
    if (!state_)
      return ParseState(scanner, page_language);
    if (!country_)
      return ParseCountry(scanner, page_language);
    if (!zip_)
      return ParseZipCode(scanner, page_language);
  }

  // Check for matches to both the name and the label.
  ParseNameLabelResult dependent_locality_result =
      ParseNameAndLabelForDependentLocality(scanner, page_language);
  if (dependent_locality_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult city_result =
      ParseNameAndLabelForCity(scanner, page_language);
  if (city_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult state_result =
      ParseNameAndLabelForState(scanner, page_language);
  if (state_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult country_result =
      ParseNameAndLabelForCountry(scanner, page_language);
  if (country_result == RESULT_MATCH_NAME_LABEL)
    return true;
  ParseNameLabelResult zip_result =
      ParseNameAndLabelForZipCode(scanner, page_language);
  if (zip_result == RESULT_MATCH_NAME_LABEL)
    return true;

  int num_of_matches = 0;
  for (const auto result : {dependent_locality_result, city_result,
                            state_result, country_result, zip_result}) {
    if (result != RESULT_MATCH_NONE)
      ++num_of_matches;
  }

  // Check if there is only one potential match.
  if (num_of_matches == 1) {
    if (dependent_locality_result != RESULT_MATCH_NONE)
      return SetFieldAndAdvanceCursor(scanner, &dependent_locality_);
    if (city_result != RESULT_MATCH_NONE)
      return SetFieldAndAdvanceCursor(scanner, &city_);
    if (state_result != RESULT_MATCH_NONE)
      return SetFieldAndAdvanceCursor(scanner, &state_);
    if (country_result != RESULT_MATCH_NONE)
      return SetFieldAndAdvanceCursor(scanner, &country_);
    if (zip_result != RESULT_MATCH_NONE)
      return ParseZipCode(scanner, page_language);
  }

  // If there is a clash between the country and the state, set the type of
  // the field to the country.
  if (num_of_matches == 2 && state_result != RESULT_MATCH_NONE &&
      country_result != RESULT_MATCH_NONE)
    return SetFieldAndAdvanceCursor(scanner, &country_);

  // By default give the name priority over the label.
  ParseNameLabelResult results_to_match[] = {RESULT_MATCH_NAME,
                                             RESULT_MATCH_LABEL};
  if (page_language == LanguageCode("tr") &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableLabelPrecedenceForTurkishAddresses)) {
    // Give the label priority over the name to avoid misclassifications when
    // the name has a misleading value (e.g. province field is named "city").
    std::swap(results_to_match[0], results_to_match[1]);
  }

  for (const auto result : results_to_match) {
    if (dependent_locality_result == result)
      return SetFieldAndAdvanceCursor(scanner, &dependent_locality_);
    if (city_result == result)
      return SetFieldAndAdvanceCursor(scanner, &city_);
    if (state_result == result)
      return SetFieldAndAdvanceCursor(scanner, &state_);
    if (country_result == result)
      return SetFieldAndAdvanceCursor(scanner, &country_);
    if (zip_result == result)
      return ParseZipCode(scanner, page_language);
  }

  return false;
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForZipCode(
    AutofillScanner* scanner,
    const LanguageCode& page_language) {
  if (zip_)
    return RESULT_MATCH_NONE;

  const std::vector<MatchingPattern>& zip_code_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("ZIP_CODE",
                                                      page_language);

  const std::vector<MatchingPattern>& four_digit_zip_code_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("ZIP_4", page_language);

  ParseNameLabelResult result = ParseNameAndLabelSeparately(
      scanner, kZipCodeRe, kZipCodeMatchType, zip_code_patterns, &zip_,
      {log_manager_, "kZipCodeRe"});

  if (result != RESULT_MATCH_NAME_LABEL || scanner->IsEnd())
    return result;

  size_t saved_cursor = scanner->SaveCursor();
  bool found_non_zip4 = ParseCity(scanner, page_language);
  if (found_non_zip4)
    city_ = nullptr;
  scanner->RewindTo(saved_cursor);
  if (!found_non_zip4) {
    found_non_zip4 = ParseState(scanner, page_language);
    if (found_non_zip4)
      state_ = nullptr;
    scanner->RewindTo(saved_cursor);
  }

  if (!found_non_zip4) {
    // Look for a zip+4, whose field name will also often contain
    // the substring "zip".
    ParseFieldSpecifics(scanner, kZip4Re, kZipCodeMatchType,
                        four_digit_zip_code_patterns, &zip4_,
                        {log_manager_, "kZip4Re"});
  }
  return result;
}

AddressField::ParseNameLabelResult
AddressField::ParseNameAndLabelForDependentLocality(
    AutofillScanner* scanner,
    const LanguageCode& page_language) {
  const bool is_enabled_dependent_locality_parsing =
      base::FeatureList::IsEnabled(
          features::kAutofillEnableDependentLocalityParsing);
  // TODO(crbug.com/1157405) Remove feature check when launched.
  if (dependent_locality_ || !is_enabled_dependent_locality_parsing)
    return RESULT_MATCH_NONE;

  const std::vector<MatchingPattern>& dependent_locality_patterns =
      PatternProvider::GetInstance().GetMatchPatterns(
          "ADDRESS_HOME_DEPENDENT_LOCALITY", page_language);
  return ParseNameAndLabelSeparately(
      scanner, kDependentLocalityRe, kDependentLocalityMatchType,
      dependent_locality_patterns, &dependent_locality_,
      {log_manager_, "kDependentLocalityRe"});
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForCity(
    AutofillScanner* scanner,
    const LanguageCode& page_language) {
  if (city_)
    return RESULT_MATCH_NONE;

  const std::vector<MatchingPattern>& city_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("CITY", page_language);
  return ParseNameAndLabelSeparately(scanner, kCityRe, kCityMatchType,
                                     city_patterns, &city_,
                                     {log_manager_, "kCityRe"});
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForState(
    AutofillScanner* scanner,
    const LanguageCode& page_language) {
  if (state_)
    return RESULT_MATCH_NONE;

  const std::vector<MatchingPattern>& patterns_state =
      PatternProvider::GetInstance().GetMatchPatterns("STATE", page_language);
  return ParseNameAndLabelSeparately(scanner, kStateRe, kStateMatchType,
                                     patterns_state, &state_,
                                     {log_manager_, "kStateRe"});
}

AddressField::ParseNameLabelResult AddressField::ParseNameAndLabelForCountry(
    AutofillScanner* scanner,
    const LanguageCode& page_language) {
  if (country_)
    return RESULT_MATCH_NONE;

  const std::vector<MatchingPattern>& country_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("COUNTRY", page_language);

  const std::vector<MatchingPattern>& country_location_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("COUNTRY_LOCATION",
                                                      page_language);

  ParseNameLabelResult country_result = ParseNameAndLabelSeparately(
      scanner, kCountryRe, MATCH_DEFAULT | MATCH_SELECT | MATCH_SEARCH,
      country_patterns, &country_, {log_manager_, "kCountryRe"});
  if (country_result != RESULT_MATCH_NONE)
    return country_result;

  // The occasional page (e.g. google account registration page) calls this a
  // "location". However, this only makes sense for select tags.
  return ParseNameAndLabelSeparately(
      scanner, kCountryLocationRe,
      MATCH_LABEL | MATCH_NAME | MATCH_SELECT | MATCH_SEARCH,
      country_location_patterns, &country_,
      {log_manager_, "kCountryLocationRe"});
}

}  // namespace autofill
