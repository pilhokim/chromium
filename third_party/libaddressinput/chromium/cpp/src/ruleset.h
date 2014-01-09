// Copyright (C) 2014 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef I18N_ADDRESSINPUT_RULESET_H_
#define I18N_ADDRESSINPUT_RULESET_H_

#include <libaddressinput/util/basictypes.h>
#include <libaddressinput/util/scoped_ptr.h>

#include <map>
#include <string>

namespace i18n {
namespace addressinput {

class Rule;

// A recursive data structure that stores a set of rules for a region. Can store
// the rules for a country, its administrative areas, localities, and dependent
// localities, in addition to the language-specific rules.
//
// Example for Canada and some of its provinces:
//                   CA-->fr
//                   |
// -------------------------------------
// |        |        |        |        |
// v        v        v        v        v
// AB-->fr  BC-->fr  MB-->fr  NB-->fr  NL-->fr
//
// The rules in Canada are in English by default. Each rule also has a French
// language version.
class Ruleset {
 public:
  // Builds a ruleset with a region-wide |rule| in the default language of the
  // country. The |rule| should not be NULL.
  explicit Ruleset(scoped_ptr<Rule> rule);

  ~Ruleset();

  // Returns the region-wide rule in the default language of the country.
  const Rule& rule() const { return *rule_.get(); }

  // Adds and the |ruleset| for |sub_region|.
  void AddSubRegionRuleset(const std::string& sub_region,
                           scoped_ptr<Ruleset> ruleset);

  // Adds a language-specific |rule| for |language_code| for this region.
  void AddLanguageCodeRule(const std::string& language_code,
                           scoped_ptr<Rule> rule);

 private:
  // The region-wide rule in the default language of the country.
  scoped_ptr<const Rule> rule_;

  // Owned rulesets for sub-regions.
  std::map<std::string, Ruleset*> sub_regions_;

  // Owned language-specific rules for the region.
  std::map<std::string, const Rule*> language_codes_;

  DISALLOW_COPY_AND_ASSIGN(Ruleset);
};

}  // namespace addressinput
}  // namespace i18n

#endif  // I18N_ADDRESSINPUT_RULESET_H_