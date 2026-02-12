#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H
#include <chrono>
#include <dpp/cluster.h>
#include <dpp/snowflake.h>
#include <dpp/timer.h>
#include <unordered_map>
constexpr unsigned DefaultWorkPeriod = 10;
constexpr unsigned DefaultBreakPeriod = 10;
constexpr unsigned DefaultRepeat = 3;

class SessionManager
{
  using snflake = dpp::snowflake;

public:
  explicit SessionManager(dpp::cluster &bot);
  bool StartTimer(snflake usr_id, snflake channel_id, unsigned work_period_in_sec = DefaultWorkPeriod, unsigned break_period_in_sec = DefaultBreakPeriod, unsigned repeat = DefaultRepeat);
  bool CancelTimer(snflake OwnerId);
  dpp::cluster &Bot;

private:
  struct Session
  {
    enum class Phases
    {
      Work = 0,
      Break
    };
    using snflake = dpp::snowflake;
    snflake OwnerId;
    snflake MembersId[7];
    snflake const ChannelId;
    dpp::timer TimerId;
    unsigned const WorkPeriod;
    unsigned const BreakPeriod;
    unsigned const Repeat;
    unsigned WorkSessionDone;
    std::chrono::steady_clock::time_point StartTime;
    Phases CurrentPhase;

    Session(dpp::cluster &bot, snflake usr_id, snflake channel_id, unsigned work_period, unsigned break_period, unsigned repeat); // When we first init the obj it Saves the start time
    void SchedulePhase(SessionManager &Manager);
  };

  std::unordered_map<snflake, Session> ActiveSessions;
};
#endif
