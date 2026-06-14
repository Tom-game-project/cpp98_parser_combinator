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
  std::string operator()(const std::vector<char>& v) const {
    std::string s(v.begin(), v.end());
    return s;
  }
};

// <method> <request-target> <protocol>
struct RequestLine {
  std::string method;
  std::string request_target;
  std::string protocol;

  RequestLine() {}

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
  std::string operator()(const std::vector<std::string>& v) const {
    std::string rstr;
    for (std::size_t i = 0; i < v.size(); i++) {
      rstr += v[i];
    }
    return rstr;
  }
};

struct OptionalTailToVecChar {
  std::vector<char> operator() (const std::vector<std::pair<std::vector<char>, char> >& v) const {
    std::vector<char> rvec;

    for (std::size_t i = 0; i < v.size(); i++) {
      rvec.insert(rvec.end(), v[i].first.begin(), v[i].first.end());
      rvec.push_back(v[i].second);
    }
    return rvec;
  }
};

// typedef IgnoreThenParser<ThenParser<ThenIgnoreParser<DigitM, CharP/*.*/>, DigitM>, IgnoreThenParser<HttpNameP, CharP> > HttpVersionP;
struct HttpVersionPToString {
  std::string operator()(const std::pair<std::string, std::string>& v) const {
    return "HTTP/" + v.first + "." + v.second;
  }
};

