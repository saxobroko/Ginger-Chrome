// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_DATA_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_DATA_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/feedback/feedback_common.h"
#include "components/feedback/feedback_uploader.h"
#include "url/gurl.h"

namespace base {
class RefCountedString;
}

class TracingManager;

namespace feedback {

class FeedbackData : public FeedbackCommon {
 public:
  FeedbackData(FeedbackUploader* uploader, TracingManager* tracing_manager);

  // Called once we've updated all the data from the feedback page.
  void OnFeedbackPageDataComplete();

  // Kicks off compression of the system information for this instance.
  void CompressSystemInfo();

  // Sets the histograms for this instance and kicks off its
  // compression.
  void SetAndCompressHistograms(std::string histograms);

  // Sets the attached file data and kicks off its compression.
  void AttachAndCompressFileData(std::string attached_filedata);

  // Returns true if we've completed all the tasks needed before we can send
  // feedback - at this time this is includes getting the feedback page data
  // and compressing the system logs.
  bool IsDataComplete();

  // Sends the feedback report if we have all our data complete.
  void SendReport();

  // Getters
  const std::string& attached_file_uuid() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return attached_file_uuid_;
  }
  const std::string& screenshot_uuid() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return screenshot_uuid_;
  }
  bool from_assistant() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return from_assistant_;
  }
  bool assistant_debug_info_allowed() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return assistant_debug_info_allowed_;
  }

  // Setters
  void set_attached_filename(const std::string& attached_filename) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    attached_filename_ = attached_filename;
  }
  void set_attached_file_uuid(const std::string& uuid) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    attached_file_uuid_ = uuid;
  }
  void set_screenshot_uuid(const std::string& uuid) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    screenshot_uuid_ = uuid;
  }
  void set_trace_id(int trace_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    trace_id_ = trace_id;
  }
  void set_from_assistant(bool from_assistant) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    from_assistant_ = from_assistant;
  }
  void set_assistant_debug_info_allowed(bool assistant_debug_info_allowed) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    assistant_debug_info_allowed_ = assistant_debug_info_allowed;
  }

 private:
  ~FeedbackData() override;

  // Called once a compression operation is complete.
  void OnCompressComplete();

  void OnGetTraceData(int trace_id,
                      scoped_refptr<base::RefCountedString> trace_data);

  SEQUENCE_CHECKER(sequence_checker_);

  feedback::FeedbackUploader* const uploader_ = nullptr;  // Not owned.

  std::string attached_filename_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string attached_file_uuid_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string screenshot_uuid_ GUARDED_BY_CONTEXT(sequence_checker_);

  TracingManager* const tracing_manager_ = nullptr;  // Not owned.
  int trace_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  int pending_op_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 1;
  bool report_sent_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool from_assistant_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool assistant_debug_info_allowed_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  DISALLOW_COPY_AND_ASSIGN(FeedbackData);
};

}  // namespace feedback

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_DATA_H_
