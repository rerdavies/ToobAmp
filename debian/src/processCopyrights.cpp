#include "CommandLineParser.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string.h>
#include <set>
#include <map>
#include <utility>
#include "ss.hpp"
#include <algorithm>

using namespace toob;
using namespace std;

#define UNKNOWN_LICENSE "~unknown" // primary purpose: show this group last, not first.

bool excludeNones = true;

std::vector<std::string> libraryDirectories{
    "modules/RTNeural/modules/Eigen/Eigen/",
    "src/iir/",
    "modules/RTNeural/modules/xsimd/",
    "modules/RTNeural/",
    "src/"

};

static bool startsWith(const std::string &s, const std::string start)
{
    if (s.length() < start.length())
        return false;

    return std::equal(start.begin(), start.end(), s.begin());
}

std::string toProjectPath(const std::string path)
{
    for (const auto &libraryDirectory : libraryDirectories)
    {
        if (startsWith(path, libraryDirectory))
        {
            return libraryDirectory + "*";
        }
    }
    return path;
}

std::vector<std::string> unknownLicense = {
    "Unknown License",
    "."
    "These copyright notices were found, but we were unable to automatically identify an associated license."};

static bool hasFileName(const std::string &path, const std::string &fileName)
{
    if (path.length() > fileName.length())
    {
        if (path[path.length() - fileName.length() - 1] == '/')
        {
            return std::equal(fileName.rbegin(), fileName.rend(), path.rbegin());
        }
    }
    return false;
}

class Years
{
private:
    class Range
    {
    private:
        int year_;
        int count_ = 1;

    public:
        Range(int year)
            : year_(year),
              count_(1)
        {
        }
        Range(int fromYear, int toYear)
            : year_(fromYear),
              count_(toYear - fromYear + 1)
        {
        }

        bool Contains(int year) const
        {
            return year >= this->year_ && year <= this->year_ + this->count_;
        }

        bool CanMerge(int year) const
        {
            return year == this->year_ - 1 || year == this->year_ + this->count_;
        }

        bool Merge(int year)
        {
            if (year == this->year_ - 1)
            {
                --this->year_;
                ++this->count_;
                return true;
            }
            if (year == this->year_ + count_)
            {
                ++this->count_;
                return true;
            }
            return false;
        }
        int Year() const { return year_; }
        int LastYear() const { return year_ + count_ - 1; }
        int Count() const { return count_; }
        bool CanMerge(const Range &other) const
        {
            if (this->LastYear() < other.Year() - 1)
                return false;
            if (this->Year() > other.LastYear() + 1)
                return false;
            return true;
        }
        bool Merge(const Range &other)
        {
            if (!CanMerge(other))
                return false;

            int y = std::min(this->year_, other.year_);
            int l = std::max(this->LastYear(), other.LastYear());
            this->year_ = y;
            this->count_ = l - y + 1;
            return true;
        }
    };

    std::vector<Range> ranges;

    void Merge(const Range &range)
    {
        for (size_t i = 0; i < ranges.size(); ++i)
        {
            if (range.Year() <= ranges[i].LastYear()+1)
            {
                if (range.CanMerge(ranges[i]))
                {
                    ranges[i].Merge(range);
                }
                else
                {
                    ranges.insert(ranges.begin() + i, range);
                }
                while (i < ranges.size() - 1 && ranges[i].CanMerge(ranges[i + 1]))
                {
                    ranges[i].Merge(ranges[i + 1]);
                    ranges.erase(ranges.begin() + (i + 1));
                }
                return;
            }
        }
        ranges.push_back(range);
    }

public:
    Years()
    {
    }

    bool Contains(int year)
    {
        for (const auto range : ranges)
        {
            if (range.Contains(year))
            {
                return true;
            }
        }
        return false;
    }

