/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "core/newspaper/ArticleDatabase.h"
#include <cstring>
#include "engine/Fatal.h"
#include "engine/FormatCompat.h"
#include "resources/util/FileUtils.h"

namespace core::newspaper {

ArticleDatabase::ArticleDatabase() {}

void ArticleDatabase::load_articles() {
  // Load the two main article files
  parse_article_file("Z/ARTIKEL0.E", 0); // Flavor text articles
  parse_article_file("Z/ARTIKEL1.E", 1); // Event-driven articles

  // Load ingredient trend templates
  parse_ingredient_templates("T/ARTIKEL3.E");
}

void ArticleDatabase::parse_article_file(const std::string& filename,
                                         int file_id) {
  // Load file into memory
  auto file_stream = fileutils::open(filename);

  // Read entire file into buffer
  std::vector<char> buffer;
  buffer.resize(15000); // Assembly allocates 15000 bytes

  file_stream.read(buffer.data(), buffer.size());
  size_t bytes_read = file_stream.gcount();

  if (bytes_read == 0) {
    return;
  }

  // Parse each article (up to 50 per file)
  for (int article_num = 0; article_num < 50; ++article_num) {
    std::string title;
    std::string body;

    if (extract_article(buffer.data(), bytes_read, article_num, title, body)) {
      Article article;
      article.article_id = article_num + 1; // 1-based indexing
      article.title = title;
      article.body = body;
      article.image_id = get_image_id(file_id, article.article_id);

      articles_[file_id][article.article_id] = article;
    }
  }
}

void ArticleDatabase::parse_ingredient_templates(const std::string& filename) {
  auto file_stream = fileutils::open(filename);
  if (!file_stream.is_open()) {
    return;
  }

  // ARTIKEL3.E uses a simple alternating-line format (unlike ARTIKEL0/1.E):
  //   line 1: title template (e.g. "is in!")
  //   line 2: body template with # placeholders
  //   line 3: next title...
  //   line 4: next body...
  // 8 templates total: 4 categories x 2 polarities (positive, negative)
  int template_num = 0;
  std::string title_line;
  std::string body_line;

  while (std::getline(file_stream, title_line) &&
         std::getline(file_stream, body_line)) {
    // Trim trailing \r from Windows-style line endings
    if (!title_line.empty() && title_line.back() == '\r') {
      title_line.pop_back();
    }
    if (!body_line.empty() && body_line.back() == '\r') {
      body_line.pop_back();
    }

    if (title_line.empty() && body_line.empty()) {
      continue;
    }

    IngredientArticleTemplate tmpl;
    tmpl.template_id = template_num;
    tmpl.title_template = title_line;
    tmpl.body_template = body_line;

    ingredient_templates_.push_back(tmpl);
    template_num++;
  }
}

bool ArticleDatabase::extract_article(const char* data, size_t data_size,
                                      int article_number, std::string& title,
                                      std::string& body) {
  // Format:
  //   § Title text !
  //   $ Body text
  //   § Next article...

  const char* ptr = data;
  const char* end = data + data_size;

  // Find the Nth article by counting § markers
  int count = 0;
  while (ptr < end && count < article_number) {
    if (*ptr == '\xA7') { // § marker (0xA7)
      count++;
    }
    ptr++;
  }

  // If we didn't find enough articles, return false
  if (count < article_number || ptr >= end) {
    return false;
  }

  // Skip to next § if not already there
  while (ptr < end && *ptr != '\xA7') {
    ptr++;
  }

  if (ptr >= end) {
    return false;
  }

  // Skip the § marker and any whitespace
  ptr++;
  while (ptr < end && (*ptr == ' ' || *ptr == '\t')) {
    ptr++;
  }

  // Extract title (until ! marker)
  const char* title_start = ptr;
  while (ptr < end && *ptr != '!') {
    ptr++;
  }

  if (ptr >= end) {
    return false;
  }

  // Copy title (and trim)
  const char* title_end = ptr;
  while (title_end > title_start &&
         (*(title_end - 1) == ' ' || *(title_end - 1) == '\t')) {
    title_end--;
  }
  title.assign(title_start, title_end);

  // Skip past ! and find $ marker
  ptr++;
  while (ptr < end && *ptr != '$') {
    ptr++;
  }

  if (ptr >= end) {
    return false;
  }

  // Skip $ and any whitespace
  ptr++;
  while (ptr < end &&
         (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) {
    ptr++;
  }

  // Extract body (until next § or end of file)
  const char* body_start = ptr;
  while (ptr < end && *ptr != '\xA7') {
    ptr++;
  }

  // Copy body, converting newlines to spaces and trimming
  const char* body_end = ptr;
  while (body_end > body_start &&
         (*(body_end - 1) == ' ' || *(body_end - 1) == '\n' ||
          *(body_end - 1) == '\r')) {
    body_end--;
  }

  // Build body string, replacing newlines with spaces
  body.clear();
  for (const char* p = body_start; p < body_end; ++p) {
    if (*p == '\n' || *p == '\r') {
      if (!body.empty() && body.back() != ' ') {
        body += ' ';
      }
    }
    else {
      body += *p;
    }
  }

  return !title.empty() || !body.empty();
}

const Article* ArticleDatabase::get_article(int file_id, int article_id) const {
  auto file_it = articles_.find(file_id);
  if (file_it != articles_.end()) {
    auto article_it = file_it->second.find(article_id);
    if (article_it != file_it->second.end()) {
      return &article_it->second;
    }
  }
  return nullptr;
}

const IngredientArticleTemplate*
ArticleDatabase::get_ingredient_template(int template_id) const {
  if (template_id >= 0 &&
      template_id < static_cast<int>(ingredient_templates_.size())) {
    return &ingredient_templates_[template_id];
  }
  return nullptr;
}

Article ArticleDatabase::format_ingredient_article(
    int category, int8_t polarity, std::string_view ingredient_name) const {
  Article result;
  result.article_id = 200; // Special flag for ingredient articles
  result.image_id = 14;

  // Templates are stored as pairs: positive then negative for each category
  int template_id = (category * 2) + (polarity < 0 ? 1 : 0);

  const auto* tmpl = get_ingredient_template(template_id);
  if (!tmpl) {
    fatal("Unable to get ingredient template!");
  }

  result.title = compat::format("{} {}", ingredient_name, tmpl->title_template);
  result.body = tmpl->body_template;

  // Templates use # as ingredient placeholder
  size_t pos;
  while ((pos = result.body.find('#')) != std::string::npos) {
    result.body.replace(pos, 1, ingredient_name);
  }

  return result;
}

int ArticleDatabase::get_image_id(int file_id, int article_id) const {
  if (article_id >= 1 && article_id <= 50) {
    const auto& table = (file_id == 1) ? event_image_map_ : flavor_image_map_;
    return table[article_id - 1];
  }
  return 0;
}

} // namespace core::newspaper
