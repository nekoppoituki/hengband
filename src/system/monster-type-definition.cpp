﻿#include "system/monster-type-definition.h"

bool monster_type::is_friendly() const
{
    return this->mflag2.has(MonsterConstantFlagType::FRIENDLY);
}
