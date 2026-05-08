/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "factories/SupplierFactory.h"

#include <algorithm>
#include <ranges>

std::unordered_map<SupplierID, std::unique_ptr<const Supplier>>
    SupplierFactory::cache_ = {};

std::vector<const Supplier*> SupplierFactory::all_suppliers() {
  std::vector<const Supplier*> list;
  for (int i = static_cast<int>(SupplierID::FREDS_TILE_EMPORIUM);
       i <= static_cast<int>(SupplierID::RESTAURANT_RATIO_LTD); i++) {
    list.push_back(create_supplier(static_cast<SupplierID>(i)));
  }

  // Sort first by quality (descending), then by ID (ascending)
  std::sort(list.begin(), list.end(), [](const Supplier* a, const Supplier* b) {
    if (a->quality() != b->quality()) {
      return a->quality() < b->quality();
    }
    return static_cast<int>(a->id()) <
           static_cast<int>(b->id()); // Lower ID first as tie-breaker
  });

  return list;
}

std::vector<const Supplier*>
SupplierFactory::all_suppliers_by_type(SupplierType type, int current_year) {
  auto suppliers = all_suppliers();
  auto filtered =
      suppliers |
      std::ranges::views::filter([type, current_year](const Supplier*& s) {
        return s && s->type() == type && current_year >= s->introduction_year();
      });
  return std::vector<const Supplier*>(filtered.begin(), filtered.end());
}

