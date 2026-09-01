#include "TravelPhaseAdapter.h"

#include "PhaseMgr.h"

#include "AreaDefines.h"
#include "Chat.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Transport.h"

#include <cstdint>

namespace
{
    constexpr std::uint32_t MAP_OUTLAND_ID = 530;
    constexpr std::uint32_t MAP_NORTHREND_ID = 571;

    /*
     * Naxxramas usa técnicamente Parent=571, pero Grim
     * reutiliza map 533 para Naxx40 antes de WotLK.
     *
     * Por eso no puede heredarse automáticamente el gate
     * global de Northrend.
     */
    constexpr std::uint32_t MAP_NAXXRAMAS_ID = 533;

    /*
     * Estas dos áreas son usadas por el gate histórico de
     * Individual Progression para las zonas iniciales TBC,
     * pero no existen actualmente en AreaDefines.h.
     */
    constexpr std::uint32_t AREA_VEILED_SEA_ID = 3479;
    constexpr std::uint32_t AREA_AMMEN_VALE_ID = 3526;

    enum class TravelRestriction
    {
        None,
        OutlandPhase,
        OutlandLevel,
        NorthrendPhase,
        NorthrendLevel
    };

    bool IsTbcStarterZone(std::uint32_t zoneId)
    {
        switch (zoneId)
        {
            case AREA_EVERSONG_WOODS:
            case AREA_GHOSTLANDS:
            case AREA_SILVERMOON_CITY:
            case AREA_AZUREMYST_ISLE:
            case AREA_BLOODMYST_ISLE:
            case AREA_THE_EXODAR:
            case AREA_AMMEN_VALE_ID:
            case AREA_VEILED_SEA_ID:
                return true;

            default:
                return false;
        }
    }

    bool IsBypassed(Player* player)
    {
        return !player ||
               !sPhaseMgr.IsEnabled() ||
               (sPhaseMgr.IsGMBypassEnabled() &&
                player->IsGameMaster());
    }

    std::uint32_t ResolveExpansionMapId(
        std::uint32_t mapId)
    {
        /*
         * Naxxramas es una excepción deliberada.
         *
         * Aunque instance_template.Parent sea Northrend,
         * map 533 también representa Naxx40 dentro de Grim.
         * Su disponibilidad histórica sigue perteneciendo
         * a Individual Progression.
         */
        if (mapId == MAP_NAXXRAMAS_ID)
            return mapId;

        InstanceTemplate const* instanceTemplate =
            sObjectMgr->GetInstanceTemplate(mapId);

        if (!instanceTemplate)
            return mapId;

        if (instanceTemplate->Parent == MAP_OUTLAND_ID ||
            instanceTemplate->Parent == MAP_NORTHREND_ID)
        {
            return instanceTemplate->Parent;
        }

        return mapId;
    }

    TravelRestriction GetRestriction(
        Player* player,
        std::uint32_t mapId,
        float x,
        float y,
        float z)
    {
        if (!player)
            return TravelRestriction::None;

        PhaseDefinition const& phase =
            sPhaseMgr.GetActiveDefinition();

        /*
         * Para una instancia de expansión utilizamos el Parent
         * como autoridad global.
         *
         * Ejemplos:
         *   Hellfire Ramparts 543 -> 530
         *   Black Temple      564 -> 530
         *   Utgarde Keep      574 -> 571
         *   Ulduar            603 -> 571
         *
         * Naxxramas 533 queda excluido deliberadamente en
         * ResolveExpansionMapId().
         */
        std::uint32_t const expansionMapId =
            ResolveExpansionMapId(mapId);

        /*
         * NORTHREND
         *
         * El continente y sus instancias obedecen la apertura
         * global de Northrend.
         *
         * El requisito de nivel 68 pertenece únicamente al
         * viaje directo al continente. Las instancias conservan
         * sus propios requisitos de nivel del core/contenido.
         */
        if (expansionMapId == MAP_NORTHREND_ID)
        {
            if (!phase.northrendEnabled)
                return TravelRestriction::NorthrendPhase;

            if (mapId == MAP_NORTHREND_ID &&
                player->GetLevel() < 68)
            {
                return TravelRestriction::NorthrendLevel;
            }

            return TravelRestriction::None;
        }

        /*
         * OUTLAND / EXPANSIÓN TBC
         *
         * Las zonas iniciales Blood Elf/Draenei de map 530
         * permanecen accesibles incluso antes de abrir Outland.
         *
         * Las instancias con Parent=530 no son starter zones y
         * sí obedecen la disponibilidad global de la expansión.
         */
        if (expansionMapId == MAP_OUTLAND_ID)
        {
            if (mapId == MAP_OUTLAND_ID)
            {
                std::uint32_t zoneId =
                    sMapMgr->GetZoneId(
                        player->GetPhaseMask(),
                        mapId,
                        x,
                        y,
                        z);

                if (IsTbcStarterZone(zoneId))
                    return TravelRestriction::None;
            }

            if (!phase.outlandEnabled)
                return TravelRestriction::OutlandPhase;

            /*
             * Igual que Northrend: el mínimo 58 es requisito
             * del viaje directo a Outland, no del Parent de
             * cada instancia TBC.
             */
            if (mapId == MAP_OUTLAND_ID &&
                player->GetLevel() < 58)
            {
                return TravelRestriction::OutlandLevel;
            }

            return TravelRestriction::None;
        }

        return TravelRestriction::None;
    }

