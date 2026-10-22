#include <catch2/catch.hpp>
#include "abk/bangladesh_helpers.h"
#include "abk/domain_packs.h"
#include "abk/engine.h"
#include "abk/abk.h"
#include "abk/utf8_util.h"

TEST_CASE("Bangladesh Helpers: Lakh/Crore Digit Grouping", "[bangladesh_helpers]") {
    // Bengali digits (০-৯)
    CHECK(abk::BangladeshHelpers::format_lakh_crore(0, true) == "০");
    CHECK(abk::BangladeshHelpers::format_lakh_crore(25, true) == "২৫");
    CHECK(abk::BangladeshHelpers::format_lakh_crore(500, true) == "৫০০");
    CHECK(abk::BangladeshHelpers::format_lakh_crore(1250, true) == "১,২৫০");
    CHECK(abk::BangladeshHelpers::format_lakh_crore(1250000, true) == "১২,৫০,০০০");
    CHECK(abk::BangladeshHelpers::format_lakh_crore(10000000, true) == "১,০০,০০,০০০");
    CHECK(abk::BangladeshHelpers::format_lakh_crore(-1250000, true) == "-১২,৫০,০০০");

    // Standard ASCII digits (0-9)
    CHECK(abk::BangladeshHelpers::format_lakh_crore(0, false) == "0");
    CHECK(abk::BangladeshHelpers::format_lakh_crore(1250, false) == "1,250");
    CHECK(abk::BangladeshHelpers::format_lakh_crore(1250000, false) == "12,50,000");
    CHECK(abk::BangladeshHelpers::format_lakh_crore(10000000, false) == "1,00,00,000");
    CHECK(abk::BangladeshHelpers::format_lakh_crore(-50000, false) == "-50,000");
}

TEST_CASE("Bangladesh Helpers: Number to Words", "[bangladesh_helpers]") {
    CHECK(abk::BangladeshHelpers::number_to_bangla_words(0) == "শূন্য");
    CHECK(abk::BangladeshHelpers::number_to_bangla_words(1) == "এক");
    CHECK(abk::BangladeshHelpers::number_to_bangla_words(12) == "বারো");
    CHECK(abk::BangladeshHelpers::number_to_bangla_words(21) == "একুশ");
    CHECK(abk::BangladeshHelpers::number_to_bangla_words(99) == "নিরানব্বই");
    CHECK(abk::BangladeshHelpers::number_to_bangla_words(100) == "একশত");
    CHECK(abk::BangladeshHelpers::number_to_bangla_words(105) == "একশত পাঁচ");
    CHECK(abk::BangladeshHelpers::number_to_bangla_words(1250) == "এক হাজার দুইশত পঞ্চাশ");
    CHECK(abk::BangladeshHelpers::number_to_bangla_words(1500000) == "পনেরো লক্ষ");
    CHECK(abk::BangladeshHelpers::number_to_bangla_words(75000000) == "সাত কোটি পঞ্চাশ লক্ষ");
    CHECK(abk::BangladeshHelpers::number_to_bangla_words(-50) == "ঋণাত্মক পঞ্চাশ");
}

TEST_CASE("Bangladesh Helpers: Bangabda Calendar & Ordinals", "[bangladesh_helpers]") {
    // Ordinal tests
    CHECK(abk::BangladeshHelpers::get_bangla_ordinal(1) == "১লা");
    CHECK(abk::BangladeshHelpers::get_bangla_ordinal(2) == "২রা");
    CHECK(abk::BangladeshHelpers::get_bangla_ordinal(3) == "৩রা");
    CHECK(abk::BangladeshHelpers::get_bangla_ordinal(4) == "৪ঠা");
    CHECK(abk::BangladeshHelpers::get_bangla_ordinal(5) == "৫ই");
    CHECK(abk::BangladeshHelpers::get_bangla_ordinal(18) == "১৮ই");
    CHECK(abk::BangladeshHelpers::get_bangla_ordinal(19) == "১৯শে");
    CHECK(abk::BangladeshHelpers::get_bangla_ordinal(21) == "২১শে");
    CHECK(abk::BangladeshHelpers::get_bangla_ordinal(31) == "৩১শে");

    // Pahela Boishakh (April 14, 2026 -> 1st Boishakh 1433)
    auto d1 = abk::BangladeshHelpers::gregorian_to_bangabda(2026, 4, 14);
    CHECK(d1.year == 1433);
    CHECK(d1.month == 1);
    CHECK(d1.month_name == "বৈশাখ");
    CHECK(d1.day == 1);
    CHECK(d1.ordinal == "১লা");
    CHECK(d1.formatted == "১লা বৈশাখ ১৪৩৩ বঙ্গাব্দ");

    // Last day of Boishakh (May 14, 2026 -> 31st Boishakh 1433)
    auto d2 = abk::BangladeshHelpers::gregorian_to_bangabda(2026, 5, 14);
    CHECK(d2.year == 1433);
    CHECK(d2.month == 1);
    CHECK(d2.day == 31);
    CHECK(d2.ordinal == "৩১শে");

    // First day of Joishtho (May 15, 2026 -> 1st Joishtho 1433)
    auto d3 = abk::BangladeshHelpers::gregorian_to_bangabda(2026, 5, 15);
    CHECK(d3.year == 1433);
    CHECK(d3.month == 2);
    CHECK(d3.month_name == "জ্যৈষ্ঠ");
    CHECK(d3.day == 1);
    CHECK(d3.ordinal == "১লা");

    // Language Movement Day (February 21, 2026 -> 8th Falgun 1432)
    auto d4 = abk::BangladeshHelpers::gregorian_to_bangabda(2026, 2, 21);
    CHECK(d4.year == 1432);
    CHECK(d4.month == 11);
    CHECK(d4.month_name == "ফাল্গুন");
    CHECK(d4.day == 8);
    CHECK(d4.ordinal == "৮ই");
}

