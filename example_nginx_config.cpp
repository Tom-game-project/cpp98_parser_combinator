#include <cstddef>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>
#include <utility>
#include "parser_combinator.hpp"

// ========================================================== 
//                          ast
// ========================================================== 

struct KeyValueNode;
typedef std::vector<KeyValueNode*> BraceNode;

struct KeyValueNode {
  std::string key;
  std::vector<std::string> values;
  BraceNode brace_node;

  // ★パース失敗時に ParseResult が空の値を返すために必須！
  KeyValueNode() {} 
};

// ========================================================== 
//                          functor
// ========================================================== 

// std::vector<char>をstd::stringにする関数
struct VecCharToString {
  std::string operator()(const std::vector<char> v) const {
    std::string s(v.begin(), v.end());
    return s;
  }
};

struct SemiToEmptyBrace {
  // セミコロン(char) を無視して、空の配列を返す
  BraceNode operator()(char /*c*/) const {
    return BraceNode();
  }
};

struct BuildNode {
  typedef
    std::pair< 
      std::pair<
        std::string, std::vector<std::string> 
      >,
      BraceNode 
    > InputTree;

  KeyValueNode* operator()(const InputTree& p) const {
    KeyValueNode* node = new KeyValueNode();
    node->key = p.first.first;         // 左側の左側が key
    node->values = p.first.second;     // 左側の右側が values

    node->brace_node = p.second;       // 右側が terminator (BraceNode)
    
    return node;
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
  return c_list;
}

// ========================================================== 

void print_key_value_node (KeyValueNode* key_value_node, std::size_t depth) {
  if (key_value_node == NULL) return ;

  std::string depth_space(depth * 4, ' ');

  std::cout << depth_space << "key [" << key_value_node->key << "]" << std::endl;
  for (std::size_t j = 0; j < key_value_node->values.size(); j++) {
    std::cout << depth_space << "value (" << j << ") "<< key_value_node->values[j] << std::endl;
  }

  if (!key_value_node->brace_node.empty()) {
    std::cout << depth_space << "brace_node >> " << std::endl;
    for (std::size_t i = 0; i < key_value_node->brace_node.size(); i++) {
      print_key_value_node(key_value_node->brace_node[i], depth + 1);
    }
  }
}

void free_key_value_node (KeyValueNode* key_value_node) {
  if (key_value_node == NULL) return ;
  for (std::size_t i = 0; i < key_value_node->brace_node.size(); i++) {
    free_key_value_node(key_value_node->brace_node[i]);
  }
  delete key_value_node;
}

// 【重要】ASTを使い終わったらメモリを解放する関数
void free_ast(KeyValueNode* node) {
  if (!node) return;
  for (size_t i = 0; i < node->brace_node.size(); ++i) {
    free_ast(node->brace_node[i]); // 子ノードを再帰的に削除
  }
  delete node; // 自分自身を削除
}


// ========================================================== 
//                          parser
// ========================================================== 
ParseResult<std::string::const_iterator, BraceNode> nginx_like_config_parser(std::string input) {
  typedef std::string::const_iterator Iter;
  typedef ChoiceParser<CharParser<Iter > > SomeChar;
  typedef Many1Parser<SomeChar> Word;

  typedef 
    ThenIgnoreParser< 
      IgnoreThenParser<
      ManyParser<AnyCharExcludeParser<StringParser<Iter>, 1>/* comment close */>,
      StringParser<Iter> > /* comment open */,
    StringParser<Iter> > /*comment close*/
   CommentBlockP;

  typedef ManyParser<OrParser<CommentBlockP, ManyParser<SomeChar> > > Padded;

  typedef PaddedParser<Word, Padded> PaddedWord;
  typedef MapParser<PaddedWord, VecCharToString, std::string> MapWord;

  typedef Recursive<Iter, KeyValueNode*> NodeP;
  typedef PaddedParser<MapParser<PaddedParser<CharParser<Iter>, Padded>, SemiToEmptyBrace,  BraceNode>, Padded> SemiP;

  typedef PaddedParser<
    ThenIgnoreParser< 
      IgnoreThenParser<PaddedParser<
        ManyParser<
          RefParser<
            NodeP
          >
        >,
        Padded
      >,
      PaddedParser<CharParser<Iter>, Padded> >,
    PaddedParser<CharParser<Iter>, Padded> >, 
  Padded> BlockP;

  typedef PaddedParser<OrParser<SemiP, BlockP>, Padded> SemiOrBlockP;
  typedef 
    ThenIgnoreParser<ManyParser<RefParser<NodeP> >, EofParser<Iter> > ConfigP;

  NodeP node_rule;

  CharParser<Iter> padded_char[4] = {
      chr<Iter>(' '),
      chr<Iter>('\n'),
      chr<Iter>('\r'),
      chr<Iter>('\t')
  };

  std::vector<CharParser<Iter> > word_char= generate_word_char() /*a-z A-Z 0-9 -_*/;
  Word some_word = many1(choice(word_char));

  StringParser<Iter> comment_exclude[] = {str<Iter>("\n")};
  CommentBlockP comment_block_p = thenignore_p(
      ignorethen_p(
        many(any_exclude_p(comment_exclude)),
        str<Iter>("#")
      ), 
      str<Iter>("\n"));

  Padded padded_set = many(comment_block_p | many(choice(padded_char)) /* manyが必ず成功してしまうため最後 */);

  PaddedWord padded_word = padded_p(some_word, padded_set);
  MapWord map_word = map_p<std::string>(padded_word, VecCharToString());

  SemiP semi = padded_p(
      map_p<BraceNode>(
        padded_p(chr<Iter>(';'), padded_set),
        SemiToEmptyBrace()), 
      padded_set);

  BlockP block = padded_p(
    thenignore_p(
          ignorethen_p(
            padded_p(many(ref_p(node_rule)), padded_set),
            padded_p(chr<Iter>('{'), padded_set)
          ),
          padded_p(chr<Iter>('}'), padded_set)
      ),
    padded_set);

  // 終端記号は、semiまたはblock(型はどちらも BraceNode に揃える)
  SemiOrBlockP terminator = padded_p(semi | block, padded_set);

  // key & values & terminator を結合し、BuildNode でポインタに変換する
  node_rule = map_p<KeyValueNode*>(
      padded_p(
        map_word & many(map_word) & terminator, padded_set),
      BuildNode()
    );

  ConfigP config_parser = 
    thenignore_p(many(ref_p(node_rule)), eof_p<Iter>());

  Iter it = input.begin();
  Iter end = input.end();

  ParseResult<Iter, BraceNode> res = config_parser.parse(it, end);
  return res;
}

int main() {
  // ========================================================== test00
  {
    std::string input = 
        "server { \n"
        "  listen 80; # こんにちは\n"
        "  server_name localhost; \n"
        "}\n"
        "hello world;";

    ParseResult<std::string::const_iterator, BraceNode> res = nginx_like_config_parser(input);

    if (res.success){
      for (std::size_t i = 0; i < res.value.size(); i++) {
        std::cout << res.value[i]->key <<std::endl;
      }
    }

    std::cout << "パースしたノード数: " << res.value.size() << std::endl;

    // ★最強のデバッグ：パーサーがどこで立ち往生したかを見る
    // std::cout << "停止位置の残りの文字列:\n--->" << std::string(res.next, end) << "<---" << std::endl;

    if (res.success) {
      for (std::size_t i = 0; i < res.value.size(); i++) {
        std::cout << i << std::endl;
        print_key_value_node(res.value[i], 0);
      }
    }

    for (std::size_t i = 0; i < res.value.size(); i++) {
      free_key_value_node(res.value[i]);
    }
  }
}
