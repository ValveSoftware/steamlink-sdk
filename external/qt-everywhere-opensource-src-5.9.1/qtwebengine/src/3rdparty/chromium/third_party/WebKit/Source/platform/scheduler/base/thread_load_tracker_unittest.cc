#include "platform/scheduler/base/thread_load_tracker.h"

#include "base/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace blink {
namespace scheduler {

namespace {

void AddToVector(std::vector<std::pair<base::TimeTicks, double>>* vector,
                 base::TimeTicks time,
                 double load) {
  vector->push_back({time, load});
}

base::TimeTicks SecondsToTime(int seconds) {
  return base::TimeTicks() + base::TimeDelta::FromSeconds(seconds);
}

base::TimeTicks MillisecondsToTime(int milliseconds) {
  return base::TimeTicks() + base::TimeDelta::FromMilliseconds(milliseconds);
}

}  // namespace

TEST(ThreadLoadTrackerTest, RecordTasks) {
  std::vector<std::pair<base::TimeTicks, double>> result;

  ThreadLoadTracker thread_load_tracker(
      SecondsToTime(1), base::Bind(&AddToVector, base::Unretained(&result)),
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(10));

  // We should discard first ten seconds of information.
  thread_load_tracker.RecordTaskTime(SecondsToTime(1), SecondsToTime(3));

  thread_load_tracker.RecordTaskTime(MillisecondsToTime(11300),
                                     MillisecondsToTime(11400));

  thread_load_tracker.RecordTaskTime(MillisecondsToTime(12900),
                                     MillisecondsToTime(13100));

  thread_load_tracker.RecordTaskTime(MillisecondsToTime(13700),
                                     MillisecondsToTime(13800));

  thread_load_tracker.RecordTaskTime(MillisecondsToTime(14500),
                                     MillisecondsToTime(16500));

  thread_load_tracker.RecordIdle(MillisecondsToTime(17500));

  // Because of 10-second delay we're getting information starting with 11s.
  EXPECT_THAT(result, ElementsAre(std::make_pair(SecondsToTime(11), 0),
                                  std::make_pair(SecondsToTime(12), 0.1),
                                  std::make_pair(SecondsToTime(13), 0.1),
                                  std::make_pair(SecondsToTime(14), 0.2),
                                  std::make_pair(SecondsToTime(15), 0.5),
                                  std::make_pair(SecondsToTime(16), 1.0),
                                  std::make_pair(SecondsToTime(17), 0.5)));
}

TEST(ThreadLoadTrackerTest, PauseAndResume) {
  std::vector<std::pair<base::TimeTicks, double>> result;

  ThreadLoadTracker thread_load_tracker(
      SecondsToTime(1), base::Bind(&AddToVector, base::Unretained(&result)),
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromSeconds(10));

  thread_load_tracker.RecordTaskTime(SecondsToTime(3), SecondsToTime(4));
  thread_load_tracker.Pause(SecondsToTime(5));
  // This task should be ignored.
  thread_load_tracker.RecordTaskTime(SecondsToTime(10), SecondsToTime(11));
  thread_load_tracker.Resume(SecondsToTime(15));
  thread_load_tracker.RecordTaskTime(MillisecondsToTime(20900),
                                     MillisecondsToTime(21100));

  thread_load_tracker.RecordIdle(MillisecondsToTime(23500));

  thread_load_tracker.Pause(MillisecondsToTime(23500));
  thread_load_tracker.Resume(SecondsToTime(26));

  thread_load_tracker.RecordTaskTime(MillisecondsToTime(26100),
                                     MillisecondsToTime(26200));
  thread_load_tracker.RecordIdle(MillisecondsToTime(27500));

  EXPECT_THAT(result, ElementsAre(std::make_pair(SecondsToTime(21), 0.1),
                                  std::make_pair(SecondsToTime(22), 0.1),
                                  std::make_pair(SecondsToTime(23), 0),
                                  std::make_pair(SecondsToTime(27), 0.1)));
}

}  // namespace scheduler
}  // namespace blink
