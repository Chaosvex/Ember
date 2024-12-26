/**
 * Copyright (C) 2021 - Marcel Breyer - All Rights Reserved
 * Licensed under the MIT License. See LICENSE.md file in the project root for full license information.
 *
 * Implements a string_view with null some null-termination guarantees.
 * Also see: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1402r0.pdf.
 */

#ifndef CPP_UTIL_CSTRING_VIEW_HPP
#define CPP_UTIL_CSTRING_VIEW_HPP

#include <cassert>      // assert
#include <cstddef>      // std::size_t
#include <functional>   // std::hash
#include <ostream>      // std::basic_ostream
#include <string>       // std::basic_string, std::char_traits
#include <string_view>  // std::basic_string_view

#if __has_include(<compare>)
#include <compare>  // std::strong_ordering
#endif

#if __has_include(<ranges>)
#include <ranges>  // std::enable_borrowed_range, std::enable_view
#endif

#if __has_include(<format>)
#include <format>  // std::formatter, std::format_context
#endif

namespace ember {

template <typename charT, typename traits = std::char_traits<charT>>
class basic_cstring_view {
 public:
  static constexpr struct null_terminated_t {
  } null_terminated{};

  /*******************************************************************************************************************/
  /**                                                     types                                                     **/
  /*******************************************************************************************************************/
  using string_view_type = std::basic_string_view<charT, traits>;

  using traits_type = typename string_view_type::traits_type;
  using value_type = typename string_view_type::value_type;
  using pointer = typename string_view_type::pointer;
  using const_pointer = typename string_view_type::const_pointer;
  using reference = typename string_view_type::reference;
  using const_reference = typename string_view_type::const_reference;
  using iterator = typename string_view_type::iterator;
  using const_iterator = typename string_view_type::const_iterator;
  using reverse_iterator = typename string_view_type::reverse_iterator;
  using const_reverse_iterator = typename string_view_type::const_reverse_iterator;
  using size_type = typename string_view_type::size_type;
  using difference_type = typename string_view_type::difference_type;

  static constexpr size_type npos = string_view_type::npos;

  /*******************************************************************************************************************/
  /**                                          construction and assignment                                          **/
  /*******************************************************************************************************************/
  constexpr basic_cstring_view() noexcept = default;
  constexpr basic_cstring_view(const basic_cstring_view&) noexcept = default;
  constexpr basic_cstring_view(const charT* s) : sv_{ s } {}
  constexpr basic_cstring_view(const std::basic_string<charT, traits>& str) : sv_{ str } {}

  constexpr basic_cstring_view(null_terminated_t, const charT* s, const size_type len) : sv_{ s, len } {}
  constexpr basic_cstring_view(null_terminated_t, const string_view_type& sv) noexcept : sv_{ sv } {}

  constexpr basic_cstring_view& operator=(const basic_cstring_view&) noexcept = default;