TEST_CASE("Bangladesh Helpers: Sadhu <-> Cholito Conversion", "[bangladesh_helpers]") {
    // Sadhu to Cholito
    std::string sadhu = "তাহারা ভাত খাইতেছে এবং পানি পান করিতেছে।";
    auto res_cholito = abk::BangladeshHelpers::convert_sadhu_to_cholito(sadhu);
    CHECK(res_cholito.text == "তারা ভাত খাচ্ছে এবং পানি পান করছে।");
    CHECK(!res_cholito.flags.empty());

    std::string sadhu2 = "সে সেখানে গিয়াছিল এবং কাজ করিয়াছিল।";
    auto res_cholito2 = abk::BangladeshHelpers::convert_sadhu_to_cholito(sadhu2);
    CHECK(res_cholito2.text == "সে সেখানে গিয়েছিল এবং কাজ করেছিল।");

    std::string sadhu3 = "যাহারা সত্য বলে, তাহাদের সম্মান করিবে।";
    auto res_cholito3 = abk::BangladeshHelpers::convert_sadhu_to_cholito(sadhu3);
    CHECK(res_cholito3.text == "যারা সত্য বলে, তাদের সম্মান করবে।");

    // Cholito to Sadhu
    std::string cholito = "তারা ভাত খাচ্ছে।";
    auto res_sadhu = abk::BangladeshHelpers::convert_cholito_to_sadhu(cholito);
    CHECK(res_sadhu.text == "তাহারা ভাত খাইতেছে।");
}

TEST_CASE("Bangladesh Helpers: Currency Shortcut & Greetings in Engine", "[bangladesh_helpers]") {
    abk::EngineConfig config;
    config.enable_shortcut_expansion = true;
    abk::AmarBanglaKothaEngine engine(config);

    // Typing "tk" should expand to Taka sign "৳"
    auto tk_res = engine.process("tk");
    REQUIRE(!tk_res.candidates.empty());
    CHECK(tk_res.candidates[0].word == "৳");

    // Typing "taka" should also produce "৳"
    auto taka_res = engine.process("taka");
    REQUIRE(!taka_res.candidates.empty());
    bool found_tk = false;
    for (const auto& c : taka_res.candidates) {
        if (c.word == "৳") found_tk = true;
    }
    CHECK(found_tk);

    // Typing "salam" should offer Islamic greeting
    auto salam_res = engine.process("salam");
    REQUIRE(!salam_res.candidates.empty());
    CHECK(salam_res.candidates[0].word == "আসসালামু আলাইকুম");
}

TEST_CASE("Bangladesh Helpers: Domain Dictionary Packs", "[bangladesh_helpers]") {
    abk::DomainPackManager mgr;
    REQUIRE(mgr.load_pack(abk::DomainPackType::BANGLADESH_PLACES, "data/domains/bangladesh_places.tsv"));
    REQUIRE(mgr.load_pack(abk::DomainPackType::ISLAMIC_TERMS, "data/domains/islamic_terms.tsv"));
    REQUIRE(mgr.load_pack(abk::DomainPackType::LEGAL_MEDICAL_BANKING, "data/domains/legal_medical_banking.tsv"));

    CHECK(mgr.get_pack_size(abk::DomainPackType::BANGLADESH_PLACES) > 50);
    CHECK(mgr.get_pack_size(abk::DomainPackType::ISLAMIC_TERMS) > 100);
    CHECK(mgr.get_pack_size(abk::DomainPackType::LEGAL_MEDICAL_BANKING) > 200);

    // Toggling
    mgr.set_pack_enabled(abk::DomainPackType::BANGLADESH_PLACES, false);
    CHECK_FALSE(mgr.is_pack_enabled(abk::DomainPackType::BANGLADESH_PLACES));

    mgr.set_pack_enabled(abk::DomainPackType::BANGLADESH_PLACES, true);
    CHECK(mgr.is_pack_enabled(abk::DomainPackType::BANGLADESH_PLACES));

    // Test engine integration with domain packs
    abk::EngineConfig config;
    abk::AmarBanglaKothaEngine engine(config);
    auto dhaka_res = engine.process("dhaka");
    bool found_dhaka = false;
    for (const auto& c : dhaka_res.candidates) {
        if (c.word == "ঢাকা") found_dhaka = true;
    }
    CHECK(found_dhaka);
}

TEST_CASE("Bangladesh Helpers: C API Integration", "[bangladesh_helpers]") {
    char buf[256];

    // Lakh crore formatting via C API
    int len = abk_format_lakh_crore(1250000, true, buf, sizeof(buf));
    REQUIRE(len > 0);
    CHECK(std::string(buf) == "১২,৫০,০০০");

    // Number to words via C API
    len = abk_number_to_bangla_words(1250, buf, sizeof(buf));
    REQUIRE(len > 0);
    CHECK(std::string(buf) == "এক হাজার দুইশত পঞ্চাশ");

    // Bangabda date via C API
    len = abk_gregorian_to_bangabda(2026, 4, 14, buf, sizeof(buf));
    REQUIRE(len > 0);
    CHECK(std::string(buf) == "১লা বৈশাখ ১৪৩৩ বঙ্গাব্দ");

    // Sadhu to Cholito via C API
    len = abk_convert_sadhu_to_cholito("তাহারা ভাত খাইতেছে।", buf, sizeof(buf));
    REQUIRE(len > 0);
    CHECK(std::string(buf) == "তারা ভাত খাচ্ছে।");
}
