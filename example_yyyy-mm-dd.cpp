#include <iostream>
#include <ostream>
#include <string>
#include <utility>
#include "parser_combinator.hpp"

typedef unsigned int YearT;  // 0000 - 9999
typedef unsigned int MonthT;
typedef unsigned int DayT;

template <typename T, typename E>
struct Result {
  enum {
    Ok,
    Err
  } ty;
  union {
    T ok_value;
    E err_value;
  } value;
};

struct Date {
  YearT year;
  MonthT month;
  DayT day;
};

namespace FailedReason {
enum FailedReason {
  BadInput
};
}

unsigned int interpret_char_as_int(char c) {
  return c - '0';
}

struct ToDate {
  Result<Date, FailedReason::FailedReason> operator()(const std::pair<std::pair<YearT, MonthT>, DayT> v) const {
    // // 1. 月と日の下限チェック
    Result<Date, FailedReason::FailedReason> r;

    YearT yyyy = v.first.first;
    MonthT mm = v.first.second;
    DayT dd = v.second;

    if (mm < 1 || mm > 12 || dd < 1) {
        r.ty = Result<Date, FailedReason::FailedReason>::Err;
        r.value.err_value = FailedReason::BadInput;
        return r;
    }

    // 2. 各月の日数テーブル（インデックス1〜12を使用するためにサイズ13で初期化）
    MonthT days_in_month[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    // 3. うるう年の補正
    if (mm == 2) {
        bool is_leap = (yyyy % 4 == 0 && yyyy % 100 != 0) || (yyyy % 400 == 0);
        if (is_leap) {
            days_in_month[2] = 29;
        }
    }

    // 4. 日の上限チェック
    if (dd <= days_in_month[mm]) {
      Date rd;
      rd.year = yyyy;
      rd.month = mm;
      rd.day = dd;
      r.ty = Result<Date, FailedReason::FailedReason>::Ok;
      r.value.ok_value = rd;
      return r;
    } else {
      r.ty = Result<Date, FailedReason::FailedReason>::Err;
      r.value.err_value = FailedReason::BadInput;
      return r;
    }
  }
};

// 入力をyearに変換
// 0000 - 9999の入力のみを受け取る
// 必ず成功する。もし４桁でなければparse error
struct YearMap {
  YearT operator()(const std::pair<std::pair<std::pair<char, char>, char>, char> v) const {
    YearT year =
      interpret_char_as_int(v.first.first.first)  * 1000 +
      interpret_char_as_int(v.first.first.second) * 100  +
      interpret_char_as_int(v.first.second)       * 10   +
      interpret_char_as_int(v.second);
    return year;
  }
};

struct MonthMap {
  MonthT operator()(const std::pair<char, char> v) const {
    MonthT month =
      interpret_char_as_int(v.first) * 10 + 
      interpret_char_as_int(v.second);
    return month;
  }
};

struct DayMap {
  DayT operator()(const std::pair<char, char> v) const {
    MonthT day =
      interpret_char_as_int(v.first) * 10 + 
      interpret_char_as_int(v.second);
    return day;
  }
};

// parser generator
// ここで必要なparserを生成する

int main() {

  typedef std::string::const_iterator Iter;
  // typedef StringParser<Iter> StringP;
  typedef CharParser<Iter> CharP;
  typedef ChoiceParser<CharP> AnyCharP;
  typedef ThenParser<AnyCharP, AnyCharP> CharCharP;
  typedef CharCharP MonthP;  // MM

  typedef CharCharP DayP;    // DD
  typedef ThenParser<
    ThenParser<
      ThenParser<
        AnyCharP,
        AnyCharP>, 
      AnyCharP>,
    AnyCharP> YearP; // 0000-9999

  typedef MapParser<YearP, YearMap, YearT> YearM;
  typedef MapParser<MonthP, MonthMap, MonthT> MonthM;
  typedef MapParser<DayP, DayMap, DayT> DayM;

  typedef 
    ThenParser<
      ThenIgnoreParser<ThenParser<ThenIgnoreParser<YearM, CharP>, MonthM>, CharP>, 
      DayM
    > TimeStampFormatP;
  typedef MapParser<TimeStampFormatP, ToDate, Result<Date, FailedReason::FailedReason> > TimeStampFormatM;

  CharParser<Iter> num_set[] = {
    chr<Iter>('0'),
    chr<Iter>('1'),
    chr<Iter>('2'),
    chr<Iter>('3'),
    chr<Iter>('4'),
    chr<Iter>('5'),
    chr<Iter>('6'),
    chr<Iter>('7'),
    chr<Iter>('8'),
    chr<Iter>('9'),
  };

  CharP sep_p = CharP('-');
  AnyCharP number_p = choice(num_set);

  YearP year_p = number_p & number_p & number_p & number_p;
  YearM year_m = map_p<YearT>(year_p, YearMap());

  CharCharP numnum_p = number_p & number_p;
  MonthM month_m = map_p<MonthT>(numnum_p, MonthMap());
  DayM day_m = map_p<DayT>(numnum_p, DayMap());

  TimeStampFormatP time_stamp_format_p = thenignore_p(thenignore_p(year_m, sep_p) & month_m/*month_p*/, sep_p) & day_m /*day_p*/;

  TimeStampFormatM time_stamp_format_m = map_p<Result<Date, FailedReason::FailedReason> >(time_stamp_format_p, ToDate());

  std::string input = "2026-04-27";
  Iter it = input.begin();
  Iter end = input.end();
  ParseResult<Iter, Result<Date, FailedReason::FailedReason> > parsed_date = time_stamp_format_m.parse(it, end);

  if (parsed_date.success) {
    switch (parsed_date.value.ty) {
      case Result<Date, FailedReason::FailedReason>::Ok:
        {
          Date date = parsed_date.value.value.ok_value;
          std::cout << date.year << "-" << date.month << "-" << date.day << std::endl;
        }
        break;
      case Result<Date, FailedReason::FailedReason>::Err:
        {
          FailedReason::FailedReason fr = parsed_date.value.value.err_value;
          switch (fr) {
            case FailedReason::BadInput:
              {
                std::cout << "BadInput" << std::endl;
              }
              break;
          }
        }
        break;
    }
  } else {
    std::cout << "failed to parse" << std::endl;
  }
}

