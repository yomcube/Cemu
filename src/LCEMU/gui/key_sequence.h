#pragma once
#include "../macro_core/macro_manager.h"

void keyseq_init(MacroManager*);
void keyseq_refresh(const bool& hard);
void keyseq_set_window_pos(const int& x, const int& y);
void keyseq_change_config(const int& preseq, const bool& now_as_pad, const int& show_frame_mode);
bool keyseq_config_validate(const int& preseq, const bool& now_as_pad, const int& show_frame_mode);