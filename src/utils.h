#ifndef UTILS_INCLUDED
#define UTILS_INCLUDED

#include <string>
#include <algorithm>
#include <sstream>
class Utils
{
public:
    enum Module
    {
        m_NONE,
        m_ALL,
        m_ChatBot,
        m_RETROSHARERPC,
        m_AUTORESPONSE,
        m_IRC
    };

    enum IMCType
    {
        imct_NONE,
        imct_CHAT,
        imct_COMMAND
    };

    static const char InterModuleCommunicationMessage_splitter = ';';
    struct InterModuleCommunicationMessage
    {
        Module from = Utils::Module::m_NONE;
        Module to = Utils::Module::m_NONE;
        IMCType type = imct_NONE;
        std::string msg = "";
    };

    const static uint16_t MODULE_ALL            = 0x00;
    const static uint16_t MODULE_RETROSHARE     = 0x01;
    const static uint16_t MODULE_IRC            = 0x02;

    static inline bool isModuleAll(uint16_t& i)
    {
        return i == MODULE_ALL;
    }

    static inline bool isModuleRetroShare(uint16_t& i)
    {
        return i & MODULE_RETROSHARE;
    }

    static inline bool isModuleIRC(uint16_t& i)
    {
        return i & MODULE_IRC;
    }

    static void replaceAll(std::string& str, const std::string& from, const std::string& to)
    {
        if(from.empty())
            return;
        size_t start_pos = 0;
        while((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
        }
    }

    static std::string& stripHTMLTags(std::string& s)
    {
        // Remove all special HTML characters
        bool done = false;
        while(!done)
        {
            // Look for start of tag:
            size_t leftPos = s.find('<');
            if(leftPos != std::string::npos)
            {
                // See if tag close is in this line:
                size_t rightPos = s.find('>', leftPos);
                if(rightPos == std::string::npos)
                {
                    /*inTag = */done = true;
                    s.erase(leftPos);
                }
                else
                    s.erase(leftPos, rightPos - leftPos + 1);
            }
            else
                done = true;
        }

        replaceAll(s, "&lt;", "<");
        replaceAll(s, "&gt;", ">");
        replaceAll(s, "&amp;", "&");
        replaceAll(s, "&nbsp;", " ");

        return s;
    }
};


// to lower
static inline std::string& toLower(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static inline std::string toLower(const std::string& s_in)
{
    std::string s = s_in;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}


// trim from start
static inline std::string &ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim from end
static inline std::string &rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim from both ends
static inline std::string &trim(std::string &s)
{
    return ltrim(rtrim(s));
}

static inline bool parseBool(std::string &s)
{
    return s == "1" ? true : false;
}

static inline std::string parseBool(bool b)
{
    return b ? "1" : "0";
}

static inline std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
    {
        elems.push_back(item);
    }
    return elems;
}

static inline std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

#endif // UTILS_INCLUDED
