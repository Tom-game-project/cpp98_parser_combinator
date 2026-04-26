#ifndef PARSER_COMBINATOR_HPP 
#define PARSER_COMBINATOR_HPP

#include <cstddef>
#include <string>

#include <utility>
#include <vector>

/*
 * Parserは以下を実装している必要がある
 * 
 * - インスタンス化
 * - ParseResult<Iterator, char> parse(Iterator it, Iterator end);
 */
template <typename Iterator, typename ValueType>
struct ParseResult {
  bool success;
  ValueType value;
  Iterator next;

  ParseResult(bool s, ValueType v, Iterator n): success(s), value(v), next(n) {}
};

template <typename Iterator>
struct EofParser {
  typedef bool value_type; // 返す値に意味はない

  EofParser() {}

  ParseResult<Iterator, value_type> parse(Iterator it, Iterator end) const {
    if (it == end) {
      return ParseResult<Iterator, value_type>(true, true, it);
    }
    return ParseResult<Iterator, value_type>(false, false, it);
  }
};


template <typename Iterator>
struct CharParser {
  typedef char value_type;
  char target;
  CharParser(char c) : target(c) {}

  ParseResult<Iterator, char> parse(Iterator it, Iterator end) const {
    if (it != end /*終端に到達しておらず*/ && *it == this->target /*target文字列にマッチしている*/) {
      return ParseResult<Iterator, value_type>(true, *it, it + 1);
    }
    return ParseResult<Iterator, value_type>(false, '\0', it);
  }
};

template <typename Iterator>
struct StringParser {
  typedef std::string value_type;
  std::string target;
  
  StringParser(std::string s) : target(s) {}

  ParseResult<Iterator, std::string> parse(Iterator it, Iterator end) const {
    Iterator current = it;

    for (std::size_t i = 0; i < this->target.length(); ++i) {
      // std::cout << "今の文字列　" << *current << std::endl;
      if (current == end || *current != target[i]) {
        // 失敗した場合は、一切進めていない元の 'it' を返す（バックトラックのため）
        return ParseResult<Iterator, value_type>(false, "", it);
      }
      ++current;
    }
    return ParseResult<Iterator, value_type>(true, this->target, current);
  }
};

// ヘルパー関数（autoがないC++98では、型推論させるためにヘルパー関数が必須）

template <typename Iterator>
EofParser<Iterator> eof_p() {
  return EofParser<Iterator>();
}

template <typename Iterator>
CharParser<Iterator> chr(char c) {
  return CharParser<Iterator>(c);
}

template <typename Iterator>
StringParser<Iterator> str(std::string s) {
  return StringParser<Iterator>(s);
}

template <typename TargetParser, typename PadParser>
struct PaddedParser {
  typedef typename TargetParser::value_type value_type;
  TargetParser target_parser;
  PadParser pad_parser;

  PaddedParser(const TargetParser& target_parser, const PadParser& pad_parser): target_parser(target_parser), pad_parser(pad_parser) {} 

  template <typename Iterator>
  ParseResult<Iterator, value_type> parse(Iterator it, Iterator end) const {
    ParseResult<Iterator, typename PadParser::value_type> res1_pad = this->pad_parser.parse(it, end);
    if (!res1_pad.success)
      return ParseResult<Iterator, value_type>(false, value_type(), it);
    ParseResult<Iterator, value_type> res_target = this->target_parser.parse(res1_pad.next, end);
    if (!res_target.success) 
      return ParseResult<Iterator, value_type>(false, value_type(), it);
    ParseResult<Iterator, typename PadParser::value_type> res2_pad = this->pad_parser.parse(res_target.next, end);
    if (!res2_pad.success) 
      return ParseResult<Iterator, value_type>(false, value_type(), it);
    // 本命の値だけを取り出す
    return ParseResult<Iterator, value_type>(true, res_target.value, res2_pad.next);
  }
};

