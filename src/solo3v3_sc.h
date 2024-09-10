/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "ArenaTeamMgr.h"
#include "BattlegroundMgr.h"
#include "DisableMgr.h"
#include "Chat.h"
#include "Language.h"
#include "ScriptedGossip.h"
#include "Config.h"
#include "Battleground.h"
#include "solo3v3.h"


enum Npc3v3Actions {
    NPC_3v3_ACTION_CREATE_ARENA_TEAM = 1,
    NPC_3v3_ACTION_JOIN_QUEUE_ARENA_RATED = 2,
    NPC_3v3_ACTION_LEAVE_QUEUE = 3,
    NPC_3v3_ACTION_GET_STATISTICS = 4,
    NPC_3v3_ACTION_DISBAND_ARENATEAM = 5,
    NPC_3v3_ACTION_SCRIPT_INFO = 8
};


class NpcSolo3v3 : public CreatureScript
{
public:
    NpcSolo3v3() : CreatureScript("npc_solo3v3")
    {
        Initialize();
    }

    void Initialize();
    bool OnGossipHello(Player* player, Creature* creature) override;
    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override;
    bool ArenaCheckFullEquipAndTalents(Player* player);
    bool JoinQueueArena(Player* player, Creature* creature, bool isRated);

private:
    bool CreateArenateam(Player* player, Creature* creature);
    void fetchQueueList();

    int cache3v3Queue[MAX_TALENT_CAT];
    uint32 lastFetchQueueList;
};

class Solo3v3BG : public AllBattlegroundScript
{
public:
    Solo3v3BG() : AllBattlegroundScript("Solo3v3_BG") {}

    uint32 oldTeamRatingAlliance;
    uint32 oldTeamRatingHorde;

    void OnQueueUpdate(BattlegroundQueue* queue, uint32 /*diff*/, BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket_id, uint8 arenaType, bool isRated, uint32 /*arenaRatedTeamId*/) override;
    void OnBattlegroundUpdate(Battleground* bg, uint32 /*diff*/) override;
    bool OnQueueUpdateValidity(BattlegroundQueue* /* queue */, uint32 /*diff*/, BattlegroundTypeId /* bgTypeId */, BattlegroundBracketId /* bracket_id */, uint8 arenaType, bool /* isRated */, uint32 /*arenaRatedTeamId*/) override;
    void OnBattlegroundDestroy(Battleground* bg) override;
    void OnBattlegroundEndReward(Battleground* bg, Player* player, TeamId /* winnerTeamId */) override;
};

class ConfigLoader3v3Arena : public WorldScript
{
public:
    ConfigLoader3v3Arena() : WorldScript("config_loader_3v3_arena") {}

    virtual void OnAfterConfigLoad(bool /*Reload*/) override;
};

class Team3v3arena : public ArenaTeamScript
{
public:
    Team3v3arena() : ArenaTeamScript("team_3v3_arena") {}

    void OnGetSlotByType(const uint32 type, uint8& slot) override;
    void OnGetArenaPoints(ArenaTeam* at, float& points) override;
    void OnTypeIDToQueueID(const BattlegroundTypeId, const uint8 arenaType, uint32& _bgQueueTypeId) override;
    void OnQueueIdToArenaType(const BattlegroundQueueTypeId _bgQueueTypeId, uint8& arenaType) override;
};

class PlayerScript3v3Arena : public PlayerScript
{
public:
    PlayerScript3v3Arena() : PlayerScript("player_script_3v3_arena") {}

    void OnLogin(Player* pPlayer) override;
    void OnGetArenaPersonalRating(Player* player, uint8 slot, uint32& rating) override;
    void OnGetMaxPersonalArenaRatingRequirement(const Player* player, uint32 minslot, uint32& maxArenaRating) const override;
    void OnGetArenaTeamId(Player* player, uint8 slot, uint32& result) override;
    bool NotSetArenaTeamInfoField(Player* player, uint8 slot, ArenaTeamInfoType type, uint32 value) override;
    bool CanBattleFieldPort(Player* player, uint8 arenaType, BattlegroundTypeId BGTypeID, uint8 action) override;
};


class Arena_SC : public ArenaScript
{
public:
    Arena_SC() : ArenaScript("Arena_SC") { }