struct RequestLinePToRequestLine {
  RequestLine operator()(const std::pair<std::string, std::pair<std::string, std::string> >& v) const {
    return RequestLine(v.first, v.second.first, v.second.second);
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
  return std::isdigit(static_cast<unsigned char>(c));
}

// ```
// tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*" / "+" / "-" / "." /
//  "^" / "_" / "`" / "|" / "~" / DIGIT / ALPHA
// ```
bool is_tchar(const char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return 
    std::isdigit(uc) != 0 ||
    std::isalpha(uc) != 0 ||
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

// not space or tab
bool is_not_space(const char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return uc != ' ' && uc != '\t';
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
  typedef OrParser<StringParser<Iter>, StringParser<Iter> > CRLFP;

  typedef PredicateCharParser<Iter> PSomeCharP; // 関数で条件設定できる一文字

  //   field-content  = field-vchar
  //                  [ 1*( SP / HTAB / field-vchar ) field-vchar ]

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
  typedef Many1Parser<PSomeCharP> TokenP;                                           // token = 1*tchar

  typedef MapParser<FieldValueP, FieldValuePToString, std::string> FieldValueM;
  typedef MapParser<TokenP, VecCharToString, std::string> TokenM;

  //   field-line   = field-name ":" OWS field-value OWS
  typedef ThenParser<ThenIgnoreParser<TokenM, CharP /*:*/>, PaddedParser<FieldValueM, ManyParser<PSomeCharP> /* OWS */ > > FieldLineP;

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
  typedef MapParser<HttpVersionP, HttpVersionPToString, std::string> HttpVersionM;

  typedef MapParser<Many1Parser<PredicateCharParser<Iter> >, VecCharToString, std::string> RequestTargetM;

  // request-line = method SP request-target SP HTTP-version
  typedef ThenParser<
    TokenM,
    IgnoreThenParser<
      ThenParser<
        ThenIgnoreParser<RequestTargetM, CharP>
        , HttpVersionM>,
    CharP> > RequestLineP;
  typedef MapParser<RequestLineP, RequestLinePToRequestLine, RequestLine> RequestLineM;

  // HTTP-message = start-line CRLF *( field-line CRLF ) CRLF [ message-body ]
  typedef ThenParser<RequestLineM, IgnoreThenParser<FieldLinesP, CRLFP > > HttpMessageP;

  PSomeCharP field_vchar_p = PredicateCharParser<Iter>(is_field_vchar);
  PSomeCharP ows_p = PredicateCharParser<Iter>(is_ows);
  PSomeCharP tchar_p = PredicateCharParser<Iter>(is_tchar);
  PSomeCharP no_space_p = PredicateCharParser<Iter>(is_not_space);

  CRLFP crlf_p = or_p(str<Iter>("\r\n"), str<Iter>("\n"));

  // request-line

  HttpNameP http_name_p = str<Iter>("HTTP");
  DigitP digit_p = many1(PredicateCharParser<Iter>(is_digit));
  DigitM digit_m = map_p<std::string>(digit_p, VecCharToString());

  HttpVersionP http_version_p = ignorethen_p(
      then_p(thenignore_p(digit_m, CharP('.')), digit_m),
      then_p(http_name_p, CharP('/')));
  HttpVersionM http_version_m = map_p<std::string>(http_version_p, HttpVersionPToString());

  FieldContentP field_content_p = then_p(
      field_vchar_p,
      opt_p(
        map_p<std::vector<char> >(many(then_p(many(ows_p), field_vchar_p)),
          OptionalTailToVecChar()
        )
      )
  );

  FieldContentM field_content_m = map_p<std::string>(field_content_p, FieldContentPToString());

  FieldValueP field_value_p = many(field_content_m);
  FieldValueM field_value_m = map_p<std::string>(field_value_p, FieldValuePToString());

  TokenP token_p = many1(tchar_p); 
  TokenM token_m = map_p<std::string>(token_p, VecCharToString()); // token
                                                                         // methodとしても使える

  FieldLineP field_line_p = then_p(thenignore_p(token_m, CharP(':')), padded_p(field_value_m, many(ows_p)));
  FieldLinesP field_lines_p = 
    thenignore_p(many(thenignore_p(field_line_p, crlf_p)), crlf_p);

  RequestTargetM request_target_m = map_p<std::string>(many1(no_space_p), VecCharToString());

  RequestLineP request_line_p = then_p(
    token_m,
    ignorethen_p(
      then_p(
        thenignore_p(request_target_m, CharP(' ')),
        http_version_m),
      CharP(' '))
  );
  RequestLineM request_line_m = map_p<RequestLine>(request_line_p, RequestLinePToRequestLine());
  HttpMessageP http_message_p = then_p(request_line_m, ignorethen_p(field_lines_p, crlf_p));

  // --- test ---

  {
    std::cout << "--- test http_message_p ---" << std::endl;
    Iter it = request_header.begin();
    Iter end = request_header.end();

    ParseResult<
      Iter,
      std::pair<
        RequestLine, 
        std::vector<std::pair<std::string, std::string> > >
    > res = http_message_p.parse(it, end);

    if (res.success) {
      std::cout << "method        : " << res.value.first.method << std::endl;
      std::cout << "request_target: " << res.value.first.request_target << std::endl;
      std::cout << "protocol      : " << res.value.first.protocol << std::endl;
      for (std::size_t i = 0; i < res.value.second.size(); i++) {
        std::pair<std::string, std::string> pair = res.value.second[i];
        std::cout << "\"" << pair.first << "\"" << ": " << "\"" << pair.second << "\"" << std::endl;
      }
    } else {
      std::cout << "failed to parse" << std::endl;
    }
  }

  {
    std::cout << "--- test request_line_m ---" << std::endl;
    std::string test_case = 
      "POST /users HTTP/1.1";

    Iter it = test_case.begin();
    Iter end = test_case.end();

    ParseResult<Iter, RequestLine> res = request_line_m.parse(it, end);

    if (res.success) {
      std::cout << "method        : " << res.value.method << std::endl;
      std::cout << "request_target: " << res.value.request_target << std::endl;
      std::cout << "protocol      : " << res.value.protocol << std::endl;
    } else {
      std::cout << "failed to parse" << std::endl;
    }
  }

  {
    std::cout << "--- test http_version_m ---" << std::endl;

    std::string test_case = 
      "HTTP/1.1";

    Iter it = test_case.begin();
    Iter end = test_case.end();

    ParseResult<Iter, std::string> res = http_version_m.parse(it, end);

    if (res.success) {
      std::cout << res.value << std::endl;
    } else {
      std::cout << "failed to parse" << std::endl;
    }
  }

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
    ParseResult<Iter, std::string > res = token_m.parse(it, end);

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

  assert(is_field_vchar('H'));
  assert(is_field_vchar('h'));
  assert(is_field_vchar('.'));
}
