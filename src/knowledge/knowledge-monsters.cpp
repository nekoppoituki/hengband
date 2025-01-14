/*!
 * @file knowledge-monsters.cpp
 * @brief 既知のモンスターに関する情報を表示する
 * @date 2020/04/24
 * @author Hourier
 * @todo サブルーチン分割を行うと行数が膨れ上がりそう、再分割も検討すべし
 */

#include "knowledge/knowledge-monsters.h"
#include "core/show-file.h"
#include "core/stuff-handler.h"
#include "game-option/cheat-options.h"
#include "game-option/special-options.h"
#include "io-dump/dump-util.h"
#include "io/input-key-acceptor.h"
#include "knowledge/monster-group-table.h"
#include "locale/english.h"
#include "lore/lore-util.h"
#include "monster-race/monster-race.h"
#include "monster/monster-describer.h"
#include "monster/monster-description-types.h"
#include "monster/monster-info.h"
#include "monster/monster-status.h"
#include "monster/smart-learn-types.h"
#include "pet/pet-util.h"
#include "system/floor-type-definition.h"
#include "system/monster-entity.h"
#include "system/monster-race-info.h"
#include "system/player-type-definition.h"
#include "term/gameterm.h"
#include "term/screen-processor.h"
#include "term/term-color-types.h"
#include "term/z-form.h"
#include "util/angband-files.h"
#include "util/bit-flags-calculator.h"
#include "util/int-char-converter.h"
#include "util/string-processor.h"
#include "view/display-lore.h"
#include "view/display-monster-status.h"
#include "world/world.h"

/*!
 * @brief 特定の与えられた条件に応じてモンスターのIDリストを作成する / Build a list of monster indexes in the given group.
 * @param player_ptr プレイヤーへの参照ポインタ
 * @param grp_cur グループ種別。リスト表記中の左一覧（各シンボル及び/ユニーク(-1)/騎乗可能モンスター(-2)/賞金首(-3)/アンバーの王族(-4)）を参照できる
 * @param mode 思い出の扱いに関するモード
 * @return 作成したモンスターのIDリスト
 */
static std::vector<MonsterRaceId> collect_monsters(PlayerType *player_ptr, IDX grp_cur, monster_lore_mode mode)
{
    concptr group_char = monster_group_char[grp_cur];
    bool grp_unique = (monster_group_char[grp_cur] == (char *)-1L);
    bool grp_riding = (monster_group_char[grp_cur] == (char *)-2L);
    bool grp_wanted = (monster_group_char[grp_cur] == (char *)-3L);
    bool grp_amberite = (monster_group_char[grp_cur] == (char *)-4L);

    const auto &monraces = MonraceList::get_instance();
    std::vector<MonsterRaceId> monrace_ids;
    for (const auto &[monrace_id, monrace] : monraces) {
        if (((mode != MONSTER_LORE_DEBUG) && (mode != MONSTER_LORE_RESEARCH)) && !cheat_know && !monrace.r_sights) {
            continue;
        }

        if (grp_unique) {
            if (monrace.kind_flags.has_not(MonsterKindType::UNIQUE)) {
                continue;
            }
        } else if (grp_riding) {
            if (monrace.misc_flags.has_not(MonsterMiscType::RIDING)) {
                continue;
            }
        } else if (grp_wanted) {
            auto wanted = player_ptr->knows_daily_bounty && (w_ptr->today_mon == monrace_id);
            wanted |= MonsterRace(monrace_id).is_bounty(false);
            if (!wanted) {
                continue;
            }
        } else if (grp_amberite) {
            if (monrace.kind_flags.has_not(MonsterKindType::AMBERITE)) {
                continue;
            }
        } else {
            if (angband_strchr(group_char, monrace.symbol_definition.character) == nullptr) {
                continue;
            }
        }

        monrace_ids.push_back(monrace_id);
        if (mode == MONSTER_LORE_NORMAL) {
            break;
        }
        if (mode == MONSTER_LORE_DEBUG) {
            break;
        }
    }

    std::stable_sort(monrace_ids.begin(), monrace_ids.end(), [&monraces](auto x, auto y) { return monraces.order_level(x, y); });
    return monrace_ids;
}

