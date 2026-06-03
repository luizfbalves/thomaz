/*
    thomaz — AnimatedBox.

    A Box that animates itself in on appearance: a gentle fade + upward rise,
    driven by an Animatable it OWNS. Because the Animatable is a member, the
    tween auto-stops when the view is destroyed (the Ticking dtor removes itself
    from the running list) — so there is no dangling pointer if the user
    navigates away mid-animation.

    Give sibling cards a staggered `entranceDelay` to get the cascading "bento"
    reveal that runs across every screen.

    XML attributes:
      entranceDelay    (float, ms)  — wait before this box starts (stagger)
      entranceRise     (float, px)  — how far below its final spot it starts
      entranceDuration (float, ms)  — length of the fade/rise
*/

#pragma once

#include <borealis.hpp>

namespace thomaz {

class AnimatedBox : public brls::Box
{
  public:
    AnimatedBox();

    void willAppear(bool resetState = false) override;

    static brls::View* create();

  private:
    void runEntrance();

    brls::Animatable entrance { 0.0f }; // 0 -> 1

    float entranceDelay    = 0.0f;   // ms before this box starts
    float entranceRise     = 22.0f;  // px it rises from
    float entranceDuration = 340.0f; // ms
};

} // namespace thomaz