    bool IsEmpty() const
    {
        return ranges.size() == 0;
    }
    static Years Merge(const Years &left, const Years &right)
    {
        Years result;
        // Merge anything with  (c) Name =>  (c) Name (no year range).
        if (left.ranges.size() == 0 || right.ranges.size() == 0)
        {
            return result;
        }
        int ixLeft = 0;
        int ixRight = 0;

        while (ixLeft < left.ranges.size() && ixRight < right.ranges.size())
        {
            if (left.ranges[ixLeft].Year() < right.ranges[ixRight].Year())
            {
                result.Merge(left.ranges[ixLeft++]);
            }
            else
            {
                result.Merge(right.ranges[ixRight++]);
            }
        }
        while (ixLeft != left.ranges.size())
        {
            result.Merge(left.ranges[ixLeft++]);
        }
        while (ixRight != right.ranges.size())
        {
            result.Merge(right.ranges[ixRight++]);
        }
        return result;
    }
    void Add(int fromYear, int toYear)
    {
        Merge(Range(fromYear, toYear));
    }
    void Add(int year)
    {
        for (size_t i = 0; i < ranges.size(); ++i)
        {
            if (ranges[i].Contains(year))
                return;
            if (ranges[i].CanMerge(year))
            {
                ranges[i].Merge(year);
                if (i > 0)
                {
                    if (ranges[i - 1].CanMerge(ranges[i]))
                    {
                        ranges[i - 1].Merge(ranges[i]);
                        ranges.erase(ranges.begin() + 1);
                        return;
                    }
                }
                if (i < ranges.size() - 1)
                {
                    if (ranges[i].CanMerge(ranges[i + 1]))
                    {
                        ranges[i].Merge(ranges[i + 1]);
                        ranges.erase(ranges.begin() + i + 1);
                    }
                }
            }
            if (year < ranges[i].Year())
            {
                ranges.insert(ranges.begin() + i, Range(year));
            }
        }
        ranges.push_back(Range(year));
    }
    bool operator<(const Years &other) const
    {
        size_t l = std::min(this->ranges.size(), other.ranges.size());
        for (size_t i = 0; i < l; ++i)
        {
            const Range &rangeL = this->ranges[i];
            const Range &rangeR = other.ranges[i];
            if (rangeL.Year() != rangeR.Year())
            {
                return rangeL.Year() < rangeR.Year();
            }
            if (rangeL.LastYear() != rangeR.LastYear())
            {
                return rangeL.LastYear() < rangeR.LastYear();
            }
        }
        return this->ranges.size() < other.ranges.size();
    }
    void write(std::ostream &os) const
    {
        bool firstTime = true;
        for (const auto &range : ranges)
        {
            if (!firstTime)
            {
                os << ",";
            }
            firstTime = false;
            if (range.Count() == 1)
            {
                os << range.Year();
            }
            else
            {
                os << range.Year() << '-' << range.LastYear();
            }
        }
    }
    std::string string() const {
        std::stringstream s;
        write(s);
        return s.str();
    }
};

inline std::ostream &operator<<(std::ostream &os, const Years &years)
{
    years.write(os);
    return os;
}

class Copyright
{
private:
    Years years;
    std::string copyrightHolder;
    static bool isDigit(int c)
    {
        return c >= '0' && c <= '9';
    }
    bool readDate(std::istream &s, int *pYear)
    {
        int year = 0;
        *pYear = 0;
        while (s.peek() == ' ')
            s.get();
        if (!isDigit(s.peek()))
        {
            return false;
        }
        while (isDigit(s.peek()))
        {
            year = s.get() - '0' + year * 10;
        }
        *pYear = year;
        return true;
    }

public:
    Copyright() {}
    Copyright(const Copyright &other)
        : years(other.years),
          copyrightHolder(other.copyrightHolder)
    {
    }

    Copyright(Copyright &&other)
        : years(std::forward<Years>(other.years)),
          copyrightHolder(std::forward<std::string>(other.copyrightHolder))
    {
    }

    Copyright(const std::string &copyright)
    {
        std::stringstream s(copyright);

        int year;
        while (readDate(s, &year))
        {
            while (s.peek() == ' ')
                s.get();
            if (s.peek() == '-')
            {
                s.get();
                int yearEnd;
                if (!readDate(s, &yearEnd))
                {
                    throw std::invalid_argument("Invalid date.");
                }
                years.Add(year, yearEnd);
            } else {
                years.Add(year);
            }
            while (s.peek() == ' ')
                s.get();
            if (s.peek() != ',')
            {
                break;
            }
            s.get();
        }
        while (s.peek() == ' ')
            s.get();
        std::stringstream os;
        while (s.peek() != -1)
        {
            os << (char)s.get();
        }
        copyrightHolder = os.str();
    }