    bool CanAddMember(ArenaTeam* team, ObjectGuid /* playerGuid */) override
    {
        if (!team)
            return false;

        if (!team->GetMembersSize())
            return true;

        if (/* team->GetType() == ARENA_TEAM_1v1 || */ team->GetType() == ARENA_TEAM_SOLO_3v3)
            return false;

        return true;
    }

    void OnGetPoints(ArenaTeam* team, uint32 /* memberRating */, float& points) override
    {
        if (!team)
            return;

        // if (team->GetType() == ARENA_TEAM_1v1)
        //     points *= sConfigMgr->GetOption<float>("Azth.Rate.Arena1v1", 0.40f);

        if (team->GetType() == ARENA_TEAM_SOLO_3v3)
            points *= sConfigMgr->GetOption<float>("Solo.3v3.ArenaPointsMulti", 0.88f);
    }

    bool CanSaveToDB(ArenaTeam* team) override
    {
        if (team->GetId() >= MAX_ARENA_TEAM_ID)
using namespace Acore::ChatCommands;

class CommandJoinSolo : public CommandScript
{
public:
    CommandJoinSolo() : CommandScript("CommandJoinSolo") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable SoloCommandTable =
        {
            { "qsolo", HandleQueueSoloArena,           SEC_PLAYER, Console::No }, // command .qsolo
            { "create soloteam", HandleCreateSoloTeam, SEC_PLAYER, Console::No }  // command .create soloteam (works)
        };

        return SoloCommandTable;
    }

    static bool HandleQueueSoloArena(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (sConfigMgr->GetOption<bool>("Solo.3v3.Enable", true) == false)
        {
            ChatHandler(player->GetSession()).SendSysMessage("Solo 3v3 Arena is disabled.");
            return true;
        }
        if (player->IsInCombat())
        {
            ChatHandler(player->GetSession()).SendSysMessage("Can't be in combat.");
            return false;
        }

        NpcSolo3v3 SoloCommand;
        if (player->HasAura(26013) && (sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnAfk", true) || sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnLeave", true)))
        {
            WorldPacket data;
            sBattlegroundMgr->BuildGroupJoinedBattlegroundPacket(&data, ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
            player->GetSession()->SendPacket(&data);
            return false;
        }

        // Use this when not testing
        /*
        if (!SoloCommand.ArenaCheckFullEquipAndTalents(player))
        {
            return false;
        }
        if (SoloCommand.JoinQueueArenaCommand(player, true))
        {
            handler->PSendSysMessage("You have joined the solo 3v3 arena queue.");
        }
        else 
        {
            //handler->PSendSysMessage("Join failed.");
        }*/


        // TESTING PURPOSES!
        // Queues 3v3 Solo for all players (saving time)
        for (auto& pair : ObjectAccessor::GetPlayers())
        {
            Player* currentPlayer = pair.second; // Access the Player* directly from the pair
            if (currentPlayer)
            {
                if (!SoloCommand.ArenaCheckFullEquipAndTalents(currentPlayer))
                {
                    continue;
                }
                //bool NpcSolo3v3::JoinQueueArena(Player* player, Creature* creature, bool isRated)
                if (SoloCommand.JoinQueueArena(currentPlayer, nullptr, true))
                //if (SoloCommand.JoinQueueArenaCommand(currentPlayer, true))
                {
                    handler->PSendSysMessage("Player {} has joined the solo 3v3 arena queue.", currentPlayer->GetName().c_str());
                }
                else
                {
                    handler->PSendSysMessage("Failed to join queue for player {}.", currentPlayer->GetName().c_str());
                }
            }
        }

        return true;
    }

    static bool HandleCreateSoloTeam(ChatHandler* handler, const char* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        /*
        if (player->GetArenaTeamId(ArenaTeam::GetSlotByType(ARENA_TEAM_SOLO_3v3)))
        {
            handler->PSendSysMessage("You already have a solo 3v3 team.");
            return false;
        }*/
        if (sConfigMgr->GetOption<uint32>("Solo.3v3.MinLevel", 80) <= player->GetLevel())
        {
            if (!player)
                return false;

            uint8 slot = ArenaTeam::GetSlotByType(ARENA_TEAM_SOLO_3v3);

            // Check if player is already in an arena team
            if (player->GetArenaTeamId(slot))
            {
                player->GetSession()->SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, player->GetName(), "", ERR_ALREADY_IN_ARENA_TEAM);
                return false;
            }

            // if team name exist, we have to choose another name (playername + number)
            int i = 1;
            std::stringstream teamName;
            teamName << player->GetName();

            do
            {
                if (sArenaTeamMgr->GetArenaTeamByName(teamName.str()) != NULL) // teamname exist, so choose another name
                {
                    teamName.str(std::string());
                    teamName << player->GetName() << i++;
                }
                else
                    break;
            } while (i < 100); // should never happen

            // Create arena team
            ArenaTeam* arenaTeam = new ArenaTeam();

            if (!arenaTeam->Create(player->GetGUID(), uint8(ARENA_TEAM_SOLO_3v3 /*ARENA_TYPE_3v3_SOLO*/), teamName.str(), 4283124816, 45, 4294242303, 5, 4294705149))
            {
                delete arenaTeam;
                return false;
            }

            // Register arena team
            sArenaTeamMgr->AddArenaTeam(arenaTeam);

            ChatHandler(player->GetSession()).SendSysMessage("Arena team successful created!");

            return true;
        }
        else
        {
            ChatHandler(player->GetSession()).PSendSysMessage("You need level {}+ to create an arena team.", sConfigMgr->GetOption<uint32>("Solo.3v3.MinLevel", 80));
            return false;
        }

        return true;
    }
};

