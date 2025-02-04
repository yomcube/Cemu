#include "macro_manager.h"
#include "gui/key_sequence.h"

bool MacroManager::get_next_input(MacroCode& next, const bool runmode)
{
	while (read_next(next) && next.type != MacroCode_Input) // all command
	{
		if (runmode)
		{
			switch (next.type)
			{
			case MacroCode_SLP:
				Sleep(next.intargs.at(0));
				break;
			case MacroCode_TSTART:
				m_current_total_frames = 0;
				break;
			case MacroCode_SHOWFRAMES:
				m_requested_show_frames = m_current_total_frames;
				break;
			case MacroCode_SEQCFG:
				keyseq_change_config(next.intargs.at(0), next.intargs.at(1), next.intargs.at(2));
				break;
			}
		}
		else
		{
			switch (next.type)
			{
			case MacroCode_TSTART:
				// current size = index
				m_tstart_index = m_all_inputs.size();
				break;
			}
		}

		// common
		if (next.type == MacroCode_EV_WAIT)
		{
			if (runmode)
				m_waitevent = (WaitEvents)next.intargs[0];
			break; // consume a frame
		}
		else if (next.type == MacroCode_CALL)
		{
			if (m_now_run->depth >= next.intargs.at(0))
			{
				// skip
				continue;
			}
			else if (m_now_run->depth >= MAX_CALL_DEPTH)
			{
				if (!runmode)
				{
					// todo: improve
					macro_error(format("Maximum recursion depth({}) exceeded.", MAX_CALL_DEPTH), next.line);
				}
				next.eof = true;
				break; // interrupt
			}
			RunInfo* inf = new RunInfo();
			inf->parent = m_now_run;
			inf->target = &m_sub_macro[next.strargs.at(0)];
			inf->depth = m_now_run->depth + 1;
			m_now_run = inf;
		}
		else if (next.type == MacroCode_QUIT)
		{
			next.eof = true;
			break;
		}
	}
	return !next.eof; // next == input or eof
}