  /*******************************************************************************************************************/
  /**                                               iterator support                                                **/
  /*******************************************************************************************************************/
  [[nodiscard]] constexpr const_iterator begin() const noexcept { return sv_.begin(); }
  [[nodiscard]] constexpr const_iterator end() const noexcept { return sv_.end(); }
  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return sv_.cbegin(); }
  [[nodiscard]] constexpr const_iterator cend() const noexcept { return sv_.cend(); }
  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return sv_.rbegin(); }
  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return sv_.rend(); }
  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return sv_.crbegin(); }
  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return sv_.crend(); }

  [[nodiscard]] friend constexpr const_iterator begin(basic_cstring_view sv) noexcept { return sv.begin(); }
  [[nodiscard]] friend constexpr const_iterator end(basic_cstring_view sv) noexcept { return sv.end(); }

  /*******************************************************************************************************************/
  /**                                                  capacity                                                     **/
  /*******************************************************************************************************************/
  [[nodiscard]] constexpr size_type size() const noexcept { return sv_.size(); }
  [[nodiscard]] constexpr size_type length() const noexcept { return sv_.length(); }
  [[nodiscard]] constexpr size_type max_size() const noexcept { return sv_.max_size(); }
  [[nodiscard]] constexpr bool empty() const noexcept { return sv_.empty(); }

  /*******************************************************************************************************************/
  /**                                                 element access                                                **/
  /*******************************************************************************************************************/
  [[nodiscard]] constexpr const_reference operator[](const size_type pos) const {
    assert((pos < this->size()) && "Undefined behavior if pos >= size()!");
    return sv_[pos];
  }
  [[nodiscard]] constexpr const_reference at(const size_type pos) const {
    return sv_.at(pos);
  }
  [[nodiscard]] constexpr const_reference front() const {
    assert((!this->empty()) && "Calling front() is undefined for empty cstring_views!");
    return sv_.front();
  }
  [[nodiscard]] constexpr const_reference back() const {
    assert((!this->empty()) && "Calling back() is undefined for empty cstring_views!");
    return sv_.back();
  }
  [[nodiscard]] constexpr const_pointer data() const noexcept { return sv_.data(); }

  [[nodiscard]] constexpr const charT* c_str() const noexcept { return sv_.data(); }

  [[nodiscard]] constexpr operator string_view_type() const noexcept { return sv_; }

  /*******************************************************************************************************************/
  /**                                                   modifiers                                                   **/
  /*******************************************************************************************************************/
  constexpr void remove_prefix(const size_type count) {
    assert((count <= this->size()) && "Undefined behavior if count > size()!");
    return sv_.remove_prefix(count);
  }
  // constexpr void remove_suffix(const size_type n);  // -> not applicable on a basic_cstring_view
  constexpr void swap(basic_cstring_view& sv) noexcept { sv_.swap(sv.sv_); }

  /*******************************************************************************************************************/
  /**                                               string operations                                               **/
  /*******************************************************************************************************************/
  constexpr size_type copy(charT* s, const size_type count, const size_type pos = 0) const {
    assert((s != nullptr) && "Undefined behavior if s == nullptr!");
    assert((pos <= this->size()) && "Undefined behavior if pos > size()!");
    return sv_.copy(s, count, pos);
  }

  [[nodiscard]] constexpr basic_cstring_view substr(const size_type pos = 0) const {
    assert((pos <= this->size()) && "Undefined behavior if pos > size()!");
    return basic_cstring_view{null_terminated, sv_.substr(pos)};
  }
  // constexpr basic_cstring_view substr(const size_type pos, const size_type count) const;  // -> not applicable on a basic_cstring_view
  [[nodiscard]] constexpr string_view_type substr(const size_type pos, const size_type count) const {
    assert((pos <= this->size()) && "Undefined behavior if pos > size()!");
    return sv_.substr(pos, count);
  }

  [[nodiscard]] constexpr int compare(const string_view_type sv) const noexcept { return sv_.compare(sv); }
  [[nodiscard]] constexpr int compare(const size_type pos, const size_type count, const string_view_type sv) const {
    assert((pos <= this->size()) && "Undefined behavior if pos > size()!");
    return sv_.compare(pos, count, sv);
  }
  [[nodiscard]] constexpr int compare(const size_type pos1, const size_type count1, const string_view_type sv, const size_type pos2,
                                      const size_type count2) const {
    assert((pos1 <= this->size()) && "Undefined behavior if pos1 > size()!");
    assert((pos2 <= sv.size()) && "Undefined behavior if pos2 > sv.size()!");
    return sv_.compare(pos1, count1, sv, pos2, count2);
  }
  [[nodiscard]] constexpr int compare(const charT* s) const { return sv_.compare(s); }
  [[nodiscard]] constexpr int compare(const size_type pos, const size_type count, const charT* s) const {
    assert((pos <= this->size()) && "Undefined behavior if pos > size()!");
    return sv_.compare(pos, count, s);
  }
  [[nodiscard]] constexpr int compare(const size_type pos, const size_type count1, const charT* s, const size_type count2) const {
    assert((pos <= this->size()) && "Undefined behavior if pos > size()!");
    return sv_.compare(pos, count1, s, count2);
  }

