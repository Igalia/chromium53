// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_CURSOR_LOADER_UPDATER_AURALINUX_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_CURSOR_LOADER_UPDATER_AURALINUX_H_

#include "base/compiler_specific.h"
#include "ui/views/widget/desktop_aura/desktop_cursor_loader_updater.h"

namespace views {

// Loads the subset of aura cursors that X11 doesn't provide.
class DesktopCursorLoaderUpdaterAuraLinux : public DesktopCursorLoaderUpdater {
 public:
  DesktopCursorLoaderUpdaterAuraLinux();
  ~DesktopCursorLoaderUpdaterAuraLinux() override;

  // Overridden from DesktopCursorLoaderUpdater:
  void OnCreate(float device_scale_factor, ui::CursorLoader* loader) override;
  void OnDestroy(ui::CursorLoader* loader) override;
  void OnDisplayUpdated(const display::Display& display,
                        ui::CursorLoader* loader) override;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_DISPLAY_CHANGE_HANDLER_AURALINUX_H_
