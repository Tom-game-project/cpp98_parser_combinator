#include "parser_combinator.hpp"
#include <cstddef>
#include <cstdlib>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include <iostream>

bool is_digit(const char c) {
  return std::isdigit(static_cast<unsigned char>(c));
}

// ```
// ALPHA = %x41-5A / %x61-7A (A-Z / a-z)
// ```
bool is_alpha(const char c) {
  return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

// ```
// HEXDIG = DIGIT / "A" / "B" / "C" / "D" / "E" / "F" / "a" / "b" / "c" / "d" / "e" / "f"
// パーセントエンコーディング（pct-encoded）やchunkedサイズのパースに使用します。
// ```
bool is_hexdig(const char c) {
  return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

// ```
// unreserved = ALPHA / DIGIT / "-" / "." / "_" / "~"
// URIの非予約文字です。
// ```
bool is_unreserved(const char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return is_alpha(c) || 
         is_digit(c) || 
         uc == '-' || 
         uc == '.' || 
         uc == '_' || 
         uc == '~';
}

// ```
// sub-delims = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
// URIのサブ区切り文字です。
// ```
bool is_sub_delims(const char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return uc == '!' || uc == '$' || uc == '&' || uc == '\'' ||
         uc == '(' || uc == ')' || uc == '*' || uc == '+' ||
         uc == ',' || uc == ';' || uc == '=';
}

// ```
// gen-delims = ":" / "/" / "?" / "#" / "[" / "]" / "@"
// URIの汎用区切り文字です。
// ```
bool is_gen_delims(const char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return uc == ':' || uc == '/' || uc == '?' || uc == '#' ||
         uc == '[' || uc == ']' || uc == '@';
}

// ```
// pchar (base) = unreserved / sub-delims / ":" / "@"
// パスセグメントやクエリを構成する基本文字です。
// ※注意: 仕様上の pchar には "pct-encoded" も含まれますが、
// "% HEXDIG HEXDIG" は3文字のシーケンスであるため、この単一文字判定からは除外しています。
// ```
bool is_pchar_base(const char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return is_unreserved(c) || 
         is_sub_delims(c) || 
         uc == ':' || 
         uc == '@';
}

// ```
// VCHAR = %x21-7E
// 表示可能なUS-ASCII文字。ヘッダーのフィールド値（field-value）などの検証に使います。
// ```
bool is_vchar(const char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return uc >= 0x21 && uc <= 0x7E;
}

// ```
// obs-text = %x80-FF
// 過去の互換性のための非ASCII文字。VCHARと共にヘッダー値として許容される場合があります。
// ```
bool is_obs_text(const char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return uc >= 0x80 /* && uc <= 0xFF */ ; // unsigned charなので 0xFF 以下は常に真ですが明示的に記述
}

//  scheme        = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
bool is_scheme_char(const char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  return is_alpha(uc) || is_digit(uc) || uc == '+' || uc == '-' || uc == '.';
}

// --- functor ---

struct CharToString {
  std::string operator()(const char c) const {
    std::string rstr(1, c);
    return rstr;
  }
};

// std::vector<char>をstd::stringにする関数
struct VecCharToString {
  std::string operator()(const std::vector<char>& v) const {
    std::string s(v.begin(), v.end());
    return s;
  }
};

struct PSomeCharPToString {
  std::string operator()(const char c) const {
    std::string rstr(1, c);
    return rstr;
  }
};

struct PctEncodedPToString {
  std::string operator()(const std::pair<char, std::pair<char, char> >& v) const {
    char arr[3] = {v.first, v.second.first, v.second.second};
    std::string s(arr, 3);
    return s;
  }
};

struct CheckRangeAndToString {
  std::pair<bool, std::string> operator()(const std::vector<char>& v) const {
    std::string rstr(v.begin(), v.end());
    long n = std::strtol(rstr.c_str(), NULL, 10);
    if (0 < n && n < 256) {
      return std::make_pair(true, rstr);
    }
    return std::make_pair(false, std::string());
  }
};

// MapParser<ThenParser<SegmentNzM, ManyParser<ThenParser<CharP /* `/` */, SegmentM> > >, PathAbsoluteHelper, std::string>
// ThenParser<SegmentNzM, ManyParser<ThenParser<CharP /* `/` */, SegmentM> > >,
struct PathAbsoluteHelper {
  std::string operator()(const std::pair<std::string, std::vector<std::pair<char, std::string> > >& v) const {
    std::string rstr = v.first;
    for (std::size_t i = 0; i < v.second.size(); i++) {
      rstr.push_back(v.second[i].first);
      rstr += v.second[i].second;
    }
    return rstr;
  }
};

// // path-noscheme = segment-nz-nc *( "/" segment )
struct VecCharStringToString {
  std::string operator()(const std::vector<std::pair<char, std::string> >& v) const {
    std::string rstr;
    for (std::size_t i = 0; i < v.size(); i++) {
      rstr.push_back(v[i].first);
      rstr += v[i].second;
    }
    return rstr;
  }
};

struct IPv4addressPToString {
  std::string operator()(const std::pair<std::string, std::pair<std::string, std::pair<std::string, std::string> > >& v) const {
    return v.first + "." + v.second.first + "." + v.second.second.first + "." + v.second.second.second;
  }
};

// typedef ThenParser<HostP, OptParser<MapParser<ThenParser<CharP /* `:` */, PortM>, AuthorityPHelper, std::string> > > AuthorityP;
struct CharStringToString {
  std::string operator()(const std::pair<char, std::string>& v) const {
    std::string rstr(1, v.first);
    rstr += v.second;
    return rstr;
  }
};

struct StringStringToString {
  std::string operator()(const std::pair<std::string, std::string>& v) const {
    std::string rstr = v.first;
    rstr += v.second;
    return rstr;
  }
};

struct AuthorityLineHelper {
  std::string operator()(const std::pair<std::string, std::pair<std::string, std::string> >& v) const {
    return v.first + v.second.first + v.second.second;
  }
};

struct CharVecCharToString {
  std::string operator()(const std::pair<char, std::vector<char> >& v) const {
    std::string head(1, v.first);
    std::string tail(v.second.begin(), v.second.end());
    return head + tail;
  }
};

struct VecStringToString {
  std::string operator()(const std::vector<std::string>& v) const {
    std::string rstr;
    for (std::size_t i = 0; i < v.size(); i++) {
      rstr += v[i];
    }
    return rstr;
  }
};

// typedef ManyParser<ThenParser<StrP /* `/` */, SegmentM> > PathAbemptyP;
struct VecStrStrToString {
  std::string operator()(const std::vector<std::pair<std::string, std::string> >& v) const {
    std::string rstr;
    for (std::size_t i = 0; i < v.size(); i++) {
      rstr += v[i].first;
      rstr += v[i].second;
    }
    return rstr;
  }
};

int main() {

  typedef std::string::const_iterator Iter;
  typedef CharParser<Iter> CharP;
  typedef StringParser<Iter> StrP;
  typedef PredicateCharParser<Iter> PSomeCharP; // 関数で条件設定できる一文字

  // 文字を文字列として扱うコンビネーター
  typedef MapParser<PSomeCharP, CharToString, std::string> CharToStringP;


  typedef ManyParser<PSomeCharP> PortP;
  typedef MapParser<PortP, VecCharToString, std::string> PortM;

  //  scheme        = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
  typedef ThenParser<
    PSomeCharP /* `ALPHA` */, 
    ManyParser<PSomeCharP /* `*( ALPHA / DIGIT / "+" / "-" / "." )`*/ > > SchemeP; // TODO OrParserを使ってもいいかもしれない

  typedef MapParser<SchemeP, CharVecCharToString, std::string> SchemeM;

  // pct-encoded = "%" HEXDIG HEXDIG
  typedef ThenParser<
    CharP /* `%` */,
    ThenParser<
      PSomeCharP /* `HEXDIG` */, 
      PSomeCharP /* `HEXDIG` */> > PctEncodedP;

  typedef MapParser<PctEncodedP, PctEncodedPToString, std::string> PctEncodedM;

  // reg-name    = *( unreserved / pct-encoded / sub-delims )

  typedef 
    ManyParser<
      OrParser<CharToStringP /* unreserved */, 
        OrParser<PctEncodedM /* pct-encoded */, CharToStringP /* sub-delims */> > > RegNameP;
  typedef MapParser<RegNameP, VecStringToString, std::string> RegNameM;

  // typedef MapParser<PSomeCharP, PSomeCharPToString, std::string> PSomeStringM;
  // // userinfo      = *( unreserved / pct-encoded / sub-delims / ":" )
  // typedef ManyParser<
  //   OrParser<
  //     PSomeStringM /* `unreserved` */, 
  //     OrParser<
  //       PctEncodedM /*`pct-encoded`*/, 
  //       OrParser<
  //         PSomeStringM /* sub-delim */,
  //         StrP /* `:` */> > > > UserInfoP;

  // h16         = 1*4HEXDIG
  //             ; 16 bits of address represented in hexadecimal
  typedef RangeParser<PSomeCharP, 1, 4> H16P; // 最小 1 回、最大 4 回の繰り返し
  typedef MapParser<H16P, VecCharToString, std::string> H16M;

  // IP-literal = "[" ( IPv6address / IPvFuture ) "]"
  // 
  // IPvFuture     = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )
  // 
  // IPv6address   =                            6( h16 ":" ) ls32
  //               /                       "::" 5( h16 ":" ) ls32
  //               / [               h16 ] "::" 4( h16 ":" ) ls32
  //               / [ *1( h16 ":" ) h16 ] "::" 3( h16 ":" ) ls32
  //               / [ *2( h16 ":" ) h16 ] "::" 2( h16 ":" ) ls32
  //               / [ *3( h16 ":" ) h16 ] "::"    h16 ":"   ls32
  //               / [ *4( h16 ":" ) h16 ] "::"              ls32
  //               / [ *5( h16 ":" ) h16 ] "::"              h16
  //               / [ *6( h16 ":" ) h16 ] "::"
  //      
  // 
  // h16           = 1*4HEXDIG
  // ls32          = ( h16 ":" h16 ) / IPv4address

  // IPvFuture     = "v" 1*HEXDIG "." 1*( unreserved / sub-delims / ":" )
  // typedef ThenParser<CharP /* `[` */, ThenParser<OrParser<typename Parser1, typename Parser2>, CharP /* `]` */> > IPLiteralP;
  typedef ThenParser<CharP /*v*/, ThenParser<PSomeCharP /* HEXDIG */, ThenParser<CharP /* `.` */, OrParser<PSomeCharP /* unreserved */, OrParser<PSomeCharP /* sub-delims */, CharP /* `:`*/> > > > > IPvFutureP;
  

  // dec-octet   = DIGIT                 ; 0-9
  //             / %x31-39 DIGIT         ; 10-99
  //             / "1" 2DIGIT            ; 100-199
  //             / "2" %x30-34 DIGIT     ; 200-249
  //             / "25" %x30-35          ; 250-255
  typedef TryMapParser<RangeParser<PSomeCharP, 1, 4>, CheckRangeAndToString, std::string> DecOctetTM; // try map

  // IPv4address = dec-octet "." dec-octet "." dec-octet "." dec-octet
  typedef 
    ThenParser<
      ThenIgnoreParser<DecOctetTM, CharP>,
      ThenParser<
        ThenIgnoreParser<DecOctetTM, CharP>,
        ThenParser<
          ThenIgnoreParser<DecOctetTM, CharP>, DecOctetTM> > > IPv4addressP;

  typedef MapParser<IPv4addressP, IPv4addressPToString, std::string> IPv4addressM;

  typedef OrParser<IPv4addressM, RegNameM> HostP; // TODO IP-literal 未対応
                                                  // return std::string

  // segment       = *pchar
  typedef ManyParser<PSomeCharP> SegmentP;
  typedef MapParser<SegmentP, VecCharToString, std::string> SegmentM;

  // segment-nz    = 1*pchar
  typedef Many1Parser<PSomeCharP> SegmentNzP;
  typedef MapParser<SegmentNzP, VecCharToString, std::string> SegmentNzM;

  // segment-nz-nc = 1*( unreserved / pct-encoded / sub-delims / "@" )
  //               ; non-zero-length segment without any colon ":"
  typedef Many1Parser<OrParser<CharToStringP, OrParser<PctEncodedM, OrParser<CharToStringP, StrP> > > > SegmentNzNcP; // return string

  // userinfoは処理しない
  // authority     = [ userinfo "@" ] host [ ":" port ]
  typedef ThenParser<HostP, OptParser<MapParser<ThenParser<CharP /* `:` */, PortM>, CharStringToString, std::string> > > AuthorityP;
  typedef MapParser<AuthorityP, StringStringToString, std::string> AuthorityM;
  // path-abempty  = *( "/" segment )
  typedef ManyParser<ThenParser<StrP /* `/` */, SegmentM> > PathAbemptyP;
  typedef MapParser<PathAbemptyP, VecStrStrToString, std::string> PathAbemptyM;

  typedef MapParser<ThenParser<StrP /* `//` */, ThenParser<AuthorityM, PathAbemptyM> >, AuthorityLineHelper, std::string> AuthorityLineM;

  // path-absolute = "/" [ segment-nz *( "/" segment ) ]
  //
  typedef ThenParser<
    CharP /* `/` */, 
    OptParser<
      MapParser<
        ThenParser<
          SegmentNzM,
          ManyParser<
            ThenParser<
              CharP /* `/` */, SegmentM> > >, PathAbsoluteHelper, std::string> > > PathAbsoluteP;
  typedef MapParser<PathAbsoluteP, CharStringToString, std::string> PathAbsoluteM;
  // helper type
  // *( "/" segment )
  // -> std::string
  typedef MapParser<
        ManyParser< ThenParser<CharP, SegmentM> >, 
        VecCharStringToString,
        std::string
    > CharSegmentBundleM;
  // path-noscheme = segment-nz-nc *( "/" segment )
  typedef 
    ThenParser<
      SegmentNzNcP, // string
      CharSegmentBundleM> PathNoschemeP;
  typedef MapParser<PathNoschemeP, StringStringToString, std::string> PathNoschemeM;
  // path-rootless = segment-nz *( "/" segment )
  typedef ThenParser<SegmentNzM, CharSegmentBundleM> PathRootlessP;
  typedef MapParser<PathRootlessP, StringStringToString, std::string> PathRootlessM;

  // path-empty

  //  hier-part     = "//" authority path-abempty
  //                / path-absolute
  //                / path-rootless
  //                / path-empty
  //
  // "//" authority path-abemptyのところをauthority lineとする

  typedef OrParser<
    AuthorityLineM,
    OrParser<
      PathAbsoluteM,
      OrParser<PathRootlessM, StrP> 
    > > HierPartP;

  // URI           = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
  // query = *( pchar / "/" / "?" )
  typedef 
    ThenParser<CharP /* `?` */, ManyParser<
      OrParser<PSomeCharP /* pchar */, OrParser<CharP /* `/` */ , CharP /* `?` */> >
    > > QueryP;
  typedef MapParser<QueryP, CharVecCharToString, std::string> QueryM;

  // URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
  typedef ThenParser<
    SchemeM, 
    IgnoreThenParser<
      ThenParser<
        HierPartP,
        OptParser<QueryM> >, 
      CharP
    > > URIP;

  // --- impl ---

  PSomeCharP digit_p = pred_p<Iter>(is_digit);
  PSomeCharP hexdig_p = pred_p<Iter>(is_hexdig);
  PSomeCharP pchar_p = pred_p<Iter>(is_pchar_base);
  PSomeCharP unreserved_p = pred_p<Iter>(is_unreserved);
  PSomeCharP sub_delim_p = pred_p<Iter>(is_sub_delims);
  PSomeCharP scheme_char_p = pred_p<Iter>(is_scheme_char);
  PSomeCharP alpha_p = pred_p<Iter>(is_alpha);

  SegmentP segment_p = many(pchar_p);
  SegmentM segment_m = map_p<std::string>(segment_p, VecCharToString());

  SegmentNzP segment_nz_p = many1(pchar_p);
  SegmentNzM segment_nz_m = map_p<std::string>(segment_nz_p, VecCharToString());

  PctEncodedP pct_encoded_p = then_p(CharP('%'), then_p(hexdig_p, hexdig_p));
  PctEncodedM pct_encoded_m = map_p<std::string>(pct_encoded_p, PctEncodedPToString());

  CharToStringP unreserved_m = map_p<std::string>(unreserved_p, CharToString());
  CharToStringP sub_delim_m = map_p<std::string>(sub_delim_p, CharToString());

  PortP port_p = many(pred_p<Iter>(is_digit));
  PortM port_m = map_p<std::string>(port_p, VecCharToString());

  SegmentNzNcP segment_nz_nc_p = many1(
      or_p(
      unreserved_m, 
      or_p(pct_encoded_m,
        or_p(
          sub_delim_m,
          StrP("@"))))
    );

  DecOctetTM dec_octet_tm = trymap_p<std::string>(
    range_p<1, 4>(digit_p), 
    CheckRangeAndToString()
  );

  IPv4addressP ipv4address_p = 
    then_p(
      thenignore_p(dec_octet_tm, CharP('.')),
        then_p(
          thenignore_p(dec_octet_tm, CharP('.')),
            then_p(
              thenignore_p(dec_octet_tm, CharP('.')), 
              dec_octet_tm
        )
      )
    );
  IPv4addressM ipv4address_m = map_p<std::string>(ipv4address_p, IPv4addressPToString());

  // helper
  CharSegmentBundleM char_segment_bundle_m = map_p<std::string>(
      many(then_p(CharP('/'), segment_m)), VecCharStringToString());

  PathAbemptyP path_abempty_p = many(then_p(StrP("/"), segment_m));
  PathAbemptyM path_abempty_m = map_p<std::string>(path_abempty_p, VecStrStrToString());

  PathAbsoluteP path_absolute_p = then_p(
      CharP('/'), 
      opt_p(
        map_p<std::string>(
          then_p(segment_nz_m, many(then_p(CharP('/'), segment_m))),
          PathAbsoluteHelper()
      )
    )
  );

  PathNoschemeP path_noscheme_p = then_p(
    segment_nz_nc_p,
    char_segment_bundle_m
  );

  PathRootlessP path_rootless_p = then_p(segment_nz_m, char_segment_bundle_m);

  RegNameP reg_name_p = many(or_p(unreserved_m, or_p(pct_encoded_m, sub_delim_m)));
  RegNameM reg_name_m = map_p<std::string>(reg_name_p, VecStringToString());
  HostP host_p = or_p(ipv4address_m, reg_name_m);
  AuthorityP authority_p = then_p(
    host_p,
    opt_p(
      map_p<std::string>(then_p(CharP(':'), port_m), CharStringToString())
    )
  );
  AuthorityM authority_m = map_p<std::string>(authority_p, StringStringToString());

  AuthorityLineM authority_line_m = map_p<std::string>(
      then_p(
        StrP("//"), 
        then_p(authority_m, path_abempty_m)),
      AuthorityLineHelper());
  PathAbsoluteM path_absolute_m = map_p<std::string>(path_absolute_p, CharStringToString());
  PathNoschemeM path_noscheme_m = map_p<std::string>(path_noscheme_p, StringStringToString());
  PathRootlessM path_rootless_m = map_p<std::string>(path_rootless_p, StringStringToString());

  HierPartP hier_part_p = or_p(authority_line_m, or_p(path_absolute_m, or_p(path_rootless_m, StrP(""))));

  SchemeP scheme_p = then_p(alpha_p, many(scheme_char_p));
  SchemeM scheme_m = map_p<std::string>(scheme_p, CharVecCharToString());

  QueryP query_p = then_p(CharP('?'), many(or_p(pchar_p, or_p(CharP('/'), CharP('?')))));
  QueryM query_m = map_p<std::string>(query_p, CharVecCharToString());

  // URI           = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
  // typedef ThenParser<SchemeM, ThenParser<CharP, ThenParser<HierPartP, OptParser<ThenParser<CharP, QueryP> > > > > URIP;
  URIP uri_p = 
    then_p(
      scheme_m,
      ignorethen_p(
        then_p(
          hier_part_p,
          opt_p(
              query_m
            )
          ),
          CharP(':')
        )
      );

  // --- test ---
  {
    std::string test_cases[] = {
      "http://a/b/c/d;p?q",
      "http:",
      "scheme:?",
      "api://localhost/v1/users?url=http://example.com/path?q=1",
      "ftp://!$&'()*+,;=%20:0/",
      "mailto:test.user@example.org?subject=hello&body=test",
      "http://:/path?",
    };

    std::size_t array_length = sizeof(test_cases) / sizeof(test_cases[0]);
    for (std::size_t i = 0; i < array_length; i++) {

      std::cout << "--- test " << i << "---" << std::endl;
      std::cout << "input: \"" << test_cases[i] << "\"" << std::endl;
      std::string::const_iterator it = test_cases[i].begin();
      std::string::const_iterator end = test_cases[i].end();

      ParseResult<Iter, std::pair<std::string, std::pair<std::string, std::string> > > res = uri_p.parse(it, end);

      if (res.success) {
        // res.value;

        std::cout << "scheme      : " << res.value.first << std::endl;
        std::cout << "hier_part_p : " << res.value.second.first << std::endl;
        std::cout << "query       : " << res.value.second.second << std::endl;
      } else {
        std::cout << "failed to parse" << std::endl;
      }
    }
  }
}

