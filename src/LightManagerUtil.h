#pragma once

#include <time.h>
#include "OpenKNX.h"

namespace LightManagerUtil
{

  inline bool tryGetLocalTime(tm &timeinfo)
  {
    if (!openknx.time.isValid())
      return false;

    openknx.time.getLocalTime().toTm(timeinfo);
    return true;
  }

} // namespace LightManagerUtil