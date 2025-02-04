// TODO improve
enum WaitEvents : int
{
	WaitEvent_NONE = 0,
	WaitEvent_SMM147_LEVEL_LOADED = 1,
};

enum MacroCodeType
{
	MacroCode_None,
	MacroCode_Input,
	MacroCode_SLP,
	MacroCode_QUIT,
	MacroCode_TSTART,
	MacroCode_CALL,
	MacroCode_SHOWFRAMES,
	MacroCode_SEQCFG,
	MacroCode_EV_WAIT,
};

struct MappingData
{
	std::unordered_map<char, uint32> to_key;
	std::unordered_map<char, char> to_show;
	std::unordered_map<char, int> to_show_index;
	char idxtoshow[16];
	std::string empty_show;
	std::string all_show;
	void build_show_data(const size_t& sz, const size_t& offset) {
		all_show.resize(sz);
		empty_show.assign(sz, ' ');
		for (size_t i = 0; i < sz; i++)
			all_show[i] = idxtoshow[offset + i];
	}
	void set(const char& c, const uint32& m, const char& s, const int& idx) {
		to_key[c] = m;
		to_show[c] = s;
		to_show_index[c] = idx;
		idxtoshow[idx] = s;
	}
};

struct MacroCode
{
	MacroCodeType type = MacroCode_None;
	int repeat = 1;
	bool eof = false;
	int line = 0;
	std::vector<std::string> strargs;
	std::vector<int> intargs;
	uint32be input_status = 0;
	MacroCode(const MacroCodeType type, const int line, const int rep = 1)
		: type(type), line(line), repeat(rep) {}
	MacroCode(const MacroCodeType type, const int line, const std::string& first_str, const int rep = 1)
		: type(type), line(line), repeat(rep)
	{
		strargs.emplace_back(first_str);
	}

	MacroCode() {}
};

struct RunInfo
{
	RunInfo* parent = nullptr;
	std::vector<MacroCode>* target;
	int now_step = 0;
	int now_pos = 0;
	int depth = 0;
};