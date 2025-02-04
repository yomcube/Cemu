#include "key_sequence.h"
#include "gui_utils.h"
#include <string>
#include <vector>
#include <thread>

// todo: improve config
using namespace std;

static constexpr int WINDOW_W = 200;
static int window_height = 220; // dummy
static constexpr int FONT_SIZE = 25;
static constexpr int X_STEP = 14;
static constexpr int Y_STEP = 22;
static constexpr int Y_START = 11;
static constexpr int X_START = 11;
static constexpr int Y_END_PADDING = 2;
static constexpr int PAD_LINE_HEIGHT = 1;

static constexpr COLORREF COL_NO_KEY = RGB(0x34, 0x34, 0x34);
static constexpr COLORREF COL_KEY = RGB(0xff, 0xff, 0xff);
static constexpr COLORREF COL_FRAME = RGB(0xcc, 0xff, 0xff);
static constexpr COLORREF COL_FIN_FRAME = RGB(0xff, 0xff, 0xff);
static constexpr COLORREF COL_NOW_BACK = RGB(0x00, 0x00, 0x33);
static constexpr COLORREF COL_START_END_BACK = RGB(0x3e, 0x00, 0);

static constexpr COLORREF COL_PAD_LINE = RGB(0x33, 0x33, 0xbe);
static constexpr COLORREF COL_PAD_TEXT = RGB(0, 0, 0);
static constexpr COLORREF COL_PAD_ON = COL_KEY;
// static constexpr COLORREF COL_PAD_ON = RGB(0xFF, 0x30, 0x30);
//static constexpr COLORREF COL_PAD_ON = RGB(0x00, 0x55, 0xff);
static constexpr COLORREF COL_PAD_OFF = COL_NO_KEY;
static HWND hwnd;

static MacroManager* mgrp;


static constexpr int SHOWMODE_NONE = 0;
static constexpr int SHOWMODE_PART = 1;
static constexpr int SHOWMODE_FULL = 2;
static int cfg_pre_seq = 3;
static bool cfg_now_as_pad = true;
static int cfg_show_frames = SHOWMODE_PART;

bool keyseq_config_validate(const int& preseq, const bool& now_as_pad, const int& show_frame_mode) {
	if (preseq < 0 || preseq > 256)
		return false;
	if (show_frame_mode < 0 || show_frame_mode > SHOWMODE_FULL)
		return false;
	return true;
}


static bool resize_request = true;

