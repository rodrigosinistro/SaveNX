#include "remote/Form.hpp"
#include "remote/Encoding.hpp"

#include <algorithm>
#include <cstring>
#include <span>

//                      ---- Public functions ----

remote::Form &remote::Form::append_parameter(std::string_view param, std::string_view value) noexcept
{
    const std::string fragment = std::string(m_offset ? "&" : "") + remote::percent_encode(param) + "=" + remote::percent_encode(value);
    if (m_offset + fragment.size() >= SIZE_FORM_BUFFER) { return *this; }
    std::copy(fragment.begin(), fragment.end(), m_formBuffer + m_offset);
    m_offset += fragment.size();
    m_formBuffer[m_offset] = 0;

    return *this;
}

const char *remote::Form::get() const noexcept { return m_formBuffer; }

size_t remote::Form::length() const noexcept { return m_offset; }
