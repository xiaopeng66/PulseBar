#pragma once
#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

inline std::optional<bool> IsNewerNumericVersion(
    std::wstring_view candidate,std::wstring_view current)
{
    const auto split=[](std::wstring_view version,
                        std::vector<std::wstring_view>& parts) {
        if(version.empty())return false;
        size_t start{};
        for(size_t position=0;position<=version.size();++position) {
            if(position<version.size()&&version[position]!=L'.') {
                if(version[position]<L'0'||version[position]>L'9')return false;
                continue;
            }
            if(position==start)return false;
            auto part=version.substr(start,position-start);
            while(part.size()>1&&part.front()==L'0')part.remove_prefix(1);
            parts.push_back(part);
            start=position+1;
        }
        return true;
    };
    std::vector<std::wstring_view> candidate_parts,current_parts;
    if(!split(candidate,candidate_parts)||!split(current,current_parts))
        return std::nullopt;
    for(size_t index=0;index<(std::max)(candidate_parts.size(),current_parts.size());++index) {
        const std::wstring_view candidate_part=index<candidate_parts.size()
            ?candidate_parts[index]:std::wstring_view(L"0");
        const std::wstring_view current_part=index<current_parts.size()
            ?current_parts[index]:std::wstring_view(L"0");
        if(candidate_part.size()!=current_part.size())
            return candidate_part.size()>current_part.size();
        if(candidate_part!=current_part)return candidate_part>current_part;
    }
    return false;
}
