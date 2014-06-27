# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from measurements import smoothness
from telemetry.core import wpr_modes
from telemetry.page import page
from telemetry.page import page_measurement_unittest_base
from telemetry.unittest import options_for_unittests

class FakePlatform(object):
  def IsRawDisplayFrameRateSupported(self):
    return False


class FakeBrowser(object):
  def __init__(self):
    self.platform = FakePlatform()
    self.category_filter = None

  def StartTracing(self, category_filter, _):
    self.category_filter = category_filter


class FakeTab(object):
  def __init__(self):
    self.browser = FakeBrowser()

  def ExecuteJavaScript(self, js):
    pass

class SmoothnessUnitTest(
      page_measurement_unittest_base.PageMeasurementUnitTestBase):
  """Smoke test for smoothness measurement

     Runs smoothness measurement on a simple page and verifies
     that all metrics were added to the results. The test is purely functional,
     i.e. it only checks if the metrics are present and non-zero.
  """
  def testSyntheticDelayConfiguration(self):
    test_page = page.Page('http://dummy', None)
    test_page.synthetic_delays = {
        'cc.BeginMainFrame': { 'target_duration': 0.012 },
        'cc.DrawAndSwap': { 'target_duration': 0.012, 'mode': 'alternating' },
        'gpu.SwapBuffers': { 'target_duration': 0.012 }
    }

    tab = FakeTab()
    measurement = smoothness.Smoothness()
    measurement.WillRunActions(test_page, tab)

    expected_category_filter = [
        'DELAY(cc.BeginMainFrame;0.012000;static)',
        'DELAY(cc.DrawAndSwap;0.012000;alternating)',
        'DELAY(gpu.SwapBuffers;0.012000;static)',
        'benchmark',
        'webkit.console'
    ]
    self.assertEquals(expected_category_filter,
                      sorted(tab.browser.category_filter.split(',')))
  def setUp(self):

    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  def testSmoothness(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')
    measurement = smoothness.Smoothness()
    results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertEquals(0, len(results.failures))

    frame_times = results.FindAllPageSpecificValuesNamed('frame_times')
    self.assertEquals(len(frame_times), 1)
    self.assertGreater(frame_times[0].GetRepresentativeNumber(), 0)

    mean_frame_time = results.FindAllPageSpecificValuesNamed('mean_frame_time')
    self.assertEquals(len(mean_frame_time), 1)
    self.assertGreater(mean_frame_time[0].GetRepresentativeNumber(), 0)

    jank = results.FindAllPageSpecificValuesNamed('jank')
    self.assertEquals(len(jank), 1)
    self.assertGreater(jank[0].GetRepresentativeNumber(), 0)

    mostly_smooth = results.FindAllPageSpecificValuesNamed('mostly_smooth')
    self.assertEquals(len(mostly_smooth), 1)
    self.assertGreaterEqual(mostly_smooth[0].GetRepresentativeNumber(), 0)

    mean_mouse_wheel_latency = results.FindAllPageSpecificValuesNamed(
        'mean_mouse_wheel_latency')
    if mean_mouse_wheel_latency:
      self.assertEquals(len(mean_mouse_wheel_latency), 1)
      self.assertGreater(
          mean_mouse_wheel_latency[0].GetRepresentativeNumber(), 0)

    mean_touch_scroll_latency = results.FindAllPageSpecificValuesNamed(
        'mean_touch_scroll_latency')
    if mean_touch_scroll_latency:
      self.assertEquals(len(mean_touch_scroll_latency), 1)
      self.assertGreater(
          mean_touch_scroll_latency[0].GetRepresentativeNumber(), 0)

  def testCleanUpTrace(self):
    self.TestTracingCleanedUp(smoothness.Smoothness, self._options)
