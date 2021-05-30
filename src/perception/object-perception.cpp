﻿#include "perception/object-perception.h"
#include "flavor/flavor-describer.h"
#include "flavor/object-flavor-types.h"
#include "game-option/play-record-options.h"
#include "io/write-diary.h"
#include "object-enchant/item-feeling.h"
#include "object-enchant/special-object-flags.h"
#include "object-enchant/trg-types.h"
#include "object/item-tester-hooker.h" // 暫定、このファイルへ引っ越す.
#include "object/object-kind.h"
#include "system/object-type-definition.h"
#include "system/player-type-definition.h"

/*!
 * @brief オブジェクトを鑑定済にする /
 * Known is true when the "attributes" of an object are "known".
 * @param o_ptr 鑑定済にするオブジェクトの構造体参照ポインタ
 * These include tohit, todam, toac, cost, and pval (charges).\n
 *\n
 * Note that "knowing" an object gives you everything that an "awareness"\n
 * gives you, and much more.  In fact, the player is always "aware" of any\n
 * item of which he has full "knowledge".\n
 *\n
 * But having full knowledge of, say, one "wand of wonder", does not, by\n
 * itself, give you knowledge, or even awareness, of other "wands of wonder".\n
 * It happens that most "identify" routines (including "buying from a shop")\n
 * will make the player "aware" of the object as well as fully "know" it.\n
 *\n
 * This routine also removes any inscriptions generated by "feelings".\n
 */
void object_known(object_type *o_ptr)
{
    o_ptr->feeling = FEEL_NONE;
    o_ptr->ident &= ~(IDENT_SENSE);
    o_ptr->ident &= ~(IDENT_EMPTY);
    o_ptr->ident |= (IDENT_KNOWN);
}

/*!
 * @brief オブジェクトを＊鑑定＊済にする /
 * The player is now aware of the effects of the given object.
 * @param owner_ptr プレーヤーへの参照ポインタ
 * @param o_ptr ＊鑑定＊済にするオブジェクトの構造体参照ポインタ
 */
void object_aware(player_type *owner_ptr, object_type *o_ptr)
{
    const bool is_already_awared = object_is_aware(o_ptr);

    k_info[o_ptr->k_idx].aware = true;

    // 以下、playrecordに記録しない場合はreturnする
    if (!record_ident)
        return;

    if (is_already_awared || owner_ptr->is_dead)
        return;

    // アーティファクト専用ベースアイテムは記録しない
    if (k_info[o_ptr->k_idx].gen_flags.has(TRG::INSTA_ART))
        return;

    // 未鑑定名の無いアイテムは記録しない
    if (!((o_ptr->tval >= TV_AMULET && o_ptr->tval <= TV_POTION) || o_ptr->tval == TV_FOOD))
        return;

    // playrecordに識別したアイテムを記録
    object_type forge;
    object_type *q_ptr;
    GAME_TEXT o_name[MAX_NLEN];

    q_ptr = &forge;
    q_ptr->copy_from(o_ptr);

    q_ptr->number = 1;
    describe_flavor(owner_ptr, o_name, q_ptr, OD_NAME_ONLY);

    exe_write_diary(owner_ptr, DIARY_FOUND, 0, o_name);
}

/*!
 * @brief オブジェクトを試行済にする /
 * Something has been "sampled"
 * @param o_ptr 試行済にするオブジェクトの構造体参照ポインタ
 */
void object_tried(object_type *o_ptr) { k_info[o_ptr->k_idx].tried = true; }

/*
 * @brief 与えられたオブジェクトのベースアイテムが鑑定済かを返す / Determine if a given inventory item is "aware"
 * @param o_ptr オブジェクトへの参照ポインタ
 * @return 鑑定済ならTRUE
 */
bool object_is_aware(object_type *o_ptr) { return k_info[(o_ptr)->k_idx].aware; }

/*
 * Determine if a given inventory item is "tried"
 */
bool object_is_tried(object_type *o_ptr) { return k_info[(o_ptr)->k_idx].tried; }

/*
 * Determine if a given inventory item is "known"
 * Test One -- Check for special "known" tag
 * Test Two -- Check for "Easy Know" + "Aware"
 */
bool object_is_known(object_type *o_ptr) { return ((o_ptr->ident & IDENT_KNOWN) != 0) || (k_info[(o_ptr)->k_idx].easy_know && k_info[(o_ptr)->k_idx].aware); }

bool object_is_fully_known(object_type *o_ptr) { return (o_ptr->ident & IDENT_FULL_KNOWN) != 0; }
