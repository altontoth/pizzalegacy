/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

template <typename T, typename Loader, bool UsePointer = true>
class ResourceManager;

// Pointer-based version (current default)
template <typename T, typename Loader> class ResourceManager<T, Loader, true> {
public:
  explicit ResourceManager(Loader loader) : loader_(std::move(loader)) {}

  T& get(const std::string& filename) {
    auto it = cache_.find(filename);
    if (it != cache_.end()) {
      return *it->second;
    }
    auto resource = loader_.load(filename);
    T& ref = *resource;
    cache_[filename] = std::move(resource);
    return ref;
  }

private:
  std::unordered_map<std::string, std::unique_ptr<T>> cache_;
  Loader loader_;
};

// Value-based version
template <typename T, typename Loader> class ResourceManager<T, Loader, false> {
public:
  explicit ResourceManager(Loader loader) : loader_(std::move(loader)) {}

  const T& get(const std::string& filename) {
    auto it = cache_.find(filename);
    if (it != cache_.end()) {
      return it->second;
    }
    T resource = loader_.load(filename);
    auto [iter, _] = cache_.emplace(filename, std::move(resource));
    return iter->second;
  }
  // Explicit method for when a modifiable copy is needed
  T clone(const std::string& filename) { return get(filename).clone(); }

private:
  std::unordered_map<std::string, T> cache_;
  Loader loader_;
};
