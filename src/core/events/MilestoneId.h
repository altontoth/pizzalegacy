/*
 * SPDX-FileCopyrightText: 2025 Cowomaly <cowomaly@nullium.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

namespace core::events {

/**
 * One milestone in a player's career: a line in their CV, and for some of them
 * a fullscreen shown when they reach it.
 *
 * The id is the line number in both original text files. LEBEN.E holds the
 * short CV label for all 139. LEVENT.E holds the fullscreen prose and runs out
 * at 97, so 100 and up only ever reach the CV. Slots 86, 93, 98 and 99 are
 * dead in the original data.
 */
enum class MilestoneId {
  LaughingStock = 1,
  Failure = 2,
  StatusWorker = 3,
  StatusSmallEntrepeneur = 4,
  StatusRestaurateur = 5,
  StatusBusinessman = 6,
  StatusSelfmadeMan = 7,
  StatusHero = 8,
  StatusStar = 9,
  StatusFreeman = 10,
  StatusMayor = 11,
  StatusSenator = 12,
  StatusPresident = 13,
  StatusTycoon = 14,

  AsGoodAsDead = 15,
  Loser = 16,
  StatusAmateur = 17,
  StatusGambler = 18,
  StatusPickpocket = 19,
  StatusThug = 20,
  StatusGanster = 21,
  StatusRacketeer = 22,
  StatusProfessional = 23,
  StatusKiller = 24,
  StatusBoss = 25,
  StatusMafiaBigWig = 26,
  StatusGodfather = 27,

  // UI headers, not events
  Career = 28,
  TargetCareer = 29,

  OpenedFirstRestaurant = 30,
  OpensSecondRestaurant = 31,
  TakesOutFirstLoan = 32,
  FinancialDifficultiesFirstTime = 33,
  HasFiveRestaurants = 34,
  Sold1KPizzasPerDay = 35,
  Sold10KPizzasPerDay = 36,
  Achieved10PctMarketShareInCity = 37,
  Achieved20PctMarketShareInCity = 38,
  Achieved50PctMarketShareInCity = 39,
  AwardedFirstStar = 40,
  FirstTwoStarRestaurant = 41,
  FirstThreeStarRestaurant = 42,
  FirstFourStarRestaurant = 43,
  FirstFiveStarRestaurant = 44,
  AchievedTurnover10KPerDay = 45,
  CreatedFirstPizza = 46,
  PizzaReachedNo1InCharts = 47,
  AbleToMeet100PctDeliveries = 48,
  AverageServiceTimeLessThan4Minutes = 49,
  FirstRestaurantInOtherCity = 50,
  OutletsInEveryWesternCity = 51,
  HasAssetsOf100K = 52,
  YouHaveAssetsOf500K = 53,
  YouHaveBecomeMillionaire = 54,
  MakesASecondMillion = 55,
  AwardInteriorOfTheYear = 56,
  AwardBiggestDumpOfAllTime = 57,
  AwardBestKidsPizza = 58,
  AwardBestYoungPersonsPizza = 59,
  AwardBestWorkersPizza = 60,
  AwardBestWhiteCollarWorkersPizza = 61,
  AwardBestBusinessPizza = 62,
  AwardBestSeniorCitizensPizza = 63,
  AwardBestPizzaOfTheYear = 64,
  AwardRestaurantOfTheYear = 65,
  FullyTrainedMasterchef = 66,
  FullyQualifiedWaiter = 67,
  ManagerIsExpertInBusinessStudies = 68,
  LosesOneMillionInOneWeek = 69,
  MakesFirstContactWithMafia = 70,
  CompletedFirstCourierService = 71,
  CompletedFirstRaidForTheUnderworld = 72,
  SuccessfullyLaunderedMoney = 73,
  CompletedFirstArmsDeal = 74,
  FirstExperienceShadyShareDealing = 75,
  FirstBribery = 76,
  Earns1MThroughShadyDeals = 77,
  YouReceivedFineFirstTime = 78,
  YouAreExecutedForMurder = 79,
  YouAreOnTheWayToPrison = 80,
  SentencedTo2050YearsHardLabour = 81,
  YourOwnPlacesBlowsUp = 82,
  YouSeeStarsAndHearPlentyOfBirdSong = 83,
  BarelyEscapedAnAttemptOnYourLife = 84,
  CoolVinnieAndBigLuigiTakeYouForABath = 85,
  UnusedArrivalInBerlin = 86,
  FirstPriceInPizzaCompetition = 87,
  NotAbleToRepayLoans = 88,
  NotAbleToPayRent = 89,
  NotAbleToPayDebts = 90,
  NotAbleToPaySuppliers = 91,
  BankruptDueToInabilityToPay = 92,
  Empty = 93,
  CarriedOutFirstAttackOnCompetitorsRestaurant = 94,
  ManagedToNegotiate50PctDiscount = 95,
  YouHaveUsed1000KiloOfIngredients = 96,
  YouHaveMadeOver3KProfitThroughWeaponDeals = 97,
  // 98-99 are unused, from here it's only in LEBEN.E:
  ArrivalInParis = 100,
  ArrivalInBerlin = 101,
  ArrivalInBaltimore = 102,
  ArrivalInVienna = 103,
  ArrivalInChicago = 104,
  ArrivalInNewYork = 105,
  ArrivalInMadrid = 106,
  ArrivalInRome = 107,
  ArrivalInStockholm = 108,
  ArrivalInLondon = 109,

  DominatesMarketInParis = 110,
  DominatesMarketInBerlin = 111,
  DominatesMarketInBaltimore = 112,
  DominatesMarketInVienna = 113,
  DominatesMarketInChicago = 114,
  DominatesMarketInNewYork = 115,
  DominatesMarketInMadrid = 116,
  DominatesMarketInRome = 117,
  DominatesMarketInStockholm = 118,
  DominatesMarketInLondon = 119,

  Used5000KgIngredientsInOneDay = 120,
  Has25KWeeksProfitInOneRestaurant = 121,
  Has50KWeeksProfitInOneRestaurant = 122,
  Won10PizzaCompetitions = 123,
  MillionaireTenTimesOver = 124,
  MillionaireTwentyTimesOver = 125,
  AwardMostBeautifulRestaurant = 126,
  AwardPizzaOfTheYearMayor = 127,
  AwardBestInvestor = 128,
  AwardBestImitator = 129,
  AwardGoldenBowTie = 130,
  AwardTransAtlanticTradePlaque = 131,
  CentralDepotFull = 132,
  Has1000CustomersPerDayInOneRestaurant = 133,
  PerfectDeliveriesFor1WeekAcross5Restaurants = 134,
  RepresentedInAllAreas = 135,
  CentralWarehouseIn5Cities = 136,
  CentralWarehouseIn10Cities = 137,
  TakesASwimInConcreteTrunks = 138,
  Has500KWeeksProfitsInOneCity = 139,
};

enum class ZwBackground {
  NONE = 0,
  BEGGING = 1,
  PRISON = 2,
  FORCED_LABOR = 3,
  EXECUTED = 4,
  BALANCE = 5,
  RESTAURANT = 6,
  POSITION = 7,
  KIDNAPPED = 8,
  SWIM_WITH_FISHES = 9,
  RICH = 10,
  PIZZA = 11,
};

// Data associated with each fullscreen message: which background image to show
// and how many score points to award the player.
struct EventData {
  ZwBackground background; // maps to gfx/zw{N}.vga; NONE = no fullscreen
  int32_t score;
};

} // namespace core::events
