/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "Globals/ObjectMgr.h"
#include "MotionGenerators/TargetedMovementGenerator.h" 

struct Replenishment : public SpellScript
{
    void OnInit(Spell* spell) const override
    {
        spell->SetMaxAffectedTargets(10);
        spell->SetFilteringScheme(EFFECT_INDEX_0, false, SCHEME_PRIORITIZE_MANA);
    }

    bool OnCheckTarget(const Spell* spell, Unit* target, SpellEffectIndex /*eff*/) const override
    {
        Unit* caster = spell->GetCaster();
        if (caster->GetMap()->IsBattleArena()) // in arenas only hits caster
            if (target != caster)
                return false;

        return true;
    }
};

struct RetaliationDummyCreature : public AuraScript
{
    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& procData) const override
    {
        // check attack comes not from behind
        if (procData.victim->IsFacingTargetsBack(procData.attacker))
            return SPELL_AURA_PROC_FAILED;

        procData.victim->CastSpell(procData.attacker, 65934, TRIGGERED_IGNORE_HIT_CALCULATION | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_COSTS);
        return SPELL_AURA_PROC_OK;
    }
};

struct Shadowmeld : public SpellScript
{
    void OnCast(Spell* spell) const override
    {
        Unit* caster = spell->GetCaster();
        caster->CastSpell(nullptr, 62196, TRIGGERED_OLD_TRIGGERED);
        std::vector<Unit*> removal;
        for (auto& data : caster->getHostileRefManager())
        {
            if (data.getSource()->getThreatList().size() == 1)
                removal.push_back(data.getSource()->getOwner());
        }
        for (Unit* enemy : removal)
            enemy->getThreatManager().modifyThreatPercent(caster, -101);
    }
};

struct StoicismAbsorb : public AuraScript
{
    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        if (data.caster)
            value = data.caster->GetMaxHealth() * 0.20f;
        return value;
    }
};

// 64568 - Blood Reserve
struct BloodReserveEnchant : public AuraScript
{
    bool OnCheckProc(Aura* aura, ProcExecutionData& /*data*/) const override
    {
        if (aura->GetTarget()->GetHealthPercent() >= 35.f)
            return false;
        return true;
    }

    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& procData) const override
    {
        procData.triggeredSpellId = 64569;
        procData.triggerTarget = nullptr;

        // need scale damage base at stack size
        if (SpellEntry const* trigEntry = sSpellTemplate.LookupEntry<SpellEntry>(procData.triggeredSpellId))
            procData.basepoints[EFFECT_INDEX_0] = trigEntry->CalculateSimpleValue(EFFECT_INDEX_0) * aura->GetStackAmount();

        aura->GetTarget()->RemoveAurasDueToSpell(64568); // uses stacks so wont autoremove itself
        return SPELL_AURA_PROC_OK;
    }
};

// 5782 - Fear, 339 - Entangling Roots, 122 - Frost Nova, 51514 - Hex
struct AutoBreakProc : public AuraScript
{
    void OnAuraInit(Aura* aura) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_1)
        {
            Unit* caster = aura->GetCaster();
            // fear - rank 3 - at 80 - 2600
            // not impacted by gear
            // not impacted by target level
            // not impacted by rank
            // asumption - depends on caster level
            uint32 damageThreshold = 0;
            auto stats = sObjectMgr.GetCreatureClassLvlStats(caster->GetLevel(), CLASS_WARRIOR, EXPANSION_WOTLK);

            switch (aura->GetId()) // keeping them separate for future research
            {
                case 5782: // Fear
                case 6213:
                case 6215:
                    damageThreshold = stats->BaseHealth / 4.75;
                    break;
                case 51514: // Hex
                    damageThreshold = stats->BaseHealth / 4.75;
                    break;
                case 339: // Entangling Roots
                case 1062:
                case 5195:
                case 5196:
                case 9852:
                case 9853:
                case 26989:
                case 53308:
                    damageThreshold = stats->BaseHealth / 4.75;
                    break;
                case 122: // Frost nova
                case 865:
                case 6131:
                case 10230:
                case 27088:
                case 42917:
                    damageThreshold = stats->BaseHealth / 4.75;
                    break;
            }
            if (Aura* script = aura->GetCaster()->GetOverrideScript(7801)) // Glyph of Fear, Glyph of Hex, Glyph of Entangling Roots, Glyph of Frost Nova
                damageThreshold *= script->GetAmount() / 100;
            aura->SetScriptValue(damageThreshold);
        }
    }

    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& procData) const override
    {
        if (aura->GetEffIndex() != EFFECT_INDEX_1)
            return SPELL_AURA_PROC_OK;

        if (int32(aura->GetScriptValue()) - procData.damage <= 0)
            return SPELL_AURA_PROC_OK;

        aura->SetScriptValue(aura->GetScriptValue() - procData.damage);
        return SPELL_AURA_PROC_CANT_TRIGGER;
    }
};