    Copyright &operator=(const Copyright &other)
    {
        this->years = other.years;
        this->copyrightHolder = other.copyrightHolder;
        return *this;
    }
    Copyright &operator=(Copyright &&other)
    {
        this->years = std::move(other.years);
        this->copyrightHolder = std::move(other.copyrightHolder);
        return *this;
    }
    bool operator<(const Copyright &other) const
    {
        if (this->copyrightHolder != other.copyrightHolder)
        {
            return this->copyrightHolder < other.copyrightHolder;
        }
        return this->years < other.years;
    }

    const Years &CopyrightYears() const { return years; }
    const std::string &CopyrightHolder() const { return copyrightHolder; }

    bool IsEmpty() const
    {
        return years.IsEmpty() && copyrightHolder.length() == 0;
    }
    bool CanMerge(const Copyright &other)
    {
        return this->copyrightHolder == other.copyrightHolder;
    }

    void Merge(const Copyright &other)
    {
        if (!CanMerge(other)) {
            throw invalid_argument("Can't merge.");
        }
        this->years = Years::Merge(this->years, other.years);
    }
    std::string string() const
    {
        std::stringstream s;
        if (!years.IsEmpty())
        {
            s << years;
            s << ", ";
            s << copyrightHolder;
        } else {
            s << copyrightHolder;
        }
        return s.str();
    }
};
inline std::ostream &operator<<(std::ostream &os, const Copyright &copyright)
{
    os << copyright.string();
    return os;
}

class FileCopyrights
{
public:
    std::filesystem::path path;
    std::vector<Copyright> copyrights;

    void AddCopyright(const Copyright &copyright)
    {
        for (size_t i = 0; i < copyrights.size(); ++i)
        {
            if (copyrights[i].CanMerge(copyright))
            {
                copyrights[i].Merge(copyright);
                return;
            }
        }
        copyrights.push_back(copyright);
    }

    bool CanMergeCopyrights(const FileCopyrights &other)
    {
        for (const Copyright &copyright : other.copyrights)
        {
            bool canMerge = false;
            for (Copyright &myCopyright : this->copyrights)
            {
                if (myCopyright.CanMerge(copyright))
                {
                    canMerge = true;
                    break;
                }
            }
            if (!canMerge)
            {
                return false;
            }
        }
        return true;
    }
    void MergeCopyrights(const FileCopyrights &other)
    {
        for (const Copyright &copyright : other.copyrights)
        {
            bool canMerge = false;
            for (Copyright &myCopyright : this->copyrights)
            {
                if (myCopyright.CanMerge(copyright))
                {
                    canMerge = true;
                    myCopyright.Merge(copyright);
                    break;
                }
            }
            if (!canMerge)
            {
                throw std::invalid_argument("Can't merge copyrights");
            }
        }
    }
};

class Copyrights
{
    struct License
    {
        std::string tag;
        std::vector<std::string> licenseText;
        std::vector<FileCopyrights> fileCopyrights;

        void ApplyLicenseCopyrights(License *unknownLicenses)
        {
            std::vector<FileCopyrights> licenseFileCopyrights;
            for (auto &fc : fileCopyrights)
            {
                if (fc.path.filename() == "LICENSE")
                {
                    // rewrite xyz/LICENSE as  xyz/*
                    fc.path = fc.path.parent_path() / "*";
                    // and save a copy.
                    licenseFileCopyrights.push_back(fc); // save a copy.
                }
            }
            for (auto &licenseFile : licenseFileCopyrights)
            {
                // children of a directory with a license file and compatible copyrights
                // get removed.
                std::string pattern = licenseFile.path.parent_path().string();
                for (auto iter = fileCopyrights.begin(); iter != fileCopyrights.end(); ++iter)
                {
                    std::string p = iter->path.string();
                    if (p.length() > pattern.length() && std::equal(pattern.begin(), pattern.end(), p.begin()))
                    {
                        if (licenseFile.CanMergeCopyrights(*iter))
                        {
                            licenseFile.MergeCopyrights(*iter);
                            fileCopyrights.erase(iter);
                            --iter;
                        }
                    }
                }
                // chldren of a directory with a license file that have  unknown licenses get removed.
                for (auto iter = unknownLicenses->fileCopyrights.begin(); iter != unknownLicenses->fileCopyrights.end(); ++iter)
                {
                    std::string p = iter->path.string();
                    if (p.length() > pattern.length() && std::equal(pattern.begin(), pattern.end(), p.begin()))
                    {
                        unknownLicenses->fileCopyrights.erase(iter);
                        --iter;
                    }
                }
            }
        }
    };

