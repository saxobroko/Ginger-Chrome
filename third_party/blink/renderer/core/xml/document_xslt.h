// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_XML_DOCUMENT_XSLT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_XML_DOCUMENT_XSLT_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Document;
class ProcessingInstruction;

class DocumentXSLT final : public GarbageCollected<DocumentXSLT>,
                           public Supplement<Document> {
 public:
  static const char kSupplementName[];

  static void SetHasTransformSource(Document&);

  // The following static methods don't use any instance of DocumentXSLT.
  // They are just using DocumentXSLT namespace.
  static void ApplyXSLTransform(Document&, ProcessingInstruction*);
  static ProcessingInstruction* FindXSLStyleSheet(Document&);
  static bool ProcessingInstructionInsertedIntoDocument(Document&,
                                                        ProcessingInstruction*);
  static bool ProcessingInstructionRemovedFromDocument(Document&,
                                                       ProcessingInstruction*);
  static bool SheetLoaded(Document&, ProcessingInstruction*);
  static bool HasTransformSourceDocument(Document&);

  explicit DocumentXSLT(Document&);
  void Trace(Visitor*) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DocumentXSLT);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_XML_DOCUMENT_XSLT_H_
