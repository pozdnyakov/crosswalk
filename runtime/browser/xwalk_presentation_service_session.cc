// Copyright (c) 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/xwalk_presentation_service_session.h"

namespace xwalk {

DisplayInfoManager* DisplayInfoManager::GetInstance() {
  return base::Singleton<DisplayInfoManager>::get();
}

DisplayInfoManager::DisplayInfoManager() {
  UpdateInfoList();
  ListenMonitorsUpdate();
}

DisplayInfoManager::~DisplayInfoManager() {
  StopListenMonitorsUpdate();
}

const DisplayInfo* DisplayInfoManager::FindAvailable() const {
  for (const DisplayInfo& info : info_list_) {
    if (!info.is_primary && !info.in_use)
      return &info;
  }
  return nullptr;
}

bool DisplayInfoManager::IsStillAvailable(
    const SystemString& display_id) const {
  for (const DisplayInfo& info : info_list_) {
    if (!info.is_primary && info.id == display_id)
      return true;
  }
  return false;
}

bool DisplayInfoManager::MarkAsUsed(
    const SystemString& id, bool in_use) {
  for (DisplayInfo& info : info_list_) {
    if (info.id == id) {
      if (in_use != info.in_use) {
        info.in_use = in_use;
        NotifyInfoChanged();
      }
      return true;
    }
  }
  return false;
}

void DisplayInfoManager::UpdateInfoList() {
  std::vector<SystemString> ids_in_use;
  for (DisplayInfo& info : this->info_list_) {
    if (info.in_use)
      ids_in_use.push_back(info.id);
  }

  this->info_list_.clear();
  this->platformFindAllAvailableMonitors();

  for ( auto& data : info_list_ ) {
    auto found = std::find(ids_in_use.begin(), ids_in_use.end(), data.id);
    if ( found != ids_in_use.end() ) {
      // Mark the id as "in_use" after re-scan monitors list
      data.in_use = true;
    }
  }

  // Broadcast the change notification
  NotifyInfoChanged();
}

void DisplayInfoManager::NotifyInfoChanged() {
  FOR_EACH_OBSERVER(Observer, observers_,
                OnDisplayInfoChanged(info_list_));
}



PresentationSession::PresentationSession(
    const std::string& presentation_url,
    const std::string& presentation_id,
    const SystemString& display_id)
  : render_process_id_(-1), render_frame_id_(-1),
    session_info_(presentation_url, presentation_id),
    display_id_(display_id),
    weak_factory_(this) {
}

PresentationSession::~PresentationSession() {
  Close();
}

void PresentationSession::OnNewRuntimeAdded(Runtime* runtime) {
  runtimes_.push_back(runtime);
  runtime->set_observer(this);
  // TODO(Mikhail): handle show popups in presentation context.
}

void PresentationSession::OnRuntimeClosed(Runtime* runtime) {
  auto found = std::find(runtimes_.begin(), runtimes_.end(), runtime);
  CHECK(found != runtimes_.end());
  runtimes_.erase(found);

  if (runtimes_.empty()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PresentationSession::NotifyClose,
        weak_factory_.GetWeakPtr()));
  }
}

void PresentationSession::NotifyClose() {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnPresentationSessionClosed(session_info_));
}


void PresentationFrame::OnPresentationSessionStarted(
    scoped_refptr<PresentationSession> session) {
  session_ = session;
  session_->AddObserver(this);
  DisplayInfoManager::GetInstance()->MarkAsUsed(session_->display_id(), true);

  if (!state_changed_cb_.is_null()) {
    state_changed_cb_.Run(
        session->session_info(),
        content::PRESENTATION_CONNECTION_STATE_CONNECTED);
  }
}

void PresentationFrame::OnPresentationSessionClosed(
    const SessionInfo& session_info) {
  if (!state_changed_cb_.is_null()) {
    state_changed_cb_.Run(
        session_info,
        content::PRESENTATION_CONNECTION_STATE_CLOSED);
  }

  DisplayInfoManager::GetInstance()->MarkAsUsed(session_->display_id(), false);
  session_ = nullptr;
}

void PresentationFrame::OnDisplayInfoChanged(
    const std::vector<DisplayInfo>& info_list) {
  if (screen_listener_) {
    if ( auto presentation_session = this->session() ) {
      // When system display is changed (e.g. rotated),
      //  check if it the display owned by me is still available
      auto my_display_id = presentation_session->display_id();
      bool still_available = DisplayInfoManager::GetInstance()->
        IsStillAvailable(my_display_id);
      screen_listener_->OnScreenAvailabilityChanged(still_available);
    } else {
      screen_listener_->OnScreenAvailabilityChanged(
        DisplayInfoManager::GetInstance()->FindAvailable() != nullptr);
    }
  }

  if (!session_)
    return;
  bool display_found = false;
  for (const DisplayInfo& info : info_list) {
    if (session_->display_id() == info.id) {
      display_found = true;
      break;
    }
  }
  if (!display_found) {
    // The display has been disconnected.
    session_->Close();
  }
}

bool PresentationFrame::SetScreenAvailabilityListener(
    PresentationScreenAvailabilityListener* listener) {
  if (screen_listener_ == listener)
    return false;

  screen_listener_ = listener;
  if (screen_listener_) {
    screen_listener_->OnScreenAvailabilityChanged(
        DisplayInfoManager::GetInstance()->FindAvailable() != nullptr);
  }
  return true;
}

bool PresentationFrame::RemoveScreenAvailabilityListener(
    PresentationScreenAvailabilityListener* listener) {
  if (screen_listener_ == listener) {
    screen_listener_ = nullptr;
    return true;
  }
  return false;
}

void PresentationFrame::Reset() {
  default_presentation_url_.clear();
  session_ = nullptr;
  screen_listener_ = nullptr;
  state_changed_cb_.Reset();
}

void PresentationFrame::ListenForSessionStateChange(
    const content::SessionStateChangedCallback& state_changed_cb) {
  CHECK(state_changed_cb_.is_null());
  state_changed_cb_ = state_changed_cb;
}


}  // namespace xwalk