    std::vector<std::string> ignoredFiles;
    std::vector<std::string> ignoredDirectories;
    std::map<std::string, std::shared_ptr<License>> licenseMap;

    static std::string trim(const std::string &value)
    {
        size_t start = 0;
        while (start < value.length() && value[start] == ' ' || value[start] == '\t')
        {
            ++start;
        }
        size_t end = value.length();
        while (end > start && value[end - 1] == ' ' || value[end - 1] == '\t')
        {
            --end;
        }
        return value.substr(start, end - start);
    }
    void addLicenseText(const std::string &license, const std::vector<std::string> &licenseText)
    {
        auto currentLicense = licenseMap.find(license);
        if (currentLicense == licenseMap.end())
        {
            std::shared_ptr<License> l = std::make_shared<License>();
            l->tag = license;
            l->licenseText = licenseText;
            licenseMap[license] = std::move(l);
        }
        else
        {
            (*currentLicense).second->licenseText = licenseText;
        }
    }

    void addCopyright(std::string license, const Copyright &copyright, const std::string &file)
    {
        if (copyright.IsEmpty())
            return;
        if (copyright.CopyrightHolder() == "no-info-found")
            return;
        if (copyright.CopyrightHolder() == "info-missing")
            return;
        if (license == "")
        {
            license = UNKNOWN_LICENSE;
        }
        const auto &currentLicense = licenseMap.find(license);
        std::shared_ptr<License> l;

        if (currentLicense == licenseMap.end())
        {
            l = std::make_shared<License>();
            l->tag = license;
            licenseMap[license] = l;
        }
        else
        {
            l = currentLicense->second;
        }

        bool found = false;
        for (auto &fileCopyright : l->fileCopyrights)
        {
            if (fileCopyright.path == file)
            {
                fileCopyright.AddCopyright(copyright);
                found = true;
            }
        }
        if (!found)
        {
            FileCopyrights newItem;
            newItem.path = file;
            newItem.AddCopyright(copyright);
            l->fileCopyrights.push_back(newItem);
        }
    }
    bool splitLicense(const std::string &license, const std::string &match, std::string *pLeft, std::string *pRight)
    {
        auto pos = license.find(match);
        if (pos != std::string::npos)
        {
            *pLeft = trim(license.substr(0, pos));
            *pRight = trim(license.substr(pos + match.length()));
            return true;
        }
        return false;
    }
    void parseLicenses(const std::string &license, std::vector<std::string> *pLicenses)
    {
        std::string l, r;
        if (splitLicense(license, " and ", &l, &r))
        {
            parseLicenses(l, pLicenses);
            parseLicenses(r, pLicenses);
        }
        else if (splitLicense(license, " or ", &l, &r))
        {
            parseLicenses(l, pLicenses);
            parseLicenses(r, pLicenses);
        }
        else
        {
            auto pos = license.find_last_of(',');
            if (pos != std::string::npos)
            {
                pLicenses->push_back(license.substr(0, pos));
            }
            else
            {
                pLicenses->push_back(license);
            }
        }
    }
    void addCopyright(
        std::string license,
        const std::vector<std::string> &licenseText,
        const std::vector<Copyright> &copyrights,
        const std::vector<std::string> &files)
    {
        if (license.length() == 0 && licenseText.size() != 0)
        {
            std::stringstream uniqueName;
            uniqueName << "unique-" << licenseMap.size();
            license = uniqueName.str();
        }
        std::vector<std::string> licenses;
        parseLicenses(license, &licenses);
        for (size_t i = 0; i < licenses.size(); ++i)
        {
            for (size_t j = 0; j < copyrights.size(); ++j)
            {
                for (size_t k = 0; k < files.size(); ++k)
                {
                    bool include = true;
                    if (excludeNones)
                    {
                        if (copyrights[j].CopyrightHolder() == "NONE")
                        {
                            include = false;
                        }
                        if (licenses[i] == "UNKNOWN")
                        {
                            include = false;
                        }

                    }
                    if (include)
                    {
                        addCopyright(licenses[i], copyrights[j], files[k]);
                    }
                }
            }
        }
        if (licenseText.size() != 0)
        {
            addLicenseText(license, licenseText);
        }
    }
    static std::vector<std::string> copyrightPrefixes;