const Supplier* SupplierFactory::create_supplier(SupplierID supplier_id) {
  // Check if the supplier already exists in the cache.
  auto it = cache_.find(supplier_id);
  if (it != cache_.end()) {
    return it->second.get();
  }

  std::unique_ptr<const Supplier> supplier;
  switch (supplier_id) {
  case SupplierID::FREDS_TILE_EMPORIUM:
    supplier = std::make_unique<Supplier>(
        static_cast<SupplierID>(0),   /* id */
        static_cast<SupplierType>(1), /* type */
        38,                           /* quality */
        "Fred's Tile Emporium", 2,    /* price per tile */
        std::array<uint8_t, 6>{
            /* quality */
            2,
            2,
            2,
            2,
            2,
            2,
        },
        1990, /* introduction year */
        152,  /* unknown */
        8     /* unknown */
    );
    break;
  case SupplierID::USED_TILES:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(1),   /* id */
                                   static_cast<SupplierType>(1), /* type */
                                   20,                           /* quality */
                                   "Used Tiles", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       0,
                                       0,
                                       0,
                                       0,
                                       0,
                                       0,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::APEX_TILE_CENTER:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(2),   /* id */
                                   static_cast<SupplierType>(1), /* type */
                                   50,                           /* quality */
                                   "Apex Tile Center", 4, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       8,
                                       8,
                                       6,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::CARPETS_UNLIMITED:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(3),   /* id */
                                   static_cast<SupplierType>(1), /* type */
                                   68,                           /* quality */
                                   "Carpets Unlimited", 10, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       0,
                                       2,
                                       2,
                                       9,
                                       9,
                                       9,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::EXCLUSIVE_CARPETS:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(4),   /* id */
                                   static_cast<SupplierType>(1), /* type */
                                   88,                           /* quality */
                                   "Exclusive Carpets", 16, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       6,
                                       4,
                                       5,
                                       3,
                                       2,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::USED_CARPET:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(5),   /* id */
                                   static_cast<SupplierType>(1), /* type */
                                   20,                           /* quality */
                                   "Used Carpet", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       1,
                                       3,
                                       2,
                                       7,
                                       6,
                                       5,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::EXCLUSIVE_TILES:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(6),   /* id */
                                   static_cast<SupplierType>(1), /* type */
                                   88,                           /* quality */
                                   "Exclusive Tiles", 20, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       14,
                                       15,
                                       17,
                                       20,
                                       17,
                                       12,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::CRAZY_TILES:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(7),   /* id */
                                   static_cast<SupplierType>(1), /* type */
                                   56,                           /* quality */
                                   "Crazy Tiles", 10, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       20,
                                       25,
                                       14,
                                       3,
                                       255,
                                       251,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::HANNOS_WOODEN_FLOORS:
    supplier = std::make_unique<Supplier>(
        static_cast<SupplierID>(8),   /* id */
        static_cast<SupplierType>(1), /* type */
        80,                           /* quality */
        "Hanno's Wooden Floors", 14,  /* price per tile */
        std::array<uint8_t, 6>{
            /* quality */
            6,
            5,
            10,
            12,
            18,
            15,
        },
        1990, /* introduction year */
        152,  /* unknown */
        8     /* unknown */
    );
    break;
  case SupplierID::MARBLE_FLOORS:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(9),   /* id */
                                   static_cast<SupplierType>(1), /* type */
                                   99,                           /* quality */
                                   "Marble Floors", 28, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       28,
                                       21,
                                       19,
                                       34,
                                       38,
                                       40,
                                   },
                                   1990, /* introduction year */
                                   204,  /* unknown */
                                   7     /* unknown */
        );
    break;
  case SupplierID::FOIL_COATING:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(10),  /* id */
                                   static_cast<SupplierType>(1), /* type */
                                   60,                           /* quality */
                                   "Foil Coating", 9, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       10,
                                       10,
                                       10,
                                       10,
                                       10,
                                       10,
                                   },
                                   1996, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::HOUSE_OF_STONE:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(11),  /* id */
                                   static_cast<SupplierType>(1), /* type */
                                   50,                           /* quality */
                                   "House of Stone", 10, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       8,
                                       8,
                                       5,
                                       8,
                                       5,
                                       6,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::RESTAURANT_FURNISHINGS:
    supplier = std::make_unique<Supplier>(
        static_cast<SupplierID>(12),  /* id */
        static_cast<SupplierType>(2), /* type */
        70,                           /* quality */
        "Restaurant Furnishings", 6,  /* price per tile */
        std::array<uint8_t, 6>{
            /* quality */
            2,
            3,
            2,
            1,
            1,
            1,
        },
        1990, /* introduction year */
        152,  /* unknown */
        8     /* unknown */
    );
    break;
  case SupplierID::CLASSIC_CROWN:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(13),  /* id */
                                   static_cast<SupplierType>(2), /* type */
                                   80,                           /* quality */
                                   "Classic Crown", 5, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::BARGAIN_BASEMENT:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(14),  /* id */
                                   static_cast<SupplierType>(2), /* type */
                                   40,                           /* quality */
                                   "Bargain Basement", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   204,  /* unknown */
                                   7     /* unknown */
        );
    break;
  case SupplierID::SECOND_HAND_FURNITURE:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(15),  /* id */
                                   static_cast<SupplierType>(2), /* type */
                                   20,                           /* quality */
                                   "2nd Hand Furniture", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   203,  /* unknown */
                                   7     /* unknown */
        );
    break;
  case SupplierID::GARDEN_FURNITURE_LTD:
    supplier = std::make_unique<Supplier>(
        static_cast<SupplierID>(16),  /* id */
        static_cast<SupplierType>(2), /* type */
        35,                           /* quality */
        "Garden Furniture Ltd", 1,    /* price per tile */
        std::array<uint8_t, 6>{
            /* quality */
            2,
            3,
            2,
            1,
            1,
            1,
        },
        1990, /* introduction year */
        152,  /* unknown */
        8     /* unknown */
    );
    break;
  case SupplierID::MODERN_LIVING:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(17),  /* id */
                                   static_cast<SupplierType>(2), /* type */
                                   46,                           /* quality */
                                   "Modern Living", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::MODERN_COMFORT:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(18),  /* id */
                                   static_cast<SupplierType>(2), /* type */
                                   80,                           /* quality */
                                   "Modern Comfort", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1996, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::TRENDSETTER_STUDIO:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(19),  /* id */
                                   static_cast<SupplierType>(2), /* type */
                                   70,                           /* quality */
                                   "Trendsetter Studio", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1997, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::SMALL_AND_SUNNY:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(20),  /* id */
                                   static_cast<SupplierType>(2), /* type */
                                   60,                           /* quality */
                                   "Small & Sunny", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::PURELY_NATURAL:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(21),  /* id */
                                   static_cast<SupplierType>(2), /* type */
                                   80,                           /* quality */
                                   "Purely Natural", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::ELEGANT_AND_EXCLUSIVE:
    supplier = std::make_unique<Supplier>(
        static_cast<SupplierID>(22),  /* id */
        static_cast<SupplierType>(2), /* type */
        80,                           /* quality */
        "Elegant & Exclusive", 1,     /* price per tile */
        std::array<uint8_t, 6>{
            /* quality */
            2,
            3,
            2,
            1,
            1,
            1,
        },
        1990, /* introduction year */
        152,  /* unknown */
        8     /* unknown */
    );
    break;
  case SupplierID::BUSINESS_CLASS:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(23),  /* id */
                                   static_cast<SupplierType>(2), /* type */
                                   86,                           /* quality */
                                   "Business Class", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::LUXURY_FURNISHINGS:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(24),  /* id */
                                   static_cast<SupplierType>(2), /* type */
                                   99,                           /* quality */
                                   "Luxury Furnishings", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::YOUNG_ACTION:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(25),  /* id */
                                   static_cast<SupplierType>(2), /* type */
                                   43,                           /* quality */
                                   "Young Action", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::DANIELS_DECOR:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(26),  /* id */
                                   static_cast<SupplierType>(4), /* type */
                                   50,                           /* quality */
                                   "Daniel's Decor", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::BRUNIS_BLOOMS:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(27),  /* id */
                                   static_cast<SupplierType>(4), /* type */
                                   50,                           /* quality */
                                   "Bruni's Blooms", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::VIDEOGAMES_AND_PINBALL:
    supplier = std::make_unique<Supplier>(
        static_cast<SupplierID>(28),  /* id */
        static_cast<SupplierType>(4), /* type */
        50,                           /* quality */
        "Videogames & Pinball", 1,    /* price per tile */
        std::array<uint8_t, 6>{
            /* quality */
            2,
            3,
            2,
            1,
            1,
            1,
        },
        1990, /* introduction year */
        152,  /* unknown */
        8     /* unknown */
    );
    break;
  case SupplierID::NEW_AGE_VIDEOGAMES:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(29),  /* id */
                                   static_cast<SupplierType>(4), /* type */
                                   50,                           /* quality */
                                   "New Age Videogames", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1996, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::A_AND_C_ENTERTAINMENT:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(30),  /* id */
                                   static_cast<SupplierType>(4), /* type */
                                   50,                           /* quality */
                                   "A&C Entertainment", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::ALIS_SLOT_MACHINES:
    supplier = std::make_unique<Supplier>(
        static_cast<SupplierID>(31),  /* id */
        static_cast<SupplierType>(4), /* type */
        50,                           /* quality */
        "Ali's Slot Machines", 1,     /* price per tile */
        std::array<uint8_t, 6>{
            /* quality */
            2,
            3,
            2,
            1,
            1,
            1,
        },
        1990, /* introduction year */
        152,  /* unknown */
        8     /* unknown */
    );
    break;
  case SupplierID::OVENS:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(32),  /* id */
                                   static_cast<SupplierType>(3), /* type */
                                   30,                           /* quality */
                                   "Ovens", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   208,  /* unknown */
                                   7     /* unknown */
        );
    break;
  case SupplierID::PIZZA_OVENS:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(33),  /* id */
                                   static_cast<SupplierType>(3), /* type */
                                   50,                           /* quality */
                                   "Pizza Ovens", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   205,  /* unknown */
                                   7     /* unknown */
        );
    break;
  case SupplierID::ELECTRIC_OVENS:
    supplier =
        std::make_unique<Supplier>(static_cast<SupplierID>(34),  /* id */
                                   static_cast<SupplierType>(3), /* type */
                                   80,                           /* quality */
                                   "Electric Ovens", 1, /* price per tile */
                                   std::array<uint8_t, 6>{
                                       /* quality */
                                       2,
                                       3,
                                       2,
                                       1,
                                       1,
                                       1,
                                   },
                                   1990, /* introduction year */
                                   152,  /* unknown */
                                   8     /* unknown */
        );
    break;
  case SupplierID::RESTAURANT_RATIO_LTD:
    supplier = std::make_unique<Supplier>(
        static_cast<SupplierID>(35),  /* id */
        static_cast<SupplierType>(3), /* type */
        80,                           /* quality */
        "Restaurant Ratio Ltd", 1,    /* price per tile */
        std::array<uint8_t, 6>{
            /* quality */
            2,
            3,
            2,
            1,
            1,
            1,
        },
        1996, /* introduction year */
        152,  /* unknown */
        8     /* unknown */
    );
    break;
  }

  auto supplier_ptr = supplier.get();
  if (supplier) {
    cache_[supplier_id] = std::move(supplier);
  }
  return supplier_ptr;
}
