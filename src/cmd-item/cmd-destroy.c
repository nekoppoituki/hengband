#include "cmd-item/cmd-destroy.h"
#include "autopick/autopick-registry.h"
#include "autopick/autopick.h"
#include "core/asking-player.h"
#include "core/stuff-handler.h"
#include "core/window-redrawer.h"
#include "flavor/flavor-describer.h"
#include "flavor/object-flavor-types.h"
#include "floor/floor-object.h"
#include "game-option/input-options.h"
#include "inventory/inventory-object.h"
#include "inventory/inventory-slot-types.h"
#include "io/input-key-acceptor.h"
#include "io/input-key-requester.h"
#include "main/sound-definitions-table.h"
#include "main/sound-of-music.h"
#include "object-hook/hook-expendable.h"
#include "object-hook/hook-magic.h"
#include "object/item-use-flags.h"
#include "object/object-generator.h"
#include "object/object-stack.h"
#include "object/object-value.h"
#include "player/attack-defense-types.h"
#include "player/avatar.h"
#include "player/special-defense-types.h"
#include "racial/racial-android.h"
#include "realm/realm-names-table.h"
#include "status/action-setter.h"
#include "status/experience.h"
#include "system/object-type-definition.h"
#include "term/screen-processor.h"
#include "util/int-char-converter.h"
#include "view/display-messages.h"

typedef struct destroy_type {
    OBJECT_IDX item;
    QUANTITY amt;
    QUANTITY old_number;
    bool force;
    object_type *o_ptr;
    object_type *q_ptr;
    GAME_TEXT o_name[MAX_NLEN];
    char out_val[MAX_NLEN + 40];
} destroy_type;

destroy_type *initialize_destroy_type(destroy_type *destroy_ptr, object_type *o_ptr)
{
    destroy_ptr->amt = 1;
    destroy_ptr->force = FALSE;
    destroy_ptr->q_ptr = o_ptr;
    return destroy_ptr;
}

/*!
 * @brief アイテムを破壊するコマンドのメインルーチン / Destroy an item
 * @param creature_ptr プレーヤーへの参照ポインタ
 * @return なし
 */
