// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_MATHML_OPERATOR_DICTIONARY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_MATHML_OPERATOR_DICTIONARY_H_

#include "third_party/blink/renderer/platform/platform_export.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

enum class MathMLOperatorDictionaryCategory : uint8_t {
  kNone,
  kA,
  kB,
  kC,
  kDorEorL,
  kForG,
  kH,
  kI,
  kJ,
  kK,
  kM,
  kUndefined = 15
};

enum MathMLOperatorDictionaryForm { kInfix, kPrefix, kPostfix };

// FindCategory takes a UTF-16 string and form (infix, prefix, postfix) as input
// and returns the operator dictionary category for this pair, see:
// https://mathml-refresh.github.io/mathml-core/#operator-dictionary
// The returned value is never MathMLOperatorDictionaryCategory::kUndefined.
PLATFORM_EXPORT MathMLOperatorDictionaryCategory
FindCategory(const String& content, MathMLOperatorDictionaryForm);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_MATHML_OPERATOR_DICTIONARY_H_
