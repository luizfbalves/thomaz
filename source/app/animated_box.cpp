/*
    thomaz — AnimatedBox implementation. See animated_box.hpp.
*/

#include "app/animated_box.hpp"

namespace thomaz {

AnimatedBox::AnimatedBox()
{
    this->registerFloatXMLAttribute("entranceDelay",
        [this](float v) { this->entranceDelay = v; });
    this->registerFloatXMLAttribute("entranceRise",
        [this](float v) { this->entranceRise = v; });
    this->registerFloatXMLAttribute("entranceDuration",
        [this](float v) { this->entranceDuration = v; });
}

void AnimatedBox::willAppear(bool resetState)
{
    Box::willAppear(resetState);
    this->runEntrance();
}

void AnimatedBox::runEntrance()
{
    // Entrance animation disabled: render instantly, no fade/rise.
    this->setAlpha(1.0f);
    this->setTranslationY(0.0f);
}

brls::View* AnimatedBox::create()
{
    return new AnimatedBox();
}

} // namespace thomaz
