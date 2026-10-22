#include "abk_fcitx5.h"
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx-utils/standardpath.h>
#include <cstdlib>

namespace fcitx {

AmarBanglaKothaEngine::AmarBanglaKothaEngine(Instance* instance)
    : instance_(instance), engine_(nullptr) {
    engine_ = abk_engine_create();

    // Locate dictionary
    std::string homeDir = getenv("HOME") ? getenv("HOME") : "";
    std::vector<std::string> dictCandidates = {
        homeDir + "/.local/share/amar-bangla-kotha/bangla_wordlist.txt",
        "/usr/local/share/amar-bangla-kotha/bangla_wordlist.txt",
        "/usr/share/amar-bangla-kotha/bangla_wordlist.txt"
    };

    for (const auto& path : dictCandidates) {
        if (abk_load_dictionary(engine_, path.c_str()) == 1) {
            break;
        }
    }
}

AmarBanglaKothaEngine::~AmarBanglaKothaEngine() {
    if (engine_) {
        abk_engine_destroy(engine_);
        engine_ = nullptr;
    }
}

void AmarBanglaKothaEngine::updatePreedit(InputContext* ic, const std::string& buffer) {
    if (buffer.empty()) {
        primaryText_.clear();
        candidates_.clear();
        ic->inputPanel().reset();
        ic->updatePreedit();
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
        return;
    }

    abk_suggestion_list_t* res = abk_get_suggestions(engine_, buffer.c_str());
    if (!res) return;

    primaryText_ = res->primary_transliteration ? res->primary_transliteration : buffer;
    candidates_.clear();
    for (size_t i = 0; i < res->candidate_count; ++i) {
        if (res->candidates[i].word) {
            candidates_.push_back(res->candidates[i].word);
        }
    }
    abk_free_suggestions(res);

    // Update preedit string
    Text preedit;
    preedit.append(primaryText_, TextFormatFlag::Underline);
    ic->inputPanel().setPreedit(preedit);

    // Update candidate list
    if (!candidates_.empty()) {
        auto candidateList = std::make_unique<CommonCandidateList>();
        for (const auto& cand : candidates_) {
            candidateList->append(std::make_unique<DisplayOnlyCandidateWord>(Text(cand)));
        }
        ic->inputPanel().setCandidateList(std::move(candidateList));
    } else {
        ic->inputPanel().setCandidateList(nullptr);
    }

    ic->updatePreedit();
    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void AmarBanglaKothaEngine::commitWord(InputContext* ic, const std::string& word, bool appendSpace) {
    std::string commitStr = appendSpace ? (word + " ") : word;
    ic->commitString(commitStr);

    buffer_.clear();
    primaryText_.clear();
    candidates_.clear();
    ic->inputPanel().reset();
    ic->updatePreedit();
    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void AmarBanglaKothaEngine::cancelComposition(InputContext* ic) {
    buffer_.clear();
    primaryText_.clear();
    candidates_.clear();
    ic->inputPanel().reset();
    ic->updatePreedit();
    ic->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void AmarBanglaKothaEngine::reset(const InputMethodEntry& /*entry*/, InputContextEvent& event) {
    cancelComposition(event.inputContext());
}

void AmarBanglaKothaEngine::keyEvent(const InputMethodEntry& /*entry*/, KeyEvent& keyEvent) {
    if (keyEvent.isRelease()) return;

    // Pass through Ctrl / Alt / Super shortcuts
    if (keyEvent.rawKey().hasModifier() &&
        (keyEvent.rawKey().checkModifier(KeyModifier::Ctrl) ||
         keyEvent.rawKey().checkModifier(KeyModifier::Alt) ||
         keyEvent.rawKey().checkModifier(KeyModifier::Super))) {
        if (!buffer_.empty()) {
            commitWord(keyEvent.inputContext(), primaryText_, false);
        }
        return;
    }

    Key sym = keyEvent.rawKey().sym();
    auto* ic = keyEvent.inputContext();

    // 1. Escape key
    if (sym == FcitxKey_Escape) {
        if (!buffer_.empty()) {
            cancelComposition(ic);
            keyEvent.filterAndAccept();
            return;
        }
        return;
    }

    // 2. Space key
    if (sym == FcitxKey_space) {
        if (!buffer_.empty()) {
            std::string target = !candidates_.empty() ? candidates_[0] : primaryText_;
            commitWord(ic, target, true);
            keyEvent.filterAndAccept();
            return;
        }
        return;
    }

    // 3. Return / Enter key
    if (sym == FcitxKey_Return || sym == FcitxKey_KP_Enter) {
        if (!buffer_.empty()) {
            std::string target = !candidates_.empty() ? candidates_[0] : primaryText_;
            commitWord(ic, target, false);
            keyEvent.filterAndAccept();
            return;
        }
        return;
    }

    // 4. Backspace
    if (sym == FcitxKey_BackSpace) {
        if (!buffer_.empty()) {
            buffer_.pop_back();
            updatePreedit(ic, buffer_);
            keyEvent.filterAndAccept();
            return;
        }
        return;
    }

    // 5. Candidate selection via 1-9
    if (!buffer_.empty() && sym >= FcitxKey_1 && sym <= FcitxKey_9) {
        size_t idx = static_cast<size_t>(sym - FcitxKey_1);
        if (idx < candidates_.size()) {
            commitWord(ic, candidates_[idx], true);
            keyEvent.filterAndAccept();
            return;
        }
    }

    // 6. Phonetic Latin characters
    if (sym >= 32 && sym < 127) {
        char ch = static_cast<char>(sym);
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == '^' || ch == ':' || ch == '`') {
            buffer_.push_back(ch);
            updatePreedit(ic, buffer_);
            keyEvent.filterAndAccept();
            return;
        }

        // Punctuation
        if (!buffer_.empty() && (ch == '.' || ch == ',' || ch == '!' || ch == '?' ||
                                 ch == ';' || ch == ':' || ch == '-' || ch == '(' || ch == ')')) {
            std::string target = !candidates_.empty() ? candidates_[0] : primaryText_;
            std::string punctStr = (ch == '.') ? "।" : std::string(1, ch);
            commitWord(ic, target + punctStr, false);
            keyEvent.filterAndAccept();
            return;
        }
    }
}

AddonInstance* AmarBanglaKothaFactory::create(AddonManager* /*manager*/) {
    return new AmarBanglaKothaEngine(instance_);
}

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::AmarBanglaKothaFactory);
