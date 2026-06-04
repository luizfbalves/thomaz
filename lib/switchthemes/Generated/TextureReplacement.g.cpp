// AUTO-GENERATED (by hand; dotnet unavailable) from exelix
// SwitchThemesCommon/Layouts/PatchTemplate.cs. See lib/switchthemes/README.md.
#include "../Layouts/Patches.hpp"
#include "../Common.hpp"
#include <string>
#include <vector>
#include <unordered_map>

using namespace std;

std::unordered_map<std::string, std::vector<TextureReplacement>> Patches::textureReplacement::NxNameToList =
{
  { "home", {
    {
        .NxThemeName = "album",
        .BntxNames = { "RdtIcoPvr_00^s" },
        .NewColorFlags = 84215045u,
        .FileName = "blyt/RdtBtnPvr.bflyt",
        .PaneName = "P_Pict_00",
        .W = 64, .H = 56,
        .Patch = { .FileName = "blyt/RdtBtnPvr.bflyt", .Patches = {
            { .PaneName = "P_Pict_00", .Position = { 22, 13, 0 }, .Size = { 64, 56 }, .ApplyFlags = 530, .UsdPatches = { { .PropName = "C_W", .PropValues = { "100", "100", "100", "100" }, .type = 1 } } },
            { .PaneName = "N_02", .Visible = false, .ApplyFlags = 1 },
            { .PaneName = "N_01", .Visible = false, .ApplyFlags = 1 },
            { .PaneName = "P_Pict_01", .Visible = false, .ApplyFlags = 1 },
            { .PaneName = "P_Color", .Visible = false, .ApplyFlags = 1 }
        } },
        .MinFirmware = ConsoleFirmware::Invariant
    },
    {
        .NxThemeName = "news",
        .BntxNames = { "RdtIcoNews_00^s", "RdtIcoNews_00_Home^s" },
        .NewColorFlags = 84215045u,
        .FileName = "blyt/RdtBtnNtf.bflyt",
        .PaneName = "P_PictNtf_00",
        .W = 64, .H = 56,
        .Patch = { .FileName = "blyt/RdtBtnNtf.bflyt", .Patches = {
            { .PaneName = "P_PictNtf_00", .Size = { 64, 56 }, .ApplyFlags = 528, .UsdPatches = { { .PropName = "C_W", .PropValues = { "100", "100", "100", "100" }, .type = 1 } } },
            { .PaneName = "P_PictNtf_01", .Visible = false, .ApplyFlags = 1 }
        } },
        .MinFirmware = ConsoleFirmware::Invariant
    },
    {
        .NxThemeName = "shop",
        .BntxNames = { "RdtIcoShop^s" },
        .NewColorFlags = 84215045u,
        .FileName = "blyt/RdtBtnShop.bflyt",
        .PaneName = "P_Pict",
        .W = 64, .H = 56,
        .Patch = { .FileName = "blyt/RdtBtnShop.bflyt", .Patches = {
            { .PaneName = "P_Pict", .Size = { 64, 56 }, .ApplyFlags = 528, .UsdPatches = { { .PropName = "C_W", .PropValues = { "100", "100", "100", "100" }, .type = 1 } } }
        } },
        .MinFirmware = ConsoleFirmware::Invariant
    },
    {
        .NxThemeName = "controller",
        .BntxNames = { "RdtIcoCtrl_00^s" },
        .NewColorFlags = 84215045u,
        .FileName = "blyt/RdtBtnCtrl.bflyt",
        .PaneName = "P_Form",
        .W = 64, .H = 56,
        .Patch = { .FileName = "blyt/RdtBtnCtrl.bflyt", .Patches = {
            { .PaneName = "P_Form", .Size = { 64, 56 }, .ApplyFlags = 528, .UsdPatches = { { .PropName = "C_W", .PropValues = { "100", "100", "100", "100" }, .type = 1 } } },
            { .PaneName = "P_Stick", .Visible = false, .ApplyFlags = 1 },
            { .PaneName = "P_Y", .Visible = false, .ApplyFlags = 1 },
            { .PaneName = "P_X", .Visible = false, .ApplyFlags = 1 },
            { .PaneName = "P_A", .Visible = false, .ApplyFlags = 1 },
            { .PaneName = "P_B", .Visible = false, .ApplyFlags = 1 }
        } },
        .MinFirmware = ConsoleFirmware::Invariant
    },
    {
        .NxThemeName = "settings",
        .BntxNames = { "RdtIcoSet^s" },
        .NewColorFlags = 84215045u,
        .FileName = "blyt/RdtBtnSet.bflyt",
        .PaneName = "P_Pict",
        .W = 64, .H = 56,
        .Patch = { .FileName = "blyt/RdtBtnSet.bflyt", .Patches = {
            { .PaneName = "P_Pict", .Size = { 64, 56 }, .ApplyFlags = 528, .UsdPatches = { { .PropName = "C_W", .PropValues = { "100", "100", "100", "100" }, .type = 1 } } }
        } },
        .MinFirmware = ConsoleFirmware::Invariant
    },
    {
        .NxThemeName = "power",
        .BntxNames = { "RdtIcoPwrForm^s" },
        .NewColorFlags = 84215045u,
        .FileName = "blyt/RdtBtnPow.bflyt",
        .PaneName = "P_Pict_00",
        .W = 64, .H = 56,
        .Patch = { .FileName = "blyt/RdtBtnPow.bflyt", .Patches = {
            { .PaneName = "P_Pict_00", .Size = { 64, 56 }, .ApplyFlags = 528, .UsdPatches = { { .PropName = "C_W", .PropValues = { "100", "100", "100", "100" }, .type = 1 } } }
        } },
        .MinFirmware = ConsoleFirmware::Invariant
    },
    {
        .NxThemeName = "nso",
        .BntxNames = { "RdtIcoLR_00^s" },
        .NewColorFlags = 84215045u,
        .FileName = "blyt/RdtBtnLR.bflyt",
        .PaneName = "P_LR_00",
        .W = 64, .H = 56,
        .Patch = { .FileName = "blyt/RdtBtnLR.bflyt", .Patches = {
            { .PaneName = "P_LR_00", .Size = { 64, 56 }, .ApplyFlags = 16 },
            { .PaneName = "P_LR_01", .Visible = false, .ApplyFlags = 1 }
        } },
        .MinFirmware = ConsoleFirmware::Fw11_0
    },
    {
        .NxThemeName = "card",
        .BntxNames = { "RdtIcoHomeVgc^s" },
        .NewColorFlags = 84215045u,
        .FileName = "blyt/RdtBtnVgc.bflyt",
        .PaneName = "P_Pict_00",
        .W = 64, .H = 56,
        .Patch = { .FileName = "blyt/RdtBtnVgc.bflyt", .Patches = {
            { .PaneName = "P_Pict_00", .Size = { 64, 56 }, .ApplyFlags = 528, .UsdPatches = { { .PropName = "C_W", .PropValues = { "100", "100", "100", "100" }, .type = 1 } } },
            { .PaneName = "P_00", .Visible = false, .ApplyFlags = 1 },
            { .PaneName = "P_01", .Visible = false, .ApplyFlags = 1 }
        } },
        .MinFirmware = ConsoleFirmware::Fw20_0
    },
    {
        .NxThemeName = "share",
        .BntxNames = { "RdtIcoHomeSplayFrame^s" },
        .NewColorFlags = 84215045u,
        .FileName = "blyt/RdtBtnSplay.bflyt",
        .PaneName = "P_Pict_00",
        .W = 64, .H = 56,
        .Patch = { .FileName = "blyt/RdtBtnSplay.bflyt", .Patches = {
            { .PaneName = "P_Pict_00", .Size = { 64, 56 }, .ApplyFlags = 528, .UsdPatches = { { .PropName = "C_W", .PropValues = { "100", "100", "100", "100" }, .type = 1 } } },
            { .PaneName = "N_Wave", .Visible = false, .ApplyFlags = 1 },
            { .PaneName = "P_Pict_01", .Visible = false, .ApplyFlags = 1 },
            { .PaneName = "P_Pict_02", .Visible = false, .ApplyFlags = 1 },
            { .PaneName = "P_Pict_03", .Visible = false, .ApplyFlags = 1 }
        } },
        .MinFirmware = ConsoleFirmware::Fw20_0
    }
  } },
  { "lock", {
    {
        .NxThemeName = "lock",
        .BntxNames = { "EntIcoHome^s" },
        .NewColorFlags = 84148994u,
        .FileName = "blyt/EntBtnResumeSystemApplet.bflyt",
        .PaneName = "P_PictHome",
        .W = 184, .H = 168,
        .Patch = { .FileName = "blyt/EntBtnResumeSystemApplet.bflyt", .Patches = {
            { .PaneName = "P_PictHome", .Position = { 0, 0, 0 }, .Size = { 184, 168 }, .ApplyFlags = 18 }
        } },
        .MinFirmware = ConsoleFirmware::Invariant
    }
  } }
};