enum
{
    // quest 25229
    SPELL_MOTIVATE_73943                      = 73943, // motivate-a-tron summon
    SPELL_MOTIVATE_73952                      = 73952, // 20 yards (Short) Apply Aura: Change Model (-1)
    SPELL_MOTIVATE_74035                      = 74035, // motivate-a-tron dummy spell
    SPELL_MOTIVATE_74080                      = 74080, // motivate-a-tron summon, kill credit
    SPELL_MOTIVATE_73953                      = 73953, // 20 yards (Short) - DUMMY
    SPELL_MOTIVATE_75078                      = 75078, // 20 yards (Short) - DUMMY
    SPELL_MOTIVATE_75086                      = 75086, // TROLL QUEST - summon, kill credit
    SPELL_MOTIVATE_75088                      = 75088, // TROLL QUEST - summon, kill credit
    SPELL_MOTIVATE_74046                      = 74046, // change model rabbit (aura)
    //SPELL_MOTIVATE_74071                      = 74071, // Apply Aura - Pet: Dummy
    //SPELL_MOTIVATE_74062                      = 74062, // unlimited range
    SPELL_MOTIVATED                           = 74034, // motivated spell (SPELL_EFFECT_APPLY_AURA))

    NPC_MOTIVATED_CITIZEN                     = 39466, // second required creature_template ID
    NPC_GNOME_CITIZEN                         = 39623, // first required creature_template ID
    NPC_GNOME_MOTIVATED                       = 39624, // not used
};

// 74035 - Motivate
struct Motivate : public SpellScript
{
    SpellCastResult OnCheckCast(Spell* spell, bool /*strict*/) const override
    {
        Unit* target = spell->m_targets.getUnitTarget();
        if (!target || target->GetEntry() != NPC_GNOME_CITIZEN)
            return SPELL_FAILED_BAD_TARGETS;
        return SPELL_CAST_OK;
    }

    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* caster = spell->GetCaster();
        Unit* target = spell->GetUnitTarget();
        if (!caster->IsPlayer())
            return;

        if (roll_chance_i(10))
        {
            // transform into rabbit
            caster->CastSpell(target, SPELL_MOTIVATE_74046, TRIGGERED_NONE);
            return;
        }
        static_cast<Creature*>(target)->UpdateEntry(NPC_MOTIVATED_CITIZEN);
        target->SetDisplayId(NPC_GNOME_CITIZEN);
        target->CastSpell(nullptr, SPELL_MOTIVATED, TRIGGERED_NONE);
        target->GetMotionMaster()->MoveFollow(caster, frand(0.5f, 3.0f), frand(M_PI_F * 0.8f, M_PI_F * 1.2f));

        static_cast<Player*>(caster)->KilledMonsterCredit(NPC_GNOME_CITIZEN);
    }
};

// 74034 - Motivated
struct Motivated : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        Unit* caster = aura->GetCaster();
        Unit* target = aura->GetTarget();
        Creature* gnome = static_cast<Creature*>(caster);
        if (apply)
        {
            aura->ForcePeriodicity(1 * IN_MILLISECONDS); // tick every second
            // cast dummy spell to trigger text
            caster->CastSpell(nullptr, SPELL_MOTIVATE_73953, TRIGGERED_NONE);
        }
        else
        {
            caster->GetMotionMaster()->MoveIdle();
            //caster->StopMoving();
            gnome->SetRespawnDelay(1s, true);
            gnome->ForcedDespawn(5000);
        }
    }

    void OnPeriodicTickEnd(Aura* aura) const override
    {
        Unit* caster = aura->GetCaster();
        Creature* gnome = static_cast<Creature*>(caster);
        
        MotionMaster* creatureMotion = caster->GetMotionMaster();
        FollowMovementGenerator const* mgen = static_cast<FollowMovementGenerator const*>(creatureMotion->top());
        Player* pPlayer = nullptr;
        if (mgen->GetCurrentTarget() && mgen->GetCurrentTarget()->GetTypeId() == TYPEID_PLAYER)
        {
            pPlayer = static_cast<Player*>(mgen->GetCurrentTarget());
        }

        if (!mgen->GetCurrentTarget() || (pPlayer && pPlayer->GetQuestStatus(25229) == QUEST_STATUS_NONE))
        {
            gnome->RemoveAura(aura);
            gnome->SetRespawnDelay(1s, true);
            gnome->ForcedDespawn(2000);
            return;
        }

        CreatureList captainTreadInRange;
        GetCreatureListWithEntryInGrid(captainTreadInRange, caster, 39675, 5.0f);
        for (const auto& tread : captainTreadInRange)
        {
            static_cast<Player*>(pPlayer)->KilledMonsterCredit(NPC_MOTIVATED_CITIZEN);
            // cast dummy spell to trigger text
            caster->CastSpell(nullptr, SPELL_MOTIVATE_75078, TRIGGERED_NONE);

            caster->GetMotionMaster()->MoveIdle();
            caster->StopMoving();
            gnome->RemoveAura(aura);
            gnome->SetRespawnDelay(1s, true);
            gnome->ForcedDespawn(2000);
        }
        
    }
};

void AddSC_spell_scripts_wotlk()
{
    RegisterSpellScript<Replenishment>("spell_replenishment");
    RegisterSpellScript<RetaliationDummyCreature>("spell_retaliation_dummy_creature");
    RegisterSpellScript<Shadowmeld>("spell_shadowmeld");
    RegisterSpellScript<StoicismAbsorb>("spell_stoicism");
    RegisterSpellScript<BloodReserveEnchant>("spell_blood_reserve_enchant");
    RegisterSpellScript<AutoBreakProc>("spell_auto_break_proc");
    RegisterSpellScript<Motivate>("spell_motivate");
    RegisterSpellScript<Motivated>("spell_motivated");
}