#if defined(__cpp_lib_starts_ends_with)
  [[nodiscard]] constexpr bool starts_with(const string_view_type sv) const noexcept { return sv_.starts_with(sv); }
  [[nodiscard]] constexpr bool starts_with(const charT c) const noexcept { return sv_.starts_with(c); }
  [[nodiscard]] constexpr bool starts_with(const charT* s) const { return sv_.starts_with(s); }
  [[nodiscard]] constexpr bool ends_with(const string_view_type sv) const noexcept { return sv_.ends_with(sv); }
  [[nodiscard]] constexpr bool ends_with(const charT c) const noexcept { return sv_.ends_with(c); }
  [[nodiscard]] constexpr bool ends_with(const charT* s) const { return sv_.ends_with(s); }
#else
  [[nodiscard]] constexpr bool starts_with(const string_view_type sv) const noexcept { return this->substr(0, sv.size()) == sv; }
  [[nodiscard]] constexpr bool starts_with(const charT c) const noexcept { return !this->empty() && traits::eq(this->front(), c); }
  [[nodiscard]] constexpr bool starts_with(const charT* s) const { return this->starts_with(string_view_type(s)); }
  [[nodiscard]] constexpr bool ends_with(const string_view_type sv) const noexcept {
    return this->size() >= sv.size() && this->compare(this->size() - sv.size(), npos, sv) == 0;
  }
  [[nodiscard]] constexpr bool ends_with(const charT c) const noexcept { return !this->empty() && traits::eq(this->back(), c); }
  [[nodiscard]] constexpr bool ends_with(const charT* s) const { return this->ends_with(string_view_type(s)); }
#endif

  [[nodiscard]] constexpr bool contains(const string_view_type sv) const noexcept { return this->find(sv) != npos; }
  [[nodiscard]] constexpr bool contains(const charT c) const noexcept { return this->find(c) != npos; }
  [[nodiscard]] constexpr bool contains(const charT* s) const { return this->find(s) != npos; }

  /*******************************************************************************************************************/
  /**                                                   searching                                                   **/
  /*******************************************************************************************************************/
  [[nodiscard]] constexpr size_type find(const string_view_type sv, const size_type pos = 0) const noexcept { return sv_.find(sv, pos); }
  [[nodiscard]] constexpr size_type find(const charT c, const size_type pos = 0) const noexcept { return sv_.find(c, pos); }
  [[nodiscard]] constexpr size_type find(const charT* s, const size_type pos, size_type count) const { return sv_.find(s, pos, count); }
  [[nodiscard]] constexpr size_type find(const charT* s, const size_type pos = 0) const { return sv_.find(s, pos); }

  [[nodiscard]] constexpr size_type rfind(const string_view_type sv, const size_type pos = npos) const noexcept { return sv_.rfind(sv, pos); }
  [[nodiscard]] constexpr size_type rfind(const charT c, const size_type pos = npos) const noexcept { return sv_.rfind(c, pos); }
  [[nodiscard]] constexpr size_type rfind(const charT* s, const size_type pos, size_type count) const { return sv_.rfind(s, pos, count); }
  [[nodiscard]] constexpr size_type rfind(const charT* s, const size_type pos = npos) const { return sv_.rfind(s, pos); }

  [[nodiscard]] constexpr size_type find_first_of(const string_view_type sv, const size_type pos = 0) const noexcept { return sv_.find_first_of(sv, pos); }
  [[nodiscard]] constexpr size_type find_first_of(const charT c, const size_type pos = 0) const noexcept { return sv_.find_first_of(c, pos); }
  [[nodiscard]] constexpr size_type find_first_of(const charT* s, const size_type pos, const size_type count) const { return sv_.find_first_of(s, pos, count); }
  [[nodiscard]] constexpr size_type find_first_of(const charT* s, const size_type pos = 0) const { return sv_.find_first_of(s, pos); }

  [[nodiscard]] constexpr size_type find_last_of(const string_view_type sv, const size_type pos = npos) const noexcept { return sv_.find_last_of(sv, pos); }
  [[nodiscard]] constexpr size_type find_last_of(const charT c, const size_type pos = npos) const noexcept { return sv_.find_last_of(c, pos); }
  [[nodiscard]] constexpr size_type find_last_of(const charT* s, const size_type pos, const size_type count) const { return sv_.find_last_of(s, pos, count); }
  [[nodiscard]] constexpr size_type find_last_of(const charT* s, const size_type pos = npos) const { return sv_.find_last_of(s, pos); }

  [[nodiscard]] constexpr size_type find_first_not_of(const string_view_type sv, const size_type pos = 0) const noexcept { return sv_.find_first_not_of(sv, pos); }
  [[nodiscard]] constexpr size_type find_first_not_of(const charT c, const size_type pos = 0) const noexcept { return sv_.find_first_not_of(c, pos); }
  [[nodiscard]] constexpr size_type find_first_not_of(const charT* s, const size_type pos, const size_type count) const { return sv_.find_first_not_of(s, pos, count); }
  [[nodiscard]] constexpr size_type find_first_not_of(const charT* s, const size_type pos = 0) const { return sv_.find_first_not_of(s, pos); }

  [[nodiscard]] constexpr size_type find_last_not_of(const string_view_type sv, const size_type pos = npos) const noexcept { return sv_.find_last_not_of(sv, pos); }
  [[nodiscard]] constexpr size_type find_last_not_of(const charT c, const size_type pos = npos) const noexcept { return sv_.find_last_not_of(c, pos); }
  [[nodiscard]] constexpr size_type find_last_not_of(const charT* s, const size_type pos, const size_type count) const { return sv_.find_last_not_of(s, pos, count); }
  [[nodiscard]] constexpr size_type find_last_not_of(const charT* s, const size_type pos = npos) const { return sv_.find_last_not_of(s, pos); }

  /*******************************************************************************************************************/
  /**                                              comparison functions                                             **/
  /*******************************************************************************************************************/
