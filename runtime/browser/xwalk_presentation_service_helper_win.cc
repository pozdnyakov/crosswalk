// Copyright (c) 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/xwalk_presentation_service_helper.h"

namespace xwalk {


class DisplayInfoManagerServiceWin
    : public DisplayInfoManagerService {
 public:
  DisplayInfoManagerServiceWin()
    : info_list_(nullptr),
      hwnd_(NULL) {
  }

  ~DisplayInfoManagerServiceWin() override {
  }

  void FindAllAvailableMonitors(
    std::vector<DisplayInfo>* info_list) override {
    info_list_ = info_list;
    EnumDisplayMonitors(
        0, 0, MonitorEnumCallback, reinterpret_cast<LPARAM>(this));
  }

  void ListenMonitorsUpdate() override {
    const auto class_name = L"_LISTEN_DISPLAYCHANGE";
    WNDCLASSEX wx = {};
    wx.cbSize = sizeof(WNDCLASSEX);
    wx.lpfnWndProc = WndProcCallback;
    wx.hInstance = GetModuleHandle(NULL);
    wx.lpszClassName = class_name;
    if (!RegisterClassEx(&wx)) {
      LOG(ERROR) << "Failed to register a window class for"
                 << "listening WM_DISPLAYCHANGE";
      return;
    }

    hwnd_ = CreateWindowEx(
        0, class_name, NULL, WS_OVERLAPPEDWINDOW, 0, 0, 0, 0,
        HWND_DESKTOP, NULL, GetModuleHandle(NULL), NULL);
    if (!hwnd_)
      LOG(ERROR)
        << "Failed to register a window for listening WM_DISPLAYCHANGE";
  }

  void StopListenMonitorsUpdate() override {
    if (hwnd_) {
      CloseWindow(hwnd_);
      hwnd_ = NULL;
    }
  }

  static BOOL CALLBACK MonitorEnumCallback(
      HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM lParam) {
    MONITORINFOEX info_platform;
    info_platform.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(hMonitor, &info_platform);

    DISPLAY_DEVICE display_device = {};
    display_device.cb = sizeof(DISPLAY_DEVICE);
    EnumDisplayDevices(info_platform.szDevice, 0, &display_device, 0);

    DisplayInfo info = {};
    info.bounds = gfx::Rect(*lprcMonitor);
    info.is_primary = info_platform.dwFlags & MONITORINFOF_PRIMARY;
    info.name = display_device.DeviceName;
    info.id = display_device.DeviceID;

    auto self = reinterpret_cast<DisplayInfoManagerServiceWin*>(lParam);
    self->info_list_->push_back(info);

    return TRUE;
  }

  static LRESULT CALLBACK WndProcCallback(
      HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DISPLAYCHANGE) {
      DisplayInfoManager::GetInstance()->UpdateInfoList();
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
  }

 private:
  HWND hwnd_;
  std::vector<DisplayInfo>* info_list_;
};


DisplayInfoManager* DisplayInfoManager::GetInstance() {
  if ( instance_ == nullptr ) {
    instance_ = new DisplayInfoManager(new DisplayInfoManagerServiceWin());
  }
  return instance_;
}

void PresentationSession::Create(
    const PresentationSession::CreateParams& params,
    PresentationSession::SessionCallback callback) {
  scoped_refptr<PresentationSession> session(
      new PresentationSession(
          params.presentation_url,
          params.presentation_id,
          params.display_info.id));
  XWalkBrowserContext* context =
      XWalkBrowserContext::FromWebContents(params.web_contents);
  DCHECK(context);
  GURL url(params.presentation_url);
  auto site = content::SiteInstance::CreateForURL(context, url);
  Runtime* runtime = Runtime::Create(context, site);
  auto rph = runtime->GetRenderProcessHost();
  if (auto security_policy = params.application->security_policy())
    security_policy->EnforceForRenderer(rph);
  runtime->set_observer(session.get());
  session->runtimes_.push_back(runtime);

  runtime->LoadURL(url);

  NativeAppWindow::CreateParams win_params;
  win_params.bounds = params.display_info.bounds;
  // TODO(Mikhail): provide a special UI delegate for presentation windows.
  auto ui_delegate = RuntimeUIDelegate::Create(runtime, win_params);
  runtime->set_ui_delegate(ui_delegate);
  runtime->Show();
  ui_delegate->SetFullscreen(true);
  callback.Run(session, "");
}

void PresentationSession::Close() {
  std::vector<Runtime*> to_be_closed(runtimes_.get());
  for (Runtime* runtime : to_be_closed)
    runtime->Close();
}

PresentationFrame::PresentationFrame(const RenderFrameHostId& render_frame_host_id)
  : screen_listener_(nullptr),
    render_frame_host_id_(render_frame_host_id) {
  DisplayInfoManager::GetInstance()->AddObserver(this);
}

PresentationFrame::~PresentationFrame() {
  if (delegate_observer_)
    delegate_observer_->OnDelegateDestroyed();
  if (session_)
    session_->RemoveObserver(this);
  DisplayInfoManager::GetInstance()->RemoveObserver(this);
}

}  // namespace xwalk
