#include "input/StylusButtonMapping.h"

namespace widemelon
{
void StylusButtonMapper::setMappings(widemelon::StylusButtonMapping l2, widemelon::StylusButtonMapping r2)
{
    l2Mapping = l2;
    r2Mapping = r2;
    previousL2Pressed = previousR2Pressed = false;
    l2TogglePressed = r2TogglePressed = false;
    l2Touch = r2Touch = false;
}

bool StylusButtonMapper::updateMapping(widemelon::StylusButtonMapping mapping, bool physicalPressed,
                                        bool &wasPhysicalPressed, bool &togglePressed)
{
    if (mapping == widemelon::StylusButtonMapping::Press && physicalPressed && !wasPhysicalPressed)
        togglePressed = !togglePressed;
    if (mapping != widemelon::StylusButtonMapping::Press)
        togglePressed = false;
    wasPhysicalPressed = physicalPressed;
    if (mapping == widemelon::StylusButtonMapping::Hold) return physicalPressed;
    if (mapping == widemelon::StylusButtonMapping::Press) return togglePressed;
    return false;
}

bool StylusButtonMapper::update(bool l2Pressed, bool r2Pressed)
{
    const bool previous = touchPressed();
    l2Touch = updateMapping(l2Mapping, l2Pressed, previousL2Pressed, l2TogglePressed);
    r2Touch = updateMapping(r2Mapping, r2Pressed, previousR2Pressed, r2TogglePressed);
    return previous != touchPressed();
}
}