void AddSC_Solo_3v3_Arena()
{
    // ArenaSlotByType
    // if (!ArenaTeam::ArenaSlotByType.count(ARENA_TEAM_1v1))
    //     ArenaTeam::ArenaSlotByType[ARENA_TEAM_1v1] = ARENA_SLOT_1v1;

    if (!ArenaTeam::ArenaSlotByType.count(ARENA_TEAM_SOLO_3v3))
        ArenaTeam::ArenaSlotByType[ARENA_TEAM_SOLO_3v3] = ARENA_SLOT_SOLO_3v3;

    // ArenaReqPlayersForType
    // if (!ArenaTeam::ArenaReqPlayersForType.count(ARENA_TYPE_1v1))
    //     ArenaTeam::ArenaReqPlayersForType[ARENA_TYPE_1v1] = 2;

    if (!ArenaTeam::ArenaReqPlayersForType.count(ARENA_TYPE_3v3_SOLO))
        ArenaTeam::ArenaReqPlayersForType[ARENA_TYPE_3v3_SOLO] = 6;

    // queueToBg
    // if (!BattlegroundMgr::queueToBg.count(BATTLEGROUND_QUEUE_1v1))
    //     BattlegroundMgr::queueToBg[BATTLEGROUND_QUEUE_1v1] = BATTLEGROUND_AA;

    if (!BattlegroundMgr::queueToBg.count(BATTLEGROUND_QUEUE_3v3_SOLO))
        BattlegroundMgr::queueToBg[BATTLEGROUND_QUEUE_3v3_SOLO] = BATTLEGROUND_AA;

    // ArenaTypeToQueue
    // if (!BattlegroundMgr::ArenaTypeToQueue.count(ARENA_TYPE_1v1))
    //     BattlegroundMgr::ArenaTypeToQueue[ARENA_TYPE_1v1] = (BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_1v1;

    if (!BattlegroundMgr::ArenaTypeToQueue.count(ARENA_TYPE_3v3_SOLO))
        BattlegroundMgr::ArenaTypeToQueue[ARENA_TYPE_3v3_SOLO] = (BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO;

    // QueueToArenaType
    // if (!BattlegroundMgr::QueueToArenaType.count(BATTLEGROUND_QUEUE_1v1))
    //     BattlegroundMgr::QueueToArenaType[BATTLEGROUND_QUEUE_1v1] = (ArenaType)ARENA_TYPE_1v1;

    if (!BattlegroundMgr::QueueToArenaType.count(BATTLEGROUND_QUEUE_3v3_SOLO))
        BattlegroundMgr::QueueToArenaType[BATTLEGROUND_QUEUE_3v3_SOLO] = (ArenaType)ARENA_TYPE_3v3_SOLO;

    new NpcSolo3v3();
    new Solo3v3BG();
    new Team3v3arena();
    new ConfigLoader3v3Arena();
    new PlayerScript3v3Arena();
    new Arena_SC();
    new CommandJoinSolo();
}