// padのパース結果を無視する
template <typename TargetParser/*本命*/, typename PadParser/*無視*/>
struct IgnoreThenParser {
  typedef typename TargetParser::value_type value_type;
  TargetParser target_parser;
  PadParser pad_parser;

  IgnoreThenParser(const TargetParser& target_parser, const PadParser& pad_parser): target_parser(target_parser), pad_parser(pad_parser) {} 

  template <typename Iterator>
  ParseResult<Iterator, value_type> parse(Iterator it, Iterator end) const {
    ParseResult<Iterator, typename PadParser::value_type> res1_pad = this->pad_parser.parse(it, end);
    if (!res1_pad.success)
      return ParseResult<Iterator, value_type>(false, value_type(), it);
    ParseResult<Iterator, value_type> res_target = this->target_parser.parse(res1_pad.next, end);
    if (!res_target.success) 
      return ParseResult<Iterator, value_type>(false, value_type(), it);
    return ParseResult<Iterator, value_type>(true, res_target.value, res_target.next);
  }
};

// padのパース結果を無視する
template <typename TargetParser/*本命*/, typename PadParser/*無視*/>
struct ThenIgnoreParser {
  typedef typename TargetParser::value_type value_type;
  TargetParser target_parser;
  PadParser pad_parser;

  ThenIgnoreParser(const TargetParser& target_parser, const PadParser& pad_parser): target_parser(target_parser), pad_parser(pad_parser) {} 

  template <typename Iterator>
  ParseResult<Iterator, value_type> parse(Iterator it, Iterator end) const {
    ParseResult<Iterator, value_type> res_target = this->target_parser.parse(it, end);
    if (!res_target.success) 
      return ParseResult<Iterator, value_type>(false, value_type(), it);
    ParseResult<Iterator, typename PadParser::value_type> res2_pad = this->pad_parser.parse(res_target.next, end);
    if (!res2_pad.success) 
      return ParseResult<Iterator, value_type>(false, value_type(), it);
    // 本命の値だけを取り出す
    return ParseResult<Iterator, value_type>(true, res_target.value, res2_pad.next);
  }
};

template <typename TargetParser, typename PadParser>
PaddedParser<TargetParser, PadParser>padded_p(
  const TargetParser& target_parser,
  const PadParser& pad_parser
) {
  return PaddedParser<TargetParser, PadParser>(target_parser, pad_parser);
}

template <typename TargetParser, typename PadParser>
ThenIgnoreParser<TargetParser, PadParser>thenignore_p(
  const TargetParser& target_parser,
  const PadParser& pad_parser
) {
  return ThenIgnoreParser<TargetParser, PadParser>(target_parser, pad_parser);
}

template <typename TargetParser, typename PadParser>
IgnoreThenParser<TargetParser, PadParser>ignorethen_p(
  const TargetParser& target_parser,
  const PadParser& pad_parser
) {
  return IgnoreThenParser<TargetParser, PadParser>(target_parser, pad_parser);
}

template <typename Parser1, typename Parser2>
// where Parser1, Parser2: 
struct OrParser {
  typedef typename Parser1::value_type value_type;
  Parser1 p1;
  Parser2 p2;

  OrParser(const Parser1& a, const Parser2& b) : p1(a), p2(b) {}

  template <typename Iterator>
  ParseResult<Iterator, value_type> parse(Iterator it, Iterator end) const {
    // まずp1を試す
    ParseResult<Iterator, value_type> res1 = p1.parse(it, end);
    if (res1.success) return res1;
    // ダメならp2を試す

    ParseResult<Iterator, value_type> res2 = p2.parse(it, end);
    return res2;
  }
};

template <typename Parser>
struct ChoiceParser {
  typedef typename Parser::value_type value_type;
  std::vector<Parser> ps;

  ChoiceParser(std::vector<Parser> ps): ps(ps) {}

