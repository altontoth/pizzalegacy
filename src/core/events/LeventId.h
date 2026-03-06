#pragma once

#include <cstdint>

namespace core::events {

/**
 * Message IDs for one-time fullscreen status updates.
 * These correspond to the original game's message system that displays
 * achievements, warnings, and game events with appropriate backgrounds.
 *
 * Values match the original game's message ID encoding.
 */
enum class LeventId {
  // Tutorial/Introduction messages (0x00-0x0B) - Background 7
  LaughingStock = 0,
  Failure = 1,
  StatusWorker = 2,
  StatusSmallEntrepeneur = 3,
  StatusRestaurateur = 4,
  StatusBusinessman = 5,
  StatusSelfmadeMan = 6,
  StatusHero = 7,
  StatusStar = 8,
  StatusFreeman = 9,
  StatusMayor = 10,
  StatusSenator = 11,
  StatusPresident = 12,
  StatusTycoon = 13,
  AsGoodAsDead = 14,
  Loser = 15,
  StatusAmateur = 16,
  StatusGambler = 17,
  StatusPickpocket = 18,
  StatusThug = 19,
  StatusGanster = 20,
  StatusRacketeer = 21,
  StatusProfessional = 22,
  StatusKiller = 23,
  StatusBoss = 24,
  StatusMafiaBigWig = 25,
  StatusGodfather = 26,
  Career = 27,
  TargetCareer = 28,
  OpenedFirstRestaurant = 29,
  OpensSecondRestaurant = 30,
  TakesOutFirstLoan = 31,
  FinancialDifficultiesFirstTime = 32,
  HasFiveRestaurants = 33,
  Sold1KPizzasPerDay = 34,
  Sold10KPizzasPerDay = 35,
  Achieved10PctMarketShareInCity = 36,
  Achieved20PctMarketShareInCity = 37,
  Achieved50PctMarketShareInCity = 38,
  AwardedFirstStar = 39,
  FirstTwoStarRestaurant = 40,
  FirstThreeStarRestaurant = 41,
  FirstFourStarRestaurant = 42,
  FirstFiveStarRestaurant = 43,
  AchievedTurnover10KPerDay = 44,
  CreatedFirstPizza = 45,
  PizzaReachedNo1InCharts = 46,
  AbleToMeet100PctDeliveries = 47,
  AverageServiceTimeLessThan4Minutes = 48,
  FirstRestaurantInOtherCity = 49,
  OutletsInEveryWesternCity = 50,
  HasAssetsOf100K = 51,
  YouHaveAssetsOf500K = 52,
  YouHaveBecomeMillionaire = 53,
  MakesASecondMillion = 54,
  AwardInteriorOfTheYear = 55,
  AwardBiggestDumpOfAllTime = 56,
  AwardBestKidsPizza = 57,
  AwardBestYoungPersonsPizza = 58,
  AwardBestWorkersPizza = 59,
  AwardBestWhiteCollarWorkersPizza = 60,
  AwardBestBusinessPizza = 61,
  AwardBestSeniorCitizensPizza = 62,
  AwardBestPizzaOfTheYear = 63,
  AwardRestaurantOfTheYear = 64,
  FullyTrainedMasterchef = 65,
  FullyQualifiedWaiter = 66,
  ManagerIsExpertInBusinessStudies = 67,
  LosesOneMillionInOneWeek = 68,
  MakesFirstContactWithMafia = 69,
  CompletedFirstMissionForTheUnderworld = 70,
  CompletedFirstRaidForTheUnderworld = 71,
  SuccessfullyLaunderedMoney = 72,
  CompletedFirstArmsDeal = 73,
  FirstExperienceShadyShareDealing = 74,
  FirstBribery = 75,
  Earns1MThroughShadyDeals = 76,
  YouReceivedFineFirstTime = 77,
  YouAreExecutedForMurder = 78,
  YouAreOnTheWayToPrison = 79,
  SentencedTo2050YearsHardLabour = 80,
  YourOwnPlacesBlowsUp = 81,
  YouSeeStarsAndHearPlentyOfBirdSong = 82,
  BarelyEscapedAnAttemptOnYourLife = 83,
  CoolVinnieAndBigLuigiTakeYouForABath = 84,
  ArrivalInBerlin = 85,
  FirstPriceInPizzaCompetition = 86,
  NotAbleToRepayLoans = 87,
  NotAbleToPayRent = 88,
  NotAbleToPayDebts = 89,
  NotAbleToPaySuppliers = 90,
  BankruptDueToInabilityToPay = 91,
  Empty = 92,
  CarriedOutFirstAttackOnCompetitorsRestaurant = 93,
  ManagedToNegotiate50PctDiscount = 94,
  YouHaveUsed1000KiloOfIngredients = 95,
  YouHaveMadeOver3KProfitThroughWeaponDeals = 96,
  Count = 97, // 100 messages (0-99)
};

/**
 * Data associated with each fullscreen message.
 * This includes the background image to use, score points to award,
 * and text colors for rendering.
 */
struct FullscreenMessageData {
  uint8_t background_index; // 1-11, maps to zw{N}.vga files
  uint16_t score_points;    // Score/reputation points to award player
  uint8_t text_fg_color;    // Foreground color for text
  uint8_t text_bg_color;    // Background color for text

  FullscreenMessageData(uint8_t bg_idx = 0, uint16_t score = 0,
                        uint8_t fg_color = 0xFF, uint8_t bg_color = 0x00)
      : background_index(bg_idx), score_points(score), text_fg_color(fg_color),
        text_bg_color(bg_color) {}
};

} // namespace core::events
