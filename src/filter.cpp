#include <array>
#include <cassert>
#include <cctype>
#include <regex>

#include "mega/filesystem.h"
#include "mega/filter.h"
#include "mega/utils.h"

namespace mega
{

class Matcher;
class Target;

// For convenience.
using FilterPtr  = std::unique_ptr<Filter>;
using MatcherPtr = std::unique_ptr<Matcher>;

class Filter
{
public:
    virtual ~Filter() = default;

    // True if this filter is applicable to type.
    bool applicable(const nodetype_t type) const;

    // True if this filter is an inclusion.
    bool inclusion() const;

    // True if this filter is inheritable.
    bool inheritable() const;

    // True if this filter matches the string pair p.
    virtual bool match(const string_pair& p) const = 0;

protected:
    Filter(MatcherPtr matcher,
           const Target& target,
           const bool inclusion,
           const bool inheritable);

    // True if this filter matches the string s.
    bool match(const string& s) const;

private:
    MatcherPtr mMatcher;
    const Target& mTarget;
    const bool mInclusion;
    const bool mInheritable;
}; /* Filter */

class NameFilter
  : public Filter
{
public:
    NameFilter(MatcherPtr matcher,
               const Target& target,
               const bool inclusion,
               const bool inheritable);

    bool match(const string_pair& p) const;
}; /* NameFilter */

class PathFilter
  : public Filter
{
public:
    PathFilter(MatcherPtr matcher,
               const Target& target,
               const bool inclusion,
               const bool inheritable);

    bool match(const string_pair& p) const;
}; /* PathFilter */

class Matcher
{
public:
    virtual ~Matcher() = default;

    // True if this matcher matches the string s.
    virtual bool match(const string& s) const = 0;

protected:
    Matcher() = default;
}; /* Matcher */

class GlobMatcher
  : public Matcher
{
public:
    GlobMatcher(const string& pattern, const bool caseSensitive);

    // True if the wildcard pattern matches the string s.
    bool match(const string& s) const override;

private:
    const string mPattern;
    const bool mCaseSensitive;
}; /* GlobMatcher */

class RegexMatcher
  : public Matcher
{
public:
    RegexMatcher(const string& pattern, const bool caseSensitive);

    // True if the regex pattern matches the string s.
    bool match(const string& s) const override;

private:
    std::regex mRegexp;
}; /* RegexMatcher */

class Target
{
public:
    virtual ~Target() = default;

