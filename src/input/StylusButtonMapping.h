#pragma once

#include "input/InputPreset.h"

namespace widemelon
{
    // POC for physical trigger -> touchscreen contact mappings. Position is
    // intentionally owned by StylusRouter/stick modes, not by this class.
    class StylusButtonMapper
    {
    public:
        void setMappings(StylusButtonMapping l2, StylusButtonMapping r2);
        bool update(bool l2Pressed, bool r2Pressed);
        bool touchPressed() const { return l2Touch || r2Touch; }

    private:
        static bool updateMapping(StylusButtonMapping mapping, bool physicalPressed,
                                  bool &wasPhysicalPressed, bool &togglePressed);

        StylusButtonMapping l2Mapping{StylusButtonMapping::Disabled};
        StylusButtonMapping r2Mapping{StylusButtonMapping::Disabled};
        bool previousL2Pressed{false};
        bool previousR2Pressed{false};
        bool l2TogglePressed{false};
        bool r2TogglePressed{false};
        bool l2Touch{false};
        bool r2Touch{false};
    };
}
