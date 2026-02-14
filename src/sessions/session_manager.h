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
    enum class Flag : uint8_t
    {
      Break = 1u << 0, // So if bit-0 was 1 in the flags then it's a break session
      Mute = 1u << 1,  // So if the bit-1 was 1 in the flags then mute is on ( 0-off )
    };
    // 8-byte / pointer-sized first
    snflake OwnerId;
    snflake ChannelId;
    snflake GuildId;

    std::vector<snflake> MembersId;
    dpp::timer TimerId;
    std::chrono::steady_clock::time_point PhaseStartTime;

    // 4-byte group
    unsigned WorkPeriod;
    unsigned BreakPeriod;
    unsigned Repeat;
    unsigned CurrentSessionNo;

    // 1-byte
    uint8_t Flags; // bit-0 for current phase , bit-1 for mute flag
    Session(
        snflake usr_id,
        snflake channel_id,
        snflake guild_id,
        std::vector<snflake> &&members_ids,
        unsigned work_period,
        unsigned break_period,
        unsigned repeat,
        uint8_t flags = 1u << 0 //
    );
    void SchedulePhase(SessionManager &Manager);
    long GetRemainingTime();
  };

  explicit SessionManager(dpp::cluster &bot);
  Session *GetSessionByOwnerId(snflake owner_id);
  Session const *GetSessionByOwnerId(snflake owner_id) const;
  Session *GetSessionByUserId(snflake usr_id);

  Session const *GetSessionByUserId(snflake usr_id) const;
  void StartTimer(
      snflake usr_id,
      snflake channel_id,
      dpp::guild *guild,
      unsigned work_period_in_min,
      unsigned break_period_in_min,
      unsigned repeat,
      uint8_t flags = 1u << 0,
      std::function<void(Session const &session)> call_back = nullptr);
  bool CancelTimer(
      snflake owner_id, std::function<void(SessionManager::Session const &session)> call_before_remove = nullptr);
  void CancelTimer(
      Session *session, std::function<void(SessionManager::Session const &session)> call_before_remove = nullptr);
  dpp::cluster &Bot;

private:
  std::unordered_map<snflake, Session> ActiveSessions;
};
#endif