    std::string stripCopyrightPrefix(std::string text)
    {
        while (true)
        {
            bool found = false;
            for (auto &prefix : copyrightPrefixes)
            {
                if (text.rfind(prefix, 0) != std::string::npos)
                {
                    text = trim(text.substr(prefix.length()));
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                return text;
            }
        }
    }
    bool isFileIgnored(std::string &fileName)
    {
        for (size_t i = 0; i < ignoredFiles.size(); ++i)
        {
            // stooopid c++ no string::endswith. :-/
            const std::string &t = ignoredFiles[i];

            if (hasFileName(fileName, t))
            {
                return true;
            }
        }
        return false;
    }
    bool isDirectoryIgnored(const std::string &fileName)
    {
        for (auto&directory: ignoredDirectories)
        {
            if (fileName.starts_with(directory)) {
                return true;
            }
        }
        return false;
    }
    std::string upstreamName;
    std::string upstreamContact;
    std::string source;

public:
    void ignoreFiles(const std::string &fileName)
    {
        this->ignoredFiles.push_back(fileName);
    }
    void ignoreDirectory(const std::string &directoryName)
    {
        this->ignoredDirectories.push_back(directoryName);
    }
    void ApplyLicenseCopyrights()
    {
        auto *unknownLicenses = this->licenseMap["UNKNOWN"].get();
        ;

        for (auto &licensePair : this->licenseMap)
        {
            auto &license = licensePair.second;
            license->ApplyLicenseCopyrights(unknownLicenses);
        }
    }

    void write(std::ostream &outputStream)
    {
        // write head paragraph
        outputStream << "Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/" << endl;
        outputStream << "Upstream-Name: " << upstreamName << endl;
        outputStream << "Upstream-Contact: " << upstreamContact << endl;
        outputStream << "Source: " << source << endl;
        outputStream << endl;

        std::set<std::string> usedLicenses;
        for (auto &licensePair : this->licenseMap)
        {
            auto &license = licensePair.second;
            for (auto &file : license->fileCopyrights)
            {
                if (file.copyrights.size() != 0)
                {
                    outputStream << "File: " << file.path << endl;
                    outputStream << "Copyright:";
                    for (auto &copyright : file.copyrights)
                    {
                        outputStream << " " << copyright << endl;
                    }
                    outputStream << "License: " << licensePair.first << endl;
                    usedLicenses.insert(licensePair.first);
                    outputStream << endl;
                }
            }
        }
        for (const auto &usedLicense : usedLicenses)
        {
            const auto&license = licenseMap[usedLicense];
            outputStream << "License: " << usedLicense << endl;
            for (const auto&text : license->licenseText)
            {
                outputStream << " " << text << endl;
            }
            outputStream << endl;
        }
    }
    void write(const std::filesystem::path &path)
    {
        std::ofstream f;
        f.open(path);
        write(f);
    }
    void loadFile(const std::filesystem::path &path)
    {
        std::ifstream f(path);
        if (!f.is_open())
        {
            stringstream s;
            s << "Can't open file " << path;
            throw std::invalid_argument(s.str());
        }
        std::vector<std::string> files;
        std::vector<Copyright> copyrights;
        std::vector<std::string> licenseText;
        std::string license;
        enum class State
        {
            none,
            copyright,
            license,
            files,
            other
        };
        State state = State::none;

        while (true)
        {
            if (f.eof())
                break;
            std::string line;
            std::getline(f, line);
            bool isContinuation = (line[0] == ' ' || line[0] == '\t');
            line = trim(line);

            if (line.length() == 0)
            {
                if (files.size() != 0 && copyrights.size() != 0 || licenseText.size() != 0)
                {
                    addCopyright(license, licenseText, copyrights, files);
                }
                copyrights.clear();
                license.clear();
                licenseText.clear();
                files.clear();
            }
            else
            {
                if (isContinuation)
                {
                    switch (state)
                    {
                    case State::files:
                        if ((!isFileIgnored(line)) && !isDirectoryIgnored(line))
                        {
                            std::string shortPath = toProjectPath(line);
                            if (std::find(files.begin(), files.end(), shortPath) == files.end())
                            {
                                files.push_back(shortPath);
                            }
                        }
                        break;
                    case State::copyright:
                        copyrights.push_back(Copyright(line));
                        break;
                    case State::none:
                        break; // permissively ignore it.
                    case State::license:
                        licenseText.push_back(line);
                        break;
                    case State::other:
                        // ignore
                        break;
                    }
                }
                else
                {
                    int nPos = line.find(':');
                    if (nPos != std::string::npos)
                    {
                        std::string tag = line.substr(0, nPos);
                        std::string arg = trim(line.substr(nPos + 1));

                        if (tag == "Upstream-Name")
                        {
                            this->upstreamName = arg;
                        }
                        if (tag == "Upstream-Contact")
                        {
                            this->upstreamContact = arg;
                        }
                        if (tag == "Source")
                        {
                            this->source = arg;
                        }

                        if (tag == "Files")
                        {
                            if ((!isFileIgnored(arg)) && !isDirectoryIgnored(arg))
                            {
                                files.push_back(toProjectPath(arg));
                            }
                            state = State::files;
                        }
                        else if (tag == "Copyright")
                        {
                            copyrights.push_back(arg);
                            state = State::copyright;
                        }
                        else if (tag == "License")
                        {
                            license = arg;
                            state = State::license;
                        }
                        else
                        {
                            state = State::other;
                        }
                    }
                }
            }
        }
    }
};

std::vector<std::string> Copyrights::copyrightPrefixes = {
    "Copyright",
    "copyright",
    "(c)",
    "(C)",
    "\u00A9"};

void cleanCopyrights(Copyrights &copyrights)
{
    // copyrights.ApplyLicenseCopyrights();
}

int main(int argc, const char *argv[])
{
    CommandLineParser parser;
    bool help = false;
    bool helpError = false;
    std::string outputFile;
    std::vector<std::string> dependentModules;
    parser.AddOption("-h", &help);
    parser.AddOption("--help", &help);
    parser.AddOption("--output", &outputFile);
    parser.AddOption("--dependent", &dependentModules);

    try
    {
        parser.Parse(argc, argv);
    }
    catch (const std::exception &e)
    {
        cerr << "Error: " << e.what() << endl;
        helpError = true;
    }
    if (help || helpError)
    {
        if (helpError)
        {
            cerr << endl;
        }
        cout << "processCopyrights - Process and merge debian copyright files." << endl
             << endl;

        cout << "Syntax:" << endl
             << "    processcopyrights [inputFiles...]  [ -dependentModule <moduleName> ]*" << endl;
        return helpError ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    try
    {
        std::ofstream outputFileStream;
        if (outputFile.length() != 0)
        {
            outputFileStream.open(outputFile, std::ofstream::out | std::ofstream::trunc);
            if (!outputFileStream.is_open())
            {
                throw std::invalid_argument(SS("Can't open output file " << outputFile));
            }
        }

        std::ostream &outputStream = outputFile.length() != 0 ? outputFileStream : std::cout;
        Copyrights copyrights;

        copyrights.ignoreFiles("README.md");
        copyrights.ignoreFiles("GeneralBlockPanelKernel.h");
        copyrights.ignoreDirectory("modules/NeuralAmpModelerCore/Dependencies/eigen/unsupported/");
        copyrights.ignoreDirectory("modules/NeuralAmpModelerCore/Dependencies/eigen/bench/");
        copyrights.ignoreDirectory("modules/NeuralAmpModelerCore/Dependencies/eigen/scripts/");

        for (const std::string &arg : parser.Arguments())
        {
            copyrights.loadFile(arg);
        }
        for (std::string dependent : dependentModules)
        {
            cout << "Processing copyrights for module " << dependent << endl;
            std::filesystem::path dependentCopyrightFile =
                std::filesystem::path("/usr/share/doc") / dependent / "copyright";
            copyrights.loadFile(dependentCopyrightFile);
        }
        cleanCopyrights(copyrights);
        copyrights.write(outputStream);

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        std::cout << "Error: " << e.what() << std::endl;
    }
}
