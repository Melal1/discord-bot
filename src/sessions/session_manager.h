#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H
#include <chrono>
#include <dpp/cluster.h>
#include <dpp/snowflake.h>
#include <dpp/timer.h>
#include <functional>
#include <unordered_map>
#include <vector>

class SessionManager
{
  using snflake = dpp::snowflake;

public:
  struct Session
  {
    enum class Phases
    {
      Work = 0,
      Break
    };
    snflake OwnerId;
    snflake const ChannelId, GuildId;
    std::vector<snflake> MembersId;
    dpp::timer TimerId;
    unsigned const WorkPeriod, BreakPeriod, Repeat;
    unsigned CurrentSession;
    std::chrono::steady_clock::time_point StartTime;
    Phases CurrentPhase;

    Session(
        snflake usr_id,
        snflake channel_id,
        snflake guild_id,
        std::vector<snflake> &&members_ids,
        unsigned work_period,
        unsigned break_period,
        unsigned repeat); // When we first init the obj it Saves the start time
    void SchedulePhase(SessionManager &Manager);
  };

  explicit SessionManager(dpp::cluster &bot);
  Session *GetSession(snflake owner_id);
  Session const *GetSession(snflake owner_id) const;
  bool StartTimer(
      snflake usr_id,
      snflake channel_id,
      dpp::guild *guild,
      unsigned work_period_in_min,
      unsigned break_period_in_min,
      unsigned repeat,
      std::function<void(Session const &session)> call_back = nullptr);
  bool CancelTimer(
      snflake owner_id, std::function<void(SessionManager::Session const &session)> call_before_remove = nullptr);
  dpp::cluster &Bot;

private:
  std::unordered_map<snflake, Session> ActiveSessions;
};
#endif