  template<typename Iterator>
  ParseResult<Iterator, value_type> parse(Iterator it, Iterator end) const {
    for (std::size_t i = 0; i < this->ps.size(); i++) {
      ParseResult<Iterator, value_type> res = this->ps[i].parse(it, end);
      if (res.success) return res;
    }
    return ParseResult<Iterator, value_type>(false, value_type(), it);
  }
};

template <typename Parser1, typename Parser2>
struct ThenParser {
  typedef std::pair<typename Parser1::value_type, typename Parser2::value_type>  value_type;
  Parser1 p1;
  Parser2 p2;

  ThenParser(const Parser1& a, const Parser2& b) : p1(a), p2(b) {}

  template <typename Iterator>
  ParseResult<Iterator, value_type> parse(Iterator it, Iterator end) const {
    ParseResult<Iterator, typename Parser1::value_type> res1 = p1.parse(it, end);
    if (!res1.success)
      return ParseResult<Iterator, value_type>(false, value_type(), it);
    ParseResult<Iterator, typename Parser2::value_type> res2 = p2.parse(res1.next, end);
    if (!res2.success)
      return ParseResult<Iterator, value_type>(false, value_type(), it);
    return ParseResult<Iterator, value_type>(true, std::make_pair(res1.value, res2.value), res2.next);
  }
};

// ManyParserはThen<P, Then<P, Then<P, ...>>>
// と同じ意味を持つ
// ただし任意長
//
// ０回以上Parserが適用可能
template <typename Parser>
struct ManyParser {
  typedef std::vector<typename Parser::value_type> value_type;
  Parser p;

  ManyParser(const Parser& a): p(a) {}
  // すべての要素がparserを

  template<typename Iterator>
  ParseResult<Iterator, value_type> parse(Iterator it, Iterator end) const {
    ManyParser::value_type results;  // = std::vector<typename Parser::value_type>();
    Iterator current = it;

    while (current != end) {
      ParseResult<Iterator, typename Parser::value_type> res = this->p.parse(current, end);
      if (!res.success)
        break;
      if (current == res.next)
        break;
      results.push_back(res.value);
      current = res.next;
    }
    return ParseResult<Iterator, value_type>(true, results, current);
  }
};

// 一回以上Parserが適用可能
template <typename Parser>
struct Many1Parser {
  typedef std::vector<typename Parser::value_type> value_type;
  Parser p;

  Many1Parser(const Parser& a): p(a) {}

  template<typename Iterator>
  ParseResult<Iterator, value_type> parse(Iterator it, Iterator end) const {
    Many1Parser::value_type results;  // = std::vector<typename Parser::value_type>();
    Iterator current = it;

    while (current != end) {
      ParseResult<Iterator, typename Parser::value_type> res = this->p.parse(current, end);
      if (!res.success)
        break;
      if (current == res.next)
        break;
      results.push_back(res.value);
      current = res.next;
    }
    if (results.empty())
      return ParseResult<Iterator, value_type>(false, value_type(), it);
    return ParseResult<Iterator, value_type>(true, results, current);
  }
};

template <typename Iterator, typename ValueType>
class Recursive{
private:
  // インターフェイス
  struct Concept {
    virtual ParseResult<Iterator, ValueType> parse(Iterator it, Iterator end) const = 0;
    virtual Concept* clone() const = 0;
    virtual ~Concept() {}
  };

  // インターフェイスを実装する構造体
  template<typename Parser>
  struct Model: Concept {
    Parser p;
    Model(const Parser& parser): p(parser) {}

    virtual ParseResult<Iterator, ValueType> parse(Iterator it, Iterator end) const {
      return this->p.parse(it, end);
    }
    virtual Concept* clone() const {
      return new Model<Parser>(this->p);
    }
  };

  // 内部で保持するParserの型にかかわらず外側の提供するinterfaceは決定
  Concept* ptr;
public:
  // 初期化設定だけする
  // recursiveな定義を作りたい場合は名前だけ先行して欲しい場合があるため
  typedef ValueType value_type;

