# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import timeline_based_metric
from metrics import rendering_stats
from telemetry.page.perf_tests_helper import FlattenList
from telemetry.util import statistics
from telemetry.core.timeline import bounds


class SmoothnessMetric(timeline_based_metric.TimelineBasedMetric):
  def __init__(self):
    super(SmoothnessMetric, self).__init__()

  def AddResults(self, model, renderer_thread, interaction_record, results):
    renderer_process = renderer_thread.parent
    time_bounds = bounds.Bounds()
    time_bounds.AddValue(interaction_record.start)
    time_bounds.AddValue(interaction_record.end)
    stats = rendering_stats.RenderingStats(
      renderer_process, model.browser_process, [time_bounds])
    if stats.mouse_wheel_scroll_latency:
      mean_mouse_wheel_scroll_latency = statistics.ArithmeticMean(
        stats.mouse_wheel_scroll_latency)
      mouse_wheel_scroll_latency_discrepancy = statistics.DurationsDiscrepancy(
        stats.mouse_wheel_scroll_latency)
      results.Add('mean_mouse_wheel_scroll_latency', 'ms',
                  round(mean_mouse_wheel_scroll_latency, 3))
      results.Add('mouse_wheel_scroll_latency_discrepancy', '',
                  round(mouse_wheel_scroll_latency_discrepancy, 4))

    if stats.touch_scroll_latency:
      mean_touch_scroll_latency = statistics.ArithmeticMean(
        stats.touch_scroll_latency)
      touch_scroll_latency_discrepancy = statistics.DurationsDiscrepancy(
        stats.touch_scroll_latency)
      results.Add('mean_touch_scroll_latency', 'ms',
                  round(mean_touch_scroll_latency, 3))
      results.Add('touch_scroll_latency_discrepancy', '',
                  round(touch_scroll_latency_discrepancy, 4))

    if stats.js_touch_scroll_latency:
      mean_js_touch_scroll_latency = statistics.ArithmeticMean(
        stats.js_touch_scroll_latency)
      js_touch_scroll_latency_discrepancy = statistics.DurationsDiscrepancy(
        stats.js_touch_scroll_latency)
      results.Add('mean_js_touch_scroll_latency', 'ms',
                  round(mean_js_touch_scroll_latency, 3))
      results.Add('js_touch_scroll_latency_discrepancy', '',
                  round(js_touch_scroll_latency_discrepancy, 4))

    # List of raw frame times.
    frame_times = FlattenList(stats.frame_times)
    results.Add('frame_times', 'ms', frame_times)

    # Arithmetic mean of frame times.
    mean_frame_time = statistics.ArithmeticMean(frame_times)
    results.Add('mean_frame_time', 'ms', round(mean_frame_time, 3))

    # Absolute discrepancy of frame time stamps.
    frame_discrepancy = statistics.TimestampsDiscrepancy(
      stats.frame_timestamps)
    results.Add('jank', 'ms', round(frame_discrepancy, 4))

    # Are we hitting 60 fps for 95 percent of all frames?
    # We use 19ms as a somewhat looser threshold, instead of 1000.0/60.0.
    percentile_95 = statistics.Percentile(frame_times, 95.0)
    results.Add('mostly_smooth', 'score', 1.0 if percentile_95 < 19.0 else 0.0)
