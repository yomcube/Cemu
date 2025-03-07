#include "ext_main.h"
#include "utils.h"
#include "./macro_core/macro_manager.h"
#include "./macro_core/vpad_ids.h"
#include "gui/key_sequence.h"
#include "../gui/guiWrapper.h"
#include <thread>
#include <windows.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <filesystem>
#include <string>
#include <fstream>
#include <iostream>

using namespace std;
namespace fs = std::filesystem;

#define TOOL_NAME "LCEMU"
#define TOOL_VER "0.1.0"
#define TOOL_VER_EX ""

constexpr int CS_W = 300;
constexpr int CS_H = 480;

std::mutex cout_mtx;
std::mutex running_mtx;
extern WindowInfo g_window_info;

static inline void setup_mapping(MappingData& data)
{
	int idx = 1;
	// J = B
	data.set('B', VPAD_B, 'B', idx);
	data.set('J', VPAD_B, 'B', idx++);

	data.set('A', VPAD_A, 'A', idx++);
	data.set('Y', VPAD_Y, 'Y', idx++);
	data.set('X', VPAD_X, 'X', idx++);


	data.set('S', VPAD_L, 'L', idx);
	data.set('L', VPAD_L, 'L', idx++);

	data.set('R', VPAD_R, 'R', idx++);

	data.set('V', VPAD_DOWN, 'v', idx);
	data.set('D', VPAD_DOWN, 'v', idx++);

	data.set('~', VPAD_UP, '^', idx);
	data.set('U', VPAD_UP, '^', idx++);

	data.set('>', VPAD_RIGHT, '>', idx++);
	data.set('<', VPAD_LEFT, '<', idx++);


	data.set('P', VPAD_PLUS, '+', idx++);
	data.set('M', VPAD_MINUS, '-', idx++);

	data.set('N', VPAD_NULL, 0, 0); // neutral

	data.build_show_data(idx - 1, 1);
}

static void output(const string& s, const bool new_line = true)
{
	std::lock_guard<std::mutex> _l(cout_mtx);
	cout << s;
	if (new_line) cout << '\n';
}


MacroManager macro_mgr;

