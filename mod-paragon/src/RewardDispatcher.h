#pragma once

#include "Define.h"

class Player;

namespace mod_paragon
{
    // Handles coins + milestone rewards, ensuring idempotency per character.
    // Coin item and quartermaster NPC ids come from ParagonMgr config.
    class RewardDispatcher
    {
    public:
        // Send missing rewards up to the account's last_reward_level.
        static void DeliverPendingFor(Player* player);

        // Deliver a specific paragon level's rewards immediately.
        static void DeliverLevelTo(Player* player, uint32 level);

        // Mail the Paragon Handbook (once-per-account gate lives in
        // ParagonMgr::MaybeSendHandbook).
        static void SendHandbookTo(Player* player);

    private:
        static bool HasDelivered(uint32 guid, uint32 level);
        static void MarkDelivered(uint32 guid, uint32 level);
    };
}
