#pragma once

#include <borealis.hpp>

namespace thomaz {

// An AppletFrame that, on top of the default activity cross-fade, slides up
// from slightly below its resting position when the screen is shown. It is a
// drop-in replacement for <brls:AppletFrame>: register it as the XML tag
// "thomaz:SlideFrame" and swap the root element in each activity's XML.
//
// Borealis' View::show() only animates alpha (a fade); there is no built-in
// slide. We add it by easing our own translationY in lockstep with that fade —
// getY() already folds translation.y in, so moving it shifts the whole frame.
class SlideFrame : public brls::AppletFrame {
  public:
    SlideFrame() = default;

    void show(std::function<void(void)> cb, bool animate, float animationDuration) override;

    static brls::View* create();

  private:
    brls::Animatable slideY = 0.0f;
};

} // namespace thomaz
