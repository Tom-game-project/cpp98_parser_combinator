// https://www.rfc-editor.org/rfc/rfc9112.html#header.field.syntax
// https://www.rfc-editor.org/rfc/rfc9112.html#name-field-line-parsing

#include "parser_combinator.hpp"
#include <cassert>
#include <cctype>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

// 
// functor
//

// std::vector<char>をstd::stringにする関数
struct VecCharToString {
  std::string operator()(const std::vector<char> v) const {
    std::string s(v.begin(), v.end());
    return s;
  }
};

// <method> <request-target> <protocol>
struct RequestLine {
  std::string method;
  std::string request_target;
  std::string protocol;

  RequestLine(
    std::string method,
    std::string request_target,
    std::string protocol
  ): 
    method(method),
    request_target(request_target),
    protocol(protocol) {}
};

struct WordLineToRequestLine {
  RequestLine  operator()(const std::pair<std::string, std::pair<std::string, std::string> > v) const {
    RequestLine r(
        v.first,
        v.second.first,
        v.second.second);
    return r;
  }
};

struct FieldContentPToString {
  std::string operator()(const std::pair<char, std::vector<char> > v) const {
    std::string rstr;
    std::string tail(v.second.begin(), v.second.end());

    rstr.push_back(v.first);
    rstr += tail;
    return rstr;
  }
};

struct FieldValuePToString {
  std::string operator()(const std::vector<std::string> v) const {
    std::string rstr;
    for (std::size_t i = 0; i < v.size(); i++) {
      rstr += v[i];
    }
    return rstr;
  }
};

struct OptionalTailToVecChar {
  std::vector<char> operator() (const std::vector<std::pair<std::vector<char>, char> > v) const {
    std::vector<char> rvec;

    for (std::size_t i = 0; i < v.size(); i++) {
      rvec.insert(rvec.end(), v[i].first.begin(), v[i].first.end());
      rvec.push_back(v[i].second);
    }
    return rvec;
  }
};

// char functor

bool is_field_vchar(const char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return (uc >= 0x21 && uc <= 0x7E) || (uc >= 0x80/* obs-text */);
}

bool is_ows(const char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return (uc == ' ' || uc == '\t');
}

bool is_digit(const char c) {
  return std::isdigit(c);
}

// ```
// tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*" / "+" / "-" / "." /
//  "^" / "_" / "`" / "|" / "~" / DIGIT / ALPHA
// ```
bool is_tchar(const char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return 
    std::isdigit(c) != 0 ||
    std::isalpha(c) != 0 ||
    uc == '!' ||
    uc == '#' ||
    uc == '$' ||
    uc == '%' ||
    uc == '&' ||
    uc == '\'' ||
    uc == '*' ||
    uc == '+' ||
    uc == '-' ||
    uc == '.' ||
    uc == '^' ||
    uc == '_' ||
    uc == '`' ||
    uc == '|'||
    uc == '~';
}

