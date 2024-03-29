// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/util.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace json_schema_compiler {
namespace util {

namespace {

bool ReportError(const base::Value& from,
                 base::Value::Type expected,
                 std::u16string* error) {
  DCHECK(error->empty());
  *error = base::ASCIIToUTF16(base::StringPrintf(
      "expected %s, got %s", base::Value::GetTypeName(expected),
      base::Value::GetTypeName(from.type())));
  return false;  // Always false on purpose.
}

}  // namespace

bool PopulateItem(const base::Value& from, int* out) {
  if (out && from.is_int()) {
    *out = from.GetInt();
    return true;
  }
  return from.is_int();
}

bool PopulateItem(const base::Value& from, int* out, std::u16string* error) {
  if (!PopulateItem(from, out))
    return ReportError(from, base::Value::Type::INTEGER, error);
  return true;
}

bool PopulateItem(const base::Value& from, bool* out) {
  if (out && from.is_bool()) {
    *out = from.GetBool();
    return true;
  }
  return from.is_bool();
}

bool PopulateItem(const base::Value& from, bool* out, std::u16string* error) {
  if (!from.is_bool())
    return ReportError(from, base::Value::Type::BOOLEAN, error);
  if (out)
    *out = from.GetBool();
  return true;
}

bool PopulateItem(const base::Value& from, double* out) {
  return from.GetAsDouble(out);
}

bool PopulateItem(const base::Value& from, double* out, std::u16string* error) {
  if (!from.GetAsDouble(out))
    return ReportError(from, base::Value::Type::DOUBLE, error);
  return true;
}

bool PopulateItem(const base::Value& from, std::string* out) {
  return from.GetAsString(out);
}

bool PopulateItem(const base::Value& from,
                  std::string* out,
                  std::u16string* error) {
  if (!from.GetAsString(out))
    return ReportError(from, base::Value::Type::STRING, error);
  return true;
}

bool PopulateItem(const base::Value& from, std::vector<uint8_t>* out) {
  if (!from.is_blob())
    return false;
  *out = from.GetBlob();
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::vector<uint8_t>* out,
                  std::u16string* error) {
  if (!from.is_blob())
    return ReportError(from, base::Value::Type::BINARY, error);
  *out = from.GetBlob();
  return true;
}

bool PopulateItem(const base::Value& from, std::unique_ptr<base::Value>* out) {
  *out = base::Value::ToUniquePtrValue(from.Clone());
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::unique_ptr<base::Value>* out,
                  std::u16string* error) {
  *out = base::Value::ToUniquePtrValue(from.Clone());
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::unique_ptr<base::DictionaryValue>* out) {
  const base::DictionaryValue* dict = nullptr;
  if (!from.GetAsDictionary(&dict))
    return false;
  *out = dict->CreateDeepCopy();
  return true;
}

bool PopulateItem(const base::Value& from,
                  std::unique_ptr<base::DictionaryValue>* out,
                  std::u16string* error) {
  const base::DictionaryValue* dict = nullptr;
  if (!from.GetAsDictionary(&dict))
    return ReportError(from, base::Value::Type::DICTIONARY, error);
  *out = dict->CreateDeepCopy();
  return true;
}

void AddItemToList(const int from, base::ListValue* out) {
  out->AppendInteger(from);
}

void AddItemToList(const bool from, base::ListValue* out) {
  out->AppendBoolean(from);
}

void AddItemToList(const double from, base::ListValue* out) {
  out->Append(from);
}

void AddItemToList(const std::string& from, base::ListValue* out) {
  out->AppendString(from);
}

void AddItemToList(const std::vector<uint8_t>& from, base::ListValue* out) {
  out->Append(base::Value(from));
}

void AddItemToList(const std::unique_ptr<base::Value>& from,
                   base::ListValue* out) {
  out->Append(from->Clone());
}

void AddItemToList(const std::unique_ptr<base::DictionaryValue>& from,
                   base::ListValue* out) {
  out->Append(from->CreateDeepCopy());
}

}  // namespace util
}  // namespace json_schema_compiler
