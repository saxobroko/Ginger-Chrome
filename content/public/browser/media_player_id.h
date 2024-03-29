// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_PLAYER_ID_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_PLAYER_ID_H_

#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

struct CONTENT_EXPORT MediaPlayerId {
  static MediaPlayerId CreateMediaPlayerIdForTests();
  MediaPlayerId() = delete;

  MediaPlayerId(GlobalFrameRoutingId routing_id, int delegate_id);
  bool operator==(const MediaPlayerId&) const;
  bool operator!=(const MediaPlayerId&) const;
  bool operator<(const MediaPlayerId&) const;

  GlobalFrameRoutingId frame_routing_id;
  int delegate_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_PLAYER_ID_H_
