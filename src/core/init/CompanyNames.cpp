/*
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/init/CompanyNames.h"

#include "core/util/Constants.h"
#include "engine/Random.h"

namespace init {
std::vector<std::string> random_company_names() {
  std::vector<std::string> company_names = {
      "Pizza King",   "Pizza World", "Pizza Land",   "Fast Slime",
      "Mac Fat",      "Paradiso",    "KF Pizza",     "Pizza-U-Eat",
      "Pizza 2000",   "Kwik Pizza",  "Salmo Pizza",  "Geno-Pizza",
      "Panda Pizza",  "Piz Perfect", "Dino-Pizza",   "Thai Ching",
      "Pizza Parlor", "Ripley's",    "Rick's Diner", "SoftRock Pizza"};

  engine::random::shuffle(company_names.begin(), company_names.end());
  company_names.resize(NUMBER_OF_PLAYERS);
  return company_names;
}
} // namespace init