/*!
 * @brief 現在のペットを表示するコマンドのメインルーチン /
 * Display current pets
 * @param player_ptr プレイヤーへの参照ポインタ
 */
void do_cmd_knowledge_pets(PlayerType *player_ptr)
{
    FILE *fff = nullptr;
    GAME_TEXT file_name[FILE_NAME_SIZE];
    if (!open_temporary_file(&fff, file_name)) {
        return;
    }

    MonsterEntity *m_ptr;
    int t_friends = 0;
    for (int i = player_ptr->current_floor_ptr->m_max - 1; i >= 1; i--) {
        m_ptr = &player_ptr->current_floor_ptr->m_list[i];
        if (!m_ptr->is_valid() || !m_ptr->is_pet()) {
            continue;
        }

        t_friends++;
        const auto pet_name = monster_desc(player_ptr, m_ptr, MD_ASSUME_VISIBLE | MD_INDEF_VISIBLE);
        fprintf(fff, "%s (%s)\n", pet_name.data(), look_mon_desc(m_ptr, 0x00).data());
    }

    int show_upkeep = calculate_upkeep(player_ptr);

    fprintf(fff, "----------------------------------------------\n");
#ifdef JP
    fprintf(fff, "    合計: %d 体のペット\n", t_friends);
#else
    fprintf(fff, "   Total: %d pet%s.\n", t_friends, (t_friends == 1 ? "" : "s"));
#endif
    fprintf(fff, _(" 維持コスト: %d%% MP\n", "   Upkeep: %d%% mana.\n"), show_upkeep);

    angband_fclose(fff);
    FileDisplayer(player_ptr->name).display(true, file_name, 0, 0, _("現在のペット", "Current Pets"));
    fd_kill(file_name);
}

/*!
 * @brief 現在までに倒したモンスターを表示するコマンドのメインルーチン /
 * @param player_ptr プレイヤーへの参照ポインタ
 * Total kill count
 * @note the player ghosts are ignored.
 */
void do_cmd_knowledge_kill_count(PlayerType *player_ptr)
{
    FILE *fff = nullptr;
    GAME_TEXT file_name[FILE_NAME_SIZE];
    if (!open_temporary_file(&fff, file_name)) {
        return;
    }

    auto total = 0;
    for (const auto &[monrace_id, monrace] : monraces_info) {
        if (monrace.kind_flags.has(MonsterKindType::UNIQUE)) {
            if (monrace.max_num == 0) {
                total++;
            }
        } else {
            if (monrace.r_pkills > 0) {
                total += monrace.r_pkills;
            }
        }
    }

    if (total < 1) {
        fprintf(fff, _("あなたはまだ敵を倒していない。\n\n", "You have defeated no enemies yet.\n\n"));
    } else {
#ifdef JP
        fprintf(fff, "あなたは%d体の敵を倒している。\n\n", total);
#else
        fprintf(fff, "You have defeated %d %s.\n\n", total, (total == 1) ? "enemy" : "enemies");
#endif
    }

    const auto &monraces = MonraceList::get_instance();
    std::vector<MonsterRaceId> monrace_ids = monraces.get_valid_monrace_ids();
    std::stable_sort(monrace_ids.begin(), monrace_ids.end(), [&monraces](auto x, auto y) { return monraces.order(x, y); });
    for (const auto monrace_id : monrace_ids) {
        const auto &monrace = monraces_info[monrace_id];
        if (monrace.kind_flags.has(MonsterKindType::UNIQUE)) {
            if (monrace.max_num == 0) {
                std::string details;
                if (monrace.defeat_level && monrace.defeat_time) {
                    details = format(_(" - レベル%2d - %d:%02d:%02d", " - level %2d - %d:%02d:%02d"), monrace.defeat_level, monrace.defeat_time / (60 * 60),
                        (monrace.defeat_time / 60) % 60, monrace.defeat_time % 60);
                }

                fprintf(fff, "     %s%s\n", monrace.name.data(), details.data());
            }

            continue;
        }

        auto this_monster = monrace.r_pkills;
        if (this_monster <= 0) {
            continue;
        }

#ifdef JP
        const auto number_of_kills = angband_strchr("pt", monrace.symbol_definition.character) ? "人" : "体";
        fprintf(fff, "     %3d %sの %s\n", this_monster, number_of_kills, monrace.name.data());
#else
        if (this_monster < 2) {
            if (monrace.name.find("coins") != std::string::npos) {
                fprintf(fff, "     1 pile of %s\n", monrace.name.data());
            } else {
                fprintf(fff, "     1 %s\n", monrace.name.data());
            }
        } else {
            const auto name = pluralize(monrace.name);
            fprintf(fff, "     %d %s\n", this_monster, name.data());
        }
#endif
    }

    fprintf(fff, "----------------------------------------------\n");
#ifdef JP
    fprintf(fff, "    合計: %d 体を倒した。\n", total);
#else
    fprintf(fff, "   Total: %d creature%s killed.\n", total, (total == 1 ? "" : "s"));
#endif

    angband_fclose(fff);
    FileDisplayer(player_ptr->name).display(true, file_name, 0, 0, _("倒した敵の数", "Kill Count"));
    fd_kill(file_name);
}