#if defined(__cpp_impl_three_way_comparison) && defined(__cpp_lib_three_way_comparison)
  [[nodiscard]] friend constexpr bool operator==(const basic_cstring_view lhs, const basic_cstring_view rhs) noexcept { return lhs.sv_ == rhs.sv_; }
  [[nodiscard]] friend constexpr std::strong_ordering operator<=>(const basic_cstring_view lhs, const basic_cstring_view rhs) noexcept { return lhs.sv_ <=> rhs.sv_; }
#else
  [[nodiscard]] friend constexpr bool operator==(basic_cstring_view lhs, basic_cstring_view rhs) noexcept { return lhs.sv_ == rhs.sv_; }
  [[nodiscard]] friend constexpr bool operator!=(basic_cstring_view lhs, basic_cstring_view rhs) noexcept { return lhs.sv_ != rhs.sv_; }
  [[nodiscard]] friend constexpr bool operator<(basic_cstring_view lhs, basic_cstring_view rhs) noexcept { return lhs.sv_ < rhs.sv_; }
  [[nodiscard]] friend constexpr bool operator<=(basic_cstring_view lhs, basic_cstring_view rhs) noexcept { return lhs.sv_ <= rhs.sv_; }
  [[nodiscard]] friend constexpr bool operator>(basic_cstring_view lhs, basic_cstring_view rhs) noexcept { return lhs.sv_ > rhs.sv_; }
  [[nodiscard]] friend constexpr bool operator>=(basic_cstring_view lhs, basic_cstring_view rhs) noexcept { return lhs.sv_ >= rhs.sv_; }
#endif

  /*******************************************************************************************************************/
  /**                                            inserters and extractors                                           **/
  /*******************************************************************************************************************/
  friend std::basic_ostream<charT, traits>& operator<<(std::basic_ostream<charT, traits>& os, basic_cstring_view csv) {
    return os << csv.sv_;
  }

 private:
  string_view_type sv_;  // exposition only
};

/*******************************************************************************************************************/
/**                                             non-member functions                                              **/
/*******************************************************************************************************************/
template <typename charT>
constexpr void swap(basic_cstring_view<charT>& lhs, basic_cstring_view<charT>& rhs) noexcept {
    lhs.swap(rhs);
}

