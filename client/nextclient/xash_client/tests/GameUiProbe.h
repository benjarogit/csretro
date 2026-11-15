#pragma once
struct GameUiProbe
{
    int exists, down, key_number, vgui_code;
    char key_name[32], binding[64];
    int count, mode_wide, mode_tall, mode_bpp;
    int current_wide, current_tall, current_bpp;
    char renderer[64];
    int windowed, hd, addons, guard, vac;
};