    char const* GetRestrictionMessage(
        TravelRestriction restriction)
    {
        switch (restriction)
        {
            case TravelRestriction::OutlandPhase:
                return
                    "Terrallende aun no esta disponible "
                    "en la fase actual del servidor.";

            case TravelRestriction::OutlandLevel:
                return
                    "Necesitas ser al menos nivel 58 "
                    "para viajar a Terrallende.";

            case TravelRestriction::NorthrendPhase:
                return
                    "Rasganorte aun no esta disponible "
                    "en la fase actual del servidor.";

            case TravelRestriction::NorthrendLevel:
                return
                    "Necesitas ser al menos nivel 68 "
                    "para viajar a Rasganorte.";

            case TravelRestriction::None:
            default:
                return "";
        }
    }

    void SendBlockedMessage(
        Player* player,
        char const* message)
    {
        if (!player || !player->GetSession())
            return;

        ChatHandler handler(player->GetSession());
        handler.SendNotification(message);
    }

    void SafelyRemoveFromTransport(Player* player)
    {
        if (!player)
            return;

        Transport* transport = player->GetTransport();

        if (!transport)
            return;

        transport->RemovePassenger(
            player,
            true);

        player->RepopAtGraveyard();
    }

    bool BlockTravel(
        Player* player,
        TravelRestriction restriction)
    {
        if (player && player->GetTransport())
            SafelyRemoveFromTransport(player);

        SendBlockedMessage(
            player,
            GetRestrictionMessage(restriction));

        return false;
    }
}

namespace TravelPhaseAdapter
{
    bool CanTeleport(
        Player* player,
        std::uint32_t mapId,
        float x,
        float y,
        float z)
    {
        if (IsBypassed(player))
            return true;

        TravelRestriction restriction =
            GetRestriction(
                player,
                mapId,
                x,
                y,
                z);

        if (restriction == TravelRestriction::None)
            return true;

        return BlockTravel(
            player,
            restriction);
    }

    bool EnsureValidLocation(Player* player)
    {
        if (IsBypassed(player))
            return true;

        TravelRestriction restriction =
            GetRestriction(
                player,
                player->GetMapId(),
                player->GetPositionX(),
                player->GetPositionY(),
                player->GetPositionZ());

        if (restriction == TravelRestriction::None)
            return true;

        /*
         * Usamos la ubicación inicial real de la raza/clase
         * como destino seguro.
         *
         * Esto evita hardcodear Stormwind/Orgrimmar y además
         * mantiene correctamente Blood Elf y Draenei.
         */
        PlayerInfo const* startInfo =
            sObjectMgr->GetPlayerInfo(
                player->getRace(),
                player->getClass());

        if (!startInfo)
        {
            LOG_ERROR(
                "module",
                "PhaseProgression: login safety no pudo "
                "obtener PlayerInfo para {} (race={}, class={}).",
                player->GetName(),
                static_cast<unsigned>(player->getRace()),
                static_cast<unsigned>(player->getClass()));

            return false;
        }

        std::uint32_t oldMap =
            player->GetMapId();

        bool const teleported =
            player->TeleportTo(
                startInfo->mapId,
                startInfo->positionX,
                startInfo->positionY,
                startInfo->positionZ,
                startInfo->orientation);

        if (!teleported)
        {
            LOG_ERROR(
                "module",
                "PhaseProgression: login safety no pudo "
                "reubicar a {} desde map {}.",
                player->GetName(),
                oldMap);

            return false;
        }

        if (player->GetSession())
        {
            ChatHandler handler(player->GetSession());

            handler.SendNotification(
                "Tu ubicacion anterior no esta disponible "
                "en la fase actual. Has sido trasladado "
                "a una zona segura.");
        }

        LOG_INFO(
            "module",
            "PhaseProgression: login safety reubico a {} "
            "desde map {} hacia map {}. Motivo={}.",
            player->GetName(),
            oldMap,
            startInfo->mapId,
            GetRestrictionMessage(restriction));

        return true;
    }
}