/*******************************************************************************************************************/
/**                                                 typedef names                                                 **/
/*******************************************************************************************************************/
using cstring_view = basic_cstring_view<char>;
#if defined(__cpp_char8_t)
using u8cstring_view = basic_cstring_view<char8_t>;
#endif
using u16cstring_view = basic_cstring_view<char16_t>;
using u32cstring_view = basic_cstring_view<char32_t>;
using wcstring_view = basic_cstring_view<wchar_t>;

inline namespace literals {
inline namespace string_view_literals {

/*******************************************************************************************************************/
/**                                     suffix for basic_cstring_view literals                                    **/
/*******************************************************************************************************************/
[[nodiscard]] constexpr ember::cstring_view operator"" _csv(const char* str, const std::size_t len) noexcept {
  return { ember::cstring_view::null_terminated, str, len };
}
#if defined(__cpp_char8_t)
[[nodiscard]] constexpr ember::u8cstring_view operator"" _csv(const char8_t* str, const std::size_t len) noexcept {
  return { ember::u8cstring_view::null_terminated, str, len };
}
#endif
[[nodiscard]] constexpr ember::u16cstring_view operator"" _csv(const char16_t* str, const std::size_t len) noexcept {
  return { ember::u16cstring_view::null_terminated, str, len };
}
[[nodiscard]] constexpr ember::u32cstring_view operator"" _csv(const char32_t* str, const std::size_t len) noexcept {
  return { ember::u32cstring_view::null_terminated, str, len };
}
[[nodiscard]] constexpr ember::wcstring_view operator"" _csv(const wchar_t* str, const std::size_t len) noexcept {
  return { ember::wcstring_view::null_terminated, str, len };
}

}  // namespace string_view_literals
}  // namespace literals

}  // namespace ember

/*******************************************************************************************************************/
/**                                                  hash support                                                 **/
/*******************************************************************************************************************/
namespace std {

template <>
struct hash<ember::cstring_view> {
  [[nodiscard]] std::size_t operator()(const ember::cstring_view csv) {
    return std::hash<typename ember::cstring_view::string_view_type>{}(csv);
  }
};
#if defined(__cpp_char8_t)
template <>
struct hash<ember::u8cstring_view> {
  [[nodiscard]] std::size_t operator()(const ember::u8cstring_view csv) {
    return std::hash<typename ember::u8cstring_view::string_view_type>{}(csv);
  }
};
#endif
template <>
struct hash<ember::u16cstring_view> {
  [[nodiscard]] std::size_t operator()(const ember::u16cstring_view csv) {
    return std::hash<typename ember::u16cstring_view::string_view_type>{}(csv);
  }
};
template <>
struct hash<ember::u32cstring_view> {
  [[nodiscard]] std::size_t operator()(const ember::u32cstring_view csv) {
    return std::hash<typename ember::u32cstring_view::string_view_type>{}(csv);
  }
};
template <>
struct hash<ember::wcstring_view> {
  [[nodiscard]] std::size_t operator()(const ember::wcstring_view csv) {
    return std::hash<typename ember::wcstring_view::string_view_type>{}(csv);
  }
};

}  // namespace std

/*******************************************************************************************************************/
/**                                            ranges helper templates                                            **/
/*******************************************************************************************************************/
#if defined(__cpp_lib_ranges)
template <typename charT, typename traits>
inline constexpr bool std::ranges::enable_borrowed_range<ember::basic_cstring_view<charT, traits>> = true;
template <typename charT, typename traits>
inline constexpr bool std::ranges::enable_view<ember::basic_cstring_view<charT, traits>> = true;
#endif

/*******************************************************************************************************************/
/**                                             std::format support                                               **/
/*******************************************************************************************************************/
#if defined(__cpp_lib_format)
template <typename charT, typename traits>
struct std::formatter<ember::basic_cstring_view<charT, traits>> : std::formatter<std::basic_string_view<charT, traits>> {
    auto format(const ember::basic_cstring_view<charT, traits>& csv, std::format_context& ctx) const {
        return std::formatter<std::basic_string_view<charT, traits>>::format(static_cast<std::basic_string_view<charT, traits>>(csv), ctx);
    }
};
#endif

#endif  // CPP_UTIL_CSTRING_VIEW_HPP
