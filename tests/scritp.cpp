#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <bitcoin/bitcoin.hpp>
using namespace bc;

bool is_number(const std::string& token)
{
    if (boost::all(token, boost::is_digit()))
        return true;
    // Now check for negative numbers
    if (!boost::starts_with(token, "-"))
        return false;
    std::string numeric_part(token.begin() + 1, token.end());
    return boost::all(numeric_part, boost::is_digit());
}

bool is_hex_data(const std::string& token)
{
    if (!boost::starts_with(token, "0x"))
        return false;
    std::string hex_part(token.begin() + 2, token.end());
    return boost::all(hex_part, [](char c) { return std::isxdigit(c); });
}

bool is_quoted_string(const std::string& token)
{
    if (token.size() < 2)
        return false;
    return boost::starts_with(token, "'") && boost::ends_with(token, "'");
}

opcode token_to_opcode(const std::string& token)
{
    std::string lower_token = token;
    boost::algorithm::to_lower(lower_token);
    return string_to_opcode(lower_token);
}

bool is_opcode(const std::string& token)
{
    return token_to_opcode(token) != opcode::bad_operation;
}

bool is_opx(int64_t value)
{
    return value == -1 || (1 <= value && value <= 16);
}

void push_literal(script& result_script, int64_t value)
{
    BITCOIN_ASSERT(is_opx(value));
    // TODO Add this opcode
    BITCOIN_ASSERT(value != -1);
    switch (value)
    {
#define PUSH_X(n) \
        case n: \
            result_script.push_operation({opcode::op_##n, data_chunk()}); \
            return;

        PUSH_X(1);
        PUSH_X(2);
        PUSH_X(3);
        PUSH_X(4);
        PUSH_X(5);
        PUSH_X(6);
        PUSH_X(7);
        PUSH_X(8);
        PUSH_X(9);
        PUSH_X(10);
        PUSH_X(11);
        PUSH_X(12);
        PUSH_X(13);
        PUSH_X(14);
        PUSH_X(15);
        PUSH_X(16);
    }
}

void push_data(script& result_script, const data_chunk& data)
{
    operation op;
    // pushdata1 = 76
    if (data.empty())
        op.code = opcode::zero;
    else if (data.size() < 76)
        op.code = opcode::special;
    else if (data.size() <= 0xff)
        op.code = opcode::pushdata1;
    else if (data.size() <= 0xffff)
        op.code = opcode::pushdata2;
    else
    {
        BITCOIN_ASSERT(data.size() <= 0xffffffff);
        op.code = opcode::pushdata4;
    }
    op.data = data;
    result_script.push_operation(op);
}

bool parse_token(script& result_script, const std::string& token)
{
    static data_chunk hex_raw;
    if (token == "ENDING")
    {
        // Hack...
    }
    else if (is_number(token))
    {
        int64_t value = boost::lexical_cast<int64_t>(token);
        if (is_opx(value))
            push_literal(result_script, value);
        else
        {
            big_number bignum;
            bignum.set_int64(value);
            push_data(result_script, bignum.data());
        }
    }
    else if (is_hex_data(token))
    {
        std::string hex_part(token.begin() + 2, token.end());
        data_chunk raw_data = bytes_from_pretty(hex_part);
        extend_data(hex_raw, raw_data);
        // Avoid processing hex_raw this time
        return true;
    }
    else if (is_quoted_string(token))
    {
        data_chunk inner_value(token.begin() + 1, token.end() - 1);
        push_data(result_script, inner_value);
    }
    else if (is_opcode(token))
    {
        opcode tokenized_opcode = token_to_opcode(token);
        result_script.push_operation({tokenized_opcode, data_chunk()});
    }
    else
    {
        log_error() << "Token parsing failed with: " << token;
        return false;
    }
    if (!hex_raw.empty())
    {
        result_script.join(parse_script(hex_raw));
        hex_raw.resize(0);
    }
    return true;
}

bool parse(script& result_script, const std::string& format)
{
    if (format.empty())
        return true;
    std::vector<std::string> tokens;
    boost::split(tokens, format, boost::is_any_of(" "));
    for (const auto& token: tokens)
        if (!parse_token(result_script, token))
            return false;
    parse_token(result_script, "ENDING");
    return true;
}

int main(int argc, char** argv)
{
    if (argc != 4)
        return -1;

    std::string input_string = argv[1];
    std::string output_string = argv[2];
    std::string description = argv[3];

    script input_script;
    if (!parse(input_script, input_string))
    {
        log_error() << "Error parsing input: " << input_string;
        return -1;
    }

    script output_script;
    if (!parse(output_script, output_string))
    {
        log_error() << "Error parsing output: " << output_string;
        return -1;
    }

    log_debug() << input_string << " -> " << input_script;
    log_debug() << output_string << " -> " << output_script;

    message::transaction tx;
    if (!output_script.run(input_script, tx, 0))
    {
        log_error() << "Error running scripts";
        return 1;
    }

    return 0;
}

