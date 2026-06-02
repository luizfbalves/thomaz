/*
    thomaz — settings activity.
    Holds app preferences; v1 exposes the UI language selector. Language changes
    are persisted and applied on the next launch (Borealis loads translations
    once at startup).
*/

#pragma once

#include <borealis.hpp>

namespace thomaz {

class SettingsActivity : public brls::Activity
{
  public:
    SettingsActivity() = default;

    CONTENT_FROM_XML_RES("activity/settings.xml");

    void onContentAvailable() override;
};

} // namespace thomaz
