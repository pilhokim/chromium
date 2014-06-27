# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page.page_set import PageSet
from telemetry.page.page import Page

class TestPageSet(PageSet):
  def __init__(self):
    super(TestPageSet, self).__init__(
      description='A pageset for testing purpose',
      archive_data_file='data/test.json',
      credentials_path='data/credential',
      user_agent_type='desktop')

    #top google property; a google tab is often open
    class Google(Page):
      def __init__(self):
        super(Google, self).__init__('https://www.google.com')

      def RunGetActionRunner(self, action_runner):
        return action_runner

    self.AddPage(Google())