    // True if this target is applicable to type.
    virtual bool applicable(const nodetype_t type) const = 0;

protected:
    Target() = default;
}; /* Target */

class AllTarget
  : public Target
{
public:
    // Always returns true.
    bool applicable(const nodetype_t) const override;

    // Returns an AllTarget instance.
    static const AllTarget& instance();

private:
    AllTarget() = default;
}; /* AllTarget */

class DirectoryTarget
  : public Target
{
public:
    // True if type is FOLDERNODE.
    bool applicable(const nodetype_t type) const override;

    // Returns a DirectoryTarget instance.
    static const DirectoryTarget& instance();

private:
    DirectoryTarget() = default;
}; /* DirectoryTarget */

class FileTarget
  : public Target
{
public:
    // True if type is FILENODE.
    bool applicable(const nodetype_t type) const override;

    // Returns a FileTarget instance.
    static const FileTarget& instance();

private:
    FileTarget() = default;
}; /* FileTarget */

// Returns true if the substring m..n is empty.
static bool isEmpty(const char* m, const char* n);

// Returns appropriate regex flags.
static std::regex::flag_type regexFlags(const bool caseSensitive);

// Logs a syntax error and returns false.
static bool syntaxError(const string& text);

// Uppercases the string text.
static string toUpper(string text);

FilterResult::FilterResult()
  : included(false)
  , matched(false)
{
}

FilterResult::FilterResult(const bool included)
  : included(included)
  , matched(true)
{
}

FilterChain::FilterChain()
  : mFilters()
{
}

FilterChain::~FilterChain()
{
    clear();
}

bool FilterChain::add(const string& text)
{
    enum FilterType
    {
        FT_NAME,
        FT_PATH
    }; /* FilterType */

    enum MatchStrategy
    {
        MS_GLOB,
        MS_REGEXP
    }; /* MatchStrategy */

    const char* m = text.data();
    const char* n = m + text.size();
    const Target* target;
    FilterType type;
    MatchStrategy strategy;
    bool caseSensitive = false;
    bool inclusion;
    bool inheritable = true;

    // What class of filter is this?
    switch (*m++)
    {
    case '+':
        // Inclusion filter.
        inclusion = true;
        break;
    case '-':
        // Exclusion filter.
        inclusion = false;
        break;
    default:
        // Invalid filter class.
        return syntaxError(text);
    }

    // What kind of node does this filter apply to?
    switch (*m)
    {
    case 'a':
        // Applies to all node types.
        ++m;
        target = &AllTarget::instance();
        break;
    case 'd':
        // Applies only to directories.
        ++m;
        target = &DirectoryTarget::instance();
        break;
    case 'f':
        // Applies only to files.
        ++m;
        target = &FileTarget::instance();
        break;
    default:
        // Default applies to all node types.
        target = &AllTarget::instance();
        break;
    }

    // What type of filter is this?
    switch (*m)
    {
    case 'N':
        // Local name filter.
        ++m;
        inheritable = false;
        type = FT_NAME;
        break;
    case 'n':
        // Subtree name filter.
        ++m;
        type = FT_NAME;
        break;
    case 'p':
        // Path filter.
        ++m;
        type = FT_PATH;
        break;
    default:
        // Default to subtree name filter.
        type = FT_NAME;
        break;
    }

    // What matching strategy does this filter use?
    switch (*m)
    {
    case 'G':
        // Case-sensitive glob match.
        caseSensitive = true;
        ++m;
        strategy = MS_GLOB;
        break;
    case 'g':
        // Case-insensitive glob match.
        ++m;
        strategy = MS_GLOB;
        break;
    case 'R':
        // Case-sensitive regexp match.
        caseSensitive = true;
        ++m;
        strategy = MS_REGEXP;
        break;
    case 'r':
        // Case-insensitive regexp match.
        ++m;
        strategy = MS_REGEXP;
        break;
    default:
        // Default to case-insensitive glob match.
        strategy = MS_GLOB;
        break;
    }

    // Make sure we're at the start of the pattern.
    if (*m++ != ':')
    {
        return syntaxError(text);
    }

    // Is the pattern effectively empty?
    if (isEmpty(m, n))
    {
        return syntaxError(text);
    }

    // Create the filter's matcher.
    MatcherPtr matcher;

    try
    {
        switch (strategy)
        {
        case MS_GLOB:
            matcher.reset(new GlobMatcher(m, caseSensitive));
            break;
        case MS_REGEXP:
            // This'll throw if the regex is malformed.
            matcher.reset(new RegexMatcher(m, caseSensitive));
            break;
        }
    }
    catch (std::regex_error&)
    {
        return syntaxError(text);
    }

    // Create the filter.
    FilterPtr filter;

    switch (type)
    {
    case FT_NAME:
        filter.reset(new NameFilter(std::move(matcher),
                                    *target,
                                    inclusion,
                                    inheritable));
        break;
    case FT_PATH:
        filter.reset(new PathFilter(std::move(matcher),
                                    *target,
                                    inclusion,
                                    inheritable));
        break;
    }

    // Add the filter to the chain.
    mFilters.emplace_back(filter.get());
    filter.release();

    return true;
}

void FilterChain::clear()
{
    for (Filter* filter : mFilters)
    {
        delete filter;
    }

    mFilters.clear();
}

bool FilterChain::empty() const
{
    return mFilters.empty();
}

bool FilterChain::load(FileAccess& fileAccess)
{
    FileInputStream isAccess(&fileAccess);

    return load(isAccess);
}

bool FilterChain::load(InputStreamAccess& isAccess)
{
    string_vector filters;

    // Read the filters, line by line.
    // Empty lines are omitted by readLines(...).
    if (!readLines(isAccess, filters))
    {
        return false;
    }

    // Save the current filters in case of error.
    vector<Filter*> oldFilters(std::move(mFilters));

    // Make sure mFilters is in a well-defined state.
    mFilters.clear();

    // Add all the filters.
    for (const auto& f : filters)
    {
        // Skip comments.
        if (f[0] == '#')
        {
            continue;
        }

        // Try and add the filter.
        if (!add(f))
        {
            // Restore previous filters.
            mFilters = std::move(oldFilters);

            // Changes are not committed.
            return false;
        }
    }

    // Changes are committed.
    return true;
}

FilterResult FilterChain::match(const string_pair& p,
                                const nodetype_t type,
                                const bool onlyInheritable) const
{
    for (auto i = mFilters.rbegin(); i != mFilters.rend(); ++i)
    {
        if (onlyInheritable && !(*i)->inheritable())
        {
            continue;
        }

        if ((*i)->applicable(type) && (*i)->match(p))
        {
            return FilterResult((*i)->inclusion());
        }
    }

    return FilterResult();
}

bool Filter::applicable(const nodetype_t type) const
{
    return mTarget.applicable(type);
}

bool Filter::inclusion() const
{
    return mInclusion;
}

bool Filter::inheritable() const
{
    return mInheritable;
}

Filter::Filter(MatcherPtr matcher,
               const Target& target,
               const bool inclusion,
               const bool inheritable)
  : mMatcher(std::move(matcher))
  , mTarget(target)
  , mInclusion(inclusion)
  , mInheritable(inheritable)
{
}

bool Filter::match(const string& s) const
{
    return mMatcher->match(s);
}

NameFilter::NameFilter(MatcherPtr matcher,
                       const Target& target,
                       const bool inclusion,
                       const bool inheritable)
  : Filter(std::move(matcher), target, inclusion, inheritable)
{
}

bool NameFilter::match(const string_pair& p) const
{
    return Filter::match(p.first);
}

PathFilter::PathFilter(MatcherPtr matcher,
                       const Target& target,
                       const bool inclusion,
                       const bool inheritable)
  : Filter(std::move(matcher), target, inclusion, inheritable)
{
}

bool PathFilter::match(const string_pair& p) const
{
    return Filter::match(p.second);
}

GlobMatcher::GlobMatcher(const string &pattern, const bool caseSensitive)
  : mPattern(caseSensitive ? pattern : toUpper(pattern))
  , mCaseSensitive(caseSensitive)
{
}

bool GlobMatcher::match(const string& s) const
{
    if (mCaseSensitive)
    {
        return wildcardMatch(s.c_str(), mPattern.c_str());
    }

    return wildcardMatch(toUpper(s).c_str(), mPattern.c_str());
}

RegexMatcher::RegexMatcher(const string& pattern, const bool caseSensitive)
  : mRegexp(pattern, regexFlags(caseSensitive))
{
}

bool RegexMatcher::match(const string& s) const
{
    return std::regex_match(s, mRegexp);
}

bool AllTarget::applicable(const nodetype_t) const
{
    return true;
}

const AllTarget& AllTarget::instance()
{
    static AllTarget instance;

    return instance;
}

bool DirectoryTarget::applicable(const nodetype_t type) const
{
    return type == FOLDERNODE;
}

const DirectoryTarget& DirectoryTarget::instance()
{
    static DirectoryTarget instance;

    return instance;
}

bool FileTarget::applicable(const nodetype_t type) const
{
    return type == FILENODE;
}

const FileTarget& FileTarget::instance()
{
    static FileTarget instance;

    return instance;
}

bool isEmpty(const char* m, const char* n)
{
    const char* w = m;

    while (m < n)
    {
        w += std::isspace(*m++) > 0;
    }

    return n == w;
}

std::regex::flag_type regexFlags(const bool caseSensitive)
{
    using std::regex_constants::extended;
    using std::regex_constants::icase;
    using std::regex_constants::optimize;

    const std::regex::flag_type flags = extended | optimize;

    if (caseSensitive)
    {
        return flags;
    }

    return flags | icase;
}

bool syntaxError(const string& text)
{
    LOG_verbose << "Syntax error parsing: " << text;

    return false;
}

string toUpper(string text)
{
    for (char& character : text)
    {
        character = (char)std::toupper((unsigned char)character);
    }

    return text;
}

} /* mega */

