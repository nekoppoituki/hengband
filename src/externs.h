﻿/*!
 * @file externs.h
 * @brief Angband(変愚蛮怒)基本関数、グローバル変数ヘッダファイル / 
 * extern declarations (variables and functions)
 * @date 2014/08/08
 * @author
 * Copyright (c) 1997 Ben Harrison
 * @details
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.
 * Note that some files have their own header files
 * (z-virt.h, z-util.h, z-form.h, term.h, random.h)
 */
#include "geometry.h"
#include "grid.h"
#include "floor-save.h"
#include "monster.h"
#include "monsterrace.h"
#include "object.h"
#include "player-status.h"

/* tables.c */

extern const byte adj_mag_study[];
extern const byte adj_mag_mana[];
extern const byte adj_mag_fail[];
extern const byte adj_mag_stat[];
extern const byte adj_chr_gold[];
extern const byte adj_int_dev[];
extern const byte adj_wis_sav[];
extern const byte adj_dex_dis[];
extern const byte adj_int_dis[];
extern const byte adj_dex_ta[];
extern const byte adj_str_td[];
extern const byte adj_dex_th[];
extern const byte adj_str_th[];
extern const byte adj_str_wgt[];
extern const byte adj_str_hold[];
extern const byte adj_str_dig[];
extern const byte adj_dex_safe[];
extern const byte adj_con_fix[];
extern const byte adj_con_mhp[];
extern const byte adj_chr_chm[];

/* variable.c */

extern bool repair_monsters;
extern bool repair_objects;



/*
 * Software options (set via the '=' command).  See "tables.c"
 */

extern STR_OFFSET quark__num;
extern concptr *quark__str;

extern player_type *p_ptr;

extern MONSTER_IDX pet_t_m_idx;
extern MONSTER_IDX riding_t_m_idx;
extern MONRACE_IDX today_mon;
