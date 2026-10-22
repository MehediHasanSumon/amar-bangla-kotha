#include "catch2/catch.hpp"
#include "abk/quality_of_life.h"
#include "abk/engine.h"
#include <cstdio>

using namespace abk;

TEST_CASE("QualityOfLife: Smart Punctuation", "[qol]") {
    SECTION("Ellipsis conversion: '...' -> '…'") {
        std::string input = "Opekkha korun... kichukkhon por ashchi...";
        std::string res = QualityOfLife::apply_smart_punctuation(input);
        REQUIRE(res == "Opekkha korun… kichukkhon por ashchi…");
    }

    SECTION("Smart quotes conversion: '\"' -> '“' and '”'") {
        std::string input = "He said \"Hello World\" to everyone";
        std::string res = QualityOfLife::apply_smart_punctuation(input);
        REQUIRE(res == "He said “Hello World” to everyone");
    }

    SECTION("Smart period '.' to Bengali Dari '।'") {
        std::string input = "Ami bhalo achi. Tumi kemon acho.";
        std::string res = QualityOfLife::apply_smart_punctuation(input);
        REQUIRE(res == "Ami bhalo achi। Tumi kemon acho।");
    }

    SECTION("Bypass period in floating point numbers") {
        std::string input = "Pi er man holo 3.14159 motamuti.";
        std::string res = QualityOfLife::apply_smart_punctuation(input);
        REQUIRE(res == "Pi er man holo 3.14159 motamuti।");
    }

    SECTION("Bypass period in web domains and abbreviations") {
        std::string input = "Visit google.com e.g. for search.";
        std::string res = QualityOfLife::apply_smart_punctuation(input);
        REQUIRE(res.find("google.com") != std::string::npos);
        REQUIRE(res.find("e.g.") != std::string::npos);
    }
}

TEST_CASE("QualityOfLife: Emoji Suggestions", "[qol]") {
    SECTION("Smile triggers 😊") {
        auto cands = QualityOfLife::get_emoji_suggestions("smile");
        REQUIRE(!cands.empty());
        bool found = false;
        for (const auto& c : cands) {
            if (c.word == "😊") found = true;
        }
        REQUIRE(found);
    }

    SECTION("Emoticon :) triggers 😊 with high score") {
        auto cands = QualityOfLife::get_emoji_suggestions(":)");
        REQUIRE(!cands.empty());
        REQUIRE(cands[0].word == "😊");
        REQUIRE(cands[0].score > 9000.0);
    }

    SECTION("Love triggers ❤️") {
        auto cands = QualityOfLife::get_emoji_suggestions("bhalobasha");
        REQUIRE(!cands.empty());
        bool found = false;
        for (const auto& c : cands) {
            if (c.word == "❤️") found = true;
        }
        REQUIRE(found);
    }

    SECTION("Fire triggers 🔥 and tk triggers ৳") {
        auto cands_fire = QualityOfLife::get_emoji_suggestions("fire");
        REQUIRE(!cands_fire.empty());
        REQUIRE(cands_fire[0].word == "🔥");

        auto cands_tk = QualityOfLife::get_emoji_suggestions("tk");
        REQUIRE(!cands_tk.empty());
        REQUIRE(cands_tk[0].word == "৳");
    }
}

TEST_CASE("QualityOfLife: Quick Convert Banglish <-> Bangla", "[qol]") {
    SECTION("Banglish to Bangla conversion") {
        std::string input = "ami banglay gan gai";
        std::string res = QualityOfLife::convert_banglish_to_bangla(input);
        REQUIRE(res.find("আমি") != std::string::npos);
        REQUIRE(res.find("গান") != std::string::npos);
    }

    SECTION("Banglish to Bangla preserves URLs and emails") {
        std::string input = "amake email koro sumon@example.com othoba browse koro https://amar-bangla.org thikache?";
        std::string res = QualityOfLife::convert_banglish_to_bangla(input);
        REQUIRE(res.find("sumon@example.com") != std::string::npos);
        REQUIRE(res.find("https://amar-bangla.org") != std::string::npos);
    }

    SECTION("Bangla to Banglish transliteration") {
        std::string input = "আমি বাংলায় কথা বলি";
        std::string res = QualityOfLife::convert_bangla_to_banglish(input);
        REQUIRE(res.find("ami") != std::string::npos);
        REQUIRE(res.find("boli") != std::string::npos);
    }
}

