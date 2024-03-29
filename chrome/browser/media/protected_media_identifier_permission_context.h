// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_request_id.h"

// Manages protected media identifier permissions flow, and delegates UI
// handling via PermissionQueueController.
class ProtectedMediaIdentifierPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit ProtectedMediaIdentifierPermissionContext(
      content::BrowserContext* browser_context);
  ~ProtectedMediaIdentifierPermissionContext() override;

  // PermissionContextBase implementation.
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;

 private:
  friend class ProtectedMediaIdentifierPermissionContextTest;
  static bool IsOriginAllowed(const GURL& origin);

  void UpdateTabContext(const permissions::PermissionRequestID& id,
                        const GURL& requesting_frame,
                        bool allowed) override;
  bool IsRestrictedToSecureOrigins() const override;

  // Returns whether "Protected content" is enabled based on factors other
  // than the protected media identifier content setting itself. For example,
  // it can be disabled by a master switch in content settings, in incognito or
  // guest mode, or by the device policy.
  bool IsProtectedMediaIdentifierEnabled() const;

  DISALLOW_COPY_AND_ASSIGN(ProtectedMediaIdentifierPermissionContext);
};

#endif  // CHROME_BROWSER_MEDIA_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_CONTEXT_H_