//
int main () {

  std::string request_header = 
    "POST /users HTTP/1.1\n"
    "Host: example.com\n"
    "Content-Type: application/x-www-form-urlencoded\n"
    "Content-Length: 49\n"
    "\n"
    "name=FirstName+LastName&email=bsmth%40example.com\n";

  std::string request_header_fields = 
    "Host: example.com\n"
    "Content-Type: application/x-www-form-urlencoded\n"
    "Content-Length: 49  \n"
    "\n"
    "name=FirstName+LastName&email=bsmth%40example.com\n";

  std::string request_header_field = 
    "Host: example.com\n";

  typedef std::string::const_iterator Iter;
  typedef CharParser<Iter> CharP;
  // typedef ChoiceParser<CharP> SomeChar;
  typedef OrParser<StringParser<Iter>, StringParser<Iter> > CRLFP;

  // typedef 
  //
  typedef PredicateCharParser<Iter> PSomeCharP; // 関数で条件設定できる一文字

  //   field-content  = field-vchar
  //                  [ 1*( SP / HTAB / field-vchar ) field-vchar ]
  // field-content

// 概念的なC++コード
// field_vchar_p が必ず最後に来るようにブロック化する
// then_p(
//     field_vchar_p, 
//     many0(
//         then_p(many0(ows_p), field_vchar_p)
//     )
// )

  // ThenParser<Many1Parser<OrParser<PSomeCharP/*SP HTAB*/, PSomeCharP/*field-vchar*/> >, PSomeCharP/*field-vchar*/> 
  typedef 
    ThenParser<
      PSomeCharP,   // field-vchar
      OptParser<
        MapParser<
          ManyParser<
            ThenParser<
              ManyParser<
                PSomeCharP/*ows*/
              >, 
              PSomeCharP/* field vchar*/
            > 
          >,
          OptionalTailToVecChar,
          std::vector<char>
        > 
      >
    >
    FieldContentP;

  typedef MapParser<FieldContentP, FieldContentPToString, std::string> FieldContentM;

  typedef ManyParser<FieldContentM> FieldValueP;                                        // field-value
  typedef Many1Parser<PSomeCharP> FieldNameP;                                           // token = 1*tchar

  typedef MapParser<FieldValueP, FieldValuePToString, std::string> FieldValueM;
  typedef MapParser<FieldNameP, VecCharToString, std::string> FieldNameM;

  //   field-line   = field-name ":" OWS field-value OWS
  typedef ThenParser<ThenIgnoreParser<FieldNameM, CharP /*:*/>, PaddedParser<FieldValueM, ManyParser<PSomeCharP> /* OWS */ > > FieldLineP;

  typedef ThenIgnoreParser<ManyParser<ThenIgnoreParser<FieldLineP, CRLFP> >, CRLFP>  FieldLinesP;

  // HTTP-message = start-line CRLF *( field-line CRLF ) CRLF [ message-body ]
  // start-line = request-line / status-line
  //
  // server -> request-line
  // request-line = method SP request-target SP HTTP-version
  //
  // request-target = origin-form / absolute-form / authority-form / asterisk-form
  // origin-form = absolute-path [ "?" query ]
  // authority-form = uri-host ":" port
  // asterisk-form = "*"
  // uri-host = <host, see [URI], Section 3.2.2>
  //
  // HTTP-version  = HTTP-name "/" DIGIT "." DIGIT
  // HTTP-name     = %s"HTTP"
  typedef StringParser<Iter> HttpNameP;

  typedef Many1Parser<PredicateCharParser<Iter> > DigitP;
  typedef MapParser<DigitP, VecCharToString, std::string> DigitM;

  typedef IgnoreThenParser<ThenParser<ThenIgnoreParser<DigitM, CharP/*.*/>, DigitM>, ThenParser<HttpNameP, CharP> > HttpVersionP;

  PSomeCharP field_vchar_p = PredicateCharParser<Iter>(is_field_vchar);
  PSomeCharP ows_p = PredicateCharParser<Iter>(is_ows);
  PSomeCharP tchar_p = PredicateCharParser<Iter>(is_tchar);

  CRLFP crlf_p = or_p(str<Iter>("\r\n"), str<Iter>("\n"));

  // request-line

  HttpNameP http_name_p = str<Iter>("HTTP");
  DigitP digit_p = many1(PredicateCharParser<Iter>(is_digit));
  DigitM digit_m = map_p<std::string>(digit_p, VecCharToString());

  HttpVersionP http_version_p = ignorethen_p(
      then_p(thenignore_p(digit_m, CharP('.')), digit_m),
      then_p(http_name_p, CharP('/')));

  OptParser<
        MapParser<
          ManyParser<
            ThenParser<
              ManyParser<
                PSomeCharP/*ows*/
              >, 
              PSomeCharP/* field vchar*/
            > 
          >,
          OptionalTailToVecChar,
          std::vector<char>
        > 
    > tmp_p = 
      opt_p(
        map_p<std::vector<char> >(many(then_p(many(ows_p), field_vchar_p)),
          OptionalTailToVecChar()
        )
      );
  
  FieldContentP field_content_p = then_p(
      field_vchar_p,
      tmp_p
  );

  FieldContentM field_content_m = map_p<std::string>(field_content_p, FieldContentPToString());

  FieldValueP field_value_p = many(field_content_m);
  FieldValueM field_value_m = map_p<std::string>(field_value_p, FieldValuePToString());

  FieldNameP field_name_p = many1(tchar_p); 
  FieldNameM field_name_m = map_p<std::string>(field_name_p, VecCharToString()); // token
                                                                                       // methodとしても使える

  FieldLineP field_line_p = then_p(thenignore_p(field_name_m, CharP(':')), padded_p(field_value_m, many(ows_p)));
  FieldLinesP field_lines_p = 
    thenignore_p(many(thenignore_p(field_line_p, crlf_p)), crlf_p);

  // --- test ---

  {
    std::cout << "--- test field_lines_p ---" << std::endl;
    Iter it = request_header_fields.begin();
    Iter end = request_header_fields.end();
    ParseResult<Iter, std::vector<std::pair<std::string, std::string> > > res = field_lines_p.parse(it, end);

    if (res.success) {
      for (std::size_t i = 0; i < res.value.size(); i++) {
        std::pair<std::string, std::string> pair = res.value[i];
        std::cout << "\"" << pair.first << "\"" << ": " << "\"" << pair.second << "\"" << std::endl;
      }
    } else {
      std::cout << "failed to parse" << std::endl;
    }
  }

  {
    std::cout << "--- test: field_line_p ---" << std::endl;
    Iter it = request_header_field.begin();
    Iter end = request_header_field.end();
    ParseResult<Iter, std::pair<std::string, std::string> > res = field_line_p.parse(it, end);

    if (res.success) {
      std::cout << "\"" << res.value.first << "\"" << ": " << "\"" << res.value.second << "\"" << std::endl;
    } else {
      std::cout << "failed to parse" << std::endl;
    }
  }

  {
    std::cout << "--- test: field_name_m ---" << std::endl;
    std::string field_name_string = "Hello";
    Iter it = field_name_string.begin();
    Iter end = field_name_string.end();
    ParseResult<Iter, std::string > res = field_name_m.parse(it, end);

    if (res.success) {
      std::cout << res.value << std::endl;
    } else {
      std::cout << "failed to parse" << std::endl;
    }
  }

  {
    std::cout << "--- test: field_value_m ---" << std::endl;
    std::string field_value_string = "example.com  aaaaa   ";
    Iter it = field_value_string.begin();
    Iter end = field_value_string.end();
    ParseResult<Iter, std::string > res = field_value_m.parse(it, end);

    if (res.success) {
      std::cout << "\"" << res.value << "\"" << std::endl;
    } else {
      std::cout << "failed to parse" << std::endl;
    }
  }

  {
    std::cout << "--- test: field_content_p ---" << std::endl;
    std::string field_value_string = "exa";
    Iter it = field_value_string.begin();
    Iter end = field_value_string.end();
    ParseResult<Iter, std::pair<char, std::vector<char> > > res = field_content_p.parse(it, end);

    if (res.success) {
      std::cout << res.value.first << std::endl;
      std::string s(res.value.second.begin(), res.value.second.end());
      std::cout << "\"" << s << "\"" << std::endl;
    } else {
      std::cout << "failed to parse" << std::endl;
    }
  }

  {
    std::cout << "--- test: tmp_p ---" << std::endl;
    std::string field_value_string = "hello";
    Iter it = field_value_string.begin();
    Iter end = field_value_string.end();
    ParseResult<Iter, std::vector<char> > res = tmp_p.parse(it, end);

    if (res.success) {
      std::string s(res.value.begin(), res.value.end());
      std::cout << s << std::endl;
    } else {
      std::cout << "failed to parse" << std::endl;
    }

  }
  assert(is_field_vchar('H'));
  assert(is_field_vchar('h'));
  assert(is_field_vchar('.'));
}
