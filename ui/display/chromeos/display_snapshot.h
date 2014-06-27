// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_DISPLAY_SNAPSHOT_H_
#define UI_DISPLAY_CHROMEOS_DISPLAY_SNAPSHOT_H_

#include <vector>

#include "ui/display/chromeos/display_mode.h"
#include "ui/display/chromeos/display_snapshot.h"
#include "ui/display/display_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

// This class represents the state of a display at one point in time. Platforms
// will extend this class in order to add platform specific configuration and
// identifiers required to configure this display.
class DISPLAY_EXPORT DisplaySnapshot {
 public:
  DisplaySnapshot(int64_t display_id,
                  bool has_proper_display_id,
                  const gfx::Point& origin,
                  const gfx::Size& physical_size,
                  OutputType type,
                  bool is_aspect_preserving_scaling,
                  const std::vector<const DisplayMode*>& modes,
                  const DisplayMode* current_mode,
                  const DisplayMode* native_mode);
  virtual ~DisplaySnapshot();

  const gfx::Point& origin() const { return origin_; }
  const gfx::Size& physical_size() const { return physical_size_; }
  ui::OutputType type() const { return type_; }
  bool is_aspect_preserving_scaling() const {
    return is_aspect_preserving_scaling_;
  }
  int64_t display_id() const { return display_id_; }
  bool has_proper_display_id() const { return has_proper_display_id_; }

  const DisplayMode* current_mode() const { return current_mode_; }
  const DisplayMode* native_mode() const { return native_mode_; }

  const std::vector<const DisplayMode*>& modes() const { return modes_; }

  void set_current_mode(const DisplayMode* mode) { current_mode_ = mode; }
  void set_origin(const gfx::Point& origin) { origin_ = origin; }
  void add_mode(const DisplayMode* mode) { modes_.push_back(mode); }

  // Generates the human readable string for this display. Generally this is
  // parsed from the EDID information.
  virtual std::string GetDisplayName() = 0;

  // Returns true if the overscan flag is set to true in the display
  // information. Generally this is read from the EDID flags.
  virtual bool GetOverscanFlag() = 0;

  // Returns a textual representation of this display state.
  virtual std::string ToString() const = 0;

 protected:
  // Display id for this output.
  int64_t display_id_;
  bool has_proper_display_id_;

  // Output's origin on the framebuffer.
  gfx::Point origin_;

  gfx::Size physical_size_;

  OutputType type_;

  bool is_aspect_preserving_scaling_;

  std::vector<const DisplayMode*> modes_;  // Not owned.

  // Mode currently being used by the output.
  const DisplayMode* current_mode_;

  // "Best" mode supported by the output.
  const DisplayMode* native_mode_;

  DISALLOW_COPY_AND_ASSIGN(DisplaySnapshot);
};

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_DISPLAY_SNAPSHOT_H_
