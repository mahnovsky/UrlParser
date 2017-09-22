#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <algorithm>
#include <vector>
#include <functional>
#include <limits>
#include <map>
#include <cassert>
#include <memory>

#define MAX_BUFFER_SIZE 1024 * 1024

struct Comparer
{
	bool operator() (const char* lhs, const char* rhs) const
	{
		return strncmp(lhs, rhs, MAX_BUFFER_SIZE) < 0;
	}
};

using PairVector = std::vector<Pair>;
using StringVector = std::vector<std::string>;
using CStringMap = std::map <const char*, size_t, Comparer>;


struct Args
{
	enum
	{
		Key,
		Number,
		InFile,
		OutFile,

		Count
	};

	int			Num;
	char const* inFile;
	char const* outFile;
};

inline bool CheckSymbol(char from, char to, char ch)
{
	return ch >= from && ch <= to;
}

inline bool CheckEnd(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

bool CheckSymbol(char const* str, char ch)
{
	for(char c = *str; *str; c = (*++str))
	{
		if (c == ch) return true;
	}

	return false;
}

struct Pair
{
	int			_count;
	char*		_str;

	Pair(const Pair& other) = delete;

	Pair(Pair&& other)
		:_count(other._count)
		, _str(other._str)
	{
		char* p = (_str - sizeof(char));
		other._str = nullptr;
	}

	Pair(int count, const char* str, size_t len)
		:_count(count)
		, _str(nullptr)
	{
		_str = (char*)malloc(len + sizeof(char));
		strncpy(_str, str, len);
		_str[len] = '\0';
	}

	bool operator==(const char* other) const
	{
		return strcmp(_str, other) == 0;
	}

	bool operator>(const Pair& other) const
	{
		if (_count == other._count) return strcmp(_str, other._str) > 0;
		return _count > other._count;
	}

	Pair& operator = ( Pair& other)
	{
		_count = other._count;
		_str = other._str;
		other._str = nullptr;

		return *this;
	}

	Pair& operator = ( Pair&& other)
	{
		_count = other._count;
		_str = other._str;
		other._str = nullptr;

		return *this;
	}
};

class StringPool
{
public:
	struct Node
	{
		size_t pos;
		std::unique_ptr<char, void(*)(void*)> buffer;

		Node(size_t l, char* b)
			: pos(l)
			, buffer(b, free)
		{}
	};

	StringPool(size_t max)
		:_maxSize(max)
	{
	}

	char* add(const char* str, size_t len)
	{
		size_t nlen = len + 1;
		char* res = nullptr;

		for (auto& n : _strings)
		{
			size_t last = _maxSize - n.pos;
			if (n.pos < _maxSize && (_maxSize - n.pos) > nlen)
			{
				char *bpos = n.buffer.get() + n.pos;
				strncpy(bpos, str, len);
				bpos[len] = '\0';
				n.pos += nlen;
				res = bpos;

				break;
			}
		}

		if (!res)
		{
			grow();
			return add(str, len);
		}

		return res;
	}

	char* add(const char* str)
	{
		return add(str, strnlen(str, _maxSize));
	}

	void grow()
	{
		_strings.emplace_back(0, (char*)malloc(_maxSize));
	}

private:
	const size_t	_maxSize;
	
	std::vector<Node> _strings;
};

class StringBuilder
{
public:
	StringBuilder(size_t buffSize)
		:_strBuff(nullptr)
		,_size(buffSize)
		, _pos(0)
	{
		assert(_size > 32);
		_strBuff = (char*)malloc(_size);
	}

	~StringBuilder()
	{
		free(_strBuff);
		_strBuff = nullptr;
	}

	void clear()
	{
		_pos = 0;
		*_strBuff = '\0';
	}

	void add(char c)
	{
		if (_pos < _size)
		{
			_strBuff[_pos] = c;
			++_pos;
			_strBuff[_pos] = '\0';
		}
	}

	char back() const { return _strBuff[_pos - 1]; }

	const char* getCStr() const { return _strBuff; }

	size_t getLen() const { return _pos; }

private:
	char*	_strBuff;
	size_t	_size;
	size_t	_pos;
};

class UrlCollector
{
	using HandleFunc = void (UrlCollector::*)(char, char*);
	

	enum
	{
		Begin = 0,
		Prefix,
		Domain,
		Path,
		Done,
		Count
	};

	HandleFunc _funcs[Count];

public:
	UrlCollector()
		:_word(1024)
		,_state(Begin)
		,_index(0)
		,_prefixLen(0)
		,_domainLen(0)
		,_prefix("http://")
		,_prefixS("https://")
		,_strings(MAX_BUFFER_SIZE)
	{
		_funcs[Begin]	= &UrlCollector::begin;
		_funcs[Prefix]	= &UrlCollector::prefix;
		_funcs[Domain]	= &UrlCollector::domain;
		_funcs[Path]	= &UrlCollector::path;
		_funcs[Done]	= &UrlCollector::done;
	}

	void update(char ch, char* pch)
	{
		(this->*_funcs[_state])(ch, pch);
	}

	bool done() const { return _state == Done; }

	size_t getPrefixLen() const { return _prefixLen; }

	size_t getDomainLen() const { return _domainLen; }

	const PairVector& getSortedDomains()
	{
		sortEqual(_domains, _domainIndexes);

		return _domains;
	}

	const PairVector& getSortedPaths()
	{
		sortEqual(_paths, _pathIndexes);

		return _paths;
	}

protected:

	void begin(char ch, char* pch)
	{
		char* m = pch;
		if (ch == 'h')
		{
			_word.clear();
			_index = 0;
			_state = Prefix;
			update(ch, pch);
		}
	}

	void prefix(char ch, char* pch)
	{
		bool pref = ch == _prefix[_index];
		bool prefS = ch == _prefixS[_index];

		if (!pref && !prefS)
		{
			_state = Begin;

			return;
		}

		if (ch == '/' && _word.getLen() >= (_prefixS.size() - 2) && _word.back() == ch)
		{
			_word.add(ch);

			if (pref && _word.getLen() == 7)
			{
				_state = Domain;
				_prefixLen = _word.getLen();
			}
			else if (prefS && _word.getLen() == _prefixS.size())
			{
				_state = Domain;
				_prefixLen = _word.getLen();
			}
		}
		else
		{
			_word.add(ch);
		}
		
		++_index;
	}

	void domain(char ch, char* pch)
	{
		if (ch == '/')
		{
			_state = Path;
			_domainLen = _word.getLen() - _prefixLen;
			
			size_t offset = _prefixLen;
			const char* buf = _word.getCStr();
			size_t len = _word.getLen() - offset;
			const char* str = _strings.add(buf + offset, len);

			put(_domains, _domainIndexes, str, len);
			
			update(ch, pch);
			return;
		}

		bool isValid =
			CheckSymbol('a', 'z', ch) ||
			CheckSymbol('A', 'Z', ch) ||
			CheckSymbol('0', '9', ch) ||
			CheckSymbol(".-", ch);

		if (CheckEnd(ch) || !isValid)
		{
			put(_paths, _pathIndexes, "/", 1);
			update('/', pch);
			_state = Done;
			
			return;
		}

		_word.add(ch);
	}

	void path(char ch, char* pch)
	{
		bool isValid =
			CheckSymbol('a', 'z', ch) ||
			CheckSymbol('A', 'Z', ch) ||
			CheckSymbol('0', '9', ch) ||
			CheckSymbol(".,/+_", ch);

		if (CheckEnd(ch) || !isValid)
		{
			_state = Done;
			
			size_t offset = _prefixLen + _domainLen;
			const char* buf = _word.getCStr();
			size_t len = _word.getLen() - offset;
			const char* str = _strings.add(buf + offset, len);

			put(_paths, _pathIndexes, str, len);

			return;
		}

		_word.add(ch);
	}

	void done(char, char*) { _state = Begin; }

	int swap(PairVector& cont, CStringMap& links, int newIndex, int index)
	{
		if (newIndex >= 0 && newIndex != index)
		{
			auto it = links.find(cont[newIndex]._str);
			if (it != links.end())
			{
				it->second = index;
				Pair p ( std::move(cont[index]) );
				cont[index] = std::move(cont[newIndex]);
				cont[newIndex] = std::move(p);
				index = newIndex;
			}
		}

		return index;
	}

	static bool cmp(const Pair& p1, const Pair& p2)
	{
		return strcmp(p1._str, p2._str) < 0;
	}

	void sortEqual(PairVector& cont, CStringMap& links)
	{
		if (cont.empty())
		{
			return;
		}

		int sValue = cont[0]._count;
		int prevIndex = 0;
		for (int i = 0; i < cont.size(); ++i)
		{
			int delta = i - prevIndex;
			if (cont[i]._count != sValue)
			{
				if (delta > 1)
				{
					std::sort(cont.begin() + prevIndex, cont.begin() + i, cmp);
				}

				sValue = cont[i]._count;
				prevIndex = i;
			}
		}
	}

	int sort(PairVector& cont, CStringMap& links, int index)
	{
		int lastIndex = -1;
		int sIndex = -1;
		for (int i = index - 1; i >= 0; --i)
		{
			if (cont[i]._count > cont[index]._count)
			{
				break;
			}

			if (cont[i]._count < cont[index]._count)
			{
				lastIndex = i;
			}
			else
			{
				break;
			}
		}

		index = swap(cont, links, lastIndex, index);

		return index;
	}

	void put(PairVector& cont, CStringMap& links, const char* str, size_t len)
	{
		auto it = links.find(str);
		if (it != links.end())
		{
			int index = it->second;
			++cont[index]._count;
			it->second = sort(cont, links, index);
		}
		else
		{
			size_t index = cont.size();
			cont.push_back(Pair(1, str, len));
			links[str] = index;
		}
	}

private:
	
	StringBuilder			_word;
	int						_state;
	int						_index;
	size_t					_prefixLen;
	size_t					_domainLen;
	PairVector				_domains;
	PairVector				_paths;

	CStringMap				_pathIndexes;
	CStringMap				_domainIndexes;

	const std::string		_prefix;
	const std::string		_prefixS;

	StringPool _strings;
};

bool isEntry(const char* str)
{
	return 
		str[0] == 'h' ||
		str[1] == 'h' ||
		str[2] == 'h' ||
		str[3] == 'h';
}

bool ParseArgs(int argv, char** const argc, Args& out)
{
	printf("Start parse args. \n");
	if (argv < 3)
	{
		return false;
	}

	int pos = argv - 1;
	out.outFile = argc[pos--];
	out.inFile = argc[pos--];

	if (pos == 2 && strncmp(argc[1], "-n", 2) == 0)
	{
		int num = atoi(argc[pos]);

		if (num > 0)
		{
			out.Num = num;
		}
	}
	else
	{
		out.Num = 0;
	}

	printf("--- Args ---\nIn file: %s\nOut file: %s\nNumber: %d\n------\n", out.inFile, out.outFile, out.Num);
	printf("End parse args. \n\n");

	return true;
}

void printResults(FILE* file, int num, const PairVector& v)
{
	int counter = 0;
	for (const Pair& p : v)
	{
		fprintf(file, "%d %s\n", p._count, p._str);
		
		if (++counter >= num && num != 0)
		{
			break;
		}
	}
}

void printResults(FILE* file, int num, int urls, UrlCollector& cheker)
{
	const PairVector& domains = cheker.getSortedDomains();
	const PairVector& paths = cheker.getSortedPaths();
	fprintf(file, "total urls %d, domains %d, paths %d\n", urls, domains.size(), paths.size());

	fprintf(file, "\ntop domains\n");
	printResults(file, num, domains);

	fprintf(file, "\ntop paths\n");
	printResults(file, num, paths);
}


int main(int argv, char** const argc)
{
	struct Pause { ~Pause() { system("pause"); } } pause;

	Args args;

	if (!ParseArgs(argv, argc, args))
	{
		printf("Failed parse arguments.\n");

		return 1;
	}

	FILE* inFile = fopen(args.inFile, "r");
	if (inFile)
	{
		char* buffer = (char*)malloc(MAX_BUFFER_SIZE);

		memset(buffer, 0, MAX_BUFFER_SIZE );

		int urlCount = 0;
		UrlCollector checker;
		
		while (!feof(inFile))
		{
			size_t readed = fread(buffer, sizeof(char), MAX_BUFFER_SIZE, inFile);
			
			for (size_t i = 0; i < readed; ++i)
			{
				checker.update(buffer[i], buffer);

				if (checker.done())
				{
					++urlCount;
				}
			}
		}

		FILE* outFile = fopen(args.outFile, "w");
		if (outFile)
		{
			printResults(outFile, args.Num, urlCount, checker);
		}

		free(buffer);
	}

	return 0;
}