﻿#pragma once

#include "system/angband.h"

enum class RaceBlowEffectType;
enum class RaceBlowMethodType;
class PlayerType;
struct monster_type;
struct object_type;
class MonsterAttackPlayer {
public:
    MonsterAttackPlayer(PlayerType *player_ptr, short m_idx);
#ifdef JP
    int abbreviate = 0; // 2回目以降の省略表現フラグ.
#endif
    short m_idx;
    monster_type *m_ptr;
    RaceBlowMethodType method;
    RaceBlowEffectType effect;
    bool do_silly_attack;
    concptr act = nullptr;
    int do_cut = 0;
    int do_stun = 0;
    bool touched = false;
    bool explode = false;
    DEPTH rlev = 0;
    GAME_TEXT m_name[MAX_NLEN]{};
    int d_dice = 0;
    int d_side = 0;
    object_type *o_ptr = nullptr;
    bool obvious = false;
    HIT_POINT damage = 0;
    bool blinked = false;
    GAME_TEXT o_name[MAX_NLEN]{};
    HIT_POINT get_damage = 0;
    GAME_TEXT ddesc[MAX_MONSTER_NAME]{};
    ARMOUR_CLASS ac = 0;
    bool alive = true;
    bool fear = false;

    void make_attack_normal();

private:
    PlayerType *player_ptr;

    static int stat_value(const int raw);
    bool check_no_blow();
    bool process_monster_blows();
    void postprocess_monster_blows();
    bool check_monster_continuous_attack();
    bool process_monster_attack_hit();
    bool effect_protecion_from_evil();
    void describe_silly_attacks();
    void select_cut_stun();
    void calc_player_cut();
    void process_player_stun();
    void monster_explode();
    void describe_attack_evasion();
    void gain_armor_exp();
    void process_monster_attack_evasion();
    void increase_blow_type_seen(const int ap_cnt);
};