void do_cmd_destroy(player_type *creature_ptr)
{
    if (creature_ptr->special_defense & KATA_MUSOU)
        set_action(creature_ptr, ACTION_NONE);

    object_type forge;
    destroy_type tmp_destroy;
    destroy_type *destroy_ptr = initialize_destroy_type(&tmp_destroy, &forge);
    if (command_arg > 0)
        destroy_ptr->force = TRUE;

    concptr q = _("どのアイテムを壊しますか? ", "Destroy which item? ");
    concptr s = _("壊せるアイテムを持っていない。", "You have nothing to destroy.");
    destroy_ptr->o_ptr = choose_object(creature_ptr, &destroy_ptr->item, q, s, USE_INVEN | USE_FLOOR, 0);
    if (destroy_ptr->o_ptr == NULL)
        return;

    if (!destroy_ptr->force && (confirm_destroy || (object_value(creature_ptr, destroy_ptr->o_ptr) > 0))) {
        describe_flavor(creature_ptr, destroy_ptr->o_name, destroy_ptr->o_ptr, OD_OMIT_PREFIX);
        sprintf(destroy_ptr->out_val, _("本当に%sを壊しますか? [y/n/Auto]", "Really destroy %s? [y/n/Auto]"), destroy_ptr->o_name);
        msg_print(NULL);
        message_add(destroy_ptr->out_val);
        creature_ptr->window |= PW_MESSAGE;
        handle_stuff(creature_ptr);
        while (TRUE) {
            prt(destroy_ptr->out_val, 0, 0);
            char i = inkey();
            prt("", 0, 0);
            if (i == 'y' || i == 'Y')
                break;

            if (i == ESCAPE || i == 'n' || i == 'N')
                return;

            if (i == 'A') {
                if (autopick_autoregister(creature_ptr, destroy_ptr->o_ptr))
                    autopick_alter_item(creature_ptr, destroy_ptr->item, TRUE);

                return;
            }
        }
    }

    if (destroy_ptr->o_ptr->number > 1) {
        destroy_ptr->amt = get_quantity(NULL, destroy_ptr->o_ptr->number);
        if (destroy_ptr->amt <= 0)
            return;
    }

    destroy_ptr->old_number = destroy_ptr->o_ptr->number;
    destroy_ptr->o_ptr->number = destroy_ptr->amt;
    describe_flavor(creature_ptr, destroy_ptr->o_name, destroy_ptr->o_ptr, 0);
    destroy_ptr->o_ptr->number = destroy_ptr->old_number;
    take_turn(creature_ptr, 100);
    if (!can_player_destroy_object(creature_ptr, destroy_ptr->o_ptr)) {
        free_turn(creature_ptr);
        msg_format(_("%sは破壊不可能だ。", "You cannot destroy %s."), destroy_ptr->o_name);
        return;
    }

    object_copy(destroy_ptr->q_ptr, destroy_ptr->o_ptr);
    msg_format(_("%sを壊した。", "You destroy %s."), destroy_ptr->o_name);
    sound(SOUND_DESTITEM);
    reduce_charges(destroy_ptr->o_ptr, destroy_ptr->amt);
    vary_item(creature_ptr, destroy_ptr->item, -destroy_ptr->amt);
    if (item_tester_high_level_book(destroy_ptr->q_ptr)) {
        bool gain_expr = FALSE;
        if (creature_ptr->prace == RACE_ANDROID) {
        } else if ((creature_ptr->pclass == CLASS_WARRIOR) || (creature_ptr->pclass == CLASS_BERSERKER)) {
            gain_expr = TRUE;
        } else if (creature_ptr->pclass == CLASS_PALADIN) {
            if (is_good_realm(creature_ptr->realm1)) {
                if (!is_good_realm(tval2realm(destroy_ptr->q_ptr->tval)))
                    gain_expr = TRUE;
            } else {
                if (is_good_realm(tval2realm(destroy_ptr->q_ptr->tval)))
                    gain_expr = TRUE;
            }
        }

        if (gain_expr && (creature_ptr->exp < PY_MAX_EXP)) {
            s32b tester_exp = creature_ptr->max_exp / 20;
            if (tester_exp > 10000)
                tester_exp = 10000;

            if (destroy_ptr->q_ptr->sval < 3)
                tester_exp /= 4;

            if (tester_exp < 1)
                tester_exp = 1;

            msg_print(_("更に経験を積んだような気がする。", "You feel more experienced."));
            gain_exp(creature_ptr, tester_exp * destroy_ptr->amt);
        }

        if (item_tester_high_level_book(destroy_ptr->q_ptr) && destroy_ptr->q_ptr->tval == TV_LIFE_BOOK) {
            chg_virtue(creature_ptr, V_UNLIFE, 1);
            chg_virtue(creature_ptr, V_VITALITY, -1);
        } else if (item_tester_high_level_book(destroy_ptr->q_ptr) && destroy_ptr->q_ptr->tval == TV_DEATH_BOOK) {
            chg_virtue(creature_ptr, V_UNLIFE, -1);
            chg_virtue(creature_ptr, V_VITALITY, 1);
        }

        if ((destroy_ptr->q_ptr->to_a != 0) || (destroy_ptr->q_ptr->to_h != 0) || (destroy_ptr->q_ptr->to_d != 0))
            chg_virtue(creature_ptr, V_ENCHANT, -1);

        if (object_value_real(creature_ptr, destroy_ptr->q_ptr) > 30000)
            chg_virtue(creature_ptr, V_SACRIFICE, 2);
        else if (object_value_real(creature_ptr, destroy_ptr->q_ptr) > 10000)
            chg_virtue(creature_ptr, V_SACRIFICE, 1);
    }

    if ((destroy_ptr->q_ptr->to_a != 0) || (destroy_ptr->q_ptr->to_d != 0) || (destroy_ptr->q_ptr->to_h != 0))
        chg_virtue(creature_ptr, V_HARMONY, 1);

    if (destroy_ptr->item >= INVEN_RARM)
        calc_android_exp(creature_ptr);
}