/*
 * Display the monsters in a group.
 */
static void display_monster_list(int col, int row, int per_page, const std::vector<MonsterRaceId> &mon_idx, int mon_cur, int mon_top, bool visual_only)
{
    int i;
    for (i = 0; i < per_page && mon_top + i < static_cast<int>(mon_idx.size()); i++) {
        const auto monrace_id = mon_idx[mon_top + i];
        const auto &monrace = monraces_info[monrace_id];
        const auto color = ((i + mon_top == mon_cur) ? TERM_L_BLUE : TERM_WHITE);
        c_prt(color, (monrace.name.data()), row + i, col);
        const auto &symbol_config = monrace.symbol_config;
        if (per_page == 1) {
            c_prt(color, format("%02x/%02x", symbol_config.color, symbol_config.character), row + i, (w_ptr->wizard || visual_only) ? 56 : 61);
        }

        if (w_ptr->wizard || visual_only) {
            c_prt(color, format("%d", enum2i(monrace_id)), row + i, 62);
        }

        term_erase(69, row + i);
        term_queue_bigchar(use_bigtile ? 69 : 70, row + i, { symbol_config, {} });
        if (!visual_only) {
            if (monrace.kind_flags.has_not(MonsterKindType::UNIQUE)) {
                put_str(format("%5d", monrace.r_pkills), row + i, 73);
            } else {
                c_put_str((monrace.max_num == 0 ? TERM_L_DARK : TERM_WHITE), (monrace.max_num == 0 ? _("死亡", " dead") : _("生存", "alive")), row + i, 74);
            }
        }
    }

    for (; i < per_page; i++) {
        term_erase(col, row + i);
    }
}

/*!
 * Display known monsters.
 * @param player_ptr プレイヤーへの参照ポインタ
 * @param need_redraw 画面の再描画が必要な時TRUE
 * @param visual_only ？？？
 * @param direct_r_idx モンスターID
 * @todo 引数の詳細について加筆求む
 */
