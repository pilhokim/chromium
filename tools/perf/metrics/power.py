# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from metrics import Metric
from telemetry.core.platform import factory


class PowerMetric(Metric):
  """A metric for measuring power usage."""

  enabled = True

  def __init__(self):
    super(PowerMetric, self).__init__()
    self._browser = None
    self._running = False
    self._starting_cpu_stats = None
    self._results = None

  def __del__(self):
    # TODO(jeremy): Remove once crbug.com/350841 is fixed.
    # Don't leave power monitoring processes running on the system.
    self._StopInternal()
    parent = super(PowerMetric, self)
    if hasattr(parent, '__del__'):
      parent.__del__()

  def _StopInternal(self):
    """ Stop monitoring power if measurement is running. This function is
    idempotent."""
    if not self._running:
      return
    self._running = False
    self._results = self._browser.platform.StopMonitoringPowerAsync()
    if self._results: # StopMonitoringPowerAsync() can return None.
      self._results['cpu_stats'] = (
          _SubtractCpuStats(self._browser.cpu_stats, self._starting_cpu_stats))

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    PowerMetric.enabled = options.report_root_metrics

    # Friendly informational messages if measurement won't run.
    system_supports_power_monitoring = (
        factory.GetPlatformBackendForCurrentOS().CanMonitorPowerAsync())
    if system_supports_power_monitoring:
      if not PowerMetric.enabled:
        logging.warning(
            "--report-root-metrics omitted, power measurement disabled.")
    else:
      logging.info("System doesn't support power monitoring, power measurement"
          " disabled.")

  def Start(self, _, tab):
    if not PowerMetric.enabled:
      return

    if not tab.browser.platform.CanMonitorPowerAsync():
      return

    self._results = None
    self._browser = tab.browser
    self._StopInternal()

    # This line invokes top a few times, call before starting power measurement.
    self._starting_cpu_stats = self._browser.cpu_stats
    self._browser.platform.StartMonitoringPowerAsync()
    self._running = True

  def Stop(self, _, tab):
    if not PowerMetric.enabled:
      return

    if not tab.browser.platform.CanMonitorPowerAsync():
      return

    self._StopInternal()

  def AddResults(self, _, results):
    if not self._results:
      return

    energy_consumption_mwh = self._results['energy_consumption_mwh']
    results.Add('energy_consumption_mwh', 'mWh', energy_consumption_mwh)

    # Add idle wakeup numbers for all processes.
    for process_type in self._results['cpu_stats']:
      trace_name_for_process = 'idle_wakeups_%s' % (process_type.lower())
      results.Add(trace_name_for_process, 'count',
                  self._results['cpu_stats'][process_type])

    self._results = None

def _SubtractCpuStats(cpu_stats, start_cpu_stats):
  """Computes number of idle wakeups that occurred over measurement period.

  Each of the two cpu_stats arguments is a dict as returned by the
  Browser.cpu_stats call.

  Returns:
    A dict of process type names (Browser, Renderer, etc.) to idle wakeup count
    over the period recorded by the input.
  """
  cpu_delta = {}
  for process_type in cpu_stats:
    assert process_type in start_cpu_stats, 'Mismatching process types'
    # Skip any process_types that are empty.
    if (not cpu_stats[process_type]) or (not start_cpu_stats[process_type]):
      continue
    idle_wakeup_delta = (cpu_stats[process_type]['IdleWakeupCount'] -
                        start_cpu_stats[process_type]['IdleWakeupCount'])
    cpu_delta[process_type] = idle_wakeup_delta
  return cpu_delta
