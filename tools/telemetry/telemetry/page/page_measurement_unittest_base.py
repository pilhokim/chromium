# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from telemetry.core import util
from telemetry.page import page_runner
from telemetry.page import page as page_module
from telemetry.page import page_set
from telemetry.page import test_expectations
from telemetry.unittest import options_for_unittests

class PageMeasurementUnitTestBase(unittest.TestCase):
  """unittest.TestCase-derived class to help in the construction of unit tests
  for a measurement."""

  def CreatePageSetFromFileInUnittestDataDir(self, test_filename):
    return self.CreatePageSet('file://' + test_filename)

  def CreatePageSet(self, test_filename):
    base_dir = util.GetUnittestDataDir()
    ps = page_set.PageSet(file_path=base_dir)
    page = page_module.Page(test_filename, ps, base_dir=base_dir)
    setattr(page, 'RunSmoothness', {'action': 'scroll'})
    setattr(page, 'RunRepaint',
            { "action": "repaint_continuously", "seconds": 2 })
    ps.pages.append(page)
    return ps

  def RunMeasurement(self, measurement, ps,
      expectations=test_expectations.TestExpectations(),
      options=None):
    """Runs a measurement against a pageset, returning the rows its outputs."""
    if options is None:
      options = options_for_unittests.GetCopy()
    assert options
    temp_parser = options.CreateParser()
    page_runner.AddCommandLineArgs(temp_parser)
    measurement.AddCommandLineArgs(temp_parser)
    measurement.SetArgumentDefaults(temp_parser)
    defaults = temp_parser.get_default_values()
    for k, v in defaults.__dict__.items():
      if hasattr(options, k):
        continue
      setattr(options, k, v)

    measurement.CustomizeBrowserOptions(options)
    options.output_file = None
    options.output_format = 'none'
    options.output_trace_tag = None
    page_runner.ProcessCommandLineArgs(temp_parser, options)
    measurement.ProcessCommandLineArgs(temp_parser, options)
    return page_runner.Run(measurement, ps, expectations, options)

  def TestTracingCleanedUp(self, measurement_class, options=None):
    ps = self.CreatePageSetFromFileInUnittestDataDir('blank.html')
    start_tracing_called = [False]
    stop_tracing_called = [False]

    class BuggyMeasurement(measurement_class):
      def __init__(self, *args, **kwargs):
        measurement_class.__init__(self, *args, **kwargs)

      # Inject fake tracing methods to browser
      def TabForPage(self, page, browser):
        ActualStartTracing = browser.StartTracing
        def FakeStartTracing(*args, **kwargs):
          ActualStartTracing(*args, **kwargs)
          start_tracing_called[0] = True
          raise Exception('Intentional exception')
        browser.StartTracing = FakeStartTracing

        ActualStopTracing = browser.StopTracing
        def FakeStopTracing(*args, **kwargs):
          ActualStopTracing(*args, **kwargs)
          stop_tracing_called[0] = True
        browser.StopTracing = FakeStopTracing

        return measurement_class.TabForPage(self, page, browser)

    measurement = BuggyMeasurement()
    self.RunMeasurement(measurement, ps, options=options)
    if start_tracing_called[0]:
      self.assertTrue(stop_tracing_called[0])
