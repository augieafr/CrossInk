#include <ReadingStatsUtils.h>
#include <gtest/gtest.h>

namespace {
constexpr uint32_t MINUTE = 60;
constexpr uint32_t RESTORE_MINUTES = READING_STREAK_RESTORE_SECONDS / MINUTE;

// A reading history plus its freeze state, driven by day-of-month in October 2026.
struct StreakHistory {
  uint32_t anchorDay = 0;
  std::array<uint8_t, READING_HISTORY_BYTES> bits{};
  ReadingStreakFreeze freeze;

  static ReadingStatsDate date(const uint8_t day) { return ReadingStatsDate{2026, 10, day}; }

  void read(const uint8_t day, const uint32_t minutes, const uint8_t hour = 9, const uint8_t minute = 0) {
    ReadingStatsDateTime start;
    start.date = date(day);
    start.hour = hour;
    start.minute = minute;
    recordReadingSpanIntoHistory(anchorDay, bits, freeze, start, minutes * MINUTE);
  }

  ReadingStreakStatus statusOn(const uint8_t day) const {
    const ReadingStatsDate today = date(day);
    return computeReadingStreakStatus(anchorDay, bits, freeze, &today);
  }
};

void expectStatus(const ReadingStreakStatus& status, const ReadingStreakState state, const uint16_t days,
                  const uint16_t freezeMinutes = 0) {
  EXPECT_EQ(status.state, state);
  EXPECT_EQ(status.days, days);
  EXPECT_EQ(status.freezeSeconds, freezeMinutes * MINUTE);
}

StreakHistory threeDayStreak() {
  StreakHistory history;
  history.read(1, 20);
  history.read(2, 15);
  history.read(3, 30);
  return history;
}
}  // namespace

TEST(ReadingStreakFreezeTest, StreakStaysActiveThroughTheDayAfterReading) {
  const StreakHistory history = threeDayStreak();

  expectStatus(history.statusOn(3), ReadingStreakState::Active, 3);
  expectStatus(history.statusOn(4), ReadingStreakState::Active, 3);
}

TEST(ReadingStreakFreezeTest, MissedDayFreezesStreakBeforeAnyReading) {
  const StreakHistory history = threeDayStreak();

  expectStatus(history.statusOn(5), ReadingStreakState::Frozen, 3);
}

TEST(ReadingStreakFreezeTest, PartialReadingKeepsStreakFrozenWithProgress) {
  StreakHistory history = threeDayStreak();
  history.read(5, 25);

  expectStatus(history.statusOn(5), ReadingStreakState::Frozen, 3, 25);
}

TEST(ReadingStreakFreezeTest, ReachingRestoreTargetRestoresStreakWithoutCountingMissedDay) {
  StreakHistory history = threeDayStreak();
  history.read(5, 25);
  history.read(5, RESTORE_MINUTES - 25, 20);

  expectStatus(history.statusOn(5), ReadingStreakState::Active, 4);

  history.read(6, 10);
  expectStatus(history.statusOn(6), ReadingStreakState::Active, 5);
  expectStatus(history.statusOn(7), ReadingStreakState::Active, 5);
}

TEST(ReadingStreakFreezeTest, UnfinishedRestoreStartsNewStreakOnFrozenDay) {
  StreakHistory history = threeDayStreak();
  history.read(5, 40);

  expectStatus(history.statusOn(6), ReadingStreakState::Active, 1);

  history.read(6, 20);
  expectStatus(history.statusOn(6), ReadingStreakState::Active, 2);
}

TEST(ReadingStreakFreezeTest, TwoMissedDaysLoseStreak) {
  StreakHistory history = threeDayStreak();

  expectStatus(history.statusOn(6), ReadingStreakState::None, 0);

  history.read(6, 90);
  expectStatus(history.statusOn(6), ReadingStreakState::Active, 1);
}

TEST(ReadingStreakFreezeTest, SecondMissWithinCooldownIsNotFrozen) {
  StreakHistory history = threeDayStreak();
  history.read(5, RESTORE_MINUTES);
  history.read(6, 10);

  // Day 7 missed; day 8 is only 3 days after the restore on day 5.
  expectStatus(history.statusOn(8), ReadingStreakState::None, 0);

  history.read(8, 90);
  expectStatus(history.statusOn(8), ReadingStreakState::Active, 1);
}

TEST(ReadingStreakFreezeTest, SecondMissAfterCooldownCanBeRestored) {
  StreakHistory history = threeDayStreak();
  history.read(5, RESTORE_MINUTES);
  for (uint8_t day = 6; day <= 10; ++day) {
    history.read(day, 10);
  }

  // Day 11 missed; day 12 is exactly 7 days after the restore on day 5.
  expectStatus(history.statusOn(12), ReadingStreakState::Frozen, 9);

  history.read(12, RESTORE_MINUTES);
  expectStatus(history.statusOn(12), ReadingStreakState::Active, 10);
}

TEST(ReadingStreakFreezeTest, ReadingBeforeAndAfterMidnightOnlyCountsFrozenDayMinutes) {
  StreakHistory history = threeDayStreak();
  // Enough reading to restore, but started so that half of it lands on day 6.
  history.read(5, RESTORE_MINUTES, 23, 60 - RESTORE_MINUTES / 2);

  EXPECT_EQ(history.freeze.lastRestoreDay, 0U);
  expectStatus(history.statusOn(6), ReadingStreakState::Active, 2);
}

TEST(ReadingStreakFreezeTest, LateReadingForMissedDayRemovesBridge) {
  StreakHistory history = threeDayStreak();
  history.read(5, RESTORE_MINUTES);
  ASSERT_EQ(history.freeze.bridgedDays, 1U);

  history.read(4, 15);

  EXPECT_EQ(history.freeze.bridgedDays, 0U);
  expectStatus(history.statusOn(5), ReadingStreakState::Active, 5);
}

TEST(ReadingStreakFreezeTest, MergeDropsBridgeForDayReadOnOtherDevice) {
  StreakHistory local = threeDayStreak();
  local.read(5, RESTORE_MINUTES);
  StreakHistory other;
  other.read(4, 15);

  mergeReadingHistory(local.anchorDay, local.bits, local.freeze, other.anchorDay, other.bits);

  EXPECT_EQ(local.freeze.bridgedDays, 0U);
  expectStatus(local.statusOn(5), ReadingStreakState::Active, 5);
}

TEST(ReadingStreakFreezeTest, MergeDoesNotBridgeNewerGapFromOtherDevice) {
  StreakHistory local = threeDayStreak();
  local.read(5, RESTORE_MINUTES);
  StreakHistory other;
  other.read(7, 15);

  mergeReadingHistory(local.anchorDay, local.bits, local.freeze, other.anchorDay, other.bits);

  // Day 6 was missed and never restored, so only day 7 counts.
  expectStatus(local.statusOn(7), ReadingStreakState::Active, 1);
}

TEST(ReadingStreakFreezeTest, MergeKeepsBridgeWhenOtherDeviceExtendsRun) {
  StreakHistory local = threeDayStreak();
  local.read(5, RESTORE_MINUTES);
  StreakHistory other;
  other.read(6, 15);

  mergeReadingHistory(local.anchorDay, local.bits, local.freeze, other.anchorDay, other.bits);

  expectStatus(local.statusOn(6), ReadingStreakState::Active, 5);
}
