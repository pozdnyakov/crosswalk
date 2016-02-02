// Copyright (c) 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/xwalk_presentation_service_helper.h"
#include "xwalk/runtime/browser/android/xwalk_presentation_host.h"

namespace xwalk {


class DisplayInfoManagerServiceAndroid
    : public DisplayInfoManagerService {
 public:
  ~DisplayInfoManagerServiceAndroid() override {
  }

  void FindAllAvailableMonitors(
      std::vector<DisplayInfo>* info_list) override {
    if ( auto presentation_api = XWalkPresentationHost::GetInstance() ) {
      auto vec = presentation_api->GetAndroidDisplayInfo();
      for ( auto data : vec ) {
        DisplayInfo info = {};
        info.name = data.name;
        info.id = std::to_string(data.id);
        info.is_primary = data.is_primary;

        info_list->push_back(info);
      }
    } else {
      LOG(ERROR) << "XWalkPresentationHost instance not found";
    }
  }

  static void DisplayChangeCallback(int /*display_id*/) {
    DisplayInfoManager::GetInstance()->UpdateInfoList();
  }

  void ListenMonitorsUpdate() override {
    if ( auto presentation_api = XWalkPresentationHost::GetInstance() ) {
      presentation_api->SetDisplayChangeCallback(DisplayChangeCallback);
    } else {
      LOG(ERROR) << "Failed to listen to Android Display change event";
    }
  }

  void StopListenMonitorsUpdate() override {
    if ( auto presentation_api = XWalkPresentationHost::GetInstance() ) {
      presentation_api->SetDisplayChangeCallback(nullptr);
    } else {
    }
  }
};


DisplayInfoManager* DisplayInfoManager::GetInstance() {
  if ( instance_ == nullptr ) {
    instance_ = new DisplayInfoManager(new DisplayInfoManagerServiceAndroid());
  }
  return instance_;
}


void PresentationSession::Create(
    const PresentationSession::CreateParams& params,
    PresentationSession::SessionCallback callback) {

  if ( auto presentation_api = XWalkPresentationHost::GetInstance() ) {
    int displayId = std::stoi(params.display_info.id);
    presentation_api->ShowPresentation(params.render_process_id,
      params.render_frame_id, displayId, params.presentation_url);

    scoped_refptr<PresentationSession> session(
        new PresentationSession(
            params.presentation_url,
            params.presentation_id,
            params.display_info.id));
    session->set_render_process_id(params.render_process_id);
    session->set_render_frame_id(params.render_frame_id);
    callback.Run(session, "");

  } else {
    LOG(ERROR) << "XWalkPresentationHost instance not found";
  }
}

void PresentationSession::Close() {
  if ( auto presentation_api = XWalkPresentationHost::GetInstance() ) {
    presentation_api->closePresentation(get_render_process_id(),
      get_render_frame_id());
  }
}

PresentationFrame::PresentationFrame(
  const RenderFrameHostId& render_frame_host_id)
  : screen_listener_(nullptr),
    render_frame_host_id_(render_frame_host_id) {
  DisplayInfoManager::GetInstance()->AddObserver(this);
  if ( XWalkPresentationHost* p = XWalkPresentationHost::GetInstance() ) {
    p->AddSessionObserver(this);
  }
}

PresentationFrame::~PresentationFrame() {
  if (delegate_observer_)
    delegate_observer_->OnDelegateDestroyed();
  if (session_)
    session_->RemoveObserver(this);
  DisplayInfoManager::GetInstance()->RemoveObserver(this);
  if ( auto presentation_api = XWalkPresentationHost::GetInstance() ) {
    presentation_api->RemoveSessionObserver(this);
  }
}

void PresentationFrame::OnPresentationClosed(int render_process_id,
    int render_frame_id) {
  if ( this->render_frame_host_id_.first == render_process_id
    && this->render_frame_host_id_.second == render_frame_id ) {
    this->session_->NotifyClose();
  }
}

}  // namespace xwalk
