/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "core/supply/Supplier.h"

#include <memory>
#include <unordered_map>
#include <vector>

class SupplierFactory {
public:
  static const Supplier* create_supplier(SupplierID);
  static std::vector<const Supplier*> all_suppliers();
  static std::vector<const Supplier*> all_suppliers_by_type(SupplierType);

private:
  static std::unordered_map<SupplierID, std::unique_ptr<const Supplier>> cache_;
};