#define ERROR_RETURN(mess) return macro_error(mess, lineno), unload(), false
bool MacroManager::load(const fs::path& path)
{
	// todo: rework -> macro_compiler.cpp
	if (!fs::exists(path))
	{
		put_with_color("[ERROR] File not found : " + path.string(), FOREGROUND_RED);
		return false;
	}
	unload();
	std::ifstream infile(path);
	string line;
	int lineno = 0;
	m_now_path = fs::absolute(path);
	m_now_parent_path = m_now_path.parent_path();
	vector<MacroCode>* now_vec = &m_main_macro;
	bool is_in_main_macro = true;
	vector<pair<string, int>> called_sub;

	while (std::getline(infile, line))
	{
		lineno++;
		auto slash_pos = line.find("//");
		if (slash_pos != string::npos)
		{
			line = line.substr(0, slash_pos);
		}
		boost::trim(line);
		if (line.empty())
			continue;
		vector<string> dats;
		boost::split(dats, line, boost::is_any_of("*")); // repeater
		int rep = 1;
		if (dats.size() >= 2)
		{
			if (dats.size() > 2 || !try_int(dats[1], rep, 1))
			{
				ERROR_RETURN("Parse error");
			}
		}

		boost::trim(dats[0]);
		if (dats[0].empty())
		{
			macro_warning("Ignored empty code", lineno);
			continue;
		}

		// auto def_dat = dats[0];
		boost::to_upper(dats[0]);

		if (dats[0].starts_with('!')) // command
		{
			vector<string> subcmd = split_space(dats[0]);
			if (rep > 1 && (subcmd[0] != "!CALL"))
			{
				macro_warning("Can't repeat", lineno);
			}

			if (subcmd[0] == "!SLP")
			{
				int wait = 500;
				if (subcmd.size() >= 2)
				{
					if (subcmd.size() > 2 || !try_int(subcmd[1], wait, 500))
						ERROR_RETURN("Parse error");
				}
				MacroCode d(MacroCode_SLP, lineno);
				d.intargs.emplace_back(wait);
				now_vec->emplace_back(d);
			}
			else if (subcmd[0] == "!SLPL")
			{
				MacroCode d(MacroCode_SLP, lineno);
				d.intargs.emplace_back(1000);
				now_vec->emplace_back(d);
			}
			else if (subcmd[0] == "!CALL")
			{
				if (subcmd.size() != 2 && subcmd.size() != 3)
				{
					ERROR_RETURN("Arg error");
				}
				int lim = MAX_CALL_DEPTH + 4;
				if (subcmd.size() == 3)
				{
					if (!try_int(subcmd[2], lim, MAX_CALL_DEPTH + 4))
					{
						ERROR_RETURN("Parse error");
					}
				}
				MacroCode d(MacroCode_CALL, lineno, subcmd[1], rep);
				d.intargs.emplace_back(lim);
				called_sub.emplace_back(subcmd[1], lineno); // check after
				now_vec->emplace_back(d);
			}
			else if (subcmd[0] == "!TSTART")
			{
				m_has_tstart = true;
				now_vec->emplace_back(MacroCode_TSTART, lineno, 1);
			}
			else if (subcmd[0] == "!QUIT")
			{
				now_vec->emplace_back(MacroCode_QUIT, lineno, 1);
			}
			else if (subcmd[0] == "!SHOWFRAMES")
			{
				now_vec->emplace_back(MacroCode_SHOWFRAMES, lineno, 1);
			}
			else if (subcmd[0] == "!SEQCFG")
			{
				vector<int> args(3);
				if (subcmd.size() != 4 || 
					!try_int(subcmd[1], args[0], 0) ||
					!try_int(subcmd[2], args[1], 0) ||
					!try_int(subcmd[3], args[2], 0) ||
					!keyseq_config_validate(args[0], args[1], args[2])
					)
				{
					ERROR_RETURN("Parse error");
				}
				MacroCode d(MacroCode_SEQCFG, lineno, 1);
				d.intargs.swap(args);
				now_vec->emplace_back(d);
			}
			// else if (subcmd[0] == "!!WAITEV") // !!: consume a frame
			//{
			//	if (subcmd.size() != 2 || subcmd[1] != "SMM147_LEVEL_LOAD") // todo improve
			//	{
			//		ERROR_RETURN("Parse error");
			//	}
			//	MacroCode d(MacroCode_EV_WAIT, lineno);
			//	d.intargs.emplace_back(WaitEvent_SMM147_LEVEL_LOADED);
			//	d.input_status = VPAD_NULL;
			//	d.strargs.emplace_back(m_mapping.empty_show);
			//	now_vec->push_back(d);
			// }
			else
			{
				ERROR_RETURN(format("Unknown command: \"{}\"", subcmd[0]));
			}
		}
		else if (dats[0].starts_with('#'))
		{
			vector<string> subcmd = split_space(dats[0]);
			// preprocessor
			if (subcmd[0] == "#SUB")
			{
				if (subcmd.size() != 2)
				{
					ERROR_RETURN("Arg error");
				}
				if (!is_in_main_macro)
				{
					ERROR_RETURN("Subroutines cannot be nested");
				}
				const string name = subcmd[1];
				if (m_sub_macro.contains(name))
				{
					ERROR_RETURN(format("\"{}\" is already defined", name));
				}
				is_in_main_macro = false;
				m_sub_macro[name] = vector<MacroCode>();
				now_vec = &m_sub_macro[name];
			}
			else if (subcmd[0] == "#ENDSUB" || subcmd[0] == "#ESUB")
			{
				if (is_in_main_macro)
				{
					ERROR_RETURN(format("Unexcepted {}", subcmd[0]));
				}
				is_in_main_macro = true;
				now_vec = &m_main_macro;
			}
			// todo #EXPAND
			else
			{
				ERROR_RETURN(format("Unknown preprocessor \"{}\"", subcmd[0]));
			}
		}
		else // macro
		{
			bool check = true;
			string optim;
			// for (auto& e : def_dat)
			//	if (isalpha(e) && islower(e))
			//		macro_warning("Lower case letter", lineno);
			for (auto& c : dats[0])
			{
				if (isspace(c))
					continue;
				check &= m_mapping.to_key.contains(c);
				optim.push_back(c);
			}
			if (!check)
				macro_warning("Contains unknown key", lineno);

			// copy
			string show = m_mapping.empty_show;
			uint32be status = 0;
			for (auto& c : optim)
			{
				if (!m_mapping.to_key.contains(c) || m_mapping.to_show_index[c] <= 0)
					continue;
				status |= m_mapping.to_key[c];
				show[m_mapping.to_show_index[c] - 1] = m_mapping.to_show[c];
			}
			MacroCode d(MacroCode_Input, lineno, show, rep);
			d.input_status = status;
			now_vec->emplace_back(d);
		}
	}
	lineno++;
	if (!is_in_main_macro)
	{
		ERROR_RETURN("Excepted: ENDSUB");
	}
	for (auto& [name, lin] : called_sub)
	{
		if (!m_sub_macro.contains(name))
		{
			// lin != lineno
			macro_error(format("Unknown name: \"{}\"", name), lin);
			return unload(), false;
		}
	}

	generate_all_inputs();
	return true;
}