#include <cstddef>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

#include "parser_combinator.hpp"

typedef std::vector<std::string> Row;

template <typename T>
struct Option {
  enum st {
    Some,
    None
  } ty;
  T value;
};

struct CsvData {
  std::vector<Row> rows;
};

struct DataToRow {
  Row operator()(std::pair<std::vector<std::string>, Option<std::string> > v) const {
    Row r;
    for (std::size_t i = 0; i < v.first.size(); i++) {
      r.push_back(v.first[i]);
    }
    switch (v.second.ty) {
      case Option<std::string>::None:
        break;
      case Option<std::string>::Some:
        r.push_back(v.second.value);
        break;
    }
    return r;
  }
};

struct VecCharToString {
  std::string operator()(const std::vector<char> v) const {
    std::string s(v.begin(), v.end());
    return s;
  }
};

struct StringToOption {
  Option<std::string> operator()(const std::string s) const {
    if (s == "") {
      Option<std::string> r;
      r.ty = Option<std::string>::None;
      r.value = "";
      return r;
    } else {
      Option<std::string> r;
      r.ty = Option<std::string>::Some;
      r.value = s;
      return r;
    }
  }
};

// generate char parser chr(a-z A-Z 0-9 -_)
std::vector<CharParser<std::string::const_iterator> > generate_word_char() {
  std::vector<CharParser<std::string::const_iterator> > c_list;

  for (char i = 'a'; i <= 'z'; i++)
    c_list.push_back(CharParser<std::string::const_iterator>(i));
  for (char i = 'A'; i <= 'Z'; i++)
    c_list.push_back(CharParser<std::string::const_iterator>(i));
  for (char i = '0'; i <= '9'; i++)
    c_list.push_back(CharParser<std::string::const_iterator>(i));
  c_list.push_back(CharParser<std::string::const_iterator>('-'));
  c_list.push_back(CharParser<std::string::const_iterator>('_'));
  c_list.push_back(CharParser<std::string::const_iterator>(' '));
  return c_list;
}

int main () {
  typedef std::string::const_iterator Iter;

  typedef CharParser<Iter> CharP;
  typedef ChoiceParser<CharP> SomeCharP;
  typedef ManyParser<SomeCharP> WordP;
  typedef MapParser<WordP, VecCharToString, std::string> WordM; // 任意のString

  typedef StringParser<Iter> StringP;                           // 特定のString

  typedef WordP Padded;
  typedef PaddedParser<WordM, Padded> PaddedWordP;
  typedef ThenIgnoreParser<PaddedWordP, StringP /*`,`*/> PaddedWordCharP;

  typedef 
    OrParser<
      OrParser<
        PaddedWordCharP, // "xxxx ," -> Some("xxxx")
        PaddedWordP>,    // "xxxx"   -> Some("xxxx")
      StringP            // ""       -> None
    >
    StringOrNoneP;

  typedef MapParser<
    StringOrNoneP,
    StringToOption,
    Option<std::string> > StringOrNoneM;

  typedef 
    ThenIgnoreParser<
    ThenParser<
      ManyParser<PaddedWordCharP> ,          // 
      StringOrNoneM // 最後に`,`を許容しても良い
    >,
    Padded> ManyPaddedWordCharP;

    // ThenParser<
    //   ManyParser<PaddedWordCharP> ,          // 
    //   StringOrNoneM // 最後に`,`を許容しても良い
    // > ManyPaddedWordCharP;

  typedef MapParser<ManyPaddedWordCharP, DataToRow, Row> ManyPaddedWordCharM;

  CharParser<Iter> padded_char_set[] = {
      chr<std::string::const_iterator>(' '),
      chr<std::string::const_iterator>('\n'),
      chr<std::string::const_iterator>('\r'),
      chr<std::string::const_iterator>('\t')
  };

  std::vector<CharParser<Iter> > value_char_set = generate_word_char();
  SomeCharP value_char_p = choice(value_char_set);
  WordP value_word_p = many(value_char_p);
  WordM value_word_m = map_p<std::string>(value_word_p, VecCharToString());

  Padded padded_char_p = many(choice(padded_char_set));
  PaddedWordP padded_word_p = padded_p(value_word_m, padded_char_p);
  PaddedWordCharP padded_word_char_p = thenignore_p(padded_word_p, StringP(","));

  StringOrNoneP string_or_none_p = padded_word_char_p | padded_word_p | StringP("");
  StringOrNoneM string_or_none_m = map_p<Option<std::string> >(string_or_none_p, StringToOption());

  ManyPaddedWordCharP many_padded_word_char_p = thenignore_p(many(padded_word_char_p) & string_or_none_m, padded_char_p);
  ManyPaddedWordCharM many_padded_word_char_m = map_p<Row>(many_padded_word_char_p, DataToRow());

  // 
  std::string input = "   hello , world      , Tom   ";
  Iter it = input.begin();
  Iter end = input.end();
  ParseResult<Iter, Row> parse_result = many_padded_word_char_m.parse(it, end);

  if (parse_result.success) {
    for (std::size_t i = 0; i < parse_result.value.size(); i++) {
      std::cout << parse_result.value[i] << std::endl;
    }
  }
  return 0;
}