TEST_CASE("QualityOfLife: Local Feedback Reporting", "[qol]") {
    std::string test_feedback_file = "/tmp/test_abk_feedback.jsonl";
    std::remove(test_feedback_file.c_str());

    FeedbackItem item1;
    item1.raw_input = "kichhu";
    item1.received_suggestion = "কিছু";
    item1.expected_word = "কিছু";
    item1.app_name = "gedit";

    FeedbackItem item2;
    item2.raw_input = "ondhokar";
    item2.received_suggestion = "অন্ধকার";
    item2.expected_word = "অন্ধকার";
    item2.app_name = "Chrome";

    REQUIRE(QualityOfLife::save_feedback_locally(item1, test_feedback_file));
    REQUIRE(QualityOfLife::save_feedback_locally(item2, test_feedback_file));

    auto loaded = QualityOfLife::load_local_feedback(test_feedback_file);
    REQUIRE(loaded.size() == 2);
    REQUIRE(loaded[0].raw_input == "kichhu");
    REQUIRE(loaded[0].app_name == "gedit");
    REQUIRE(loaded[1].raw_input == "ondhokar");
    REQUIRE(loaded[1].app_name == "Chrome");

    QualityOfLife::clear_local_feedback(test_feedback_file);
    auto after_clear = QualityOfLife::load_local_feedback(test_feedback_file);
    REQUIRE(after_clear.empty());

    std::remove(test_feedback_file.c_str());
}

TEST_CASE("QualityOfLife: CorrectionLogger Privacy & Persistence", "[qol]") {
    std::string test_log_file = "/tmp/test_abk_correction_log.jsonl";
    std::remove(test_log_file.c_str());

    CorrectionLogger logger(test_log_file);
    REQUIRE_FALSE(logger.is_enabled());

    // Strict Privacy Test 1: When disabled, records are dropped
    logger.record_correction("kotha", "কথা", false);
    REQUIRE(logger.record_count() == 0);

    logger.set_enabled(true);
    REQUIRE(logger.is_enabled());

    // Strict Privacy Test 2: In SECURE / PASSWORD MODE, NOTHING is logged even if enabled
    logger.record_correction("SecretPass123", "SecretPass123", true);
    REQUIRE(logger.record_count() == 0);

    // Normal logging when enabled and NOT secure mode
    logger.record_correction("khela", "খেলা", false);
    logger.record_correction("boi", "বই", false);
    REQUIRE(logger.record_count() == 2);

    auto recs = logger.get_records();
    REQUIRE(recs.size() == 2);
    REQUIRE(recs[0].raw_input == "khela");
    REQUIRE(recs[0].corrected_word == "খেলা");
    REQUIRE(recs[1].raw_input == "boi");
    REQUIRE(recs[1].corrected_word == "বই");

    // Clear records
    logger.clear_records();
    REQUIRE(logger.record_count() == 0);

    std::remove(test_log_file.c_str());
}

TEST_CASE("QualityOfLife: Engine Integration", "[qol]") {
    AmarBanglaKothaEngine engine;
    engine.load_dictionary_file("data/bangla_wordlist.txt");

    SECTION("Emoji candidate included in process()") {
        auto res = engine.process("smile");
        bool found_emoji = false;
        for (const auto& c : res.candidates) {
            if (c.word == "😊" && c.type == CandidateType::EMOJI_SUGGESTION) {
                found_emoji = true;
                break;
            }
        }
        REQUIRE(found_emoji);
    }

    SECTION("Correction logging via record_commit()") {
        std::string test_log = "/tmp/test_engine_correction.jsonl";
        std::remove(test_log.c_str());

        engine.get_correction_logger() = CorrectionLogger(test_log);
        engine.set_correction_logging(true);

        // When secure mode is active, record_commit must not log
        engine.set_secure_mode(true);
        engine.record_commit("mypassword", "mypassword", false);
        REQUIRE(engine.get_correction_logger().record_count() == 0);

        // When secure mode is false, it logs corrections
        engine.set_secure_mode(false);
        engine.record_commit("gaan", "গান", true);
        REQUIRE(engine.get_correction_logger().record_count() == 1);

        engine.get_correction_logger().clear_records();
        std::remove(test_log.c_str());
    }
}