  Recursive(): ptr(NULL) {}

  // Recursive<...> block = 右辺の巨大な型;としたときに呼ばれるコンストラクタ
  template<typename Parser>
  Recursive(const Parser& parser): ptr(new Model<Parser>(parser)) {}


  // コピーコンストラクタ
  Recursive(const Recursive& other): ptr(other.ptr? other.ptr->clone(): NULL) {}

  Recursive& operator=(const Recursive& other) {
    if (this != &other) {
      delete ptr;
      ptr = other.ptr ? other.ptr->clone() : NULL;
    }
    return *this;
  }

  // 
  template<typename Parser>
  Recursive& operator=(const Parser& parser) {
    delete ptr;
    this->ptr = new Model<Parser>(parser);
    return *this;
  }

  ~Recursive() { delete ptr; }

  ParseResult<Iterator, ValueType> parse(Iterator it, Iterator end) const {
    if (!ptr) { 
      // 直ちに終わらせる
      return ParseResult<Iterator, ValueType>(false, ValueType(), it);
    }
    // インターフェイスが提供する仮想関数越しに内部のパーサーのメソッドにアクセスする
    return ptr->parse(it, end);
  }
};

template <typename Parser>
struct RefParser {
  typedef typename Parser::value_type value_type;

  const Parser *p;

  RefParser(const Parser& p): p(&p) {}

  template<typename Iterator>
  ParseResult<Iterator, value_type>parse(Iterator it, Iterator end) const {
    return p->parse(it, end);
  }
};

template <typename Parser>
RefParser<Parser> ref_p(const Parser& p) {
  return RefParser<Parser>(p);
}

template <typename ParserT, typename FuncT, typename OutValueT>
struct MapParser {
  // 変換後の型を、このパーサーの value_type として公開する
  typedef OutValueT value_type;
  
  ParserT p;
  FuncT f; // 変換処理を行う関数オブジェクト

  MapParser(const ParserT& parser, FuncT func) : p(parser), f(func) {}

  template <typename Iterator>
  ParseResult<Iterator, OutValueT> parse(Iterator it, Iterator end) const {
    // まず中のパーサーを実行
    ParseResult<Iterator, typename ParserT::value_type> res = p.parse(it, end);

    if (res.success) {
      OutValueT mapped_value = f(res.value);
      return ParseResult<Iterator, OutValueT>(true, mapped_value, res.next);
    }
    
    return ParseResult<Iterator, OutValueT>(false, OutValueT(), it);
  }
};

// 使いやすくするためのヘルパー関数
// ※C++98では変換後の型 OutValueT だけは手動で指定する必要があります
template <typename OutValueT, typename ParserT, typename FuncT>
MapParser<ParserT, FuncT, OutValueT> map_p(const ParserT& p, FuncT f) {
    return MapParser<ParserT, FuncT, OutValueT>(p, f);
}

template <typename P1, typename P2>
OrParser<P1, P2> operator|(const P1& p1, const P2& p2) {
  return OrParser<P1, P2>(p1, p2);
}

template <typename P1, typename P2>
ThenParser<P1, P2> operator&(const P1& p1, const P2& p2) {
  return ThenParser<P1, P2>(p1, p2);
}

template <typename P, std::size_t N>
ChoiceParser<P> choice(const P (&arr)[N]) {
  std::vector<P> ps(arr, arr + N);
  return ChoiceParser<P>(ps);
}

template <typename P>
ChoiceParser<P> choice(std::vector<P> ps) {
  return ChoiceParser<P>(ps);
}

template <typename Parser>
ManyParser<Parser> many(const Parser& p) {
  return ManyParser<Parser>(p);
}

template <typename Parser>
Many1Parser<Parser> many1(const Parser& p) {
  return Many1Parser<Parser>(p);
}

#endif
