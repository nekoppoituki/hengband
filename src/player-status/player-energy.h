﻿#pragma once

#include "system/angband.h"

typedef struct player_type player_type;
void update_player_turn_energy(player_type *creature_ptr, PERCENTAGE need_cost);
void reset_player_turn(player_type *creature_ptr);