static const string& get_data(const int& pos) {
	if (pos < 0)
		return mgrp->get_mapping().empty_show;
	if (pos >= mgrp->get_all_inputs().size())
		return mgrp->get_mapping().empty_show;
	return mgrp->get_all_inputs()[pos];
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_PAINT:
	{
		{
			hdc = BeginPaint(hwnd, &ps);
			const HFONT hf = CreateFontW(FONT_SIZE, 0, 0, 0, FW_BOLD, 0, 0, 0,
								   ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY,
								   FIXED_PITCH | FF_SWISS, NULL);
			const auto fnttmp = SelectObject(hdc, hf);

			int nowy = Y_START;
			const auto colback = GetTextColor(hdc);
			const auto bkback = GetBkColor(hdc);

			SetBkColor(hdc, 0);
			// inputs
			{
				const int row = cfg_pre_seq + (int)(!cfg_now_as_pad);
				for (int i = 0; i < row; i++)
				{
					const int tar = mgrp->get_last_runned_input_index() + (row - (int)(!cfg_now_as_pad) - i);
					const size_t all_input_sz = mgrp->get_all_inputs().size();
					if (all_input_sz != 0 && (tar == mgrp->get_tstart_index() || tar + 1 == all_input_sz))
					{
						SetBkColor(hdc, COL_START_END_BACK);
					}
					else if (i + 1 == row && !cfg_now_as_pad)
					{
						SetBkColor(hdc, COL_NOW_BACK);
					}
					else
					{
						SetBkColor(hdc, 0);
					}

					const auto& s = get_data(tar);
					const char* ap = mgrp->get_mapping().all_show.c_str();
					for (int j = 0, nowx = X_START; *ap; j++, nowx += X_STEP, ap++)
					{
						if (s[j] != ' ')
						{
							SetTextColor(hdc, COL_KEY);
							TextOutA(hdc, nowx, nowy, ap, 1);
						}
						else
						{
							SetTextColor(hdc, COL_NO_KEY);
							TextOutA(hdc, nowx, nowy, ap, 1);
						}
					}

					nowy += Y_STEP;
				}
				nowy += max(0, FONT_SIZE - Y_STEP);
			}


			//if (is_waiting_event)
			//{
			//	SetTextColor(hdc, COL_KEY);
			//	TextOutA(hdc, X_START, nowy - Y_STEP, "[WAITING...]", lstrlenA("[WAITING...]"));
			//}
		
			if(cfg_now_as_pad) { // dirty :(
				SetBkColor(hdc, 0);
				nowy++;
				if (cfg_pre_seq > 0)
				{ // split line
					const auto hPen = CreatePen(PS_SOLID, PAD_LINE_HEIGHT, COL_PAD_LINE);
					const auto hOldPen = SelectObject(hdc, hPen);
					MoveToEx(hdc, X_START, nowy, NULL);
					LineTo(hdc, X_START + X_STEP * mgrp->get_mapping().empty_show.size(), nowy);
					SelectObject(hdc, hOldPen);
					DeleteObject(hPen);
					nowy += PAD_LINE_HEIGHT;
				}
				const auto last_input = mgrp->get_last_input().input_status;

				//const auto hOffBrush = CreateSolidBrush(COL_PAD_OFF);
				//const auto hOnBrush = CreateSolidBrush(COL_PAD_ON);
				//const auto hOldBrush = SelectObject(hdc, hOffBrush);
				//const auto oldMode = SetBkMode(hdc, TRANSPARENT);
				//SetTextColor(hdc, COL_PAD_TEXT);
				#define PUT(idx, v, c) \
					SetTextColor(hdc,(last_input & v) == 0 ? COL_PAD_OFF : COL_PAD_ON); \
				/* SelectObject(hdc, (last_input & v) == 0 ? hOffBrush : hOnBrush); \
					Rectangle(hdc, X_START + X_STEP * idx, nowy, X_START + X_STEP * idx + X_STEP, nowy + Y_STEP); */ \
					TextOutA(hdc, X_START + X_STEP * idx, nowy, c, lstrlenA(c)); 
				/*
				L..........R
				..U......X..
				.<.>.-+.Y.A.
				..v......B..
				*/
				PUT(0, VPAD_L, "L");
				PUT(11, VPAD_R, "R");
				nowy += Y_STEP;
				PUT(2, VPAD_UP, "^");
				PUT(9, VPAD_X, "X");
				nowy += Y_STEP;
				PUT(1, VPAD_LEFT, "<");
				PUT(3, VPAD_RIGHT, ">");
				PUT(5, VPAD_MINUS, "-");
				PUT(6, VPAD_PLUS, "+");
				PUT(8, VPAD_Y, "Y");
				PUT(10, VPAD_A, "A");
				nowy += Y_STEP;
				PUT(2, VPAD_DOWN, "v");
				PUT(9, VPAD_B, "B");
				#undef PUT
				nowy += FONT_SIZE;

				//SetBkMode(hdc, oldMode);
				//SelectObject(hdc, hOldBrush);
				//DeleteObject(hOffBrush);
				//DeleteObject(hOnBrush);

			}

			if (cfg_show_frames != SHOWMODE_NONE) 
			{ // frame
				const bool is_finished = !mgrp->is_running();
				int tar_frame;

				if (cfg_show_frames == SHOWMODE_FULL || is_finished)
					tar_frame = mgrp->get_now_frame_count();
				else if (mgrp->get_requested_show_frames() >= 0) 
					tar_frame = mgrp->get_requested_show_frames();
				else tar_frame = -1;
				nowy++;

				if (tar_frame >= 0)
				{
					SetBkColor(hdc, 0);
					const auto st = to_string(tar_frame) + "f   ";
					const auto cst = st.c_str();
					if (is_finished)
					{
						SetTextColor(hdc, COL_FIN_FRAME);
						TextOutA(hdc, X_START, nowy, cst, lstrlenA(cst));
					}
					else
					{
						SetTextColor(hdc, COL_FRAME);
						TextOutA(hdc, X_START, nowy, cst, lstrlenA(cst));
					}
				}
				nowy += FONT_SIZE;
			}

			if (resize_request)
			{
				window_height = CalcWindowSize(hwnd, WINDOW_W, nowy + 1).second + Y_END_PADDING;
				SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, WINDOW_W, window_height, SWP_SHOWWINDOW | SWP_NOMOVE);

				resize_request = false;
			}

			SetTextColor(hdc, colback);
			SetBkColor(hdc, bkback);
			DeleteObject(SelectObject(hdc, fnttmp));
			EndPaint(hwnd, &ps);
		}
	}
	break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

static void window_main()
{
	hwnd = UtilCreateWindow(TEXT("Input viewer"), TEXT("Input viewer"), WndProc, 0, GUI_NO_CLOSE_WINDOW);
	if (!hwnd)
		return;
	ShowWindow(hwnd, SW_SHOW);
	SetWindowPos(hwnd, HWND_TOPMOST, 0, 110, WINDOW_W, window_height, SWP_SHOWWINDOW);
	FixWindowCornor(hwnd);
	UpdateWindow(hwnd);
	MessageLoop(hwnd);

}

void keyseq_refresh(const bool& hard)
{
	InvalidateRect(hwnd, NULL, hard);
}


void keyseq_init(MacroManager* _mgrp)
{
	mgrp = _mgrp;
	std::thread _t(window_main);
	_t.detach();
}

void keyseq_set_window_pos(const int& x, const int& y) {
	SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
}

void keyseq_change_config(const int& preseq, const bool& now_as_pad, const int& show_frame_mode) {
	cfg_pre_seq = preseq;
	cfg_now_as_pad = now_as_pad;
	cfg_show_frames = show_frame_mode;
	resize_request = true;
	keyseq_refresh(true);
}