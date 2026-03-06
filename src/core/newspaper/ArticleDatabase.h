/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace core::newspaper {

// Individual article entry from ARTIKEL0.E or ARTIKEL1.E
struct Article {
  int article_id{0}; // 1-50
  std::string title;
  std::string body;
  int image_id{0}; // 0 = no image, 1-20 = image number
};

// Template for ingredient trend articles (ARTIKEL3.E)
// Contains placeholders like "#" that get replaced with ingredient names
struct IngredientArticleTemplate {
  int template_id{0};         // Index into ARTIKEL3.E
  std::string title_template; // e.g., "# is in!"
  std::string body_template;  // e.g., "If you eat #, you're IN!"
};

// Manages loading and accessing newspaper articles from original game files
class ArticleDatabase {
public:
  ArticleDatabase();

  // Load article files from original game data
  void load_articles();

  // Get a specific article by ID (1-50), searching ARTIKEL0 then ARTIKEL1
  const Article* get_article(int article_id) const;

  // Get article from a specific file (0=ARTIKEL0.E, 1=ARTIKEL1.E)
  const Article* get_article(int file_id, int article_id) const;

  // Get ingredient trend template by ID
  const IngredientArticleTemplate*
  get_ingredient_template(int template_id) const;

  // Format an ingredient article by replacing # with ingredient name.
  // category (0-3) and polarity (+1/-1) select the template pair from
  // ARTIKEL3.E.
  Article format_ingredient_article(int category, int8_t polarity,
                                    std::string_view ingredient_name) const;

  // Get image ID for an article (from byte_773A0 lookup table)
  int get_image_id(int file_id, int article_id) const;

private:
  // Parse a single article file (ARTIKEL0.E, ARTIKEL1.E)
  void parse_article_file(const std::string& filename, int file_id);

  // Parse ingredient template file (ARTIKEL3.E)
  void parse_ingredient_templates(const std::string& filename);

  // Extract individual article from file buffer
  bool extract_article(const char* data, size_t data_size, int article_number,
                       std::string& title, std::string& body);

  // Article storage: file_id (0 or 1) -> article_id (1-50) -> Article
  std::map<int, std::map<int, Article>> articles_;

  // Ingredient templates from ARTIKEL3.E
  std::vector<IngredientArticleTemplate> ingredient_templates_;

  // Image lookup tables
  // First 50 bytes: ARTIKEL0.E (flavor text), second 50: ARTIKEL1.E
  // (event articles).
  static constexpr std::array<int, 50> flavor_image_map_ = {
      0, 0, 9, 4, 0,  0, 0, 6, 5,  0, // Articles 1-10
      0, 4, 2, 0, 10, 3, 4, 0, 0,  0, // Articles 11-20
      0, 1, 0, 0, 0,  0, 8, 0, 0,  0, // Articles 21-30
      0, 0, 0, 0, 0,  0, 0, 0, 10, 3, // Articles 31-40
      0, 0, 7, 0, 0,  0, 0, 0, 0,  0, // Articles 41-50
  };
  static constexpr std::array<int, 50> event_image_map_ = {
      13, 13, 0,  0,  15, 0,  14, 19, 19, 19, // Articles 1-10
      19, 16, 20, 0,  14, 16, 20, 11, 19, 19, // Articles 11-20
      17, 17, 18, 12, 13, 14, 13, 11, 12, 13, // Articles 21-30
      14, 14, 14, 14, 14, 14, 14, 14, 14, 13, // Articles 31-40
      14, 13, 14, 14, 14, 14, 14, 14, 14, 14, // Articles 41-50
  };
};

} // namespace core::newspaper