void do_cmd_knowledge_monsters(PlayerType *player_ptr, bool *need_redraw, bool visual_only, std::optional<MonsterRaceId> direct_r_idx)
{
    TermCenteredOffsetSetter tcos(MAIN_TERM_MIN_COLS, std::nullopt);

    const auto &[wid, hgt] = term_get_size();
    std::vector<MonsterRaceId> r_idx_list;
    std::vector<IDX> grp_idx;

    int max = 0;
    bool visual_list = false;
    TERM_COLOR color_top = 0;
    byte character_left = 0;
    monster_lore_mode mode;
    const int browser_rows = hgt - 8;
    if (!direct_r_idx) {
        mode = visual_only ? MONSTER_LORE_DEBUG : MONSTER_LORE_NORMAL;
        int len;
        for (IDX i = 0; monster_group_text[i] != nullptr; i++) {
            len = strlen(monster_group_text[i]);
            if (len > max) {
                max = len;
            }

            if ((monster_group_char[i] == ((char *)-1L)) || !collect_monsters(player_ptr, i, mode).empty()) {
                grp_idx.push_back(i);
            }
        }
    } else {
        r_idx_list.push_back(*direct_r_idx);
        auto &monrace = monraces_info[*direct_r_idx];
        auto &symbol_config = monrace.symbol_config;
        (void)visual_mode_command('v', &visual_list, browser_rows - 1, wid - (max + 3),
            &color_top, &character_left, &symbol_config.color, &symbol_config.character, need_redraw);
    }

    grp_idx.push_back(-1); // Sentinel
    mode = visual_only ? MONSTER_LORE_RESEARCH : MONSTER_LORE_NONE;
    IDX old_grp_cur = -1;
    IDX grp_cur = 0;
    IDX grp_top = 0;
    IDX mon_cur = 0;
    IDX mon_top = 0;
    int column = 0;
    bool flag = false;
    bool redraw = true;
    const auto &symbols_cb = DisplaySymbolsClipboard::get_instance();
    while (!flag) {
        if (redraw) {
            clear_from(0);
            prt(format(_("%s - モンスター", "%s - monsters"), !visual_only ? _("知識", "Knowledge") : _("表示", "Visuals")), 2, 0);
            if (!direct_r_idx) {
                prt(_("グループ", "Group"), 4, 0);
            }
            prt(_("名前", "Name"), 4, max + 3);
            if (w_ptr->wizard || visual_only) {
                prt("Idx", 4, 62);
            }
            prt(_("文字", "Sym"), 4, 67);
            if (!visual_only) {
                prt(_("殺害数", "Kills"), 4, 72);
            }

            for (IDX i = 0; i < 78; i++) {
                term_putch(i, 5, { TERM_WHITE, '=' });
            }

            if (!direct_r_idx) {
                for (IDX i = 0; i < browser_rows; i++) {
                    term_putch(max + 1, 6 + i, { TERM_WHITE, '|' });
                }
            }

            redraw = false;
        }

        if (!direct_r_idx) {
            if (grp_cur < grp_top) {
                grp_top = grp_cur;
            }
            if (grp_cur >= grp_top + browser_rows) {
                grp_top = grp_cur - browser_rows + 1;
            }

            display_group_list(0, 6, max, browser_rows, grp_idx.data(), monster_group_text, grp_cur, grp_top);
            if (old_grp_cur != grp_cur) {
                old_grp_cur = grp_cur;
                r_idx_list = collect_monsters(player_ptr, grp_idx[grp_cur], mode);
            }

            while (mon_cur < mon_top) {
                mon_top = std::max<short>(0, mon_top - browser_rows / 2);
            }

            while (mon_cur >= mon_top + browser_rows) {
                auto remain = static_cast<int>(r_idx_list.size()) - browser_rows;
                mon_top = static_cast<short>(std::min(remain, mon_top + browser_rows / 2));
            }
        }

        if (!visual_list) {
            display_monster_list(max + 3, 6, browser_rows, r_idx_list, mon_cur, mon_top, visual_only);
        } else {
            mon_top = mon_cur;
            display_monster_list(max + 3, 6, 1, r_idx_list, mon_cur, mon_top, visual_only);
            display_visual_list(max + 3, 7, browser_rows - 1, wid - (max + 3), color_top, character_left);
        }

        prt(format(_("%lu 種", "%lu Races"), r_idx_list.size()), 3, 26);
        prt(format(_("<方向>%s%s%s, ESC", "<dir>%s%s%s, ESC"), (!visual_list && !visual_only) ? _(", 'r'で思い出を見る", ", 'r' to recall") : "",
                visual_list ? _(", ENTERで決定", ", ENTER to accept") : _(", 'v'でシンボル変更", ", 'v' for visuals"),
                (symbols_cb.symbol != DisplaySymbol()) ? _(", 'c', 'p'でペースト", ", 'c', 'p' to paste") : _(", 'c'でコピー", ", 'c' to copy")),
            hgt - 1, 0);

        DisplaySymbol symbol_dummy;
        auto *symbol_ptr = &symbol_dummy;
        if (!r_idx_list.empty()) {
            auto &monrace = monraces_info[r_idx_list[mon_cur]];
            symbol_ptr = &monrace.symbol_config;
            if (!visual_only) {
                monster_race_track(player_ptr, r_idx_list[mon_cur]);
                handle_stuff(player_ptr);
            }

            if (visual_list) {
                const auto &symbol_config = monrace.symbol_config;
                place_visual_list_cursor(max + 3, 7, symbol_config.color, symbol_config.character, color_top, character_left);
            } else if (!column) {
                term_gotoxy(0, 6 + (grp_cur - grp_top));
            } else {
                term_gotoxy(max + 3, 6 + (mon_cur - mon_top));
            }
        }

        char ch = inkey();
        if (visual_mode_command(ch, &visual_list, browser_rows - 1, wid - (max + 3), &color_top, &character_left, &symbol_ptr->color, &symbol_ptr->character, need_redraw)) {
            if (direct_r_idx) {
                switch (ch) {
                case '\n':
                case '\r':
                case ESCAPE:
                    flag = true;
                    break;
                }
            }

            continue;
        }

        switch (ch) {
        case ESCAPE: {
            flag = true;
            break;
        }

        case 'R':
        case 'r': {
            if (!visual_list && !visual_only && MonsterRace(r_idx_list[mon_cur]).is_valid()) {
                screen_roff(player_ptr, r_idx_list[mon_cur], MONSTER_LORE_NORMAL);

                (void)inkey();

                redraw = true;
            }

            break;
        }

        default: {
            browser_cursor(ch, &column, &grp_cur, grp_idx.size() - 1, &mon_cur, r_idx_list.size());

            break;
        }
        }
    }
}