static void ctrl_loop()
{
	const fs::path macro_dir = fs::current_path() / "macro";
	string prepath_str = "./macro.txt";
	while (1)
	{
		string instr;
		output("> ", false);

		getline(cin, instr);
		{
			std::lock_guard<std::mutex> _l(running_mtx);
			boost::trim(instr);
			vector<string> strs = split_space(instr);
			const string cmd = boost::to_upper_copy( strs[0]);

			switch (cmd) {
				case "PWD":
					output(macro_dir.string());
					break;
				
				case "CLS":
					clear_screen();
					break;
			
				case "SEQPOS":
					int py, px;
					if (strs.size() < 3 || !try_int(strs[1], px, 0) || !try_int(strs[2], py, 0))
					{
						output("[ERROR] Arg error.");
						continue;
					}
					keyseq_set_window_pos(px, py);
				break;
				case "SEQCFG":
					int pre_seq, now_pad, mode;
					if (strs.size() < 4 || 
						!try_int(strs[1], pre_seq, 0) || 
						!try_int(strs[2], now_pad, 0) ||
						!try_int(strs[3], mode, 0) ||
						!keyseq_config_validate(pre_seq, now_pad, mode)
						)
					{
						output("[ERROR] Arg error.");
						continue;
					}
					keyseq_change_config(pre_seq, now_pad, mode);
					break;

#pragma region Macro
				case "PRE":
					output(prepath_str);
					break;
			
				case "START":
				case "S":
				case "STARTWITH":
				case "SW":
					macro_mgr.start(cmd == "STARTWITH" || cmd == "SW");
					keyseq_refresh(true);
					output("Started");
					break;
				
				case "STOP":
					case "X":
					macro_mgr.stop();
					output("Stopped");
					break;
			
				case "RS":	case "RSW":
					macro_mgr.stop();
				case "LOAD":    case "L":
				case "RELOAD":  case "R":
					if (macro_mgr.is_running())
					{
						output("[ERROR] Stop the running macro.");
						output("Load failed");
						continue;
					}
					else
					{
						const bool is_load = cmd == "LOAD" || cmd == "L";
						if (is_load && strs.size() < 2)
						{
							output("[ERROR] Please specify a file.");
							continue;
						}
						string path_str = (is_load)? strs[strs.size() - 1] : prepath_str;
						if (is_load) prepath_str = path_str;
						fs::path path;
						if (!try_eval_relative(path_str, macro_dir, path))
						{
							output("[ERROR] File not found: " + path_str);
							output("Load failed");
							continue;
						}
						bool loadret;
						{
							std::lock_guard<std::mutex> _l(cout_mtx);
							loadret = macro_mgr.load(path);
						}
						if (!loadret)
						{
							output("Load failed");
							continue;
						}
						
						output("Loaded: " + path_str);
						output(format("{} frames", macro_mgr.get_total_frame_count()));
						if (is_rs)
						{
							macro_mgr.start(cmd == "RSW");
							output("Started");
						}
						keyseq_refresh(true);
					}
			
				case "DUMPMACRO":
				case "DUMPMACRO2":
					const auto& inputs = macro_mgr.get_all_inputs(); 
					if (inputs.empty())
					{
						output("[ERROR] Macro is empty.");
						continue;
					}
					int offset = 0;
					if (strs.size() >= 2)
					{
						if (!try_int(strs[1], offset, 0))
						{
							output("[ERROR] Parse error.");
							continue;
						}
						else if (offset >= inputs.size() || offset < 0)
						{
							output("[ERROR] Out of range.");
							continue;
						}
					}
					auto npath = macro_mgr.get_now_path().string() + ".dump.txt";
					FILE* fp;
					if (fopen_s(&fp, npath.c_str(), "w"))
						output("[ERROR] Could not create a file.");
					else
					{
						bool rle_style = cmd == "DUMPMACRO2";
						if (rle_style)
						{
							fprintf(fp, "input,length\n");
						}
						else
						{
							fprintf(fp, "start(f),end(f),input\n");
						}
						string pre = inputs[offset];
						int st = offset;
						for (int i = offset + 1; i <= inputs.size(); i++)
						{
							if (inputs.size() == i || pre != inputs[i])
							{
								const int l = st + 1 - offset;
								const int r = i - offset;
								if (rle_style)
								{
									fprintf(fp, "%s,%d\n", pre.c_str(),r - l + 1);
								}
								else
								{
									fprintf(fp, "%d,%d,%s\n", l, r, pre.c_str());
								}
								st = i;
								if (i < inputs.size())
									pre = inputs[i];
							}
						}
	
						fclose(fp);
						output("Success");
					}
					break;
			}
#pragma endregion

			else
			{
				output("[ERROR] Unknown command");
			}
		} // release lock
	}
}

void ext_init()
{
	setup_mapping(macro_mgr.get_mapping());
	keyseq_init(&macro_mgr); // after mapping
#ifndef CEMU_DEBUG_ASSERT
	FreeConsole();
	AllocConsole();
	FILE* fp;
	HWND csw = GetConsoleWindow();
	SetWindowPos(csw, HWND_TOP, 288, 0, CS_W, CS_H, SWP_SHOWWINDOW);
	freopen_s(&fp, "CONIN$", "r", stdin);
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONOUT$", "w", stderr);
#endif
	auto title = format("{} : {}{}", TOOL_NAME, TOOL_VER,TOOL_VER_EX);
	SetConsoleTitleA(title.c_str());
	cout << title << endl;
	std::thread thr(ctrl_loop);
	thr.detach();
}

bool ext_onframe(uint32be& status)
{
	std::lock_guard<std::mutex> _l(running_mtx);
	// Frame Advance
	// Todo: Config
	if (g_window_info.get_keystate(VK_LSHIFT))
		Sleep(500);

	if (macro_mgr.is_running())
	{
		bool ret = macro_mgr.on_frame(status);
		keyseq_refresh(false);
		return ret;
	}
	else
	{
		return false;
	}
}
