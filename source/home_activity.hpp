/*
    thomaz — home activity (bento hub). Phase-1 scaffold: a single screen
    that confirms the app boots. The real bento menu lands in the UI phase.
*/

#pragma once

#include <borealis.hpp>

class HomeActivity : public brls::Activity
{
  public:
    // Content is defined by the XML layout in romfs.
    CONTENT_FROM_XML_RES("activity/home.xml");
};