/*
 * List wanted monsters
 * @param player_ptr プレイヤーへの参照ポインタ
 */
void do_cmd_knowledge_bounty(PlayerType *player_ptr)
{
    FILE *fff = nullptr;
    GAME_TEXT file_name[FILE_NAME_SIZE];
    if (!open_temporary_file(&fff, file_name)) {
        return;
    }

    fprintf(fff, _("今日のターゲット : %s\n", "Today's target : %s\n"),
        player_ptr->knows_daily_bounty ? monraces_info[w_ptr->today_mon].name.data() : _("不明", "unknown"));
    fprintf(fff, "\n");
    fprintf(fff, _("賞金首リスト\n", "List of wanted monsters\n"));
    fprintf(fff, "----------------------------------------------\n");

    bool listed = false;
    for (const auto &[r_idx, is_achieved] : w_ptr->bounties) {
        if (!is_achieved) {
            fprintf(fff, "%s\n", monraces_info[r_idx].name.data());
            listed = true;
        }
    }

    if (!listed) {
        fprintf(fff, "\n%s\n", _("賞金首はもう残っていません。", "There are no more wanted monster."));
    }

    angband_fclose(fff);
    FileDisplayer(player_ptr->name).display(true, file_name, 0, 0, _("賞金首の一覧", "Wanted monsters"));
    fd_kill(file_name);
}
