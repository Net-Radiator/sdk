/**
 * @file mega/filter.h
 * @brief Classes representing file filters.
 *
 * (c) 2013-2014 by Mega Limited, Auckland, New Zealand
 *
 * This file is part of the MEGA SDK - Client Access Engine.
 *
 * Applications using the MEGA API must present a valid application key
 * and comply with the the rules set forth in the Terms of Service.
 *
 * The MEGA SDK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @copyright Simplified (2-clause) BSD License.
 *
 * You should have received a copy of the license along with this
 * program.
 */
#ifndef MEGA_FILTER_H
#define MEGA_FILTER_H 1

#include "types.h"

namespace mega
{

// Forward Declarations (from Filesystem)
class FileAccess;
class InputStreamAccess;

// Forward Declaration
class Filter;

class MEGA_API FilterResult
{
public:
    FilterResult();

    explicit FilterResult(const bool included);

    MEGA_DEFAULT_COPY(FilterResult);
    MEGA_DEFAULT_MOVE(FilterResult);

    bool included;
    bool matched;
}; /* FilterResult */

class MEGA_API FilterChain
{
public:
    FilterChain();

    ~FilterChain();

    MEGA_DISABLE_COPY(FilterChain);
    MEGA_DEFAULT_MOVE(FilterChain);

    // Adds the filter represented by text to the chain.
    bool add(const string& text);

    // Removes all filters in this chain.
    void clear();

    // True if this chain contains no filters.
    bool empty() const;

    // Loads filters from a file.
    bool load(FileAccess& fileAccess);

    // Loads filters from a stream.
    bool load(InputStreamAccess& isAccess);

    // Attempts to locate a match for the string pair p.
    FilterResult match(const string_pair& p,
                       const nodetype_t type,
                       const bool onlyInheritable) const;

private:
    vector<Filter*> mFilters;
}; /* FilterChain */

} /* mega */

#endif /* ! MEGA_FILTER_H */

