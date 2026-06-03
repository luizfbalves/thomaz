#include "app/slide_frame.hpp"

namespace thomaz {

void SlideFrame::show(std::function<void(void)> cb, bool animate, float animationDuration)
{
    if (animate)
    {
        // Start a touch below the resting spot and ease up to 0 over the same
        // duration as the base fade, so the screen rises into place as it
        // appears. quadraticOut matches Borealis' own show easing.
        this->slideY.reset(48.0f);
        this->slideY.addStep(0.0f, (int)animationDuration, brls::EasingFunction::quadraticOut);
        this->slideY.setTickCallback([this] { this->setTranslationY(this->slideY); });
        this->slideY.start();
    }
    else
    {
        this->setTranslationY(0.0f);
    }

    brls::AppletFrame::show(cb, animate, animationDuration);
}

brls::View* SlideFrame::create()
{
    return new SlideFrame();
}

} // namespace thomaz
