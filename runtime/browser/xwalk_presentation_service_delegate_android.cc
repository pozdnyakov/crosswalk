// Copyright (c) 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/xwalk_presentation_service_delegate.h"
#include "xwalk/runtime/browser/xwalk_presentation_service_session.h"

namespace xwalk {

content::PresentationServiceDelegate* XWalkPresentationServiceDelegate::
    GetOrCreateForWebContents(content::WebContents* web_contents) {
  DCHECK(web_contents);

  // CreateForWebContents does nothing if the delegate instance already exists.
  XWalkPresentationServiceDelegate::CreateForWebContents(web_contents);
  return XWalkPresentationServiceDelegate::FromWebContents(web_contents);
}

void XWalkPresentationServiceDelegate::StartSession(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_url,
    const PresentationSessionStartedCallback& success_cb,
    const PresentationSessionErrorCallback& error_cb) {
  if (presentation_url.empty() || !IsValidPresentationUrl(presentation_url)) {
    error_cb.Run(content::PresentationError(content::PRESENTATION_ERROR_UNKNOWN,
                                            "Invalid presentation arguments."));
    return;
  }

  const DisplayInfo* available_monitor =
      DisplayInfoManager::GetInstance()->FindAvailable();
  if (!available_monitor) {
    error_cb.Run(content::PresentationError(
        content::PRESENTATION_ERROR_NO_AVAILABLE_SCREENS,
        "No available monitors"));
    return;
  }

  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  const std::string& presentation_id = base::GenerateGUID();

  PresentationSession::CreateParams params = {};
  params.display_info = *available_monitor;
  params.presentation_id = presentation_id;
  params.presentation_url = presentation_url;
  params.web_contents = web_contents_;
  params.render_process_id = render_process_id;
  params.render_frame_id = render_frame_id;
  params.application = nullptr;

  auto callback = base::Bind(
      &XWalkPresentationServiceDelegate::OnSessionStarted,
      AsWeakPtr(), render_frame_host_id, success_cb, error_cb);
  PresentationSession::Create(params, callback);
}

}  // namespace xwalk
