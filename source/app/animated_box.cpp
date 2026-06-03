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
    // Start hidden and nudged down, then ease up into place.
    this->entrance.reset(0.0f);
    this->setAlpha(0.0f);
    this->setTranslationY(this->entranceRise);

    if (this->entranceDelay > 0.0f)
        this->entrance.addStep(0.0f, (int32_t)this->entranceDelay, brls::EasingFunction::linear);
    this->entrance.addStep(1.0f, (int32_t)this->entranceDuration, brls::EasingFunction::quadraticOut);

    this->entrance.setTickCallback([this] {
        float v = this->entrance.getValue();
        this->setAlpha(v);
        this->setTranslationY((1.0f - v) * this->entranceRise);
    });
    this->entrance.setEndCallback([this](bool) {
        this->setAlpha(1.0f);
        this->setTranslationY(0.0f);
    });

    this->entrance.start();
}

brls::View* AnimatedBox::create()
{
    return new AnimatedBox();
}

} // namespace thomaz
