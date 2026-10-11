/*
 * SPDX-FileCopyrightText: 2020 Morgan McGuire & Mara Gagiu
 * SPDX-FileCopyrightText: 2026 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: MIT
 */

#include "engine/gfx/Mmpx.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "engine/gfx/Color.h"
#include "engine/gfx/PaletteBank.h"
#include "engine/gfx/Surface.h"

namespace gfx {

namespace {

bool all_eq2(uint8_t b, uint8_t a0, uint8_t a1) { return b == a0 && b == a1; }

bool all_eq3(uint8_t b, uint8_t a0, uint8_t a1, uint8_t a2) {
  return b == a0 && b == a1 && b == a2;
}

bool all_eq4(uint8_t b, uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3) {
  return b == a0 && b == a1 && b == a2 && b == a3;
}

bool any_eq3(uint8_t b, uint8_t a0, uint8_t a1, uint8_t a2) {
  return b == a0 || b == a1 || b == a2;
}

bool none_eq2(uint8_t b, uint8_t a0, uint8_t a1) { return b != a0 && b != a1; }

bool none_eq4(uint8_t b, uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3) {
  return b != a0 && b != a1 && b != a2 && b != a3;
}

} // namespace

LumaTable luma_table(PaletteHandle palette) {
  const std::vector<Color>& colors = PaletteBank::instance().colors(palette);
  LumaTable table{};
  for (std::size_t i = 0; i < table.size() && i < colors.size(); ++i) {
    const Color& c = colors[i];
    table[i] = c.r + c.g + c.b + 1;
  }
  // Transparent weighs 256 times its color, as MMPX weighs alpha.
  table[TRANSPARENT] *= 256;
  return table;
}

void mmpx(const uint8_t* src, int src_pitch, int w, int h, uint8_t* dst,
          int dst_pitch, UpscaleEdge edge, const LumaTable& luma) {
  auto at = [&](int x, int y) -> uint8_t {
    if (x < 0 || x >= w || y < 0 || y >= h) {
      if (edge == UpscaleEdge::Blank) {
        return TRANSPARENT;
      }
      x = std::clamp(x, 0, w - 1);
      y = std::clamp(y, 0, h - 1);
    }
    return src[(y * src_pitch) + x];
  };

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const uint8_t A = at(x - 1, y - 1);
      const uint8_t B = at(x, y - 1);
      const uint8_t C = at(x + 1, y - 1);
      const uint8_t D = at(x - 1, y);
      const uint8_t E = at(x, y);
      const uint8_t F = at(x + 1, y);
      const uint8_t G = at(x - 1, y + 1);
      const uint8_t H = at(x, y + 1);
      const uint8_t I = at(x + 1, y + 1);

      uint8_t J = E;
      uint8_t K = E;
      uint8_t L = E;
      uint8_t M = E;

      if (!(all_eq4(E, A, B, C, D) && all_eq4(E, F, G, H, I))) {
        const uint8_t P = at(x, y - 2);
        const uint8_t Q = at(x - 2, y);
        const uint8_t R = at(x + 2, y);
        const uint8_t S = at(x, y + 2);
        const uint32_t Bl = luma[B];
        const uint32_t Dl = luma[D];
        const uint32_t El = luma[E];
        const uint32_t Fl = luma[F];
        const uint32_t Hl = luma[H];

        // 1:1 slope rules
        if ((D == B && D != H && D != F) && (El >= Dl || E == A) &&
            any_eq3(E, A, C, G) && ((El < Dl) || A != D || E != P || E != Q)) {
          J = D;
        }
        if ((B == F && B != D && B != H) && (El >= Bl || E == C) &&
            any_eq3(E, A, C, I) && ((El < Bl) || C != B || E != P || E != R)) {
          K = B;
        }
        if ((H == D && H != F && H != B) && (El >= Hl || E == G) &&
            any_eq3(E, A, G, I) && ((El < Hl) || G != H || E != S || E != Q)) {
          L = H;
        }
        if ((F == H && F != B && F != D) && (El >= Fl || E == I) &&
            any_eq3(E, C, G, I) && ((El < Fl) || I != H || E != R || E != S)) {
          M = F;
        }

        // Intersection rules
        if ((E != F && all_eq4(E, C, I, D, Q) && all_eq2(F, B, H)) &&
            (F != at(x + 3, y))) {
          K = M = F;
        }
        if ((E != D && all_eq4(E, A, G, F, R) && all_eq2(D, B, H)) &&
            (D != at(x - 3, y))) {
          J = L = D;
        }
        if ((E != H && all_eq4(E, G, I, B, P) && all_eq2(H, D, F)) &&
            (H != at(x, y + 3))) {
          L = M = H;
        }
        if ((E != B && all_eq4(E, A, C, H, S) && all_eq2(B, D, F)) &&
            (B != at(x, y - 3))) {
          J = K = B;
        }
        if (Bl < El && all_eq4(E, G, H, I, S) && none_eq4(E, A, D, C, F)) {
          J = K = B;
        }
        if (Hl < El && all_eq4(E, A, B, C, P) && none_eq4(E, D, G, I, F)) {
          L = M = H;
        }
        if (Fl < El && all_eq4(E, A, D, G, Q) && none_eq4(E, B, C, I, H)) {
          K = M = F;
        }
        if (Dl < El && all_eq4(E, C, F, I, R) && none_eq4(E, B, A, G, H)) {
          J = L = D;
        }

        // 2:1 slope rules
        if (H != B) {
          if (H != A && H != E && H != C) {
            if (all_eq3(H, G, F, R) && none_eq2(H, D, at(x + 2, y - 1))) {
              L = M;
            }
            if (all_eq3(H, I, D, Q) && none_eq2(H, F, at(x - 2, y - 1))) {
              M = L;
            }
          }
          if (B != I && B != G && B != E) {
            if (all_eq3(B, A, F, R) && none_eq2(B, D, at(x + 2, y + 1))) {
              J = K;
            }
            if (all_eq3(B, C, D, Q) && none_eq2(B, F, at(x - 2, y + 1))) {
              K = J;
            }
          }
        }
        if (F != D) {
          if (D != I && D != E && D != C) {
            if (all_eq3(D, A, H, S) && none_eq2(D, B, at(x + 1, y + 2))) {
              J = L;
            }
            if (all_eq3(D, G, B, P) && none_eq2(D, H, at(x + 1, y - 2))) {
              L = J;
            }
          }
          if (F != E && F != A && F != G) {
            if (all_eq3(F, C, H, S) && none_eq2(F, B, at(x - 1, y + 2))) {
              K = M;
            }
            if (all_eq3(F, I, B, P) && none_eq2(F, H, at(x - 1, y - 2))) {
              M = K;
            }
          }
        }
      }

      uint8_t* top = dst + (y * 2 * dst_pitch) + (x * 2);
      uint8_t* bottom = top + dst_pitch;
      top[0] = J;
      top[1] = K;
      bottom[0] = L;
      bottom[1] = M;
    }
  }
}

} // namespace gfx
