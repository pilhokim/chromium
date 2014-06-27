# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from measurements import timeline_controller
from metrics import timeline
from telemetry.page import page_measurement

class ThreadTimes(page_measurement.PageMeasurement):
  def __init__(self):
    super(ThreadTimes, self).__init__('RunSmoothness')
    self._timeline_controller = None

  @classmethod
  def AddCommandLineArgs(cls, parser):
    parser.add_option('--report-silk-results', action='store_true',
                      help='Report results relevant to silk.')
    parser.add_option('--report-silk-details', action='store_true',
                      help='Report details relevant to silk.')

  @property
  def results_are_the_same_on_every_page(self):
    return False

  def WillRunActions(self, page, tab):
    self._timeline_controller = timeline_controller.TimelineController()
    if self.options.report_silk_details:
      # We need the other traces in order to have any details to report.
      self.timeline_controller.trace_categories = \
          timeline_controller.DEFAULT_TRACE_CATEGORIES
    else:
      self._timeline_controller.trace_categories = \
          timeline_controller.MINIMAL_TRACE_CATEGORIES
    self._timeline_controller.Start(page, tab)

  def DidRunAction(self, page, tab, action):
    self._timeline_controller.AddActionToIncludeInMetric(action)

  def DidRunActions(self, page, tab):
    self._timeline_controller.Stop(tab)

  def MeasurePage(self, page, tab, results):
    metric = timeline.ThreadTimesTimelineMetric(
      self._timeline_controller.model,
      self._timeline_controller.renderer_process,
      self._timeline_controller.action_ranges)
    if self.options.report_silk_results:
      metric.results_to_report = timeline.ReportSilkResults
    if self.options.report_silk_details:
      metric.details_to_report = timeline.ReportSilkDetails
    metric.AddResults(tab, results)

  def CleanUpAfterPage(self, _, tab):
    self._timeline_controller.CleanUp(tab)
