#include "stdafx.h"
#include <random>

struct Screen
{
    int32_t Width;
    int32_t Height;
    float fWidth;
    float fHeight;
    int32_t Width43;
    float fAspectRatio;
    float fAspectRatioDiff;
    float fHUDScaleX;
    float fHudOffset;
    float fHudOffsetReal;
} Screen;

void Init()
{
    CIniReader iniReader("");
    Screen.Width = iniReader.ReadInteger("MAIN", "ResX", 0);
    Screen.Height = iniReader.ReadInteger("MAIN", "ResY", 0);
    bool bFixHUD = iniReader.ReadInteger("MAIN", "FixHUD", 1) != 0;
    bool bFixFOV = iniReader.ReadInteger("MAIN", "FixFOV", 1) != 0;
    bool bRandomSongOrderFix = iniReader.ReadInteger("MAIN", "RandomSongOrderFix", 1) != 0;

    if (!Screen.Width || !Screen.Height)
        std::tie(Screen.Width, Screen.Height) = GetDesktopRes();

    Screen.fWidth = static_cast<float>(Screen.Width);
    Screen.fHeight = static_cast<float>(Screen.Height);
    Screen.Width43 = static_cast<int32_t>(Screen.fHeight * (4.0f / 3.0f));
    Screen.fAspectRatio = (Screen.fWidth / Screen.fHeight);
    Screen.fAspectRatioDiff = 1.0f / (Screen.fAspectRatio / (4.0f / 3.0f));
    Screen.fHUDScaleX = 1.0f / Screen.fWidth * (Screen.fHeight / 480.0f);
    Screen.fHudOffset = ((480.0f * Screen.fAspectRatio) - 640.0f) / 2.0f;
    Screen.fHudOffsetReal = (Screen.fWidth - Screen.fHeight * (4.0f / 3.0f)) / 2.0f;

    //Fixes (sort of) crash when using high resolutions
    static uint8_t vec[100000];
    auto pattern = hook::pattern("B9 ? ? ? ? 89 74 24 14 8B FF 8B C3 99 F7 FE"); //0x50D195
    injector::WriteMemory(pattern.get_first(1), &vec[0], true);

    //Resolution
    pattern = hook::pattern("89 0D ? ? ? ? 89 0D ? ? ? ? 8A 0D ? ? ? ? 84 C9");
    static int32_t* dword_6D2224 = *pattern.get_first<int32_t*>(2);
    static int32_t* dword_6D2228 = *hook::get_pattern<int32_t*>("89 3D ? ? ? ? 89 3D ? ? ? ? 89 15 ? ? ? ? 89 15 ? ? ? ? 74", 2);
    static int32_t* dword_7F3A2C = *pattern.get_first<int32_t*>(8);
    static int32_t* dword_7F3A30 = *hook::get_pattern<int32_t*>("89 3D ? ? ? ? 89 3D ? ? ? ? 89 15 ? ? ? ? 89 15 ? ? ? ? 74", 8);
    struct SetResHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *dword_6D2224 = Screen.Width;
            *dword_6D2228 = Screen.Height;
            *dword_7F3A2C = Screen.Width;
            *dword_7F3A30 = Screen.Height;
        }
    };
    pattern = hook::pattern(pattern_str(0xA3, to_bytes(dword_6D2224)));
    for (size_t i = 0; i < pattern.size(); ++i)
        injector::MakeInline<SetResHook>(pattern.get(i).get<uint32_t>(0));

    pattern = hook::pattern(pattern_str(0x89, '?', to_bytes(dword_6D2224)));
    if (!pattern.count_hint(1).empty())
        injector::MakeInline<SetResHook>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern(pattern_str(0x89, '?', to_bytes(dword_6D2228)));
    injector::MakeInline<SetResHook>(pattern.count(2).get(0).get<void*>(0), pattern.count(2).get(0).get<void*>(6));
    injector::MakeInline<SetResHook>(pattern.count(2).get(1).get<void*>(0), pattern.count(2).get(1).get<void*>(6));

    pattern = hook::pattern(pattern_str(0x89, '?', to_bytes(dword_7F3A2C)));
    for (size_t i = 0; i < pattern.size(); ++i)
        injector::MakeInline<SetResHook>(pattern.get(i).get<uint32_t>(0), pattern.get(i).get<uint32_t>(6));

    pattern = hook::pattern(pattern_str(0x89, '?', to_bytes(dword_7F3A30)));
    for (size_t i = 0; i < pattern.size(); ++i)
        injector::MakeInline<SetResHook>(pattern.get(i).get<uint32_t>(0), pattern.get(i).get<uint32_t>(6));

    //Aspect Ratio
    pattern = hook::pattern("68 ? ? ? ? E8 ? ? ? ? 6A 00 68 ? ? ? ? E8 ? ? ? ? D9");
    injector::WriteMemory<float>(pattern.get_first(1), Screen.fAspectRatio, true);
    pattern = hook::pattern("E8 ? ? ? ? D8 0D ? ? ? ? D8 7C 24");
    injector::WriteMemory(injector::GetBranchDestination(pattern.get_first()).as_int() + 2, &Screen.fAspectRatio, true);

    //FOV
    if (bFixFOV)
    {
        pattern = hook::pattern("D9 9E C4 00 00 00 E8 ? ? ? ? D9 86 C4 00 00 00 5E 59 C3"); //0x4C23D9
        struct FovHook
        {
            void operator()(injector::reg_pack& regs)
            {
                float fov = 0.0f;
                _asm {fstp dword ptr[fov]}
                *(float*)(regs.esi + 0xC4) = AdjustFOV(fov, Screen.fAspectRatio);
            }
        }; injector::MakeInline<FovHook>(pattern.get_first(), pattern.get_first(6));
    }

    //HUD
    if (bFixHUD)
    {
        static int32_t nHudOffset = static_cast<int32_t>(Screen.fHudOffsetReal);
        static int32_t* dword_787D88;
        struct SetOffsetHook
        {
            void operator()(injector::reg_pack& regs)
            {
                *dword_787D88 = nHudOffset;
            }
        };

        pattern = hook::pattern("D8 0D ? ? ? ? 8B 35 ? ? ? ? 85 F6 D9 1D");
        injector::WriteMemory(pattern.get_first(2), &Screen.fHUDScaleX, true);
        dword_787D88 = *hook::get_pattern<int32_t*>("89 1D ? ? ? ? C7 05 ? ? ? ? 10 00 00 00", 2);

        pattern = hook::pattern(pattern_str(0x89, '?', to_bytes(dword_787D88)));
        injector::MakeInline<SetOffsetHook>(pattern.get_first(0), pattern.get_first(6));

        pattern = hook::pattern(pattern_str(0xC7, '?', to_bytes(dword_787D88)));
        injector::MakeInline<SetOffsetHook>(pattern.get_first(0), pattern.get_first(10));
    }

    if (bRandomSongOrderFix)
    {
        pattern = hook::pattern("BD D0 07 00 00");
        auto rpattern = hook::range_pattern((uintptr_t)pattern.get_first(0), (uintptr_t)pattern.get_first(110), "75 ? A1");
        static auto dword_69B718 = *rpattern.get_first<int32_t*>(3);
        static auto dword_681D14 = *hook::range_pattern((uintptr_t)pattern.get_first(0), (uintptr_t)pattern.get_first(60), "8B 15 ? ? ? ?").get_first<int32_t*>(2);
        struct RandomHook
        {
            void operator()(injector::reg_pack& regs)
            {
                auto num_songs = *dword_681D14;
                int32_t* sp_xbox_randomized_songs = dword_69B718;

                std::vector<int32_t> songs;
                songs.assign(std::addressof(sp_xbox_randomized_songs[0]), std::addressof(sp_xbox_randomized_songs[num_songs]));
                std::mt19937 r{ std::random_device{}() };
                std::shuffle(std::begin(songs), std::end(songs), r);

                std::copy(songs.begin(), songs.end(), sp_xbox_randomized_songs);
            }
        }; injector::MakeInline<RandomHook>(pattern.get_first(0), rpattern.get_first(2));
    }
}

CEXP void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init, hook::pattern("89 0D ? ? ? ? 89 0D ? ? ? ? 8A 0D ? ? ? ? 84 C9"));
    });
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {

    }
    return TRUE;
}